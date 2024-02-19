/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * Copyright (C) 2022 embedded brains GmbH (http://www.embedded-brains.de)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <rtems.h>
#include <rtems/shell.h>

#include <grisp.h>

#ifdef GRISP_PLATFORM_GRISP2
#include <bsp/fdt.h>
#include <bsp/imx-gpio.h>

#include <assert.h>
#include <dev/spi/spi.h>
#include <err.h>
#include <fcntl.h>
#include <libfdt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "pmod_rfid.h"

#define MAX14906_SetOUT			0x00
#define MAX14906_SetLED			0x01
#define MAX14906_DoiLevel		0x02
#define MAX14906_Interrupt		0x03
#define MAX14906_OvrLdChF		0x04
#define MAX14906_OpnWirChF		0x05
#define MAX14906_ShtVDDChF		0x06
#define MAX14906_GlobalErr		0x07
#define MAX14906_OpnWrEn		0x08
#define MAX14906_ShtVDDEn		0x09
#define MAX14906_Config1		0x0A
#define MAX14906_Config2		0x0B
#define MAX14906_ConfigDI		0x0C
#define MAX14906_ConfigDO		0x0D
#define MAX14906_CurrLim		0x0E
#define MAX14906_Mask			0x0F

#define MAX14906_REGISTERS		0x10

#define MAX_CASCADED_BOARDS		4

#define	CLEAR_BIT(x, bit)		(x &= (uint8_t)~(1u << (bit)))
#define	SET_BIT(x, bit)			(x |= (uint8_t)(1u << (bit)))

struct pmod_dio_ctx {
	rtems_shell_cmd_t shell_cmd;
	int bus;
	uint8_t cs;
	struct imx_gpio_pin fault;
	struct imx_gpio_pin ready;
	bool initialized;
	uint8_t regs[MAX_CASCADED_BOARDS][MAX14906_REGISTERS];
};

static uint8_t
max14906_build_addr(int board, uint8_t addr, uint8_t write)
{
	return (uint8_t)((board << 6) + (addr << 1) + write);
}

/*
 * Calculate the CRC from first_bit to last_bit. According to AN 6633, the
 * checksum from CPU to MAX14915 is calculated from bit 0 to bit 18 and the
 * checksum from MAX14915 to CPU is calculated from bit 2 to bit 18.
 */
static uint8_t
max14906_crc5encode(uint8_t *buf, size_t first_bit, size_t last_bit)
{
	uint8_t crc5_start = 0x1f;
	uint8_t crc5_poly = 0x15;
	uint8_t crc_result = crc5_start;
	size_t i;

	for (i = first_bit; i <= last_bit; ++i) {
		uint8_t bit = (buf[i/8] >> (7-(i%8))) & 0x01;
		uint8_t crc_result_bit4 = (crc_result >> 4) & 0x01;

		if (bit ^ crc_result_bit4) {
			crc_result =
				(uint8_t)(crc5_poly ^ ((crc_result<<1) & 0x1f));
		} else {
			crc_result = (uint8_t)((crc_result<<1) & 0x1f);
		}
	}

	return crc_result;
}

/*
 * Return returned second byte on success or -1 on error.
 */
static int
pmod_dio_register_rw(
	struct pmod_dio_ctx *ctx,
	int board,
	uint8_t addr,
	uint8_t write,
	uint8_t value
)
{
	uint8_t tx_buf[] = {
		max14906_build_addr(board, addr, write),
		value,
		0
	};
	uint8_t rx_buf[sizeof(tx_buf)];
	spi_ioc_transfer msg = {
		.len = sizeof(tx_buf),
		.rx_buf = rx_buf,
		.tx_buf = tx_buf,
		.speed_hz = 200000,
		.bits_per_word = 8,
		.mode = SPI_MODE_0,
		.cs = ctx->cs,
		.cs_change = true,
	};
	int error;
	uint8_t crc;

	tx_buf[sizeof(tx_buf)-1] = max14906_crc5encode(tx_buf, 0,
			sizeof(tx_buf) * 8 - 6);

	error = ioctl(ctx->bus, SPI_IOC_MESSAGE(1), &msg);
	if (error != 0) {
		return -1;
	}
	crc = max14906_crc5encode(rx_buf, 2, sizeof(rx_buf) * 8 - 6);
	if (crc != (rx_buf[sizeof(rx_buf)-1] & 0x1F)) {
		return -1;
	}
	return rx_buf[1];
}

static int
pmod_dio_register_write(
	struct pmod_dio_ctx *ctx,
	int board,
	uint8_t addr
)
{
	return pmod_dio_register_rw(ctx, board, addr, 1,
	    ctx->regs[board][addr]);
}

static int
pmod_dio_register_read(
	struct pmod_dio_ctx *ctx,
	int board,
	uint8_t addr
)
{
	return pmod_dio_register_rw(ctx, board, addr, 0, 0);
}

static int
pmod_dio_set_op_mode_func(
	struct pmod_dio_ctx *ctx,
	int board,
	int chan,
	int op_mode
)
{
	int rv;

	if (chan < 0 || chan > 3 || op_mode < 0 || op_mode > 6) {
		printf("Usage:\n%s", ctx->shell_cmd.usage);
		return -1;
	};

	 if (op_mode < 4) {
		CLEAR_BIT(ctx->regs[board][MAX14906_SetOUT], chan+4);

		if (op_mode == 0) { /* High-side */
			CLEAR_BIT(ctx->regs[board][MAX14906_ConfigDO], chan*2);
			CLEAR_BIT(ctx->regs[board][MAX14906_ConfigDO], chan*2+1);
		} else if (op_mode == 1) { /* High-side with 2x inrush current */
			SET_BIT(ctx->regs[board][MAX14906_ConfigDO], chan*2);
			CLEAR_BIT(ctx->regs[board][MAX14906_ConfigDO], chan*2+1);
		} else if (op_mode == 2) { /* Active-clamp push-pull */
			CLEAR_BIT(ctx->regs[board][MAX14906_ConfigDO], chan*2);
			SET_BIT(ctx->regs[board][MAX14906_ConfigDO], chan*2+1);
		} else { /* Simple push-pull */
			SET_BIT(ctx->regs[board][MAX14906_ConfigDO], chan*2);
			SET_BIT(ctx->regs[board][MAX14906_ConfigDO], chan*2+1);
		}

		rv = pmod_dio_register_write(ctx, board, MAX14906_SetOUT);
		assert(rv >= 0);
		rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(500));

		rv = pmod_dio_register_write(ctx, board, MAX14906_ConfigDO);
		assert(rv >= 0);
		rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(500));
	} else {
		SET_BIT(ctx->regs[board][MAX14906_SetOUT], chan+4);

		if (op_mode == 4) { /* Low-leakage, High-impedance */
			SET_BIT(ctx->regs[board][MAX14906_ConfigDO], (chan*2+1));
		} else if (op_mode == 5) { /* DI Mode, Type 1 and 3 */
			CLEAR_BIT(ctx->regs[board][MAX14906_ConfigDI], 7);
			CLEAR_BIT(ctx->regs[board][MAX14906_ConfigDO], (chan*2+1));
		} else if (op_mode == 6) { /* DI Mode, Type 2 */
			SET_BIT(ctx->regs[board][MAX14906_ConfigDI], 7);
			CLEAR_BIT(ctx->regs[board][MAX14906_ConfigDO], (chan*2+1));
		}

		rv = pmod_dio_register_write(ctx, board, MAX14906_SetOUT);
		assert(rv >= 0);
		rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(500));

		rv = pmod_dio_register_write(ctx, board, MAX14906_ConfigDO);
		assert(rv >= 0);
		rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(500));

		rv = pmod_dio_register_write(ctx, board, MAX14906_ConfigDI);
		assert(rv >= 0);
		rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(500));
	}

	return 0;
}

