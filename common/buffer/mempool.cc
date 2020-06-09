/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include "mempool/mempool.h"

//debug mempool
bool mempool::debug_mode = false;

void mempool::set_debug_mode(bool debug_enable) {
  debug_mode = debug_enable;
}

const char *mempool::get_pool_name(mempool::pool_type_id pool_index) {
#define P(x) #x,
    static const char *names[num_pools] = {
        DEFINE_MEMORY_POOLS_HELPER(P)
    };
#undef P
    return names[pool_index];
}

mempool::pool_type& mempool::get_pool(mempool::pool_type_id pool_index) {
    // static type to be initialized before any caller.
    static mempool::pool_type table[num_pools];
    return table[pool_index];
}
