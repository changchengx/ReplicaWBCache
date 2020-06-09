/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef BUFFER_RAW_POSIX_ALIGNED_H
#define BUFFER_RAW_POSIX_ALIGNED_H

#include "buffer_raw.h"
#include "buffer_error.h"
#include "buffer_debug.h"

namespace spec {

namespace buffer {

class raw_posix_aligned : public raw {
private:
    uint64_t alignment;

public:
    MEMPOOL_CLASS_HELPERS(); // MEMPOOL_DEFINE_OBJECT_FACTORY(buffer::raw_posix_aligned, buffer_raw_posix_aligned, buffer_meta)

    raw_posix_aligned(uint64_t len, uint64_t alignment)
        : raw(len), alignment(alignment) {
        spec_assert((this->alignment >= sizeof(void *)) &&
                    (this->alignment & (this->alignment - 1)) == 0);

        #ifdef DARWIN
        m_data = (char *)valloc(m_len);
        #else
        int rst = ::posix_memalign((void**)(void*)&m_data, this->alignment, m_len);
        if (rst) {
            throw bad_alloc();
        }
        #endif /* DARWIN */

        if (!m_data) {
            throw bad_alloc();
        }
        bdout << "raw_posix_aligned " << this << " alloc "
              << (void *)m_data << " len = " << len << ", "
              << "alignment = " << this->alignment << bendl;
    }

    ~raw_posix_aligned() override {
        ::free(m_data);
        bdout << "raw_posix_aligned " << this
              << " free " << (void *)m_data << bendl;
    }

    raw* clone_empty() override {
        return new raw_posix_aligned(m_len, alignment);
    }
};


} // namespace:buffer

} // namespace:spec

#endif // BUFFER_RAW_POSIX_ALIGNED_H
