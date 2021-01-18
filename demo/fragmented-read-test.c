/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2021 embedded brains GmbH.
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

#include "fragmented-read-test.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <unistd.h>

#define TEST_SUBDIR "test-dir"

static const char small_content[] = "I'm a small file";
static const char big_content[1024] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi ligula tellus, euismod nec faucibus in, ultrices at odio. Nunc mollis luctus turpis, at tempus tortor hendrerit eget. Nulla at dapibus libero, nec consequat magna. Nulla mattis lacus semper sollicitudin eleifend. Morbi arcu lacus, volutpat ac dolor eget, pretium lacinia neque. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Sed eget augue sed lacus ultricies ultricies in lobortis sem. Nunc mauris urna, maximus et odio eget, commodo lacinia sem. Curabitur molestie dolor et augue suscipit porttitor. Pellentesque quis diam imperdiet, suscipit ex eget, aliquam enim. Pellentesque nec porttitor risus, id viverra justo. In ultrices est egestas elit venenatis, eu iaculis sapien ullamcorper. Aenean sed ligula a libero pulvinar maximus. Fusce bibendum, risus sit amet dapibus pharetra, arcu libero lobortis sapien, quis varius enim mi ut nisl. Quisque a augue dapibus, portt.";

static int
snprint_testdir(char *path, size_t max, const char *dir)
{
	int rv;

	rv = snprintf(path, max, "%s/" TEST_SUBDIR, dir);
	if (rv < 0) {
		perror("Couldn't create test path name");
	}
	return rv;
}

static int
snprint_small(char *path, size_t max, const char *dir, unsigned i)
{
	int rv;

	rv = snprintf(path, max, "%s/" TEST_SUBDIR "/%d", dir, i);
	if (rv < 0) {
		perror("Couldn't create test file name for small file");
	}
	return rv;
}

static int
snprint_big(char *path, size_t max, const char *dir)
{
	int rv;

	rv = snprintf(path, max, "%s/" TEST_SUBDIR "/big", dir);
	if (rv < 0) {
		perror("Couldn't create test file name for big file");
	}
	return rv;
}

static int
create_test_dir(const char *dir)
{
	char path[PATH_MAX+1] = {0};
	int rv;

	puts("Create test directory");

	rv = snprint_testdir(path, PATH_MAX, dir);
	if (rv < 0) {
		return rv;
	}
	rv = mkdir(path, S_IRWXU);
	if (rv < 0 && errno != EEXIST) {
		perror("Couldn't create test path");
		return rv;
	}

	return 0;
}

static int
fill_disk_with_small_files(const char *dir)
{
	unsigned i = 0;
	ssize_t written = sizeof(small_content);
	char path[PATH_MAX+1] = {0};
	int rv;
	int fd;

	printf("Fill disk with small test files\n");

	while(written == sizeof(small_content)) {
		printf("Working on file %d    \r", i);
		rv = snprint_small(path, sizeof(path), dir, i);
		if (rv < 0) {
			return rv;
		}
		fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd < 0) {
			perror("Couldn't open small file");
			return fd;
		}
		written = write(fd, small_content, sizeof(small_content));
		close(fd);
		++i;
	}

	printf("%d small files written.\n", i);

	return (int) i;
}

/* Return < 0 on error, 0 on success or ENOENT if file does not exist. */
static int
remove_small_file(const char *dir, unsigned file)
{
	int rv;
	int error = 0;
	char path[PATH_MAX+1] = {0};

	rv = snprint_small(path, sizeof(path), dir, file);
	if (rv < 0) {
		return rv;
	}

	rv = unlink(path);
	if (rv < 0) {
		error = errno;
		if (error != ENOENT) {
			perror("Couldn't remove small file");
			return rv;
		}
	}
	return error;
}

static int
create_big_file(const char *dir)
{
	char path[PATH_MAX+1] = {0};
	int fd;
	int rv;
	ssize_t written;

	puts("Create big file");

	rv = snprint_big(path, sizeof(path), dir);
	if (rv < 0) {
		return rv;
	}

	fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		perror("Couldn't open big file\n");
		return rv;
	}

	do {
		written = write(fd, big_content, sizeof(big_content));
	} while (written > 0);

	close(fd);

	return 0;
}

/*
 * Return predictable patterns of more or less random numbers. Algorithm is the
 * one of a linear congruential generator.
 */
static unsigned
pseudo_random(unsigned *state, unsigned max)
{
	const unsigned a = 0x973a5fu;
	const unsigned c = 0x84au;

	*state = (*state) * a + c;

	return *state % max;
}

static int
remove_some_small_files_and_create_big_file(const char *dir, unsigned nr_files)
{
	/* Remove a third of the files. */
	const unsigned to_delete = nr_files / 3;

	unsigned rnd_state = 0;
	unsigned i;
	unsigned deleted = 0;
	int rv;
	int fd = -1;
	char path[PATH_MAX+1] = {0};
	ssize_t written;

	printf("Remove some small files and create a big one that fills the space\n");

	rv = snprint_big(path, sizeof(path), dir);
	if (rv < 0) {
		return rv;
	}

	while (deleted < to_delete) {
		do {
			i = pseudo_random(&rnd_state, nr_files);
			rv = remove_small_file(dir, i);
			if (rv < 0) {
				printf("Error while deleting file\n");
				return rv;
			}
		} while(rv != 0);
		deleted += 1;
		printf("deleted: %d / %d    \r", deleted, to_delete);

		if (fd < 0) {
			/* big file not yet opened */
			fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
			if (fd < 0) {
				perror("Couldn't open big file\n");
				return rv;
			}
		}

		do {
			written = write(fd, big_content, sizeof(big_content));
		} while (written > 0);
	}

	close(fd);
	return 0;
}

