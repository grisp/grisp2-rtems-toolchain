/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * Copyright (C) 2020 embedded brains GmbH (http://www.embedded-brains.de)
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

#include <bsp.h>
#ifdef LIBBSP_ARM_ATSAM_BSP_H
#define IS_GRISP1 1
#else
#define IS_GRISP2 1
#endif

#include <rtems.h>
#include <rtems/shell.h>

#ifdef IS_GRISP2
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

/* Address command word */
#define TRF7970_AC_IS_CMD			(1 << 7)
#define TRF7970_AC_READ				(1 << 6)
#define TRF7970_AC_WRITE			(0 << 6)
#define TRF7970_AC_CONTINUOUS			(1 << 5)
#define TRF7970_AC_CONT_READ			(TRF7970_AC_READ | \
						TRF7970_AC_CONTINUOUS)
#define TRF7970_AC_CONT_WRITE			(TRF7970_AC_WRITE | \
						TRF7970_AC_CONTINUOUS)
#define TRF7970_AC_ADDRESS(x)			((x) & 0x1F)

#define TRF7970_AC_CMD_IDLE			(TRF7970_AC_IS_CMD | 0x00)
#define TRF7970_AC_CMD_SW_INIT			(TRF7970_AC_IS_CMD | 0x03)
#define TRF7970_AC_CMD_RF_COLL_AVOIDANCE	(TRF7970_AC_IS_CMD | 0x04)
#define TRF7970_AC_CMD_RESP_RF_COLL_AVOIDANCE	(TRF7970_AC_IS_CMD | 0x05)
#define TRF7970_AC_CMD_RESP_RF_COLL_AVOIDANCE_0	(TRF7970_AC_IS_CMD | 0x06)
#define TRF7970_AC_CMD_RESET_FIFO		(TRF7970_AC_IS_CMD | 0x0F)
#define TRF7970_AC_CMD_TRANSM_WITHOUT_CRC	(TRF7970_AC_IS_CMD | 0x10)
#define TRF7970_AC_CMD_TRANSM_WITH_CRC		(TRF7970_AC_IS_CMD | 0x11)
#define TRF7970_AC_CMD_DELAYED_TRANSM_WITH_CRC	(TRF7970_AC_IS_CMD | 0x13)
#define TRF7970_AC_CMD_EOF_AND_TRANSM_NEXT_SLOT	(TRF7970_AC_IS_CMD | 0x14)
#define TRF7970_AC_CMD_BLOCK_RECEIVER		(TRF7970_AC_IS_CMD | 0x16)
#define TRF7970_AC_CMD_ENABLE_RECEIVER		(TRF7970_AC_IS_CMD | 0x17)
#define TRF7970_AC_CMD_TEST_INTERNAL_RF		(TRF7970_AC_IS_CMD | 0x18)
#define TRF7970_AC_CMD_TEST_EXTERNAL_RF		(TRF7970_AC_IS_CMD | 0x19)

#define TRF7970_REG_CHIP_STATUS_CONTROL		0x00
#define TRF7970_STAT_CTRL_STBY			(1 << 7)
#define TRF7970_STAT_CTRL_DIRECT		(1 << 6)
#define TRF7970_STAT_CTRL_RF_ON			(1 << 5)
#define TRF7970_STAT_CTRL_RF_PWR		(1 << 4)
#define TRF7970_STAT_CTRL_PM_ON			(1 << 3)
#define TRF7970_STAT_CTRL_REC_ON		(1 << 1)
#define TRF7970_STAT_CTRL_VRS5_3		(1 << 0)

#define TRF7970_REG_ISO_CONTROL			0x01
#define TRF7970_ISO_CTRL_RX_CRC_N		(1 << 7)
#define TRF7970_ISO_CTRL_DIR_MODE		(1 << 6)
#define TRF7970_ISO_CTRL_RFID			(1 << 5)
#define TRF7970_ISO_CTRL_ISO_4			(1 << 4)
#define TRF7970_ISO_CTRL_ISO_3			(1 << 3)
#define TRF7970_ISO_CTRL_ISO_2			(1 << 2)
#define TRF7970_ISO_CTRL_ISO_1			(1 << 1)
#define TRF7970_ISO_CTRL_ISO_0			(1 << 0)

