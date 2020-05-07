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

set(UNITTEST_LIBS GMock::Main GMock::GMock GTest::GTest)

add_library(unit-main OBJECT unit.cc)
target_include_directories(unit-main PRIVATE
    $<TARGET_PROPERTY:GTest::GTest,INTERFACE_INCLUDE_DIRECTORIES>)

# unittest_bufferlist
add_executable(unittest_bufferlist
    buffer_list.cc
    $<TARGET_OBJECTS:unit-main>
)

target_link_libraries(unittest_bufferlist common::libbuffer)
target_link_libraries(unittest_bufferlist common::libencode)
target_link_libraries(unittest_bufferlist common::libassert)
target_link_libraries(unittest_bufferlist common::libcompat)
target_link_libraries(unittest_bufferlist common::libarch)
target_link_libraries(unittest_bufferlist ${UNITTEST_LIBS})