static int
pmod_dio_read_input_func(struct pmod_dio_ctx *ctx, int board, int chan)
{
	int rv;

	if (chan < 0 || chan > 3) {
		printf("Usage:\n%s", ctx->shell_cmd.usage);
		return -1;
	};

	/* VDDFaultSe = 0 */
	CLEAR_BIT(ctx->regs[board][MAX14906_ConfigDI], 4);
	rv = pmod_dio_register_write(ctx, board, MAX14906_ConfigDI);
	assert(rv >= 0);
	rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(500));

	/* Read input */
	rv = pmod_dio_register_read(ctx, board, MAX14906_DoiLevel);
	assert(rv >= 0);
	ctx->regs[board][MAX14906_DoiLevel] = (uint8_t) rv;

	if ((rv & (1 << chan)) != 0) {
		printf("input is high\n");
	} else {
		printf("input is low\n");
	}

	return 0;
}

static int
pmod_dio_write_output_func(
	struct pmod_dio_ctx *ctx,
	int board,
	int chan,
	int lvl
)
{
	int rv;

	/* FIXME: Better error handling instead of asserts */

	if (chan < 0 || chan > 3 || lvl < 0 || lvl > 1) {
		printf("Usage:\n%s", ctx->shell_cmd.usage);
		return -1;
	};

	if (lvl == 0) {
		CLEAR_BIT(ctx->regs[board][MAX14906_SetOUT], chan);
	} else {
		SET_BIT(ctx->regs[board][MAX14906_SetOUT], chan);
	}

	rv = pmod_dio_register_write(ctx, board, MAX14906_SetOUT);
	assert(rv >= 0);

	return 0;
}

