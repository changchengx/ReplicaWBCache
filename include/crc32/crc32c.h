/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef CRC32C_H
#define CRC32C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*crc32c_func_t)(uint32_t crc,
                                  unsigned char const *data,
                                  unsigned length);

/* global static to choose crc32c implementation on the given architecture. */
extern crc32c_func_t crc32c_func;

extern crc32c_func_t choose_crc32(void);

/* calculate crc32c for data that is entirely 0 (ZERO) */
uint32_t crc32c_zeros(uint32_t initial_crc, unsigned length);

/* if the data pointer is NULL, we calculate a crc value as if
 * it were zero-filled.
 *
 * crc: initial value
 * data: pointer to data buffer
 * length: length of buffer
 */
static inline uint32_t spec_crc32c(uint32_t crc,
                                  unsigned char const *data,
                                  unsigned length) {
#ifndef HAVE_POWER8
    if (!data && length > 16) {
        return crc32c_zeros(crc, length);
    }
#endif /* !HAVE_POWER8 */

    return crc32c_func(crc, data, length);
}

#ifdef __cplusplus
}
#endif

#endif //CRC32C_H
