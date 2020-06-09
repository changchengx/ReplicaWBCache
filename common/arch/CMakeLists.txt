# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>

add_library(arch STATIC
    intel.c
    probe_arch.cc
)

target_include_directories(arch
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)
