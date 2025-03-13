# ----------------------------------------------------------------------------
# utils/CMakeLists.txt
# ----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.12)

#set(PROJECT_UTILS_DIR ${PROJECT_BASE_DIR}/utils)

add_definitions(-Wall -ansi -O1 -Wno-variadic-macros -std=c++17 -ggdb)

add_library(utilities STATIC
        utilities.cpp
        md5
)

add_library(logentry STATIC
        logentry.cpp
)
target_link_libraries(logentry $<IF:$<PLATFORM_ID:Darwin>,,stdc++fs>)

add_library(network STATIC
        network.cpp
)

add_library(md5 STATIC
        md5.cpp
)

add_executable(listener
        listener.cpp
)

add_executable(socksend
        sendcmd.cpp
)