static int
pmod_dio_init_board_func(struct pmod_dio_ctx *ctx, int board)
{
	size_t i;
	int rv;

	/* FIXME: Better error handling instead of asserts */

	/* copy all registers */
	for (i = 0; i < MAX14906_REGISTERS; ++i) {
		rv = pmod_dio_register_read(ctx, board, (uint8_t)i);
		assert(rv >= 0);
		ctx->regs[board][i] = (uint8_t)rv;
		rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(100));
	}

	/* Set LEDs which are controlled directly by the MAX14906 */
	CLEAR_BIT(ctx->regs[board][MAX14906_Config1], 0);
	CLEAR_BIT(ctx->regs[board][MAX14906_Config1], 1);
	rv = pmod_dio_register_write(ctx, board, MAX14906_Config1);
	assert(rv >= 0);
	rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(100));

	/* Enable gate driver */
	SET_BIT(ctx->regs[board][MAX14906_OpnWrEn], 4);
	SET_BIT(ctx->regs[board][MAX14906_OpnWrEn], 5);
	SET_BIT(ctx->regs[board][MAX14906_OpnWrEn], 6);
	SET_BIT(ctx->regs[board][MAX14906_OpnWrEn], 7);
	rv = pmod_dio_register_write(ctx, board, MAX14906_OpnWrEn);
	assert(rv >= 0);
	rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(100));

	/* current limit up to 1.2 A */
	ctx->regs[board][MAX14906_CurrLim] = 0xff;
	rv = pmod_dio_register_write(ctx, board, MAX14906_CurrLim);
	assert(rv >= 0);
	rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(100));

	return 0;
}

static struct pmod_dio_ctx *
pmod_dio_get_ctx_from_shell(char **argv)
{
	rtems_shell_cmd_t *shell_cmd;

	shell_cmd = rtems_shell_lookup_cmd(argv[0]);
	return RTEMS_CONTAINER_OF(shell_cmd, struct pmod_dio_ctx, shell_cmd);
}

