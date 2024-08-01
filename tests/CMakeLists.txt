cmake_minimum_required(VERSION 3.12)  # Or your preferred minimum version

set(PROJECT_TESTS_DIR ${PROJECT_BASE_DIR}/tests)

#add_definitions( -Wall -ansi -O2 -Wno-variadic-macros -std=c++17 -ggdb )

# Find the Google Test library
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS}
        ${PROJECT_BASE_DIR}/utils
)
link_directories( ${PROJECT_BASE_DIR}/lib )

# Define your unit test executable
add_executable(run_unit_tests utility_tests.cpp) # List all your unit test source files here

# Link the Google Test library
target_link_libraries(run_unit_tests
        ${GTEST_LIBRARIES}
        utilities
)
