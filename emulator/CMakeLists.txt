# ----------------------------------------------------------------------------
# emulator/CMakeLists.txt
# ----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.12 )

set( EMULATOR_DIR ${PROJECT_BASE_DIR}/emulator )
set( CAMERA_DIR ${PROJECT_BASE_DIR}/camerad )

include_directories( ${PROJECT_BASE_DIR}/utils 
                     ${PROJECT_BASE_DIR}/camerad 
                     ${PROJECT_BASE_DIR}/common )

add_definitions( -Wall -ansi -O2 -Wno-variadic-macros -std=c++17 -ggdb )

add_definitions(-DSTA_ARCHON)
set( EMULATOR_TARGET emulatorArchon )
list(APPEND EMULATOR_SOURCE
        "${EMULATOR_DIR}/emulator-archon.cpp"
        )
add_library(emulatorInterface STATIC
        ${EMULATOR_DIR}/emulator-archon.cpp
        )

add_library(${EMULATOR_TARGET} ${EMULATOR_SOURCE})

target_include_directories(${EMULATOR_TARGET} PUBLIC ${PROJECT_INCL_DIR})

add_executable(emulator
        ${EMULATOR_DIR}/emulator-server.cpp
        ${CAMERA_DIR}/camera.cpp
        )

target_link_libraries(emulator
        network
        logentry
        utilities
        common
        ${EMULATOR_TARGET}
        ${CMAKE_THREAD_LIBS_INIT}
        )