#define TRF7970_REG_ISO_14443_B_TX_OPTS		0x02
#define TRF7970_REG_ISO_14443_A_HIGH_BR_OPTS	0x03
#define TRF7970_REG_TX_TIMER_HIGH_CTRL		0x04
#define TRF7970_REG_TX_TIMER_LOW_CTRL		0x05
#define TRF7970_REG_PULSE_LENGTH_CTRL		0x06
#define TRF7970_REG_RX_NO_RESPONSE_WAIT_TIME	0x07
#define TRF7970_REG_RX_WAIT_TIME		0x08

#define TRF7970_REG_MODULAR_AND_SYS_CLK_CTRL	0x09
#define TRF7970_MODSCK_27MHz			(1 << 7)
#define TRF7970_MODSCK_EN_OOK_P			(1 << 6)
#define TRF7970_MODSCK_CLO1			(1 << 5)
#define TRF7970_MODSCK_CLO0			(1 << 4)
#define TRF7970_MODSCK_EN_ANA			(1 << 3)
#define TRF7970_MODSCK_PM2			(1 << 2)
#define TRF7970_MODSCK_PM1			(1 << 1)
#define TRF7970_MODSCK_PM0			(1 << 0)

#define TRF7970_REG_RX_SPECIAL_SETTING		0x0a

#define TRF7970_REG_REGULATOR_AND_IO_CTRL	0x0b
#define TRF7970_REGIOCTL_AUTO_REG		(1 << 7)
#define TRF7970_REGIOCTL_EN_EXT_PA		(1 << 6)
#define TRF7970_REGIOCTL_IO_LOW			(1 << 5)
#define TRF7970_REGIOCTL_VRS2			(1 << 2)
#define TRF7970_REGIOCTL_VRS1			(1 << 1)
#define TRF7970_REGIOCTL_VRS0			(1 << 0)

#define TRF7970_REG_SPECIAL_FUNC_REG1		0x10
#define TRF7970_REG_SPECIAL_FUNC_REG2		0x11
#define TRF7970_REG_ADJUSTABLE_FIFO_IRQ_LVL	0x14
#define TRF7970_REG_RESERVED			0x15
#define TRF7970_REG_NFC_LOW_FIELD_LVL		0x16
#define TRF7970_REG_NFCID1_NUMBER		0x17
#define TRF7970_REG_NFC_TARGET_DETECTION_LVL	0x18
#define TRF7970_REG_NFC_TARGET_PROTOCOL		0x19

#define TRF7970_REG_IRQ_STATUS			0x0c
#define TRF7970_IRQ_TX				(1 << 7)
#define TRF7970_IRQ_SRX				(1 << 6)
#define TRF7970_IRQ_FIFO			(1 << 5)
#define TRF7970_IRQ_ERR1_CRC			(1 << 4)
#define TRF7970_IRQ_ERR2_PARITY			(1 << 3)
#define TRF7970_IRQ_ERR3_FRAMING_OR_EOF		(1 << 2)
#define TRF7970_IRQ_COL				(1 << 1)
#define TRF7970_IRQ_NORESP			(1 << 0)

#define TRF7970_REG_COLLISION_POS_AND_IRQ_MASK	0x0d
#define TRF7970_REG_COLLISION_POS		0x0e
#define TRF7970_REG_RSSI_LVL_AND_OSC_STATUS	0x0f

#define TRF7970_REG_RAM1			0x12
#define TRF7970_REG_RAM2			0x13

#define TRF7970_REG_TEST1			0x1A
#define TRF7970_REG_TEST2			0x1B

#define TRF7970_REG_FIFO_STATUS			0x1C
#define TRF7970_REG_TX_LENGTH1			0x1D
#define TRF7970_REG_TX_LENGTH2			0x1E
#define TRF7970_REG_FIFO_IO_REG			0x1F

#define verb_print(ctx, level, ...) \
	do { \
		if (ctx->verbose >= level) { \
			printf(__VA_ARGS__); \
		} \
	} while(0)

