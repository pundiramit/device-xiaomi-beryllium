/*
 * Copyright (c) 2019, Linaro Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "translate.h"

#define READONLY_PATH	"/readonly/firmware/image/"
#define READWRITE_PATH	"/readwrite/"

#ifndef ANDROID
#define FIRMWARE_BASE	"/lib/firmware/"
#define TQFTPSERV_TMP	"/tmp/tqftpserv"
#else
#define FIRMWARE_BASE	"/vendor/firmware/"
#define TQFTPSERV_TMP	"/data/vendor/tmp/tqftpserv"
#endif

/**
 * translate_readonly() - open "file" residing with remoteproc firmware
 * @file:	file requested, stripped of "/readonly/image/" prefix
 *
 * It is assumed that the readonly files requested by the client resides under
 * /lib/firmware in the same place as its associated remoteproc firmware.  This
 * function scans through all entries under /sys/class/remoteproc and read the
 * dirname of each "firmware" file in an attempt to find, and open(2), the
 * requested file.
 *
 * As these files are readonly, it's not possible to pass flags to open(2).
 *
 * Return: opened fd on success, -1 otherwise
 */
static int translate_readonly(const char *file)
{
	char firmware_value[PATH_MAX];
	char firmware_attr[32];
	char path[PATH_MAX];
	struct dirent *de;
	int firmware_fd;
	DIR *class_dir;
	int class_fd;
	ssize_t n;
	int fd = -1;

	class_fd = open("/sys/class/remoteproc", O_RDONLY | O_DIRECTORY);
	if (class_fd < 0) {
		warn("failed to open remoteproc class");
		return -1;
	}

	class_dir = fdopendir(class_fd);
	if (!class_dir) {
		warn("failed to opendir");
		goto close_class;
	}

	while ((de = readdir(class_dir)) != NULL) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;

		if (strlen(de->d_name) + sizeof("/firmware") > sizeof(firmware_attr))
			continue;
		strcpy(firmware_attr, de->d_name);
		strcat(firmware_attr, "/firmware");

		firmware_fd = openat(class_fd, firmware_attr, O_RDONLY);
		if (firmware_fd < 0)
			continue;

		n = read(firmware_fd, firmware_value, sizeof(firmware_value));
		close(firmware_fd);
		if (n < 0) {
			continue;
		}
		firmware_value[n] = '\0';

		if (strlen(FIRMWARE_BASE) + strlen(firmware_value) + 1 +
		    strlen(file) + 1 > sizeof(path))
			continue;

		strcpy(path, FIRMWARE_BASE);
		strcat(path, dirname(firmware_value));
		strcat(path, "/");
		strcat(path, file);

		fd = open(path, O_RDONLY);
		if (fd >= 0)
			break;

		if (errno != ENOENT)
			warn("failed to open %s", path);
	}

	closedir(class_dir);

close_class:
	close(class_fd);

	return fd;
}

/**
 * translate_readwrite() - open "file" from a temporary directory
 * @file:	relative path of the requested file, with /readwrite/ stripped
 * @flags:	flags to be passed to open(2)
 *
 * Return: opened fd on success, -1 otherwise
 */
static int translate_readwrite(const char *file, int flags)
{
	int base;
	int ret;
	int fd;

	ret = mkdir(TQFTPSERV_TMP, 0700);
	if (ret < 0 && errno != EEXIST) {
		warn("failed to create temporary tqftpserv directory");
		return -1;
	}

	base = open(TQFTPSERV_TMP, O_RDONLY | O_DIRECTORY);
	if (base < 0) {
		warn("failed top open temporary tqftpserv directory");
		return -1;
	}

	fd = openat(base, file, flags, 0600);
	close(base);
	if (fd < 0)
		warn("failed to open %s", file);

	return fd;
}

/**
 * translate_open() - open file after translating path
 *

 * Strips /readonly/firmware/image and search among remoteproc firmware.
 * Replaces /readwrite with a temporary directory.

 */
int translate_open(const char *path, int flags)
{
	if (!strncmp(path, READONLY_PATH, strlen(READONLY_PATH)))
		return translate_readonly(path + strlen(READONLY_PATH));
	else if (!strncmp(path, READWRITE_PATH, strlen(READWRITE_PATH)))
		return translate_readwrite(path + strlen(READWRITE_PATH), flags);

	fprintf(stderr, "invalid path %s, rejecting\n", path);
	errno = ENOENT;
	return -1;
}
