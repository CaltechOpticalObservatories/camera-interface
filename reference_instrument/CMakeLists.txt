# ----------------------------------------------------------------------------
# @file    reference_instrument/CMakeLists.txt
# @brief   builds the reference instrument's camerad and Python module
# @details Built the way an instrument repository builds: the core is a
#          FetchContent dependency and this project declares the final targets
# ----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.24)

project(reference_instrument LANGUAGES CXX)

include(FetchContent)

# A real instrument pins a released version of the core. This copy lives inside
# the core and builds the surrounding tree instead, so the source directory
# below overrides the fetch and the git URL is never contacted. A copy made for
# a new instrument removes this default
if (NOT DEFINED FETCHCONTENT_SOURCE_DIR_CAMERAD)
  cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH FETCHCONTENT_SOURCE_DIR_CAMERAD)
endif()

FetchContent_Declare(camerad
  GIT_REPOSITORY https://github.com/CaltechOpticalObservatories/camera-interface
  GIT_TAG        d4815d100ad88a1c0773b5dc1dee086479b8d24d
  )

set(CONTROLLER archon CACHE STRING "")
set(BUILD_PYTHON_MODULE ON CACHE BOOL "")
FetchContent_MakeAvailable(camerad)

# each final target compiles exactly one interface factory definition
set(REFERENCE_SOURCES
  reference_interface_factory.cpp
  reference_instrument.cpp
  )

add_executable(reference_camerad ${REFERENCE_SOURCES})
target_link_libraries(reference_camerad camerad::daemon camerad::archon)
set_target_properties(reference_camerad PROPERTIES OUTPUT_NAME camerad)

# pybind11 comes from the Python environment the module is built against, so
# the module and the interpreter that loads it always agree on the ABI
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
if (NOT pybind11_DIR)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import pybind11; print(pybind11.get_cmake_dir())"
    OUTPUT_VARIABLE PYBIND11_CMAKE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE PYBIND11_PROBE_RESULT
    ERROR_QUIET
  )
  if (NOT PYBIND11_PROBE_RESULT EQUAL 0)
    message(FATAL_ERROR
            "the module needs pybind11 in ${Python3_EXECUTABLE}. "
            "Install it with: ${Python3_EXECUTABLE} -m pip install pybind11")
  endif()
  list(APPEND CMAKE_PREFIX_PATH ${PYBIND11_CMAKE_DIR})
endif()
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(camera_interface ${REFERENCE_SOURCES})
target_link_libraries(camera_interface PRIVATE camerad::python camerad::archon)
target_compile_definitions(camera_interface PRIVATE CAMERAD_INSTRUMENT_NAME="reference")

# A shared library may leave symbols undefined, so a module missing its
# interface factory definition links and fails only when imported
enable_testing()
add_test(NAME python_import
  COMMAND ${Python3_EXECUTABLE} -c "import camera_interface\nassert camera_interface.instrument_name() == 'reference', camera_interface.instrument_name()\nassert camera_interface.controller_name() == 'archon', camera_interface.controller_name()"
)
set_tests_properties(python_import PROPERTIES
  ENVIRONMENT PYTHONPATH=$<TARGET_FILE_DIR:camera_interface>
)
