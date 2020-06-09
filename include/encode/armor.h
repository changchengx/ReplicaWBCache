/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_ARMOR_H
#define SPEC_ARMOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t spec_armor(char *dst, const char *dst_end,
                    const char *src, const char *src_end);

uint64_t spec_armor_linebreak(char *dst, const char *dst_end,
                              const char *src, const char *src_end,
                              int line_width);
int64_t spec_unarmor(char *dst, const char *dst_end,
                     const char *src, const char *src_end);
#ifdef __cplusplus
}
#endif

#endif //SPEC_ARMOR_H
