# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>

add_library(spec_assert SHARED
    spec_assert.cc
    dout.cc
    errno.cc
    BackTrace.cc
)

target_include_directories(spec_assert
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)
