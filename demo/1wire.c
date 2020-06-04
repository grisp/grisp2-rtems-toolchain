/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2020 embedded brains GmbH.
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

/* WARNING: This file is quite hacked together. It is only thought for testing
 * and does nearly no error handling! */

#include <dev/i2c/i2c.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>

#include "1wire.h"

#define DS2482_ADDR 0x18

#define DS2482_CMD_DEVICE_RESET 0xF0
#define DS2482_CMD_SET_READ_POINTER 0xE1
#define DS2482_CMD_WRITE_CONFIG 0xD2
#define DS2482_CMD_1W_RESET 0xB4
#define DS2482_CMD_1W_SINGLE_BIT 0x87
#define DS2482_CMD_1W_WRITE_BYTE 0xA5
#define DS2482_CMD_1W_READ_BYTE 0x96
#define DS2482_CMD_1W_TRIPLET 0x78

#define DS2482_PTR_STATUS 0xF0
#define DS2482_PTR_READ_DATA 0xE1
#define DS2482_PTR_CONFIG 0xC3

#define DS2482_CONFIG_1WS 0x8
#define DS2482_CONFIG_SPU 0x4
#define DS2482_CONFIG_APU 0x1
#define DS2482_CONFIG_GEN(flags) (((~((flags)<<4))&0xf0) | (flags))

#define DS2482_STATUS_DIR 0x80
#define DS2482_STATUS_TSB 0x40
#define DS2482_STATUS_SBR 0x20
#define DS2482_STATUS_RST 0x10
#define DS2482_STATUS_LL  0x08
#define DS2482_STATUS_SD  0x04
#define DS2482_STATUS_PPD 0x02
#define DS2482_STATUS_1WB 0x01

static bool
ds2482_master_reset(int bus)
{
	uint8_t buf[] = {DS2482_CMD_DEVICE_RESET};
	uint8_t rd[1] = {0};
	struct i2c_rdwr_ioctl_data work_queue;
	struct i2c_msg msg[] = {
		{
			.addr = DS2482_ADDR,
			.flags = 0,
			.len = sizeof(buf),
			.buf = buf,
		},{
			.addr = DS2482_ADDR,
			.flags = I2C_M_RD,
			.len = sizeof(rd),
			.buf = rd,
		}
	};

	work_queue.msgs = msg;
	work_queue.nmsgs = sizeof(msg)/sizeof(msg[0]);

	if (ioctl(bus, I2C_RDWR, &work_queue) < 0) {
		puts("Resetting 1-Wire master failed.");
		return false;
	}

	if ((rd[0] & DS2482_STATUS_RST) == 0) {
		printf("Reset bit of 1-Wire not set: 0x%02x\n", rd[0]);
		return false;
	}

	return true;
}

static bool
ds2482_master_set_config(int bus)
{
	uint8_t buf[] = {DS2482_CMD_WRITE_CONFIG, DS2482_CONFIG_GEN(0)};
	uint8_t rd[1] = {0};
	struct i2c_rdwr_ioctl_data work_queue;
	struct i2c_msg msg[] = {
		{
			.addr = DS2482_ADDR,
			.flags = 0,
			.len = sizeof(buf),
			.buf = buf,
		},{
			.addr = DS2482_ADDR,
			.flags = I2C_M_RD,
			.len = sizeof(rd),
			.buf = rd,
		}
	};

	work_queue.msgs = msg;
	work_queue.nmsgs = sizeof(msg)/sizeof(msg[0]);

	if (ioctl(bus, I2C_RDWR, &work_queue) < 0) {
		puts("Setting config failed.");
		return false;
	}

	if (rd[0] != 0) {
		printf("Setting config failed: %02x", rd[0]);
		return false;
	}

	return true;
}

static bool
ds2482_master_1wire_reset(int bus)
{
	uint8_t buf[] = {DS2482_CMD_1W_RESET};
	uint8_t rd[1];
	struct i2c_rdwr_ioctl_data work_queue;
	struct i2c_msg msg[] = {
		{
			.addr = DS2482_ADDR,
			.flags = 0,
			.len = sizeof(buf),
			.buf = buf,
		}
	};
	struct i2c_msg get_status[] = {
		{
			.addr = DS2482_ADDR,
			.flags = I2C_RDWR,
			.len = sizeof(rd),
			.buf = rd,
		}
	};

	work_queue.msgs = msg;
	work_queue.nmsgs = sizeof(msg)/sizeof(msg[0]);

	if (ioctl(bus, I2C_RDWR, &work_queue) < 0) {
		puts("1 Wire reset failed.");
		return false;
	}

	rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(10));

	work_queue.msgs = get_status;
	work_queue.nmsgs = sizeof(get_status)/sizeof(get_status[0]);

	ioctl(bus, I2C_RDWR, &work_queue);
	printf("1 wire status: %02x\n", rd[0]);

	return true;
}

