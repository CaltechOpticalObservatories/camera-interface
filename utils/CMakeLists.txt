# ----------------------------------------------------------------------------
# utils/CMakeLists.txt
# ----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.12)

set(PROJECT_UTILS_DIR ${PROJECT_BASE_DIR}/utils)

set(CMAKE_CXX_STANDARD 17)

add_definitions(-Wall -ansi -O1 -Wno-variadic-macros -std=c++17 -ggdb)

add_library(utilities STATIC
        ${PROJECT_UTILS_DIR}/utilities.cpp
        md5
)

add_library(logentry STATIC
        ${PROJECT_UTILS_DIR}/logentry.cpp
)
target_link_libraries(logentry $<IF:$<PLATFORM_ID:Darwin>,,stdc++fs>)

add_library(network STATIC
        ${PROJECT_UTILS_DIR}/network.cpp
)

add_library(md5 STATIC
        ${PROJECT_UTILS_DIR}/md5.c
)

add_executable(listener
        ${PROJECT_UTILS_DIR}/listener.c
)

add_executable(socksend
        ${PROJECT_UTILS_DIR}/sendcmd.c
)
