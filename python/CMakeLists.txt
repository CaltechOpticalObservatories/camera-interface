# ----------------------------------------------------------------------------
# @file    python/CMakeLists.txt
# @brief   builds the camera_interface Python module
# ----------------------------------------------------------------------------

# pybind11 comes from the Python environment the module is built against, so
# the module and the interpreter that loads it always agree on the ABI.
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# pybind11 ships its CMake config inside the wheel, so ask the interpreter
# where it is rather than making every caller pass -Dpybind11_DIR
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
            "BUILD_PYTHON_MODULE=ON needs pybind11 in ${Python3_EXECUTABLE}. "
            "Install it with: ${Python3_EXECUTABLE} -m pip install pybind11")
  endif()
  list(APPEND CMAKE_PREFIX_PATH ${PYBIND11_CMAKE_DIR})
endif()

find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(camera_interface
  ${PROJECT_BASE_DIR}/python/camera_interface_module.cpp
)

target_include_directories(camera_interface PRIVATE
  ${PROJECT_BASE_DIR}/camerad
  ${PROJECT_BASE_DIR}/camerad/Instruments
  ${PROJECT_BASE_DIR}/utils
  ${PROJECT_BASE_DIR}/common
  ${INTERFACE_INCLUDES}
)

target_compile_definitions(camera_interface PRIVATE
  CAMERAD_INSTRUMENT_NAME="${INSTRUMENT}"
)

# Same link set as camerad, minus the server sources it does not use
target_link_libraries(camera_interface PRIVATE
  network
  utilities
  logentry
  ${INTERFACE_TARGET}
  ${INTERFACE_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CCFITS_LIB}
  ${CFITS_LIB}
  ${OpenCV_LIBS}
  Boost::thread
  Boost::chrono
  ${ZMQPP_LIB}
  ${ZMQ_LIB}
)
