/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef BUFFER_RAW_STATIC_H
#define BUFFER_RAW_STATIC_H

#include "buffer_raw_char.h"

namespace spec {

namespace buffer {

class raw_static : public raw {
public:
    MEMPOOL_CLASS_HELPERS(); //MEMPOOL_DEFINE_OBJECT_FACTORY(buffer::raw_static, buffer_raw_static, buffer_meta);

    raw_static(const char *data, uint64_t len) : raw((char*)data, len) {
    }

    ~raw_static() override {
    }

    raw* clone_empty() override {
        return new raw_char(m_len);
    }
};

} // namespace:buffer

} // namespace:spec

#endif // BUFFER_RAW_STATIC_H
