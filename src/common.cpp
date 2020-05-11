/**
 * @file    common.cpp
 * @brief   common interface functions
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include <iostream>
#include <fstream>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <vector>
#include <thread>
#include <fstream>
#include <algorithm>  //!< vector iterators, find, count
#include <functional> //!< pass by reference to threads

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"

namespace Common {

  Common::Common() {
    this->image_dir = "/tmp";
    this->image_name = "image";
    this->image_num = 0;
  }


  /** Common::Common::imnum ***************************************************/
  /**
   * @fn     imnum
   * @brief  set or get the image_num member
   * @param  std::string num_in
   * @param  std::string& num_out
   * @return ERROR or NO_ERROR
   *
   */
  long Common::imnum(std::string num_in, std::string& num_out) {
    std::string function = "Common::Common::imnum";
    std::stringstream message;

    // If no string is passed then this is a request; return the current value.
    //
    if (num_in.empty()) {
      message.str(""); message << "image number: " << this->image_num;
      logwrite(function, message.str());
      num_out = std::to_string(this->image_num);
      return(NO_ERROR);
    }

    else {                             // Otherwise check the incoming value
      int num;
      try {
        num = std::stoi(num_in);
      }
      catch (std::invalid_argument ) {
        logwrite(function, "error invalid number: unable to convert to integer");
        return(ERROR);
      }
      catch (std::out_of_range) {
        logwrite(function, "error imnum out of integer range");
        return(ERROR);
      }
      if (num < 0) {                   // can't be negative
        message.str(""); message << "error requested image number " << num << " must be >= 0";
        logwrite(function, message.str());
        return(ERROR);
      }
      else {                           // value is OK
        this->image_num = num;
        num_out = num_in;
        return(NO_ERROR);
      }
    }
  }
  /** Common::Common::imnum ***************************************************/


  /** Common::Common::imname **************************************************/
  /**
   * @fn     imname
   * @brief  set or get the image_name member
   * @param  std::string name_in
   * @param  std::string& name_out
   * @return NO_ERROR
   *
   * This function is overloaded with a form that doesn't use a return value.
   *
   */
  long Common::imname(std::string name_in) {
    std::string dontcare;
    return( imname(name_in, dontcare) );
  }
  long Common::imname(std::string name_in, std::string& name_out) {
    std::string function = "Common::Common::imname";
    std::stringstream message;

    // If no string is passed then this is a request; return the current value.
    //
    if (name_in.empty()) {
      message.str(""); message << "image name:" << this->image_name;
      logwrite(function, message.str());
      name_out = this->image_name;
    }
    else {                             // Otherwise set the image name
      this->image_name = name_in;
      name_out = name_in;
    }
    return(NO_ERROR);
  }
  /** Common::Common::imname **************************************************/


  /** Common::Common::imdir ***************************************************/
  /**
   * @fn     imdir
   * @brief  set or get the image_dir member
   * @param  std::string dir_in
   * @param  std::string& dir_out
   * @return ERROR or NO_ERROR
   *
   * This function is overloaded with a form that doesn't use a return value.
   *
   */
  long Common::imdir(std::string dir_in) {
    std::string dontcare;
    return( imdir(dir_in, dontcare) );
  }
  long Common::imdir(std::string dir_in, std::string& dir_out) {
    std::string function = "Common::Common::imdir";
    std::stringstream message;

    // If no string is passed then this is a request; return the current value.
    //
    if (dir_in.empty()) {
      message.str(""); message << "image directory: " << this->image_dir;
      logwrite(function, message.str());
      dir_out = this->image_dir;
      return(NO_ERROR);
    }

    struct stat st;
    if (stat(dir_in.c_str(), &st) == 0) {
      // Use stat to check that it's a directory
      //
      if (S_ISDIR(st.st_mode)) {
        try {
          // and if it is then try writing a test file to make sure we'll be able to write to it
          //
          std::string testfile;
          testfile = dir_in + "/.tmp";
          FILE* fp = std::fopen(testfile.c_str(), "w");    // create the test file
          if (!fp) {
            message.str(""); message << "error cannot write to requested image directory " << dir_in;
            logwrite(function, message.str());
            dir_out = this->image_dir;
            return(ERROR);
          }
          else {                                           // remove the test file
            std::fclose(fp);
            if (std::remove(testfile.c_str()) != 0) {
              message.str(""); message << "error removing temporary file " << testfile;
              logwrite(function, message.str());
            }
          }
        }
        catch(...) {
          message.str(""); message << "error writing to " << dir_in;
          logwrite(function, message.str());
          dir_out = this->image_dir;
          return(ERROR);
        }
        this->image_dir = dir_in;                          // passed all tests so set the image_dir
        dir_out = this->image_dir;
        return(NO_ERROR);
      }
      else {
        message.str(""); message << "error requested image directory " << dir_in << " is not a directory";
        logwrite(function, message.str());
        dir_out = this->image_dir;
        return(ERROR);
      }
    }
    else {
      message.str(""); message << "error requested image directory " << dir_in << " does not exist";
      logwrite(function, message.str());
      dir_out = this->image_dir;
      return(ERROR);
    }
  }
  /** Common::Common::imdir ***************************************************/


  /** Common::Common:get_fitsname *********************************************/
  /**
   * @fn     get_fitsname
   * @brief  assemble the FITS filename
   * @param  none
   * @return std::string
   *
   * This function assembles the fully qualified path to the output FITS filename
   * using the parts (dir, basename, number) stored in the Common::Common class.
   * If the filename already exists then a -number is inserted, incrementing that
   * number until a unique name is achieved.
   *
   */
  std::string Common::get_fitsname() {
    // width of image_num portion of the filename is at least 4 digits, and grows as needed
    //
    int width = (this->image_num < 10000 ? 4 :   
                (this->image_num < 100000 ? 5 :   
                (this->image_num < 1000000 ? 6 :   
                (this->image_num < 10000000 ? 7 :  
                (this->image_num < 100000000 ? 8 :  
                (this->image_num < 1000000000 ? 9 : 10)))))); 
    // build the filename
    //
    std::stringstream fn;
    fn.str("");
    fn << this->image_dir << "/" << this->image_name << "_" << std::setfill('0') << std::setw(width)
       << this->image_num << ".fits";

    // Check if file exists and include a -# to set apart duplicates.
    //
    struct stat st;
    int dupnumber=1;
    while (stat(fn.str().c_str(), &st) == 0) {
      fn.str("");
      fn << this->image_dir << "/" << this->image_name << "_" << std::setfill('0') << std::setw(width)
         << this->image_num << "-" << dupnumber << ".fits";
      dupnumber++;  // keep incrementing until we have a unique filename
    }

    return fn.str();
  }
  /** Common::Common:get_fitsname *********************************************/


  /** Common::FitsKeys::get_keytype *******************************************/
  /**
   * @fn     get_keytype
   * @brief  return the keyword type based on the keyvalue
   * @param  std::string value
   * @return std::string type: "STRING", "FLOAT", "INT"
   *
   * This function looks at the contents of the value string to determine if it
   * contains an INT, FLOAT or STRING, and returns a string identifying the type.
   * That type is used in FITS_file::add_user_key() for adding keywords to the header.
   *
   */
  std::string FitsKeys::get_keytype(std::string keyvalue) {
    std::size_t pos(0);

    // skip the whitespaces
    pos = keyvalue.find_first_not_of(' ');
    if (pos == keyvalue.size()) return std::string("STRING");   // all spaces, so it's a string

    // check the significand
    if (keyvalue[pos] == '+' || keyvalue[pos] == '-') ++pos;    // skip the sign if exist

    int n_nm, n_pt;
    for (n_nm = 0, n_pt = 0; std::isdigit(keyvalue[pos]) || keyvalue[pos] == '.'; ++pos) {
        keyvalue[pos] == '.' ? ++n_pt : ++n_nm;
    }
    if (n_pt>1 || n_nm<1 || pos<keyvalue.size()){ // no more than one point, no numbers, or a non-digit character
      return std::string("STRING");               // then it's a string
    }

    // skip the trailing whitespaces
    while (keyvalue[pos] == ' ') {
        ++pos;
    }

    if (pos == keyvalue.size()) {
      if (keyvalue.find(".") == std::string::npos)
        return std::string("INT");           // all numbers and no decimals, it's an integer
      else
        return std::string("FLOAT");         // numbers with a decimal, it's a float
    }
    else return std::string("STRING");       // lastly, must be a string
  }
  /** Common::FitsKeys::get_keytype *******************************************/


  /** Common::FitsKeys::listkeys **********************************************/
  /**
   * @fn     listkeys
   * @brief  list FITS keywords in internal database
   * @param  none
   * @return NO_ERROR
   *
   */
  long FitsKeys::listkeys() {
    std::string function = "Common::FitsKeys::listkeys";
    std::stringstream message;
    fits_key_t::iterator keyit;
    for (keyit  = this->keydb.begin();
         keyit != this->keydb.end();
         keyit++) {
      message.str("");
      message << keyit->second.keyword << " = " << keyit->second.keyvalue;
      if ( ! keyit->second.keycomment.empty() ) message << " // " << keyit->second.keycomment;
      message << " (" << keyit->second.keytype << ")";
      logwrite(function, message.str());
    }
    return(NO_ERROR);
  }
  /** Common::FitsKeys::listkeys **********************************************/


  /** Common::FitsKeys::addkey ************************************************/
  /**
   * @fn     addkey
   * @brief  add FITS keyword to internal database
   * @param  std::string arg
   * @return ERROR for improper input arg, otherwise NO_ERROR
   *
   * Expected format of input arg is KEYWORD=VALUE//COMMENT
   * where COMMENT is optional. KEYWORDs are automatically converted to uppercase.
   *
   * Internal database is Common::FitsKeys::keydb
   * 
   */
  long FitsKeys::addkey(std::string arg) {
    std::string function = "Common::FitsKeys::addkey";
    std::stringstream message;
    std::vector<std::string> tokens;
    std::string keyword, keystring, keyvalue, keytype, keycomment;
    std::string comment_separator = "//";

    // There must be one equal '=' sign in the incoming string, so that will make two tokens here
    //
    Tokenize(arg, tokens, "=");
    if (tokens.size() != 2) {
      logwrite(function, "error: missing or too many '=': expected KEYWORD=VALUE//COMMENT (optional comment)");
      return(ERROR);
    }

    keyword   = tokens[0].substr(0,8);                                     // truncate keyword to 8 characters
    keyword   = keyword.erase(keyword.find_last_not_of(" ")+1);            // remove trailing spaces from keyword
    std::locale loc;
    for (std::string::size_type ii=0; ii<keyword.length(); ++ii) {         // Convert keyword to upper case:
      keyword[ii] = std::toupper(keyword[ii],loc);                         // prevents duplications in STL map
    }
    keystring = tokens[1];                                                 // tokenize the rest in a moment

    size_t pos = keystring.find(comment_separator);                        // location of the comment separator
    keyvalue = keystring.substr(0, pos);                                   // keyvalue is everything up to comment
    keyvalue = keyvalue.erase(0, keyvalue.find_first_not_of(" "));         // remove leading spaces from keyvalue
    if (pos != std::string::npos) {
      keycomment = keystring.erase(0, pos + comment_separator.length());
      keycomment = keycomment.erase(0, keycomment.find_first_not_of(" ")); // remove leading spaces from keycomment
    }

    // Delete the keydb entry for associated keyword if the keyvalue is a sole period '.'
    //
    if (keyvalue == ".") {
      fits_key_t::iterator ii = this->keydb.find(keyword);
      if (ii==this->keydb.end()) {
        message.str(""); message << "error: keyword " << keyword << " not found";
        logwrite(function, message.str());
      }
      else {
        this->keydb.erase(ii);
        message.str(""); message << "keyword " << keyword << " erased";
        logwrite(function, message.str());
      }
      return(NO_ERROR);
    }

    // check for instances of the comment separator in keycomment
    //
    if (keycomment.find(comment_separator) != std::string::npos) {
      message.str(""); message << "error: FITS comment delimiter: found too many instancces of " << comment_separator << " in keycomment";
      logwrite(function, message.str());
      return(NO_ERROR);
    }

    // insert new entry into the database
    //
    this->keydb[keyword].keyword    = keyword;
    this->keydb[keyword].keytype    = this->get_keytype(keyvalue);
    this->keydb[keyword].keyvalue   = keyvalue;
    this->keydb[keyword].keycomment = keycomment;

    return(NO_ERROR);
  }
  /** Common::FitsKeys::addkey ************************************************/
}
