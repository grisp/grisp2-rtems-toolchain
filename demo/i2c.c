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

/* The commands implemented here are heavily simplified versions of the Linux
 * i2c tools. Instead of the bus number they expect a bus path. */

#include <dev/i2c/i2c.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "i2c.h"

static int
command_i2cdetect(int argc, char *argv[])
{
	int fd;
	int rv;
	const char *bus;
	const uint16_t first = 1;
	const uint16_t last = 0x7f;
	uint16_t current;

	if (argc != 2 || strcmp(argv[1], "-h") == 0) {
		warnx("Use with: %s", shell_I2CDETECT_Command.usage);
		return 1;
	}

	bus = argv[1];
	fd = open(bus, O_RDWR);
	if (fd < 0) {
		warn("Couldn't open bus");
		return 1;
	}

	printf("    x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF\n"
	       "0x    ");
	for (current = first; current <= last; ++current) {
		i2c_msg msg = {
			.addr = current,
			.flags = 0,
			.len = 0,
			.buf = NULL,
		};

		struct i2c_rdwr_ioctl_data payload = {
			.msgs = &msg,
			.nmsgs = 1,
		};

		if ((current & 0x0F) == 0) {
			printf("\n%1xx ", current >> 4);
		}

		rv = ioctl(fd, I2C_RDWR, &payload);
		if (rv < 0) {
			if (errno != EIO) {
				warn("ioctl failed");
			}
			printf(" --");
		} else {
			printf(" %02x", current);
		}
	}
	printf("\n");
	close(fd);

	return 0;
}

rtems_shell_cmd_t shell_I2CDETECT_Command = {
	"i2cdetect",
	"i2cdetect I2CBUS",
	"app",
	command_i2cdetect,
	NULL, NULL, 0, 0, 0
};

static int
command_i2cget(int argc, char *argv[])
{
	int fd;
	int rv;
	const char *bus;
	uint16_t chip_address;
	uint8_t data_address;
	uint8_t value;
	i2c_msg msgs[] = {{
		.flags = 0,
		.buf = &data_address,
		.len = 1,
	}, {
		.flags = I2C_M_RD,
		.buf = &value,
		.len = 1,
	}};
	struct i2c_rdwr_ioctl_data payload = {
		.msgs = msgs,
		.nmsgs = sizeof(msgs)/sizeof(msgs[0]),
	};

	if (argc != 4) {
		warnx("Use with: %s", shell_I2CGET_Command.usage);
		return 1;
	}

	errno = 0;
	chip_address = (uint16_t) strtoul(argv[2], NULL, 0);
	if (errno != 0) {
		warn("Couldn't read CHIP_ADDRESS");
	}
	msgs[0].addr = chip_address;
	msgs[1].addr = chip_address;

	errno = 0;
	data_address = (uint8_t) strtoul(argv[3], NULL, 0);
	if (errno != 0) {
		warn("Couldn't read DATA_ADDRESS");
	}

	bus = argv[1];
	fd = open(bus, O_RDWR);
	if (fd < 0) {
		warn("Couldn't open bus");
		return 1;
	}


	rv = ioctl(fd, I2C_RDWR, &payload);
	if (rv < 0) {
		warn("error");
	} else {
		printf("0x%02x\n", value);
	}
	close(fd);

	return rv;
}

rtems_shell_cmd_t shell_I2CGET_Command = {
	"i2cget",
	"i2cget I2CBUS CHIP-ADDRESS DATA-ADDRESS",
	"app",
	command_i2cget,
	NULL, NULL, 0, 0, 0
};

static int
command_i2cset(int argc, char *argv[])
{
	int fd;
	int rv;
	const char *bus;
	uint16_t chip_address;
	uint8_t writebuff[2];
	i2c_msg msgs[] = {{
		.flags = 0,
		.buf = writebuff,
		.len = sizeof(writebuff),
	}};
	struct i2c_rdwr_ioctl_data payload = {
		.msgs = msgs,
		.nmsgs = sizeof(msgs)/sizeof(msgs[0]),
	};

	if (argc != 5) {
		warnx("Use with: %s", shell_I2CSET_Command.usage);
		return 1;
	}

	errno = 0;
	chip_address = (uint16_t) strtoul(argv[2], NULL, 0);
	if (errno != 0) {
		warn("Couldn't read CHIP_ADDRESS");
	}
	msgs[0].addr = chip_address;

	errno = 0;
	writebuff[0] = (uint8_t) strtoul(argv[3], NULL, 0);
	if (errno != 0) {
		warn("Couldn't read DATA_ADDRESS");
	}

	errno = 0;
	writebuff[1] = (uint8_t) strtoul(argv[4], NULL, 0);
	if (errno != 0) {
		warn("Couldn't read VALUE");
	}

	bus = argv[1];
	fd = open(bus, O_RDWR);
	if (fd < 0) {
		warn("Couldn't open bus");
		return 1;
	}

	rv = ioctl(fd, I2C_RDWR, &payload);
	if (rv < 0) {
		warn("error");
	}
	close(fd);

	return rv;
}

rtems_shell_cmd_t shell_I2CSET_Command = {
	"i2cset",
	"i2cset I2CBUS CHIP-ADDRESS DATA-ADDRESS VALUE",
	"app",
	command_i2cset,
	NULL, NULL, 0, 0, 0
};
