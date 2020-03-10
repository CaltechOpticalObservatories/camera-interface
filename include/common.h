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
