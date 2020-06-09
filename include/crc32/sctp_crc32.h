/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */


#ifndef COMMON_SCTP_CRC32_H
#define COMMON_SCTP_CRC32_H

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t crc32c_sctp(uint32_t crc, unsigned char const *data, unsigned length);

#ifdef __cplusplus
}
#endif

#endif
