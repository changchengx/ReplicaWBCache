/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_SAFE_IO
#define SPEC_SAFE_IO

#include "compiler/extensions.h"
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Safe functions wrapping the raw read() and write() libc functions.
 * These retry on EINTR, and on error return -errno instead of returning
 * -1 and setting errno).
 */
ssize_t safe_read(int fd, void *buf, size_t count) WARN_UNUSED_RESULT;
ssize_t safe_write(int fd, const void *buf, size_t count) WARN_UNUSED_RESULT;
ssize_t safe_pread(int fd, void *buf, size_t count, off_t offset) WARN_UNUSED_RESULT;
ssize_t safe_pwrite(int fd, const void *buf, size_t count, off_t offset) WARN_UNUSED_RESULT;

/* Same as the above functions, but return -EDOM unless exactly the requested
 * number of bytes can be read.
 */
ssize_t safe_read_exact(int fd, void *buf, size_t count) WARN_UNUSED_RESULT;
ssize_t safe_pread_exact(int fd, void *buf, size_t count, off_t offset) WARN_UNUSED_RESULT;

/* Safe functions to read and write an entire file.
 */
int safe_write_file(const char *base, const char *file, const char *val, size_t vallen, unsigned mode);
int safe_read_file(const char *base, const char *file, char *val, size_t vallen);

#ifdef __cplusplus
}
#endif

#endif //SPEC_SAFE_IO
