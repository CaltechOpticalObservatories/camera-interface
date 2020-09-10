# ----------------------------------------------------------------------------
# BEGIN USER DEFINITIONS
# ----------------------------------------------------------------------------
# user must define the interface type here
# valid types are: "ARC-PCIe" | "ARC-PCI" | "STA-Archon"

set(INTERFACE_TYPE "STA-Archon")
#set(INTERFACE_TYPE "ARC-PCIe")
#set(INTERFACE_TYPE "ARC-PCI")

# ----------------------------------------------------------------------------
# END USER DEFINITIONS
# DO NOT EDIT BEYOND THIS LINE
# ----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5)

project(camera-interface VERSION 1.0)

set(PROJECT_BASE_DIR $ENV{HOME}/camera-interface)
set(PROJECT_SOURCE_DIR ${PROJECT_BASE_DIR}/src)
set(PROJECT_INCL_DIR ${PROJECT_BASE_DIR}/include)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BASE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BASE_DIR}/lib)
set(CMAKE_C_COMPILER /usr/bin/g++)
set(CMAKE_CXX_COMPILER /usr/bin/g++)
add_definitions( -Wall -ansi -O2 -Wno-variadic-macros -std=c++11 )
include_directories(${PROJECT_INCL_DIR})

# ----------------------------------------------------------------------------
# Set "BITS" for detected host system processor
#
if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "i686")
        message(STATUS "detected 32 bit architecture")
        set(BITS "32")