struct pmod_rfid_ctx {
	int bus;
	uint8_t cs;
	bool initialized;
	enum {
		VERBOSE_FEW = 0,
		VERBOSE_SOME,
		VERBOSE_MORE,
		VERBOSE_ALL,
	} verbose;
	struct imx_gpio_pin led;
	struct imx_gpio_pin interrupt;
	bool led_detection;
} context;

static struct pmod_rfid_ctx *
pmod_rfid_get_context(void) {
	return &context;
}

/*
 * Check whether there is input available on stdin.
 * Return > 0 if yes. Return 0 on timeout. Return -1 on error.
 */
static int
input_available(long int timeout_ms)
{
	fd_set set;
	struct timeval timeout = {
	    .tv_sec = timeout_ms / 1000,
	    .tv_usec = (timeout_ms % 1000) * 1000,
	    };
	int rv;

	FD_ZERO(&set);
	FD_SET(STDIN_FILENO, &set);

	rv = select(STDIN_FILENO+1, &set, NULL, NULL, &timeout);
	if (rv < 0) {
		warn("Select failed: ");
	}
	return rv;
}

/*
 * Note: txbuf or rxbuf might can be NULL. They also might can be the same.
 */
static int
pmod_rfid_transfer(
	struct pmod_rfid_ctx *ctx,
	const uint8_t *txbuf,
	uint8_t *rxbuf,
	size_t len
)
{
	int error;
	spi_ioc_transfer msg = {
		.len = len,
		.rx_buf = rxbuf,
		.tx_buf = txbuf,
		.speed_hz = 2000000,
		.bits_per_word = 8,
		.mode = SPI_MODE_1,
		.cs = ctx->cs,
		.cs_change = true,
	};

	verb_print(ctx, VERBOSE_ALL, "Tx: ");
	for (size_t i = 0; i < len; ++i) {
		verb_print(ctx, VERBOSE_ALL, "%02x ",
		    txbuf != NULL ? txbuf[i] : 0);
	}
	verb_print(ctx, VERBOSE_ALL, "\n");

	error = ioctl(ctx->bus, SPI_IOC_MESSAGE(1), &msg);
	if (error != 0) {
		warn("PMOD_RFID: Error during transfer: ");
	} else if (rxbuf != NULL) {
		verb_print(ctx, VERBOSE_ALL, "Rx: ");
		for (size_t i = 0; i < len; ++i) {
			verb_print(ctx, VERBOSE_ALL, "%02x ", rxbuf[i]);
		}
		verb_print(ctx, VERBOSE_ALL, "\n");
	}
	return error;
}

static int
pmod_rfid_check_irq_status(
	struct pmod_rfid_ctx *ctx,
	uint8_t expected_flags,
	uint8_t *received_flags,
	unsigned verbosity
)
{
	uint8_t buf[] = {TRF7970_AC_READ | TRF7970_REG_IRQ_STATUS, 0};
	int error;

	verb_print(ctx, verbosity, "Check IRQ status\n");
	error = pmod_rfid_transfer(ctx, buf, buf, sizeof(buf));
	if (error == 0) {
		if (received_flags != NULL) {
			*received_flags = buf[1];
		}
		if ((buf[1] & ~expected_flags) != 0) {
			printf("Unexpected IRQ status: 0x%02x\n", buf[1]);
			error = -1;
		}
	}
	return error;
}

static void
pmod_rfid_led_off(struct pmod_rfid_ctx *ctx) {
	imx_gpio_set_output(&ctx->led, 1);
}

static void
pmod_rfid_led_on(struct pmod_rfid_ctx *ctx) {
	imx_gpio_set_output(&ctx->led, 0);
}

