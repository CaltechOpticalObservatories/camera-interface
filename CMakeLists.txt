# ----------------------------------------------------------------------------
# BEGIN USER DEFINITIONS
# ----------------------------------------------------------------------------
# user must define the interface type here
# valid types are: "AstroCam" | "Archon"

set(INTERFACE_TYPE "Archon")
#set(INTERFACE_TYPE "AstroCam")

# If your software is not installed in the indicated directory then change this:
set(PROJECT_BASE_DIR $ENV{HOME}/Software/camera-interface)

# This prints a large number of debugging messages and may be eliminated in future releases:
#add_definitions(-DLOGLEVEL_DEBUG)

# ----------------------------------------------------------------------------
# END USER DEFINITIONS
# DO NOT EDIT BEYOND THIS LINE
# ----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.12)

project(camera-interface VERSION 0.1)

set(PROJECT_SOURCE_DIR ${PROJECT_BASE_DIR}/src)
set(PROJECT_INCL_DIR ${PROJECT_BASE_DIR}/include)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BASE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BASE_DIR}/lib)
set(CMAKE_C_COMPILER /usr/bin/g++)
set(CMAKE_CXX_COMPILER /usr/bin/g++)
set(CMAKE_BUILD_TYPE Debug)
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

# AstroCam ARC-64/66 PCI/e interfaces
#
if (${INTERFACE_TYPE} STREQUAL "AstroCam")
        message(STATUS "compiling for AstroCam GenIII PCI/PCIe")
        set(INTERFACE_TARGET astrocam)
        set(ARCAPI_DIR "/opt/ARC_API/3.6")
        add_definitions( -Wall -ansi -O2 -Wno-variadic-macros -std=c++17 -ggdb )
        add_definitions(-DASTROCAM)
        add_definitions(-DARC66_PCIE)
        find_path(ARCAPI_BASE "CArcBase.h" PATHS ${ARCAPI_DIR}/CArcBase/inc)
        find_path(ARCAPI_FITS "CArcFitsFile.h" PATHS ${ARCAPI_DIR}/CArcFitsFile/inc)
        find_path(ARCAPI_CFITS "CArcFitsFile.h" PATHS ${ARCAPI_DIR}/CArcFitsFile/inc)
        find_path(ARCAPI_DEVICE "CArcDevice.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
        find_path(ARCAPI_PCI "CArcPCI.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
        find_path(ARCAPI_EXPIFACE "CExpIFace.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
        find_path(ARCAPI_CONIFACE "CConIFace.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
        find_path(ARCAPI_DEFS "ArcDefs.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)

        set(INTERFACE_SOURCE
                "${PROJECT_SOURCE_DIR}/astrocam.cpp"
                )
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

# STA Archon interface
#
elseif(${INTERFACE_TYPE} STREQUAL "Archon")
        message(STATUS "compiling for STA Archon")
        set(INTERFACE_TARGET archon)
        add_definitions( -Wall -ansi -O2 -Wno-variadic-macros -std=c++11 -ggdb )
        add_definitions(-DSTA_ARCHON)
        list(APPEND INTERFACE_SOURCE
                "${PROJECT_SOURCE_DIR}/archon.cpp"
                )
        list(APPEND INTERFACE_INCLUDES
                "${ARCHON_INCLUDE}"
                )
	add_library(interface STATIC
                ${PROJECT_SOURCE_DIR}/archon.cpp
                )

        set(EMULATOR_TARGET emulator-archon)
        list(APPEND EMULATOR_SOURCE
                "${PROJECT_SOURCE_DIR}/emulator-archon.cpp"
                )
	add_library(emulator-interface STATIC
                ${PROJECT_SOURCE_DIR}/emulator-archon.cpp
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

if (${INTERFACE_TYPE} STREQUAL "Archon")
add_library(${EMULATOR_TARGET} ${EMULATOR_SOURCE})
target_include_directories(${EMULATOR_TARGET} PUBLIC ${PROJECT_INCL_DIR})
endif()

# ----------------------------------------------------------------------------
# External libraries, such as FITS, etc.
# ----------------------------------------------------------------------------

find_library(CCFITS_LIB CCfits NAMES libCCfits PATHS /usr/local/lib)
find_library(CFITS_LIB cfitsio NAMES libcfitsio  PATHS /usr/local/lib)

find_package(Threads)

# ----------------------------------------------------------------------------
# Here are all the targets
# ----------------------------------------------------------------------------

add_library(common STATIC
        ${PROJECT_SOURCE_DIR}/common.cpp
        )

add_library(utilities STATIC
        ${PROJECT_SOURCE_DIR}/utilities.cpp
        )

add_library(logentry STATIC
        ${PROJECT_SOURCE_DIR}/logentry.cpp
        )

add_library(network STATIC
        ${PROJECT_SOURCE_DIR}/network.cpp
        )

add_executable(cameraserver 
        ${PROJECT_SOURCE_DIR}/server.cpp 
        ${INTERFACE_INCLUDES}
        )

if (${INTERFACE_TYPE} STREQUAL "Archon")
add_executable(emulator 
        ${PROJECT_SOURCE_DIR}/emulator-server.cpp 
        ${PROJECT_INCL_DIR}
        )
endif()

# ----------------------------------------------------------------------------
# Everyone gets this:
# ----------------------------------------------------------------------------

target_link_libraries(cameraserver
        common
        network
        logentry
        utilities
        ${INTERFACE_TARGET}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CCFITS_LIB}
        ${CFITS_LIB}
        )

if (${INTERFACE_TYPE} STREQUAL "Archon")
target_link_libraries(emulator
        network
        logentry
        utilities
        ${EMULATOR_TARGET}
        ${CMAKE_THREAD_LIBS_INIT}
        )
endif()

# ----------------------------------------------------------------------------
# Some targets are interface-dependent:
# ----------------------------------------------------------------------------

if (${INTERFACE_TYPE} STREQUAL "AstroCam")
        target_link_libraries(cameraserver ${CARC_BASE})
        target_link_libraries(cameraserver ${CARC_DEVICE})
        target_link_libraries(cameraserver ${CARC_FITS})
endif()

# ----------------------------------------------------------------------------
# cURL is not used here, so it's not strictly required, but cfitsio can
# use it. If cfitsio was built with libcurl then it has to be linked here.
# If it's installed on the system then link it, but don't make it mandatory.
# ----------------------------------------------------------------------------
#
include(FindCURL)

if (CURL_FOUND STREQUAL "FALSE")
        message(STATUS "cURL was not found but may be needed by some systems if built into cfitsio.\n   If you experience undefined curl references during linking \n   then please your cURL installation.")
else()
        target_link_libraries(cameraserver ${CURL_LIBRARIES})
endif()