static int
pmod_dio_func(int argc, char **argv)
{
	struct pmod_dio_ctx *ctx = pmod_dio_get_ctx_from_shell(argv);
	int board = 0; /* Number of the cascaded board */
	int second = -1; /* Second numeric argument */
	int third = -1; /* Third numeric argument */

	if (argc < 2) {
		printf("Usage:\n%s", ctx->shell_cmd.usage);
		return -1;
	}

	if (argc >= 3) {
		errno = 0;
		board = (uint8_t)strtol(argv[2], NULL, 0);
		if (errno != 0 || board < 0 || board >= MAX_CASCADED_BOARDS) {
			printf("Usage:\n%s", ctx->shell_cmd.usage);
			return -1;
		}
	}

	if (argc >= 4) {
		errno = 0;
		second = (uint8_t)strtol(argv[3], NULL, 0);
		if (errno != 0) {
			printf("Usage:\n%s", ctx->shell_cmd.usage);
			return -1;
		}
	}

	if (argc >= 5) {
		errno = 0;
		third = (uint8_t)strtol(argv[4], NULL, 0);
		if (errno != 0) {
			printf("Usage:\n%s", ctx->shell_cmd.usage);
			return -1;
		}
	}

	if (strcmp(argv[1], "set_op_mode") == 0) {
		return pmod_dio_set_op_mode_func(ctx, board, second, third);
	} else if (strcmp(argv[1], "init_board") == 0) {
		return pmod_dio_init_board_func(ctx, board);
	} else if (strcmp(argv[1], "read_input") == 0) {
		return pmod_dio_read_input_func(ctx, board, second);
	} else if (strcmp(argv[1], "write_output") == 0) {
		return pmod_dio_write_output_func(ctx, board, second, third);
	} else {
		printf("Usage:\n%s", ctx->shell_cmd.usage);
		return -1;
	}
}

static void
pmod_dio_do_init(const void *fdt, int node, const char *spi_bus)
{
	struct pmod_dio_ctx *ctx;
	const void *prop;
	const char *name;
	static const char topic[] = "dio";
	static const char usage[] = "Call with:\n"
		"  dioX init_board <board>\n"
		"  dioX set_op_mode <board> <chan> <op-mode>\n"
		"  dioX read_input <board> <chan>\n"
		"  dioX write_output <board> <chan> <lvl>\n"
		"where:\n"
		"  <board>   Number of the cascaded board. [0..3]\n"
		"  <chan>    Channel to read / write. [0..3]\n"
		"  <lvl>     Level to set. [0..1]\n"
		"  <op-mode> operational mode:\n"
		"            0: High-side\n"
		"            1: High-side with 2x inrush current\n"
		"            2: Active-clamp push-pull\n"
		"            3: Simple push-pull\n"
		"            4: Low-leakage, High-impedance\n"
		"            5: DI Mode, Type 1 and 3\n"
		"            6: DI Mode, Type 2\n";
	rtems_status_code sc;
	rtems_shell_cmd_t *cmd;

	/* FIXME: Should use better error handling than asserts! But it's only a
	 * test so let's use them for now. */

	name = fdt_get_name(fdt, node, NULL);
	assert(name != NULL);

	ctx = calloc(sizeof(*ctx), 1);
	assert(ctx != NULL);

	ctx->shell_cmd.name = name;
	ctx->shell_cmd.usage = usage;
	ctx->shell_cmd.topic = topic;
	ctx->shell_cmd.command = pmod_dio_func;

	ctx->bus = open(spi_bus, O_RDWR);
	assert(ctx->bus >= 0);

	prop = fdt_getprop(fdt, node, "reg", NULL);
	assert(prop != NULL);
	ctx->cs = (uint8_t) fdt32_ld(prop);

	sc = imx_gpio_init_from_fdt_property(
	    &ctx->fault, node, "maxim,fault", IMX_GPIO_MODE_INPUT, 0);
	assert(sc == RTEMS_SUCCESSFUL);
	sc = imx_gpio_init_from_fdt_property(
	    &ctx->ready, node, "maxim,ready", IMX_GPIO_MODE_INPUT, 0);
	assert(sc == RTEMS_SUCCESSFUL);

	cmd = rtems_shell_add_cmd_struct(&ctx->shell_cmd);
	assert(cmd != NULL);
}

void
pmod_dio_init(const char *spi_bus)
{
	const void *fdt = bsp_fdt_get();
	int node = -1;

	do {
		node = fdt_node_offset_by_compatible(fdt, node,
				"maxim,max14906");

		if (node >= 0) {
			pmod_dio_do_init(fdt, node, spi_bus);
		}
	} while (node >= 0);
}
#endif /* IS_GRISP2 */