static int
pmod_rfid_cmd_regdump_func(int argc, char **argv)
{
	struct pmod_rfid_ctx *ctx = pmod_rfid_get_context();
	int error;
	uint8_t buf[TRF7970_REG_TX_LENGTH2 + 1] = {
	    TRF7970_AC_CONT_READ | TRF7970_AC_ADDRESS(0)};

	(void) argc;
	(void) argv;

	error = pmod_rfid_transfer(ctx, buf, buf, sizeof(buf));
	if (error == 0) {
		printf("=== Main Control Registers\n"
		    "CHIP_STATUS_CONTROL            0x%02x\n"
		    "ISO_CONTROL                    0x%02x\n"
		    "ISO_14443_B_TX_OPTS            0x%02x\n"
		    "ISO_14443_A_HIGH_BR_OPTS       0x%02x\n"
		    "TX_TIMER_HIGH_CTRL             0x%02x\n"
		    "TX_TIMER_LOW_CTRL              0x%02x\n"
		    "PULSE_LENGTH_CTRL              0x%02x\n"
		    "RX_NO_RESPONSE_WAIT_TIME       0x%02x\n"
		    "RX_WAIT_TIME                   0x%02x\n"
		    "MODULAR_AND_SYS_CLK_CTRL       0x%02x\n"
		    "RX_SPECIAL_SETTING             0x%02x\n"
		    "REGULATOR_AND_IO_CTRL          0x%02x\n"
		    "SPECIAL_FUNC_REG1              0x%02x\n"
		    "SPECIAL_FUNC_REG2              0x%02x\n"
		    "ADJUSTABLE_FIFO_IRQ_LVL        0x%02x\n"
		    "RESERVED                       0x%02x\n"
		    "NFC_LOW_FIELD_LVL              0x%02x\n"
		    "NFCID1_NUMBER                  0x%02x\n"
		    "NFC_TARGET_DETECTION_LVL       0x%02x\n"
		    "NFC_TARGET_PROTOCOL            0x%02x\n"
		    "=== Status Registers\n"
		    "IRQ_STATUS                     0x%02x\n"
		    "COLLISION_POS_AND_IRQ_MASK     0x%02x\n"
		    "COLLISION_POS                  0x%02x\n"
		    "RSSI_LVL_AND_OSC_STATUS        0x%02x\n"
		    "=== RAM\n"
		    "RAM1                           0x%02x\n"
		    "RAM2                           0x%02x\n"
		    "=== Test Registers\n"
		    "TEST1                          0x%02x\n"
		    "TEST2                          0x%02x\n"
		    "=== FIFO Registers\n"
		    "FIFO_STATUS                    0x%02x\n"
		    "TX_LENGTH1                     0x%02x\n"
		    "TX_LENGTH2                     0x%02x\n",
		    buf[TRF7970_REG_CHIP_STATUS_CONTROL+1],
		    buf[TRF7970_REG_ISO_CONTROL+1],
		    buf[TRF7970_REG_ISO_14443_B_TX_OPTS+1],
		    buf[TRF7970_REG_ISO_14443_A_HIGH_BR_OPTS+1],
		    buf[TRF7970_REG_TX_TIMER_HIGH_CTRL+1],
		    buf[TRF7970_REG_TX_TIMER_LOW_CTRL+1],
		    buf[TRF7970_REG_PULSE_LENGTH_CTRL+1],
		    buf[TRF7970_REG_RX_NO_RESPONSE_WAIT_TIME+1],
		    buf[TRF7970_REG_RX_WAIT_TIME+1],
		    buf[TRF7970_REG_MODULAR_AND_SYS_CLK_CTRL+1],
		    buf[TRF7970_REG_RX_SPECIAL_SETTING+1],
		    buf[TRF7970_REG_REGULATOR_AND_IO_CTRL+1],
		    buf[TRF7970_REG_SPECIAL_FUNC_REG1+1],
		    buf[TRF7970_REG_SPECIAL_FUNC_REG2+1],
		    buf[TRF7970_REG_ADJUSTABLE_FIFO_IRQ_LVL+1],
		    buf[TRF7970_REG_RESERVED+1],
		    buf[TRF7970_REG_NFC_LOW_FIELD_LVL+1],
		    buf[TRF7970_REG_NFCID1_NUMBER+1],
		    buf[TRF7970_REG_NFC_TARGET_DETECTION_LVL+1],
		    buf[TRF7970_REG_NFC_TARGET_PROTOCOL+1],
		    buf[TRF7970_REG_IRQ_STATUS+1],
		    buf[TRF7970_REG_COLLISION_POS_AND_IRQ_MASK+1],
		    buf[TRF7970_REG_COLLISION_POS+1],
		    buf[TRF7970_REG_RSSI_LVL_AND_OSC_STATUS+1],
		    buf[TRF7970_REG_RAM1+1],
		    buf[TRF7970_REG_RAM2+1],
		    buf[TRF7970_REG_TEST1+1],
		    buf[TRF7970_REG_TEST2+1],
		    buf[TRF7970_REG_FIFO_STATUS+1],
		    buf[TRF7970_REG_TX_LENGTH1+1],
		    buf[TRF7970_REG_TX_LENGTH2+1]);
	}

	return error;
}

