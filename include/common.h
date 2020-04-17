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

  class FitsKeys {
    private:
    public:
      FitsKeys() {}
      ~FitsKeys() {}

      std::string get_keytype(std::string keyvalue);         //!< return type of keyword based on value
      long listkeys();                                       //!< list FITS keys in the internal database
      long addkey(std::string arg);                          //!< add FITS key to the internal database

      typedef struct {                                       //!< structure of FITS keyword internal database
        std::string keyword;
        std::string keytype;
        std::string keyvalue;
        std::string keycomment;
      } user_key_t;

      typedef std::map<std::string, user_key_t> fits_key_t;  //!< STL map for the actual keyword database

      fits_key_t keydb;                                      //!< keyword database
  };

  class Common {
    private:
      std::string image_dir;
      std::string image_name;
      int image_num;

    public:
      Common();
      ~Common() {}

      long imdir(std::string dir_in);
      long imdir(std::string dir_in, std::string& dir_out);
      long imname(std::string name_in);
      long imname(std::string name_in, std::string& name_out);
      long imnum(std::string num_in, std::string& num_out);
      void increment_imnum() { this->image_num++; };
      std::string get_fitsname();
  };
}
#endif
