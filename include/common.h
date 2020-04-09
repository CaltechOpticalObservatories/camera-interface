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

const long NOTHING = -1;
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
      std::string get_time_string();
  };

  class FitsTools {
    private:
    public:
      FitsTools() {}
      ~FitsTools() {}

      int get_keytype(std::string keyvalue);

      typedef enum {
        TYPE_STRING,
        TYPE_DOUBLE,
        TYPE_INTEGER
      } value_types;

      /**
       * define a structure and type for a FITS keyword internal database
       */
      typedef struct {
        std::string keyword;
        std::string keytype;
        std::string keyvalue;
        std::string keycomment;
      } user_key_t;                                          //!< user_key_t is the structure

      typedef std::map<std::string, user_key_t> fits_key_t;  //!< STL map

      fits_key_t userkeys;

//    inline std::string fitskey_type(int keytype);          //!< FITS keyword type  //TODO ?
  };

}

#endif
