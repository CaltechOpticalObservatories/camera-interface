cmake_minimum_required( VERSION 3.12 )

set( CAMERA_DIR ${PROJECT_BASE_DIR}/camerad )

set( CMAKE_CXX_STANDARD 17 )

include_directories( ${PROJECT_BASE_DIR}/utils )
include_directories( ${PROJECT_BASE_DIR}/common )
link_directories(${PROJECT_BASE_DIR}/lib)

# ----------------------------------------------------------------------------
# AstroCam ARC-64/66 PCI/e interfaces
# ----------------------------------------------------------------------------
if (${INTERFACE_TYPE} STREQUAL "AstroCam")
  message( STATUS "compiling for AstroCam GenIII PCI/PCIe")
  set( INTERFACE_TARGET astrocam)
  set( ARCAPI_DIR "${PROJECT_BASE_DIR}/ARC")
  add_definitions( -DASTROCAM)
  add_definitions( -DARC66_PCIE)
  find_path( ARCAPI_BASE "CArcBase.h" PATHS ${ARCAPI_DIR}/CArcBase/inc)
  find_path( ARCAPI_FITS "CArcFitsFile.h" PATHS ${ARCAPI_DIR}/CArcFitsFile/inc)
  find_path( ARCAPI_CFITS "CArcFitsFile.h" PATHS ${ARCAPI_DIR}/CArcFitsFile/inc)
  find_path( ARCAPI_DEVICE "CArcDevice.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
  find_path( ARCAPI_PCI "CArcPCI.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
  find_path( ARCAPI_EXPIFACE "CExpIFace.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
  find_path( ARCAPI_CONIFACE "CConIFace.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
  find_path( ARCAPI_DEFS "ArcDefs.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)

# ----------------------------------------------------------------------------
#   INTERFACE_SOURCE    list of sources required for target interface
# ----------------------------------------------------------------------------
  set(INTERFACE_SOURCE
    "${CAMERA_DIR}/astrocam_interface.cpp"
  )
# ----------------------------------------------------------------------------
#   INTERFACE_INCLUDES  list of include paths for target interface
# ----------------------------------------------------------------------------


    add_library(${INTERFACE_TARGET} ${INTERFACE_SOURCE})
    target_include_directories(${INTERFACE_TARGET} PUBLIC ${INTERFACE_INCLUDES})


    set(INTERFACE_INCLUDES
      "${ARC_INTERFACE}"
      "${ARCAPI_BASE}"
      "${ARCAPI_FITS}"
      "${ARCAPI_CFITS}"
      "${ARCAPI_DEVICE}"
      "${ARCAPI_PCI}"
      "${ARCAPI_EXPIFACE}"
      "${ARCAPI_CONIFACE}"
      "${ARCAPI_DEFS}"
    )
    find_library(CARC_BASE "CArcBase3.6" NAMES "libCArcBase3.6" PATHS ${ARCAPI_DIR}/Release)
    find_library(CARC_DEVICE "CArcDevice3.6" NAMES "libCArcDevice3.6.so" PATHS ${ARCAPI_DIR}/Release)
    find_library(CARC_FITS "CArcFitsFile3.6" NAMES "libCArcFitsFile3.6.so" PATHS ${ARCAPI_DIR}/Release)

    # ----------------------------------------------------------------------------
    # STA Archon interfaces
    # ----------------------------------------------------------------------------

elseif (${INTERFACE_TYPE} STREQUAL "Archon")
    message(STATUS "compiling for STA Archon")
    set(INTERFACE_TARGET archon)
    add_definitions(-Wall -ansi -O1 -Wno-variadic-macros -std=c++17 -ggdb)
    add_definitions(-DSTA_ARCHON)

    set(INTERFACE_SOURCE
      "${CAMERA_DIR}/archon.cpp"
    )
    add_library(interface STATIC "${INTERFACE_SOURCE}")
    if (${DETECTOR_TYPE} STREQUAL "Hxrg")
        message(STATUS "compiling for HXRG detector")
        add_definitions(-DDET_HXRG)
    else ()
        message(STATUS "compiling for CCD detector")
    endif ()
else ()
    message(FATAL_ERROR " unknown interface type: " ${INTERFACE_TYPE})
endif ()




# Now add the defined interface target:
#
add_library(${INTERFACE_TARGET} ${INTERFACE_SOURCE})
# target_include_directories(${INTERFACE_TARGET} PUBLIC ${INTERFACE_INCLUDES})


add_library(camera STATIC
        ${CAMERA_DIR}/camera.cpp
)


# ----------------------------------------------------------------------------
# External libraries, such as FITS, etc.
# ----------------------------------------------------------------------------

find_library(CCFITS_LIB CCfits NAMES libCCfits PATHS /usr/local/lib)
find_library(CFITS_LIB cfitsio NAMES libcfitsio  PATHS /usr/local/lib)

# ZeroMQ
#
find_library( ZMQPP_LIB zmqpp NAMES libzmqpp PATHS /usr/local/lib )
find_library( ZMQ_LIB zmq NAMES libzmq PATHS /usr/local/lib )

# OpenCV
find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

find_package(Threads)
find_package(nlohmann_json REQUIRED)

find_package(Boost REQUIRED COMPONENTS thread chrono)
include_directories(${BOOST_INCLUDES})

add_executable(cleand
  ${CAMERA_DIR}/camerad.cpp
  ${CAMERA_DIR}/camera.cpp
  ${CAMERA_DIR}/camera_server.cpp
  ${INTERFACE_INCLUDES}
)

target_link_libraries(cleand
  ${INTERFACE_TARGET}
  camera
  network
  logentry
  common
  utilities
  ${CMAKE_THREAD_LIBS_INIT}
  ${CCFITS_LIB}
  ${CFITS_LIB}
  ${ZMQPP_LIB}
  ${ZMQ_LIB}
  ${OpenCV_LIBS}
  nlohmann_json::nlohmann_json
  Boost::thread
  Boost::chrono
  pthread
  ${BOOST_INCLUDES}
)

