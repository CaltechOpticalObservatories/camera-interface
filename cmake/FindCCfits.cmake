#CCfits does by default provide a pkg-config recipe. Try this first, it's likely the best
#however may not work on mac or windows


#note that using the IMPORTED_TARGET argument below requires at least cmake 3.7

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  message(VERBOSE "found pkg-config, trying to locate CCfits using it")
  pkg_check_modules(CCFITS CCfits REQUIRED IMPORTED_TARGET)

  #for compatibility with camera-interface build system, alias the target to CCFITS_LIB as well  
  add_library(CCfits ALIAS PkgConfig::CCFITS)
else()
  message(VERBOSE "no pkg-config, trying to locate CCfits manually")
  find_library(CCFITS_LIB CCfits NAMES libCCfits PATHS /usr/local/lib)
  message(VERBOSE "CCFITS library: ${CCFITS_LIB}")
  
  #note that we don't even bother to find non-standard include paths.
  #neither did the previous camera-interface build system.
  #If you have a non-standard include path, your CCfits installation should
  #have included a pkg-config file and mentioned it, or you should have dropped in your
  #own find module here. Good luck.
  add_library(CCfits UNKNOWN IMPORTED)
  set_target_properties(CCfits IMPORTED_LOCATION ${CCFITS_LIB})
  
endif()