static int
check_read_speed_of_big_file(const char *dir, unsigned tries, size_t block_size)
{
	unsigned try;
	uint64_t time_start;
	uint64_t time_done;
	uint64_t time_diff;
	char *content;
	ssize_t rd;
	ssize_t total_file;
	double total_bytes;
	double total_ns;
	int fd;
	char path[PATH_MAX+1] = {0};
	int rv;

	printf("== Read big file and measure time\n");

	content = malloc(block_size);
	if (content == NULL) {
		perror("Not enough space for read blocks.");
		return -1;
	}

	rv = snprint_big(path, sizeof(path), dir);
	if (rv < 0) {
		return rv;
	}

	total_bytes = 0;
	total_ns = 0;

	for (try = 0; try < tries; ++try) {
		total_file = 0;

		time_start = rtems_clock_get_uptime_nanoseconds();

		fd = open(path, O_RDONLY);
		if (fd < 0) {
			perror("Couldn't open big file\n");
			return rv;
		}

		do {
			rd = read(fd, content, block_size);
			if (rd < 0) {
				perror("Error while reading file");
			} else {
				total_file += rd;
			}
		} while (rd > 0);
		close(fd);

		time_done = rtems_clock_get_uptime_nanoseconds();
		time_diff = time_done - time_start;

		printf("Try %3u: %d Bytes read with %d Byte blocks in %llu ms -> %.1f kiByte/s\n",
		    try, total_file, block_size, time_diff / 1000 / 1000,
		    ((double)total_file / 1024.) /
		    ((double)time_diff / 1000. / 1000. / 1000.));

		total_ns += (double) time_diff;
		total_bytes += (double) total_file;
	}

	printf("== Total: %.1f kiByte/s\n",
	    (total_bytes / 1024.) / (total_ns / 1000. / 1000. / 1000.));

	return 0;
}

static int
do_cleanup(const char *dir, unsigned nr_files)
{
	unsigned i;
	int rv;
	char path[PATH_MAX+1] = {0};
	int error = 0;

	printf("Clean up\n");

	for (i = 0; i < nr_files; ++i) {
		rv = remove_small_file(dir, i);
		if (rv < 0) {
			printf("Error removing small file %d\n", i);
			error = rv;
		}
	}

	rv = snprint_big(path, PATH_MAX, dir);
	if (rv < 0) {
		return rv;
	}

	rv = unlink(path);
	if (rv < 0 && errno != ENOENT) {
		perror("Couldn't remove big file");
		error = rv;
	}

	rv = snprint_testdir(path, PATH_MAX, dir);
	if (rv < 0) {
		return rv;
	}

	rv = rmdir(path);
	if (rv < 0) {
		perror("Error while removing test directory");
		error = rv;
	}

	return error;
}

static int
command_fragmented_read_test(int argc, char *argv[])
{
	const char *dir = NULL;
	int rv;
	int nr_files;
	bool cleanup = true;
	const unsigned tries = 6;
	const size_t read_block_size = 8 * 1024;
	int i;

	for (i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			puts(shell_FRAGMENTED_READ_TEST_Command.usage);
			return -1;
		} else if (strcmp(argv[i], "--no-cleanup") == 0) {
			cleanup = false;
		} else if (dir == NULL) {
			dir = argv[i];
		} else {
			puts("Wrong parameters");
			return -1;
		}
	}

	if (dir == NULL) {
		puts("Please provide at least a path!");
		return -1;
	}

	/* Big non-fragmented test */
	puts("\n**** Test non-fragmented file ****");
	rv = create_test_dir(dir);
	if (rv < 0) { return rv; }
	rv = create_big_file(dir);
	if (rv < 0) { return rv; }
	rv = check_read_speed_of_big_file(dir, tries, read_block_size);
	if (rv < 0) { return rv; }
	rv = do_cleanup(dir, 0);
	if (rv < 0) { return rv; }

	puts("\n**** Test fragmented file ****");
	rv = create_test_dir(dir);
	if (rv < 0) { return rv; }
	nr_files = fill_disk_with_small_files(dir);
	if (nr_files < 0) { return nr_files; }
	rv = remove_some_small_files_and_create_big_file(dir,
	    (unsigned)nr_files);
	if (rv < 0) { return rv; }
	sync();
	rv = check_read_speed_of_big_file(dir, tries, read_block_size);
	if (rv < 0) { return rv; }
	if (cleanup) {
		rv = do_cleanup(dir, (unsigned)nr_files);
		if (rv < 0) { return rv; }
	}

	return 0;
}

rtems_shell_cmd_t shell_FRAGMENTED_READ_TEST_Command = {
	.name = "frag-rd-test",
	.usage = "Use with: frag-rd-test [-h|--help] [--no-clean] <directory>\n"
	    "The test will fill the directory with lots of fragmented files.\n"
	    "After that it will check read performance for these files.\n"
	    "Use on a small disk only (e.g. 16MB). Otherwise expect very long\n"
	    "run times.\n",
	.topic = "SDtest",
	.command = command_fragmented_read_test,
	.alias = NULL,
	.next = NULL,
	.mode = 0,
	.uid = 0,
	.gid = 0,
};
