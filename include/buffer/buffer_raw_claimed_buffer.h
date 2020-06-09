/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef BUFFER_RAW_CLAIMED_BUFFER_H
#define BUFFER_RAW_CLAIMED_BUFFER_H

#include "../deleter.h"
#include "buffer_raw_char.h"

namespace spec {

namespace buffer {

class raw_claim_buffer : public raw {
private:
    deleter del;
public:
    raw_claim_buffer(const char *c, uint64_t len, deleter del)
        : raw((char*)c, len), del(std::move(del)) {
    }

    ~raw_claim_buffer() override {
    }

    raw* clone_empty() override {
        return new raw_char(m_len);
    }
};

} // namespace:buffer

} // namespace:spec

#endif // BUFFER_RAW_CLAIMED_BUFFER_H
