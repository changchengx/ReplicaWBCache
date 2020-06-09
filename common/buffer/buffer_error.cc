/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include <iostream>
#include "buffer/buffer_error.h"

std::ostream& spec::buffer::operator<<(std::ostream& out, const spec::buffer::error& berror) {
    const char* info = berror.what();
    return out << info;
}
