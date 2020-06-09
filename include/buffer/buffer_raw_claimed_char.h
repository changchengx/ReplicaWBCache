/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef BUFFER_RAW_CLAIMED_CHAR_H
#define BUFFER_RAW_CLAIMED_CHAR_H

#include "buffer_raw_char.h"

namespace spec {

namespace buffer {

class raw_claimed_char : public raw {
public:
    MEMPOOL_CLASS_HELPERS(); //MEMPOOL_DEFINE_OBJECT_FACTORY(buffer::raw_claimed_char, buffer_raw_claimed_char, buffer_meta);

    explicit
    raw_claimed_char(uint64_t len, char *buf) : raw(buf, len) {
        bdout << "raw_claimed_char " << this << " alloc "
              << (void *)m_data << " " << m_len << bendl;
    }

    ~raw_claimed_char() override {
        // delete[] data; data = nullptr;
        bdout << "raw_claimed_char " << this << " free "
              << (void *)m_data << bendl;
    }

    raw* clone_empty() override {
        return new raw_char(m_len);
    }
};

} // namespace:buffer

} // namespace:spec

#endif // BUFFER_RAW_CLAIMED_CHAR_H
