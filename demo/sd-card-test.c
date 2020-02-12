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

#ifdef __rtems__
#include "sd-card-test.h"
#endif /* __rtems__ */

#include <arpa/inet.h>
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static int
check_and_process_params(
	int argc,
	char *argv[],
	int open_flags,
	int *fd,
	size_t *size,
	size_t *block_size,
	uint8_t **block,
	uint8_t **read_block
)
{
	if (argc != 4) {
		printf("Use with %s <file> <size> <block_size>\n"
		    "    <size> and <block_size> is in bytes\n",
		    argv[0]);
		return -1;
	}

	*size = strtoul(argv[2], NULL, 0);
	if (size == 0) {
		warn("Couldn't convert size or size set to 0");
		return -1;
	}

	*block_size = strtoul(argv[3], NULL, 0);
	if (block_size == 0) {
		warn("Couldn't convert block_size or block_size set to 0");
		return -1;
	}

	if (block != NULL) {
		*block = malloc(*block_size);
		if (*block == NULL) {
			warn("Couldn't allocate block");
			return -1;
		}
	}

	if (read_block != NULL) {
		*read_block = malloc(*block_size);
		if (*read_block == NULL) {
			warn("Couldn't allocate read_block");
			if (block != NULL) {
				free(*block);
			}
			return -1;
		}
	}

	*fd = open(argv[1], open_flags, 0666);
	if (*fd < 0) {
		warn("Couldn't open file");
		if (block != NULL) {
			free(*block);
		}
		if (read_block != NULL) {
			free(*read_block);
		}
		return -1;
	}

	printf("File: %s\nSize: 0x%x\nBlock size: 0x%x\n",
	    argv[1], *size, *block_size);

	return 0;
}

/* Write addresses to the block. Keep word boundaries intact. */
static void
fill_block(uint8_t *block, size_t size, uint32_t start)
{
	uint32_t pattern_size = sizeof(start);
	uint32_t start_offset = start % pattern_size;
	uint32_t value_h = start & ~(pattern_size - 1);
	uint32_t value_n = htonl(value_h);
	uint8_t *val = (uint8_t *) &value_n;

	if (start_offset != 0) {
		size_t to_write = MIN(pattern_size - start_offset, size);
		memcpy(block, val + start_offset, to_write);
		block += to_write;
		size -= to_write;
		value_h += pattern_size;
		value_n = htonl(value_h);
	}

	while (size >= pattern_size) {
		memcpy(block, val, pattern_size);
		value_h += pattern_size;
		value_n = htonl(value_h);
		block += pattern_size;
		size -= pattern_size;
	}

	if (size > 0) {
		memcpy(block, val, size);
	}
}

static int
command_pattern_fill(int argc, char *argv[])
{
	int fd;
	size_t size;
	size_t block_size;
	uint8_t *block;
	int rv;

	rv = check_and_process_params(argc, argv, O_WRONLY | O_CREAT,
	    &fd, &size, &block_size, &block, NULL);
	if (rv != 0) {
		warnx("Error while processing parameters.\n");
		return rv;
	}

	for (size_t current = 0; current < size; current += block_size) {
		size_t write_size = MIN(block_size, size-current);
		ssize_t written;
		fill_block(block, write_size, current);
		written = write(fd, block, write_size);
		if (written != (ssize_t)write_size) {
			warn("Writing failed on block at 0x%x", current);
			break;
		}
	}

	free(block);
	close(fd);

	return 0;
}

static void
print_block(uint8_t *block, size_t size)
{
	for (size_t i = 0; i < size; ++i) {
		if (i > 0 && i % 0x10 == 0) {
			printf("\n");
		}
		printf("%02x ", block[i]);
	}
	printf("\n");
}

static int
command_pattern_check(int argc, char *argv[])
{
	int fd;
	size_t size;
	size_t block_size;
	uint8_t *block;
	uint8_t *read_block;
	int rv;
	int errors = 0;

	rv = check_and_process_params(argc, argv, O_RDONLY,
	    &fd, &size, &block_size, &block, &read_block);
	if (rv != 0) {
		warnx("Error while processing parameters.\n");
		return rv;
	}

	for (size_t current = 0; current < size; current += block_size) {
		size_t read_size = MIN(block_size, size-current);
		ssize_t received;
		fill_block(block, read_size, current);
		received = read(fd, read_block, read_size);
		if (received != (ssize_t)read_size) {
			warn("Reading failed on block at 0x%x", current);
			break;
		}
		rv = memcmp(block, read_block, read_size);
		if (rv != 0) {
			warnx("Pattern wrong in block at 0x%x", current);
			warnx("Expected:");
			print_block(block, read_size);
			warnx("Got:");
			print_block(read_block, read_size);
			++errors;
			if (errors > 20) {
				warnx("Too many errors. Refusing to continue.");
				break;
			}
		}
	}

	free(read_block);
	free(block);
	close(fd);

	return 0;
}

#ifdef __rtems__
rtems_shell_cmd_t rtems_shell_PATTERN_FILL_Command = {
	.name = "pattern-fill",
	.usage = "CAUTION: This command is destructive.\n"
	    "Use with: pattern-fill <file> <size> <block_size>",
	.topic = "SDtest",
	.command = command_pattern_fill,
	.alias = NULL,
	.next = NULL,
	.mode = 0,
	.uid = 0,
	.gid = 0,
};

rtems_shell_cmd_t rtems_shell_PATTERN_CHECK_Command = {
	.name = "pattern-check",
	.usage = "Use with: pattern-check <file> <size> <block_size>",
	.topic = "SDtest",
	.command = command_pattern_check,
	.alias = NULL,
	.next = NULL,
	.mode = 0,
	.uid = 0,
	.gid = 0,
};
#else /* __rtems__ */

int
main(int argc, char *argv[])
{
	if (argc < 2 || strcmp(argv[1], "-h") == 0) {
		printf("Use with: %s [fill|check] <file> <size> <block_size>\n",
		    argv[0]);
		return -1;
	}

	if (strcmp(argv[1], "fill") == 0) {
		return command_pattern_fill(argc-1, &argv[1]);
	}
	if (strcmp(argv[1], "check") == 0) {
		return command_pattern_check(argc-1, &argv[1]);
	}

	return 0;
}
#endif /* __rtems__ */
