# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>

# GTEST & GMock
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(googletest)
add_library(GMock::GMock ALIAS gmock)
add_library(GMock::Main ALIAS gmock_main)
target_include_directories(gmock INTERFACE
    $<TARGET_PROPERTY:gtest,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(gmock_main INTERFACE
    $<TARGET_PROPERTY:gtest,INTERFACE_INCLUDE_DIRECTORIES>)
add_library(GTest::GTest ALIAS gtest)
add_library(GTest::Main ALIAS gtest_main)
