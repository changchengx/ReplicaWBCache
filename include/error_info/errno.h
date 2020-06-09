/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_ERRNO_H
#define SPEC_ERRNO_H

#include <string>

/* Return a given error code as a string */
std::string cpp_strerror(int err);

#endif //SPEC_ERRNO_H