static rtems_shell_cmd_t pmod_rfid_cmd_regdump = {
	.name = "rfid_regdump",
	.usage = "rfid_regdump\n"
	    "Register dump for all registers of the TRF7970A.\n",
	.topic = "rfid",
	.command = pmod_rfid_cmd_regdump_func,
};

static int
pmod_rfid_cmd_init_func(int argc, char **argv)
{
	struct pmod_rfid_ctx *ctx = pmod_rfid_get_context();
	int error = 0;
	bool use_5V = false;

	if (ctx->initialized) {
		verb_print(ctx, VERBOSE_FEW, "Reinitializing\n");
		ctx->initialized = false;
	}
	if (argc >= 2) {
		if(strcmp(argv[1], "5") == 0) {
			use_5V = true;
			verb_print(ctx, VERBOSE_FEW, "Use 5V\n");
		} else {
			printf("Unknown parameter: %s\n", argv[1]);
			return -1;
		}
	}

	if (error == 0) {
		/* Software reset */
		uint8_t buf[] = {TRF7970_AC_CMD_SW_INIT};
		verb_print(ctx, VERBOSE_SOME, "Initiate software reset\n");
		error = pmod_rfid_transfer(ctx, buf, NULL, sizeof(buf));
	}
	if (error == 0) {
		uint8_t buf[] = {TRF7970_AC_CMD_IDLE};
		verb_print(ctx, VERBOSE_SOME, "Wait cycles for reset\n");
		error = pmod_rfid_transfer(ctx, buf, NULL, sizeof(buf));
	}
	rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(1));
	if (error == 0) {
		uint8_t buf[] = {TRF7970_AC_CMD_RESET_FIFO};
		verb_print(ctx, VERBOSE_SOME, "Reset FIFO\n");
		error = pmod_rfid_transfer(ctx, buf, NULL, sizeof(buf));
	}
	if (error == 0) {
		uint8_t buf[] = {
		    TRF7970_AC_WRITE | TRF7970_REG_CHIP_STATUS_CONTROL,
		    TRF7970_STAT_CTRL_RF_ON
		    };
		verb_print(ctx, VERBOSE_SOME, "Setup status control\n");
		if (use_5V) {
			buf[1] |= TRF7970_STAT_CTRL_VRS5_3;
		}
		error = pmod_rfid_transfer(ctx, buf, NULL, sizeof(buf));
	}
	if (error == 0) {
		uint8_t buf[] = {
		    TRF7970_AC_WRITE | TRF7970_REG_ISO_CONTROL,
		    TRF7970_ISO_CTRL_ISO_1
		    };
		verb_print(ctx, VERBOSE_SOME, "Setup ISO control\n");
		error = pmod_rfid_transfer(ctx, buf, NULL, sizeof(buf));
	}
	if (error == 0) {
		uint8_t buf[] = {
		    TRF7970_AC_WRITE | TRF7970_REG_MODULAR_AND_SYS_CLK_CTRL,
		    TRF7970_MODSCK_PM2 | TRF7970_MODSCK_PM1 | TRF7970_MODSCK_PM0
		    };
		verb_print(ctx, VERBOSE_SOME, "Setup Modulator and Clock control\n");
		error = pmod_rfid_transfer(ctx, buf, NULL, sizeof(buf));
	}
	if (error == 0) {
		uint8_t buf[] = {
		    TRF7970_AC_WRITE | TRF7970_REG_REGULATOR_AND_IO_CTRL,
		    TRF7970_REGIOCTL_AUTO_REG
		    };
		verb_print(ctx, VERBOSE_SOME, "Setup Regulator and I/O Control\n");
		error = pmod_rfid_transfer(ctx, buf, NULL, sizeof(buf));
	}
	if (error == 0) {
		uint8_t buf[] = {
		    TRF7970_AC_WRITE | TRF7970_REG_NFC_TARGET_DETECTION_LVL,
		    0 /* according to data sheet! */
		    };
		verb_print(ctx, VERBOSE_SOME, "Set NFC Target detection level to 0\n");
		error = pmod_rfid_transfer(ctx, buf, NULL, sizeof(buf));
	}
	if (error == 0) {
		error = pmod_rfid_check_irq_status(ctx, 0, NULL, VERBOSE_SOME);
	}
	if (error == 0) {
		verb_print(ctx, VERBOSE_FEW, "Success\n");
		ctx->initialized = true;
	} else {
		verb_print(ctx, VERBOSE_FEW, "Failure\n");
	}

	return error;
}

