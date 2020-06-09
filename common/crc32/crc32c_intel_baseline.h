/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef COMMON_CRC32C_INTEL_BASELINE_H
#define COMMON_CRC32C_INTEL_BASELINE_H

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t crc32c_intel_baseline(uint32_t crc,
                                      unsigned char const *buffer,
                                      unsigned len);

#ifdef __cplusplus
}
#endif

#endif
