# ----------------------------------------------------------------------------
# @file    Instruments/cryoscope/cryoscope.cmake
# @brief   CryoScope-specific input to the CMake build system for camerad
# @author  David Hale <dhale@caltech.edu>
# ----------------------------------------------------------------------------

set (INSTRUMENT_SOURCES
    ${CAMERAD_DIR}/Instruments/cryoscope/cryoscope_instrument.cpp
    ${CAMERAD_DIR}/Instruments/cryoscope/cryoscope_interface_factory.cpp
    ${CAMERAD_DIR}/Instruments/cryoscope/cryoscope_exposure_modes.cpp
    )
