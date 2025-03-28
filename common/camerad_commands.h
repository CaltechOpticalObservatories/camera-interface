/**
 * @file    camerad_commands.h
 * @brief   the doit() function listens for these commands.
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

const std::string CAMERAD_COMPRESSION  = "compress";  ///< FITS compression type
const std::string CAMERAD_EXPOSE       = "expose";    ///< initiate an exposure
const std::string CAMERAD_POWER        = "power";     ///< controller power state
const std::string CAMERAD_SAVEUNP      = "saveunp";   ///< save unprocessed images
const std::string CAMERAD_TEST         = "test";      ///< test routines

const std::vector<std::string> CAMERAD_SYNTAX = {
                                                  CAMERAD_COMPRESSION+" [ ? | none | rice | gzip | plio ]",
                                                  CAMERAD_EXPOSE,
                                                  CAMERAD_POWER+" [ ? | on | off ]",
                                                  CAMERAD_SAVEUNP+" [ ? | true | false ]",
                                                  CAMERAD_TEST+" ? | <testname> ..."
                                                };
