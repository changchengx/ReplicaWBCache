/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef BUFFER_RAW_CHAR_H
#define BUFFER_RAW_CHAR_H

#include "buffer_raw.h"
#include "buffer_debug.h"

namespace spec {

namespace buffer {

/* primitive buffer types */
class raw_char : public raw {
public:
    MEMPOOL_CLASS_HELPERS(); // MEMPOOL_DEFINE_OBJECT_FACTORY(buffer::raw_char, buffer_raw_char, buffer_meta);

    explicit
    raw_char(uint64_t len) : raw(len) {
        if (m_len) {
            m_data = new char[m_len];
        } else {
            m_data = nullptr;
        }
        bdout << "raw_char " << this << " alloc "
              << (void *)m_data << " " << m_len << bendl;
    }

    raw_char(uint64_t len, char *c) : raw(c, len) {
        bdout << "raw_char " << this << " alloc "
              << (void *)m_data << " " << m_len << bendl;
    }

    ~raw_char() override {
        delete[] m_data;
        m_data = nullptr;
        bdout << "raw_char " << this << " free " << (void *)m_data << bendl;
    }

    raw* clone_empty() override {
        return new raw_char(m_len);
    }
};

} // namespace:buffer

} // namespace:spec

#endif // BUFFER_RAW_CHAR_H
