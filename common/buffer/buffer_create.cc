/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include "mempool/mempool.h"
#include "buffer/buffer_create.h"
#include "buffer/buffer_raw_posix_aligned.h"
#include "buffer/buffer_raw_claimed_char.h"
#include "buffer/buffer_raw_malloc.h"
#include "buffer/buffer_raw_static.h"
#include "buffer/buffer_raw_claimed_buffer.h"

using namespace spec;

namespace spec::buffer {
/* If alignment is page-multiply, use a separate buffer::raw to
 * avoid fragmenting the heap.
 *
 * The experiment shows that there's better peformance from
 * a separate buffer::raw when the size passes 8KB.
 *
 * The experiment shows that there's better performance from
 * buffer::raw_combined than buffer::raw when
 *  1) the allocation size is page-multiply
 *  2) alignment isn't page-multiply
 */
unique_leakable_ptr<raw>
create_aligned_in_mempool(uint64_t len, uint64_t alignment,
                                  uint64_t mempool_type_id) {
    if ((alignment & ~SPEC_PAGE_MASK) == 0 || len >= SPEC_PAGE_SIZE * 2) {
        return unique_leakable_ptr<raw>(
                new raw_posix_aligned(len, alignment));
    }
    return raw_combined::create(len, alignment, mempool_type_id);
}

unique_leakable_ptr<raw>
create_in_mempool(uint64_t len, uint64_t mempool_type_id) {
    return create_aligned_in_mempool(len, sizeof(size_t),
                                     mempool_type_id);
}

unique_leakable_ptr<raw>
create_aligned(uint64_t len, uint64_t alignment) {
    return create_aligned_in_mempool(len, alignment,
                                     mempool::mempool_buffer_anon);
}

unique_leakable_ptr<raw>
create(uint64_t len) {
    return create_aligned(len, sizeof(size_t));
}

unique_leakable_ptr<raw>
create(uint64_t len, char c) {
    auto rst = create_aligned(len, sizeof(size_t));
    memset(rst->get_data(), c, len);
    return rst;
}

unique_leakable_ptr<raw>
create_page_aligned(uint64_t len) {
    return create_aligned(len, SPEC_PAGE_SIZE);
}

unique_leakable_ptr<raw>
create_small_page_aligned(uint64_t len) {
    if (len < SPEC_PAGE_SIZE) {
        return create_aligned(len, SPEC_BUFFER_ALLOC_UNIT);
    } else {
        return create_aligned(len, SPEC_PAGE_SIZE);
    }
}

unique_leakable_ptr<raw>
copy(const char *buf, uint64_t len) {
    auto rst = create_aligned(len, sizeof(size_t));
    memcpy(rst->get_data(), buf, len);
    return rst;
}

unique_leakable_ptr<raw>
claim_char(uint64_t len, char *buf) {
    return unique_leakable_ptr<raw>(
            new raw_claimed_char(len, buf));
}

unique_leakable_ptr<raw>
create_malloc(uint64_t len) {
    return unique_leakable_ptr<raw>(new raw_malloc(len));
}

unique_leakable_ptr<raw>
claim_malloc(uint64_t len, char *buf) {
    return unique_leakable_ptr<raw>(new raw_malloc(len, buf));
}

unique_leakable_ptr<raw>
create_static(uint64_t len, char *buf) {
    return unique_leakable_ptr<raw>(new raw_static(buf, len));
}

unique_leakable_ptr<raw>
claim_buffer(uint64_t len, char *buf, deleter del) {
    return unique_leakable_ptr<raw>(
            new raw_claim_buffer(buf, len, std::move(del)));
}

} //namespace spec::buffer