static rtems_shell_cmd_t pmod_rfid_cmd_init = {
	.name = "rfid_init",
	.usage = "rfid_init [5]\n"
	    "Initialize the TRF7970A.\n"
	    "With argument 5: Initialize for 5V.\n",
	.topic = "rfid",
	.command = pmod_rfid_cmd_init_func,
};

static int
pmod_rfid_cmd_detect_func(int argc, char **argv)
{
	struct pmod_rfid_ctx *ctx = pmod_rfid_get_context();
	int error = 0;
	bool stop = false;
	size_t activity = 0;

	(void) argc;
	(void) argv;

	if (!ctx->initialized) {
		verb_print(ctx, VERBOSE_FEW, "Not yet initialized. Doing that now ...\n");
		pmod_rfid_cmd_init_func(argc, argv);
	}

	printf( "Press any key to stop\n" );

	while (error == 0 && !stop) {
		uint8_t irq_status = 0;
		uint8_t retry_count = 2;
		const char indicator[] = ".oOo";
		size_t act_index = (activity / 8) % (sizeof(indicator) - 1);
		++activity;

		if (error == 0) {
			uint8_t buf[] = {
				TRF7970_AC_CMD_RESET_FIFO,
				TRF7970_AC_CMD_TRANSM_WITH_CRC,
				TRF7970_AC_CONT_WRITE | TRF7970_REG_TX_LENGTH1,
				0x00, 0x30, /* both length registers */
				0x26, 0x01, 0x00 /* all three will go into FIFO data */
				};
			verb_print(ctx, VERBOSE_MORE, "Setup for tag detection and prepare data for tag\n");
			error = pmod_rfid_transfer(ctx, buf, NULL, sizeof(buf));
		}
		while (error == 0 && retry_count > 0 &&
		    (irq_status & TRF7970_IRQ_SRX) == 0) {
			--retry_count;
			rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(1));
			error = pmod_rfid_check_irq_status(ctx,
			    TRF7970_IRQ_TX | TRF7970_IRQ_SRX, &irq_status,
			    VERBOSE_MORE);
		}
		if ((irq_status & TRF7970_IRQ_SRX) == 0) {
			if (ctx->led_detection) {
				pmod_rfid_led_off(ctx);
			}
			verb_print(ctx, VERBOSE_FEW, "\r%c No tag      ",
			    indicator[act_index]);
		} else {
			if (ctx->led_detection) {
				pmod_rfid_led_on(ctx);
			}
			verb_print(ctx, VERBOSE_FEW, "\r%c Tag detected",
			    indicator[act_index]);
		}

		if (input_available(10)) {
			stop = true;
		}
	}
	verb_print(ctx, VERBOSE_FEW, "\n");

	if (error != 0) {
		printf("Stopped due to an error.\n");
	}
	return error;
}

static rtems_shell_cmd_t pmod_rfid_cmd_detect = {
	.name = "rfid_detect",
	.usage = "rfid_detect\n"
	    "Permanently searches for a tag. Terminate with any key press.\n",
	.topic = "rfid",
	.command = pmod_rfid_cmd_detect_func,
};

