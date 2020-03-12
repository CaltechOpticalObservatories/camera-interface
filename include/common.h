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

const long NO_ERROR = 0;
const long ERROR = 1;
const long BUSY = 2;
const long TIMEOUT = 3;

// handy snprintf shortcut
#define SNPRINTF(VAR, ...) { snprintf(VAR, sizeof(VAR), __VA_ARGS__); }

namespace Common {

  class Utilities {
    private:

    public:
      Utilities() {}
      ~Utilities() {}
      unsigned int parse_val(const std::string& str);
      int Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters);
      void chrrep(char *str, char oldchr, char newchr);
      int get_keytype(std::string keyvalue);

      typedef enum {
        TYPE_STRING,
        TYPE_DOUBLE,
        TYPE_INTEGER
      } value_types;
  };

}

#endif
