/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_DOUT_H
#define SPEC_DOUT_H

#include "spec_assert/spec_assert.h"

extern void dout_emergency(const char * const str);
extern void dout_emergency(const std::string &str);

// internally conflict with endl
class _bad_endl_use_dendl_t {
public:
    _bad_endl_use_dendl_t(int) {
    }
};

static const _bad_endl_use_dendl_t endl = 0;
inline std::ostream& operator<< (std::ostream& out, _bad_endl_use_dendl_t) {
    spec_abort_msg("!!! use std::endl or dendl");
    return out;
}

class DoutPrefixProvider {
public:
    virtual std::ostream& gen_prefix(std::ostream& out) const = 0;
};

#endif //SPEC_DOUT_H
