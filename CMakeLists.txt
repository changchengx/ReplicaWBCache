# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>

cmake_minimum_required(VERSION 3.10)

project(replica_writebackcache
    VERSION 0.1
    LANGUAGES CXX C ASM)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)
# check results and add flag
if (COMPILER_SUPPORTS_CXX17)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17" CACHE STRING "Use c++17" FORCE)
else()
    message(STATUS "This probject only support c++17. Please configure the right C++ compiler.")
endif()

set(CMAKE_ASM_COMPILER  ${PROJECT_SOURCE_DIR}/tools/yasm-wrapper)
set(object_format "elf64")
set(CMAKE_ASM_FLAGS "-f ${object_format}")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message("Setting build type to 'RelWithDebInfo' as none was specified.")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(BOOST_COMPONENTS
    atomic thread system regex random program_options date_time iostreams)
list(APPEND BOOST_COMPONENTS context coroutine)
set(BOOST_HEADER_COMPONENTS container)
find_package(Boost 1.71.0 REQUIRED COMPONENTS ${BOOST_COMPONENTS} ${BOOST_HEADER_COMPONENTS})
if(Boost_FOUND)
    include_directories(BEFORE SYSTEM ${Boost_INCLUDE_DIRS})
    message("************System Boost Start***********")
    message("boost header:${Boost_INCLUDE_DIR}")
    message("boost lib: ${Boost_LIBRARIES}")
    message("************System Boost End***********")
else()
    message(SEND_ERROR "Please install Boost 1.71.0 in system")
endif()
include_directories(BEFORE SYSTEM ${Boost_INCLUDE_DIRS})

# Add sub directories
add_subdirectory(common)
add_subdirectory(test)
