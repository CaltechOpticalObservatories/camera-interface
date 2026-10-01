# ----------------------------------------------------------------------------
# @file    examples/reference_instrument/CMakeLists.txt
# @brief   how an instrument repository consumes camerad
#
# Copy this into a new instrument repository as its top-level CMakeLists.txt.
# Nothing here reaches into the core: it fetches camerad, links camerad_server,
# and supplies Camera::Interface::create().
# ----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.24)

project(reference_instrument CXX)

include(FetchContent)

# A consumer pins a tag here. The core's own CI points this at its working tree
# with -DFETCHCONTENT_SOURCE_DIR_CAMERA_INTERFACE, so a change that breaks a
# consumer fails there rather than in every instrument later.
FetchContent_Declare(camera_interface
  GIT_REPOSITORY https://github.com/CaltechOpticalObservatories/camera-interface
  GIT_TAG        main
)

set(CONTROLLER archon CACHE STRING "Controller this instrument uses")

FetchContent_MakeAvailable(camera_interface)

add_executable(camerad
  reference_instrument.cpp
  reference_interface_factory.cpp
)

target_link_libraries(camerad PRIVATE camerad_server)
