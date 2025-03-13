# ----------------------------------------------------------------------------
# CMakeLists.txt
# top-level CMake input file
# ----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.12)
project(camera C CXX)


# ----------------------------------------------------------------------------
# user must define the interface type here
# valid types are: "AstroCam" | "Archon"
# ----------------------------------------------------------------------------
set(INTERFACE_TYPE "Archon" CACHE STRING "Type of instrument interface to build for")
set_property(CACHE INTERFACE_TYPE PROPERTY STRINGS Archon AstroCam)

#for some reason all libraries are built static by default
#I'm not sure if this is for technical reasons or just preferred
#Anyway, give the option but set it to static by default 
option(BUILD_SHARED_LIBS "build shared libraries instead of static" OFF)


#the build system of camerad previously outputted binaries to the source folder
# leave this by default for compatibility but allow an option to put the binaries in the usual place
# (the build folder)
option(OUTPUT_TO_SRC_DIR "output binaries and libraries to source folder rather than build folder" ON)



if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "no build type specified defaulting to RelWithDebInfo")
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Instrument can be set via command line generic is the default value
IF (NOT DEFINED INSTR)
    set(INSTR "generic")
endif ()

# Detector type can be set via command line ( Hxrg | Ccd )
if (NOT DEFINED DETECTOR_TYPE)
    set(DETECTOR_TYPE "Ccd")
endif ()
# ----------------------------------------------------------------------------

# un-comment the following to log verbose debug messages
#add_definitions(-DLOGLEVEL_DEBUG)

#setting these standard variables will (since CMake 3.1)
#setup the proper -std=c++17 etc flags, no need to do it manually
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


if(OUTPUT_TO_SRC_DIR)
  #this is the "legacy" behaviour
  #if this option is not set, binaries will output in their default place in the CMake build output folder
  set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)
  set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    include_directories(/usr/local/include)
endif()

find_package(Threads)

#no need for absolute paths here, they are by default relative to ${CMAKE_CURRENT_LIST_DIR}
add_subdirectory(utils)
add_subdirectory(common)
add_subdirectory(camerad)
add_subdirectory(emulator)
add_subdirectory(tests EXCLUDE_FROM_ALL)
