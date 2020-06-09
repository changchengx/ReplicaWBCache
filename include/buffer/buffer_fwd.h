/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef BUFFER_FWD_H
#define BUFFER_FWD_H

namespace spec {

namespace buffer {
    class ptr;
    class list;
    class hash;
}

using buffer_ptr = buffer::ptr;
using buffer_list = buffer::list;
using buffer_hash = buffer::hash;

}

#endif //BUFFER_FWD_H
