# ----------------------------------------------------------------------------
# @file    common/CMakeLists.txt
# @brief   input to CMake build system for building common software packages
# @author  David Hale <dhale@caltech.edu>
# ----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.12)

set(PROJECT_UTILS_DIR ${PROJECT_BASE_DIR}/common)

add_definitions(-Wall -ansi -O2 -Wno-variadic-macros -std=c++17 -ggdb)

include_directories(${PROJECT_BASE_DIR}/common)
include_directories(${PROJECT_BASE_DIR}/utils)    # needed for logentry

add_library(common STATIC
        ${PROJECT_UTILS_DIR}/common.cpp
)