elseif (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86_64")
        message(STATUS "detected 64 bit architecture")
        set(BITS "64")
else()
        message(FATAL_ERROR " can't set BITS: unknown architecture: " ${CMAKE_HOST_SYSTEM_PROCESSOR})
endif()

# ----------------------------------------------------------------------------
# Setup for appropriate hardware interface...
#
# Each interface must define the following:
#   INTERFACE_TARGET    name to use for the target interface
#   INTERFACE_SOURCE    list of sources required for target interface
#   INTERFACE_INCLUDES  list of include paths for target interface
#   interface           target for interfacing to appropriate hardware
# ----------------------------------------------------------------------------

# AstroCam ARC-66 PCIe interface
#
if (${INTERFACE_TYPE} STREQUAL "ARC-PCIe")
        message(STATUS "compiling for AstroCam ARC-66 PCIe")
        set(INTERFACE_TARGET arc66)
        set(ARCAPI_DIR "/opt/ARC_API_35")
        add_definitions(-DASTROCAM)
        add_definitions(-DARC66_PCIE)
        find_path(ARCAPI_FITS ArcFitsFileCAPI.h PATHS ${ARCAPI_DIR}/CArcFitsFile/src)
        find_path(ARCAPI_CFITS CArcFitsFile.h PATHS ${ARCAPI_DIR}/CArcFitsFile/src)
        find_path(ARCAPI_DEVICE ArcDeviceCAPI.h PATHS ${ARCAPI_DIR}/CArcDevice/src)
        find_path(ARCAPI_PCI CArcPCIe.h PATHS ${ARCAPI_DIR}/CArcDevice/src)
        find_path(ARCAPI_EXPIFACE CExpIFace.h PATHS ${ARCAPI_DIR}/CArcDevice/src)
        find_path(ARCAPI_DEFS ArcDefs.h PATHS ${ARCAPI_DIR}/CArcDevice/src)
        find_path(ARC_INTERFACE arc66.h PATHS ${PROJECT_INCL_DIR})
        list(APPEND INTERFACE_SOURCE
                "${PROJECT_SOURCE_DIR}/arc66.cpp"
                "${PROJECT_SOURCE_DIR}/astrocam.cpp"
                )
        list(APPEND INTERFACE_INCLUDES
                "${ARC_INTERFACE}"
                "${ARCAPI_FITS}"
                "${ARCAPI_CFITS}"
                "${ARCAPI_DEVICE}"
                "${ARCAPI_PCI}"
                "${ARCAPI_EXPIFACE}"
                "${ARCAPI_DEFS}"
                )
        add_library(interface SHARED IMPORTED
                ${ARCAPI_DIR}/CArcFitsFile/Release_x${BITS}/libCArcFitsFile.so
                ${ARCAPI_DIR}/CArcDevice/Release_x${BITS}/libCArcDevice.so
                )
        set_target_properties(interface PROPERTIES 
                IMPORTED_LOCATION ${ARCAPI_DIR}/CArcDevice/Release_x${BITS}/libCArcFitsFile.so
                IMPORTED_LOCATION ${ARCAPI_DIR}/CArcDevice/Release_x${BITS}/libCArcDevice.so
                )

# AstroCam ARC-64 PCI interface
#
elseif(${INTERFACE_TYPE} STREQUAL "ARC-PCI")
        message(STATUS "compiling for AstroCam ARC-64 PCI")
        set(INTERFACE_TARGET arc64)
        set(ARCAPI_DIR "/opt/ARC_API_21")
        add_definitions(-DASTROCAM)
        add_definitions(-DARC64_PCI)
        find_path(ARCAPI_FITS CFitsFile.h PATHS ${ARCAPI_DIR}/CFitsFile)
        find_path(ARCAPI_DEVICE CController.h PATHS ${ARCAPI_DIR}/CController)
        find_path(ARC_INTERFACE arc64.h PATHS ${PROJECT_INCL_DIR})
        list(APPEND INTERFACE_SOURCE
                "${PROJECT_SOURCE_DIR}/arc64.cpp"
                "${PROJECT_SOURCE_DIR}/astrocam.cpp"
                )
        list(APPEND INTERFACE_INCLUDES
                "${ARC_INTERFACE}"
                "${ARCAPI_FITS}"
                "${ARCAPI_DEVICE}"
                )
        add_library(interface SHARED IMPORTED
                ${ARCAPI_DIR}/CFitsFile/linux/x${BITS}/libCFitsFile.so
                ${ARCAPI_DIR}/CController/linux/x${BITS}/libCController.so
                )
        set_target_properties(interface PROPERTIES 
                IMPORTED_LOCATION ${ARCAPI_DIR}/CFitsFile/linux/x${BITS}/libCFitsFile.so
                IMPORTED_LOCATION ${ARCAPI_DIR}/CController/linux/x${BITS}/libCController.so
                )

# STA Archon interface
#
elseif(${INTERFACE_TYPE} STREQUAL "STA-Archon")
        message(STATUS "compiling for STA Archon")
        set(INTERFACE_TARGET archon)
        add_definitions(-DSTA_ARCHON)
        list(APPEND INTERFACE_SOURCE
                "${PROJECT_SOURCE_DIR}/archon.cpp"
                )
        list(APPEND INTERFACE_INCLUDES
                "${ARCHON_INCLUDE}"
                )
        add_library(interface OBJECT
                ${PROJECT_SOURCE_DIR}/archon.cpp
                )
else()
        message(FATAL_ERROR " unknown interface type: " ${INTERFACE_TYPE})
endif()
# ----------------------------------------------------------------------------
# End interface setup
# ----------------------------------------------------------------------------

# Now add the defined interface target:
#
add_library(${INTERFACE_TARGET} ${INTERFACE_SOURCE})
target_include_directories(${INTERFACE_TARGET} PUBLIC ${INTERFACE_INCLUDES})

# ----------------------------------------------------------------------------
# External libraries, such as FITS, etc.
# ----------------------------------------------------------------------------

find_library(CCFITS_LIB CCfits NAMES libCCfits PATHS /usr/local/lib)
find_library(CFITS_LIB cfitsio NAMES libcfitsio  PATHS /usr/local/lib)

find_package(Threads)

# ----------------------------------------------------------------------------
# Here are all the targets
# ----------------------------------------------------------------------------

add_library(common OBJECT
        ${PROJECT_SOURCE_DIR}/common.cpp
        )

add_library(utilities OBJECT
        ${PROJECT_SOURCE_DIR}/utilities.cpp
        )

add_library(logentry OBJECT
        ${PROJECT_SOURCE_DIR}/logentry.cpp
        )

add_library(network OBJECT
        ${PROJECT_SOURCE_DIR}/network.cpp
        )

add_executable(cameraserver 
        ${PROJECT_SOURCE_DIR}/server.cpp 
        ${INTERFACE_INCLUDES}
        )

target_link_libraries(cameraserver
        common
        network
        logentry
        utilities
        interface
        ${INTERFACE_TARGET}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CCFITS_LIB}
        ${CFITS_LIB}
        )

