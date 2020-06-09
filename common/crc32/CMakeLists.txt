# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>

set(crc32_srcs
	crc32c.cc
	crc32c_intel_baseline.c
	sctp_crc32.c)

list(APPEND crc32_srcs
    crc32c_intel_fast.c
    crc32c_intel_fast_asm.s
    crc32c_intel_fast_zero_asm.s)

add_library(crc32 ${crc32_srcs})

target_include_directories(crc32
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(crc32 arch)
