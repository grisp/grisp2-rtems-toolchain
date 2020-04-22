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

#include <dev/i2c/i2c.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
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
		warnx("Use with: %s I2CBUS", argv[0]);
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
