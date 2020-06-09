/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include <sstream>
#include <string.h>

#include "error_info/errno.h"

std::string cpp_strerror(int err) {
    char buf[128];
    char *errmsg;

    if (err < 0) {
        err = -err;
    }
    std::ostringstream oss;

    errmsg = strerror_r(err, buf, sizeof(buf));

    oss << "(" << err << ") " << errmsg;

    return oss.str();
}
