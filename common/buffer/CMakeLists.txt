# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>

add_library(buffer SHARED
    buffer_error.cc
    buffer_ptr.cc
    buffer_create.cc
    buffer_list.cc
    buffer_pool.cc
    page.cc
    mempool.cc
)

target_include_directories(buffer
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)
