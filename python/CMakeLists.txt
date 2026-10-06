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
  ${INSTRUMENT_REGISTRATION}
  ${INSTRUMENT_OBJECTS}
)

target_include_directories(camera_interface PRIVATE
  ${PROJECT_BASE_DIR}/camerad
  ${PROJECT_BASE_DIR}/camerad/Instruments
  ${PROJECT_BASE_DIR}/utils
  ${PROJECT_BASE_DIR}/common
  ${INTERFACE_INCLUDES}
)

# Without an instrument the module source reports "none"
if (INSTRUMENT)
  target_compile_definitions(camera_interface PRIVATE
    CAMERAD_INSTRUMENT_NAME="${INSTRUMENT}"
  )
endif()

# camerad_python carries the module source, so it compiles here, with this
# target's instrument name
target_link_libraries(camera_interface PRIVATE
  camerad_python
  ${INTERFACE_TARGET}
)

if (CAMERAD_INSTALL)
  if (CAMERAD_PYTHON_PACKAGE)
    # The module keeps its name inside the package; the __init__.py lets
    # "import <package>" behave as the module too
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/package/__init__.py [[
from . import camera_interface as _module
from .camera_interface import *  # noqa: F401,F403

__all__ = [name for name in dir(_module) if not name.startswith("_")]
]])
    install(TARGETS camera_interface
            LIBRARY DESTINATION ${CAMERAD_INSTALL_LIBDIR}/${CAMERAD_PYTHON_PACKAGE})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/package/__init__.py
            DESTINATION ${CAMERAD_INSTALL_LIBDIR}/${CAMERAD_PYTHON_PACKAGE})
  else()
    install(TARGETS camera_interface LIBRARY DESTINATION ${CAMERAD_INSTALL_LIBDIR})
  endif()
endif()

# A shared library may leave symbols undefined, so a module missing its
# interface factory definition links and fails only when imported
if (INSTRUMENT)
  set(CAMERAD_EXPECTED_INSTRUMENT ${INSTRUMENT})
else()
  set(CAMERAD_EXPECTED_INSTRUMENT none)
endif()
add_test(NAME python_import
  COMMAND ${Python3_EXECUTABLE} -c "import camera_interface\nassert camera_interface.instrument_name() == '${CAMERAD_EXPECTED_INSTRUMENT}', camera_interface.instrument_name()\nassert camera_interface.controller_name() == '${CONTROLLER}', camera_interface.controller_name()"
)
set_tests_properties(python_import PROPERTIES
  ENVIRONMENT PYTHONPATH=$<TARGET_FILE_DIR:camera_interface>
)
