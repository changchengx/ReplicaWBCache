/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_BUFFER_AUDIT_H
#define SPEC_BUFFER_AUDIT_H

#include <stdint.h>

namespace spec {

namespace buffer {

// cached crc hit count (matching input)
extern uint64_t get_cached_crc();

// cached crc hit count (mismatching input, required adjustment)
extern uint64_t get_cached_crc_adjusted();

// cached crc miss count
extern uint64_t get_missed_crc();

// enable/disable track cached crc
extern void track_cached_crc(bool btrack);

} //namespace buffer
} //namespace spec

#endif //SPEC_BUFFER_AUDIT_H
