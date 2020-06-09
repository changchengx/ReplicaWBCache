# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>

add_library(encode SHARED
    armor.c
)

target_include_directories(encode
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)