static int
pmod_rfid_cmd_verbose_func(int argc, char **argv)
{
	struct pmod_rfid_ctx *ctx = pmod_rfid_get_context();

	if (argc < 2) {
		printf("Current verbose level: %d\n", ctx->verbose);
	} else {
		errno = 0;
		ctx->verbose = strtol(argv[1], NULL, 0);
		if (errno != 0) {
			warn("Couldn't process '%s'. Default to 0", argv[1]);
		}
	}

	return 0;
}

static rtems_shell_cmd_t pmod_rfid_cmd_verbose = {
	.name = "rfid_verbose",
	.usage = "rfid_verbose [<level>]\n"
	    "Get or set verbose level for RFID commands.\n",
	.topic = "rfid",
	.command = pmod_rfid_cmd_verbose_func,
};

static int
pmod_rfid_cmd_led_func(int argc, char **argv)
{
	struct pmod_rfid_ctx *ctx = pmod_rfid_get_context();

	if (argc < 2) {
		printf("Should the LED be 'on', 'off' or detection based ('detect')?\n");
	} else {
		if (strcmp(argv[1], "on") == 0) {
			pmod_rfid_led_on(ctx);
			ctx->led_detection = false;
		} else if (strcmp(argv[1], "off") == 0) {
			pmod_rfid_led_off(ctx);
			ctx->led_detection = false;
		} else if (strcmp(argv[1], "detect") == 0) {
			ctx->led_detection = true;
		}
	}

	return 0;
}

static rtems_shell_cmd_t pmod_rfid_cmd_led = {
	.name = "rfid_led",
	.usage = "rfid_led [on|off|detect]\n"
	    "Set LED function.\n",
	.topic = "rfid",
	.command = pmod_rfid_cmd_led_func,
};

static rtems_status_code
pmod_rfid_init_pins(struct pmod_rfid_ctx *ctx)
{
	rtems_status_code sc;
	int node;
	const void *fdt;

	fdt = bsp_fdt_get();

	node = fdt_node_offset_by_compatible(fdt, -1, "ti,trf7970a");
	if (node < 0) {
		return RTEMS_UNSATISFIED;
	}

	/* LED */
	sc = imx_gpio_init_from_fdt_property(&ctx->led,
	    node, "grisp,led-gpios", IMX_GPIO_MODE_OUTPUT, 0);
	if (sc != RTEMS_SUCCESSFUL) {
		return sc;
	}

	/* Interrupt pin */
	/* FIXME: The interrupt pin isn't used yet. Initialization is done so it
	 * can be checked with a debugger whether the pin works. */
	sc = imx_gpio_init_from_fdt_property(&ctx->interrupt,
	    node, "grisp,int-gpios", IMX_GPIO_MODE_INPUT, 0);

	return sc;
}

void
pmod_rfid_init(const char *spi_bus, uint8_t cs) {
	struct pmod_rfid_ctx *ctx = pmod_rfid_get_context();
	rtems_shell_cmd_t *cmd;
	rtems_status_code sc;

	ctx->initialized = false;
	ctx->cs = cs;
	ctx->verbose = VERBOSE_FEW;
	ctx->bus = open(spi_bus, O_RDWR);
	assert(ctx->bus >= 0);
	sc = pmod_rfid_init_pins(ctx);
	assert(sc == RTEMS_SUCCESSFUL);
	ctx->led_detection = true;

	cmd = rtems_shell_add_cmd_struct(&pmod_rfid_cmd_regdump);
	assert(cmd == &pmod_rfid_cmd_regdump);
	cmd = rtems_shell_add_cmd_struct(&pmod_rfid_cmd_init);
	assert(cmd == &pmod_rfid_cmd_init);
	cmd = rtems_shell_add_cmd_struct(&pmod_rfid_cmd_detect);
	assert(cmd == &pmod_rfid_cmd_detect);
	cmd = rtems_shell_add_cmd_struct(&pmod_rfid_cmd_verbose);
	assert(cmd == &pmod_rfid_cmd_verbose);
	cmd = rtems_shell_add_cmd_struct(&pmod_rfid_cmd_led);
	assert(cmd == &pmod_rfid_cmd_led);
}
#endif /* IS_GRISP2 */
