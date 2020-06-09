/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */


#ifndef BUFFER_RAW_COMBINED_H
#define BUFFER_RAW_COMBINED_H

#include "../intarith.h"
#include "buffer_raw.h"
#include "buffer_error.h"

namespace spec {

namespace buffer {

/* raw_combined is always placed within one single allocation along with the data buffer.
 * The data space is at the beginning, and raw_combined obj is at the end.
 */
class raw_combined : public raw {
private:
    uint64_t alignment;
public:

    raw_combined(char *dataptr, uint64_t len, uint64_t alignment, uint64_t mempool_type_id)
    : raw(dataptr, len, mempool_type_id), alignment(alignment) {
    }

    raw* clone_empty() override {
        return create(m_len, alignment).release();
    }

    static spec::unique_leakable_ptr<raw>
    create(uint64_t len, uint64_t alignment,
           uint64_t mempool_type_id = mempool::mempool_buffer_anon) {
        if (!alignment) {
            alignment = sizeof(uint64_t);
        }

        uint64_t rawlen = round_up_to(sizeof(raw_combined), alignof(raw_combined));
        uint64_t datalen = round_up_to(len, alignof(raw_combined));

        #ifdef DARWIN
        char *ptr = (char *) valloc(rawlen + datalen);
        #else
        char *ptr = nullptr;
        int rst = ::posix_memalign((void**)(void*)&ptr, alignment,
                                    rawlen + datalen);
        if (rst) {
            throw bad_alloc();
        }
        #endif /* DARWIN */

        if (!ptr) {
            throw bad_alloc();
        }

        /* Use placement new expression to construct raw_combined object in
         * specific address:
         * 1. The head compoent is the actual data space
         * 2. The end compoent is the raw_combined object.
         */
        return spec::unique_leakable_ptr<raw>(
            new (ptr + datalen) raw_combined(ptr, len, alignment,
                                             mempool_type_id));
    }

    static void operator delete(void *ptr) {
        raw_combined *raw = (raw_combined *)ptr;
        ::free((void *)raw->m_data);
    }
};

} //namespace: buffer

} //namespace: spec

#endif // BUFFER_RAW_COMBINED_H
