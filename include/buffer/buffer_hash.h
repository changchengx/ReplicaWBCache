/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_BUFFERLIST_HASH_H
#define SPEC_BUFFERLIST_HASH_H

#include "buffer_list.h"

namespace spec {

namespace buffer {

// efficent hash for one or more bufferlists
class hash {
private:
    uint32_t _crc;

public:
    hash(uint32_t init = 0): _crc(init) {}

    void update(const buffer_list& blist) {
        _crc = blist.crc32c(_crc);
    }

    uint32_t digest() const {
        return _crc;
    }
};

inline buffer_hash& operator<< (buffer_hash& lhs, const buffer_list& rhs) {
    lhs.update(rhs);
    return lhs;
}

} //namespace buffer
} //namespace spec

#endif //SPEC_BUFFERLIST_HASH_H