static bool
ds2482_master_1wire_write_byte(int bus, uint8_t byte)
{
	uint8_t buf[] = {DS2482_CMD_1W_WRITE_BYTE, byte};
	struct i2c_rdwr_ioctl_data work_queue;
	struct i2c_msg msg[] = {
		{
			.addr = DS2482_ADDR,
			.flags = 0,
			.len = sizeof(buf),
			.buf = buf,
		}
	};

	work_queue.msgs = msg;
	work_queue.nmsgs = sizeof(msg)/sizeof(msg[0]);

	if (ioctl(bus, I2C_RDWR, &work_queue) < 0) {
		puts("1 Wire write byte failed.");
		return false;
	}

	rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(10));

	return true;
}

static bool
ds2482_master_1wire_read_byte(int bus, uint8_t *byte)
{
	uint8_t buf[] = {DS2482_CMD_1W_READ_BYTE};
	struct i2c_rdwr_ioctl_data work_queue;
	struct i2c_msg msg[] = {
		{
			.addr = DS2482_ADDR,
			.flags = 0,
			.len = sizeof(buf),
			.buf = buf,
		}
	};
	uint8_t buf_srp[] = {DS2482_CMD_SET_READ_POINTER, DS2482_PTR_READ_DATA};
	struct i2c_msg msg_read[] = {
		{
			.addr = DS2482_ADDR,
			.flags = 0,
			.len = sizeof(buf_srp),
			.buf = buf_srp,
		}, {
			.addr = DS2482_ADDR,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = byte,
		}
	};

	work_queue.msgs = msg;
	work_queue.nmsgs = sizeof(msg)/sizeof(msg[0]);

	if (ioctl(bus, I2C_RDWR, &work_queue) < 0) {
		puts("1 Wire read byte failed.");
		return false;
	}

	rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(10));

	work_queue.msgs = msg_read;
	work_queue.nmsgs = sizeof(msg_read)/sizeof(msg_read[0]);

	if (ioctl(bus, I2C_RDWR, &work_queue) < 0) {
		puts("1 Wire read result failed.");
		return false;
	}

	return true;
}

static int
read_MAX31820(int argc, char *argv[])
{
	int i2c_file;
	bool ok;
	uint8_t scratchpad[9];

	(void)argc;
	(void)argv;

	i2c_file = open("/dev/i2c-1", O_RDWR);
	if (i2c_file < 0) {
		warn("Couldn't open i2c bus");
		ok = false;
	}
	if (ok) {
		ok = ds2482_master_reset(i2c_file);
	}
	if (ok) {
		ok = ds2482_master_set_config(i2c_file);
	}
	if (ok) {
		ok = ds2482_master_1wire_reset(i2c_file);
	}
	if (ok) {
		/* Skip ROM */
		ok = ds2482_master_1wire_write_byte(i2c_file, 0xCC);
	}
	if (ok) {
		/* Read Scratchpad */
		ok = ds2482_master_1wire_write_byte(i2c_file, 0xBE);
	}
	for (size_t i = 0; i < sizeof(scratchpad) && ok; ++i) {
		ok = ds2482_master_1wire_read_byte(i2c_file, &scratchpad[i]);
	}

	if (i2c_file >= 0) {
		close(i2c_file);
	}

	if (ok) {
		printf("Scratchpad: ");
		for (size_t i = 0; i < sizeof(scratchpad); ++i) {
			printf("%02x ", scratchpad[i]);
		}
		printf("\n");
	} else {
		printf("Something went wrong\n");
	}

	return 0;
}

rtems_shell_cmd_t shell_1wiretemp_command = {
	"1wiretemp",
	"returns the scratchpad of a max31820 connected via 1wire",
	"app",
	read_MAX31820,
	NULL, NULL, 0, 0, 0
};
