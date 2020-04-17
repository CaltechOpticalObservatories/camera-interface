CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(archon-interface)

SET(PROJECT_BASE_DIR $ENV{HOME}/archon-interface)
SET(PROJECT_SOURCE_DIR ${PROJECT_BASE_DIR}/src)
SET(PROJECT_INCL_DIR ${PROJECT_BASE_DIR}/include)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BASE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_BASE_DIR}/lib)

# Define a default configuration file for the server.
# This can be overridden by setting environment variable ARCHONSERVCFG
# or by command line argument when starting the server.
#SET(CONFIG_FILE "\"$ENV{HOME}/CMOST/archonserver.cfg\"")
SET(CONFIG_FILE "\"${PROJECT_BASE_DIR}/archonserver.cfg\"")
CONFIGURE_FILE("${PROJECT_INCL_DIR}/config.h.in" "${PROJECT_INCL_DIR}/config.h")

# Define the path for the log files
#SET(LOGPATH "\"$ENV{HOME}/CMOST/logs\"")
SET(LOGPATH "\"$ENV{HOME}/Logs\"")
CONFIGURE_FILE("${PROJECT_INCL_DIR}/logentry.h.in" "${PROJECT_INCL_DIR}/logentry.h")

FIND_PATH(ARCHON_INCLUDE server.h PATHS ${PROJECT_INCL_DIR})
FIND_PATH(LOGENTRY_INCLUDE logentry.h PATHS ${PROJECT_INCL_DIR})
FIND_PATH(TCPLINUX_INCLUDE tcplinux.h PATHS ${PROJECT_INCL_DIR})
FIND_PATH(COMMON_INCLUDE common.h PATHS ${PROJECT_INCL_DIR})
FIND_PATH(ARCHON_INCLUDE archon.h PATHS ${PROJECT_INCL_DIR})
FIND_PATH(CCFITS_INCLUDE CCfits PATHS /usr/local/include/CCfits)
FIND_PATH(FITS_INCLUDE fitsio.h PATHS /usr/local/cfitsio/include)
FIND_PATH(MY_FITS_INCLUDE fits.h PATHS ${PROJECT_INCL_DIR})
FIND_PATH(CONFIG_INCLUDE config.h PATHS ${PROJECT_INCL_DIR})

FIND_LIBRARY(CCFITS_LIB CCfits NAMES libCCfits PATHS /usr/local/lib)
FIND_LIBRARY(CFITS_LIB cfitsio NAMES libcfitsio  PATHS /usr/local/cfitsio/lib)

FIND_PACKAGE(Threads)

INCLUDE_DIRECTORIES(
        ${ARCHON_INCLUDE}
        ${LOGENTRY_INCLUDE}
        ${TCPLINUX_INCLUDE}
        ${COMMON_INCLUDE}
        ${MY_FITS_INCLUDE}
        ${FITS_INCLUDE}
        ${CCFITS_INCLUDE}
        )
ADD_DEFINITIONS( -Wall -ansi -O2 -Wno-variadic-macros -std=c++11 )

SET(COMMON_HEADERS
        ${ARCHON_INCLUDE}/server.h
        ${LOGENTRY_INCLUDE}/logentry.h
        ${TCPLINUX_INCLUDE}/tcplinux.h
        ${COMMON_INCLUDE}/common.h
        ${ARCHON_INCLUDE}/archon.h
        ${CCFITS_INCLUDE}/CCfits
        ${FITS_INCLUDE}/fitsio.h
        ${MY_FITS_INCLUDE}/fits.h
        ${CONFIG_INCLUDE}/config.h
        )

ADD_EXECUTABLE(archonserver 
        ${PROJECT_SOURCE_DIR}/server.cpp 
        ${PROJECT_SOURCE_DIR}/common.cpp
        ${PROJECT_SOURCE_DIR}/archon.cpp
        ${PROJECT_SOURCE_DIR}/tcplinux.c 
        ${PROJECT_SOURCE_DIR}/logentry.c
        ${COMMON_HEADERS}
        )

TARGET_LINK_LIBRARIES(archonserver
        ${CMAKE_THREAD_LIBS_INIT}
        ${CCFITS_LIB}
        ${CFITS_LIB}
        )

SET(CMAKE_C_COMPILER /usr/bin/g++)
SET(CMAKE_CXX_COMPILER /usr/bin/g++)

