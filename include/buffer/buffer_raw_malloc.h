/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef BUFFER_RAW_MALLOC_H
#define BUFFER_RAW_MALLOC_H

#include "buffer_raw.h"
#include "buffer_error.h"
#include "buffer_debug.h"

namespace spec {

namespace buffer {

class raw_malloc : public raw {
public:
    MEMPOOL_CLASS_HELPERS(); // MEMPOOL_DEFINE_OBJECT_FACTORY(buffer::raw_malloc, buffer_raw_malloc, buffer_meta)

    explicit
    raw_malloc(uint64_t len) : raw(len) {
        if (m_len) {
            m_data = (char *)malloc(m_len);
            if (!m_data) {
                throw bad_alloc();
            }
        } else {
            m_data = nullptr;
        }
        bdout << "raw_malloc " << this << " alloc "
              << (void *)m_data << " " << m_len << bendl;
    }

    raw_malloc(uint64_t len, char *data) : raw(data, len) {
        bdout << "raw_malloc " << this << " alloc "
              << (void *)m_data << " " << m_len << bendl;
    }

    ~raw_malloc() override {
        free(m_data);
        m_data = nullptr;
        bdout << "raw_malloc " << this << " free "
              << (void *)m_data << " " << bendl;
    }

    raw* clone_empty() override {
        return new raw_malloc(m_len);
    }
};

} // namespace:buffer

} // namespace:spec

#endif // BUFFER_RAW_MALLOC_H
