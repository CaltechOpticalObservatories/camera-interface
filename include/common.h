/**
 * @file    common.h
 * @brief   common interface functions header file
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#ifndef COMMON_H
#define COMMON_H

#include <map>
#include <vector>
#include <mutex>

const long ERROR = 0;
const long NO_ERROR = 1;

// handy snprintf shortcut
#define SNPRINTF(VAR, ...) { snprintf(VAR, sizeof(VAR), __VA_ARGS__); }

namespace Common {

  class Utilities {
    public:
      Utilities() {}
      ~Utilities() {}
      unsigned int parse_val(const std::string& str);
               int Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters);
  };

}

#endif
