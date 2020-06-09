/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include "mempool/mempool.h"
#include "buffer/buffer_raw_malloc.h"
#include "buffer/buffer_raw_posix_aligned.h"
#include "buffer/buffer_raw_char.h"
#include "buffer/buffer_raw_claimed_char.h"
#include "buffer/buffer_raw_static.h"

using namespace spec;
MEMPOOL_DEFINE_OBJECT_FACTORY(buffer::raw_malloc, buffer_raw_malloc, buffer_meta);
MEMPOOL_DEFINE_OBJECT_FACTORY(buffer::raw_posix_aligned, buffer_raw_posix_aligned, buffer_meta);
MEMPOOL_DEFINE_OBJECT_FACTORY(buffer::raw_char, buffer_raw_char, buffer_meta);
MEMPOOL_DEFINE_OBJECT_FACTORY(buffer::raw_claimed_char, buffer_raw_claimed_char, buffer_meta);
MEMPOOL_DEFINE_OBJECT_FACTORY(buffer::raw_static, buffer_raw_static, buffer_meta);
