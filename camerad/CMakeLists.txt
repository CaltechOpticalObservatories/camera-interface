cmake_minimum_required( VERSION 3.12 )

set( CAMERAD_DIR ${PROJECT_BASE_DIR}/camera_clean )

set( CMAKE_CXX_STANDARD 17 )

include_directories( ${PROJECT_BASE_DIR}/utils )
include_directories( ${PROJECT_BASE_DIR}/common )

# ----------------------------------------------------------------------------
# AstroCam ARC-64/66 PCI/e interfaces
# ----------------------------------------------------------------------------
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
  "${CAMERAD_DIR}/astrocam_interface.cpp"
  )
# ----------------------------------------------------------------------------
#   INTERFACE_INCLUDES  list of include paths for target interface
# ----------------------------------------------------------------------------
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

add_library(${INTERFACE_TARGET} ${INTERFACE_SOURCE})
target_include_directories(${INTERFACE_TARGET} PUBLIC ${INTERFACE_INCLUDES})

# ----------------------------------------------------------------------------
# External libraries, such as FITS, etc.
# ----------------------------------------------------------------------------

find_library(CCFITS_LIB CCfits NAMES libCCfits PATHS /usr/local/lib)
find_library(CFITS_LIB cfitsio NAMES libcfitsio  PATHS /usr/local/lib)

# ZeroMQ
#
find_library( ZMQPP_LIB zmqpp NAMES libzmqpp PATHS /usr/local/lib )
find_library( ZMQ_LIB zmq NAMES libzmq PATHS /usr/local/lib )

find_package(Threads)

add_executable(cleand
  ${CAMERAD_DIR}/camerad.cpp
  ${CAMERAD_DIR}/camera.cpp
  ${CAMERAD_DIR}/camera_server.cpp
  ${INTERFACE_INCLUDES}
  )

target_link_libraries(cleand
  network
  common
  utilities
  logentry
  CArcBase
  CArcDevice
  CArcFitsFile
  ${INTERFACE_TARGET}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CCFITS_LIB}
  ${CFITS_LIB}
  ${ZMQPP_LIB}
  ${ZMQ_LIB}
  )

