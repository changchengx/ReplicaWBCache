# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>

project(common)

add_subdirectory(spec_assert)
add_library(common::libassert ALIAS spec_assert)

add_subdirectory(arch)
add_subdirectory(crc32)
add_library(common::libarch ALIAS crc32)
