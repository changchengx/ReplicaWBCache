/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef COMMON_CRC32C_INTEL_FAST_H
#define COMMON_CRC32C_INTEL_FAST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
/* the fast version is compiled in */
extern int crc32c_intel_fast_exists(void);

#ifdef __x86_64__

extern uint32_t crc32c_intel_fast(uint32_t crc,
                                  unsigned char const *buffer,
                                  unsigned len);

#else

static inline uint32_t crc32c_intel_fast(uint32_t crc,
                                         unsigned char const *buffer,
                                         unsigned len) {
    return 0;
}

#endif

#ifdef __cplusplus
}
#endif

#endif
