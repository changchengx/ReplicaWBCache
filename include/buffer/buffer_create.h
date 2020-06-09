/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_BUFFER_CREATE_H
#define SPEC_BUFFER_CREATE_H

#include "../deleter.h"
#include "../unique_leakable_ptr.h"
#include "buffer_raw_combined.h"

#define SPEC_BUFFER_ALLOC_UNIT (4096U)
#define SPEC_BUFFER_APPEND_SIZE (SPEC_BUFFER_ALLOC_UNIT - sizeof(raw_combined))

namespace spec {

namespace buffer {

// abstract raw buffer with reference count
class raw;
class raw_combined;
class raw_malloc;
class raw_posix_aligned;
class raw_char;
class raw_claimed_char;
class raw_static;
class raw_claim_buffer;

extern unique_leakable_ptr<raw>
create_aligned_in_mempool(uint64_t len, uint64_t alignment,
                          uint64_t mempool_type_id);

extern unique_leakable_ptr<raw>
create_in_mempool(uint64_t len, uint64_t mempool_type_id);

extern unique_leakable_ptr<raw>
create_aligned(uint64_t len, uint64_t alignment);

extern unique_leakable_ptr<raw>
create(uint64_t len);
extern unique_leakable_ptr<raw>
create(uint64_t len, char c);

extern unique_leakable_ptr<raw>
create_page_aligned(uint64_t len);

extern unique_leakable_ptr<raw>
create_small_page_aligned(uint64_t len);

extern unique_leakable_ptr<raw>
copy(const char *buf, uint64_t len);

extern unique_leakable_ptr<raw>
claim_char(uint64_t len, char *buf);

extern unique_leakable_ptr<raw>
create_malloc(uint64_t len);

extern unique_leakable_ptr<raw>
claim_malloc(uint64_t len, char *buf);

extern unique_leakable_ptr<raw>
create_static(uint64_t len, char *buf);

extern unique_leakable_ptr<raw>
claim_buffer(uint64_t len, char *buf, deleter del);

} //namespace: buffer

} //namespace: spec

#endif // SPEC_BUFFER_CREATE_H
