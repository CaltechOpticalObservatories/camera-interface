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

#include "logentry.h"
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
    const char* function = "Common::Common::imnum";

    // If no string is passed then this is a request; return the current value.
    //
    if (num_in.empty()) {
      Logf("(%s) image number: %d\n", function, this->image_num);
      num_out = std::to_string(this->image_num);
      return(NO_ERROR);
    }

    else {                             // Otherwise check the incoming value
      int num = std::stoi(num_in);
      if (num < 0) {                   // can't be negative
        Logf("(%s) error requested image number %d must be >= 0\n", function, num);
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
    const char* function = "Common::Common::imname";

    // If no string is passed then this is a request; return the current value.
    //
    if (name_in.empty()) {
      Logf("(%s) image name: %s\n", function, this->image_name.c_str());
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
    const char* function = "Common::Common::imdir";

    // If no string is passed then this is a request; return the current value.
    //
    if (dir_in.empty()) {
      Logf("(%s) image directory: %s\n", function, this->image_dir.c_str());
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
            Logf("(%s) error cannot write to requested image directory %s\n", function, dir_in.c_str());
            dir_out = this->image_dir;
            return(ERROR);
          }
          else {                                           // remove the test file
            std::fclose(fp);
            if (std::remove(testfile.c_str()) != 0) Logf("(%s) error removing temporary file %s\n", function, testfile.c_str());
          }
        }
        catch(...) {
          Logf("(%s) error writing to %s\n", function, dir_in.c_str());
          dir_out = this->image_dir;
          return(ERROR);
        }
        this->image_dir = dir_in;                          // passed all tests so set the image_dir
        dir_out = this->image_dir;
        return(NO_ERROR);
      }
      else {
        Logf("(%s) error requested image directory %s is not a directory\n", function, dir_in.c_str());
        dir_out = this->image_dir;
        return(ERROR);
      }
    }
    else {
      Logf("(%s) error requested image directory %s does not exist\n", function, dir_in.c_str());
      dir_out = this->image_dir;
      return(ERROR);
    }
  }
  /** Common::Common::imdir ***************************************************/


  /** Common::Utilities::get_fitsname *****************************************/
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
  /** Common::Utilities::get_fitsname *****************************************/


  /** Common::Utilities::parse_val ********************************************/
  /**
   * @fn     parse_val
   * @brief  returns an unsigned int from a string
   * @param  string
   * @return unsigned int
   *
   */
  unsigned int Utilities::parse_val(const std::string& str) {
    std::stringstream v;
    unsigned int u=0;
    if ( (str.find("0x")!=std::string::npos) || (str.find("0X")!=std::string::npos))
      v << std::hex << str;
    else
      v << std::dec << str;
    v >> u;
    return u;
  }
  /** Common::Utilities::parse_val ********************************************/


  /** Common::Utilities::Tokenize *********************************************/
  /**
   * @fn     Tokenize
   * @brief  break a string into a vector
   * @param  str, input string
   * @param  tokens, vector of tokens
   * @param  delimiters, string
   * @return number of tokens
   *
   */
  int Utilities::Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters) {
    // Clear the tokens vector, we only want to putput new tokens
    tokens.clear();

    // If the string is zero length, return now with no tokens
    if (str.length() == 0) { return(0); }

    // Skip delimiters at beginning.
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);

    std::string quote("\"");
    unsigned int quote_start = str.find(quote); //finds first quote mark
    bool quotes_found = false;

    if (quote_start != std::string::npos) {
    }
    else {
      quote_start = -1;
    }

    while (std::string::npos != pos || std::string::npos != lastPos) {
      if (quotes_found == true) {
        tokens.push_back(str.substr(lastPos + 1, pos - lastPos - 2));
        pos++;
        lastPos = str.find_first_not_of(delimiters, pos);
        quotes_found = false;
      }
      else {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));

        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
      }

      // If the next character is a quote, grab between the quotes 
      if (std::string::npos != lastPos && lastPos == quote_start){
        pos = str.find_first_of("\"", lastPos + 1) + 1;
        quotes_found = true;
      }
      // Otherwise, find next "non-delimiter"
      else {
        pos = str.find_first_of(delimiters, lastPos);
      }
    }
    return(tokens.size());
  }
  /** Common::Utilities::Tokenize *********************************************/


  /** Common::Utilities::chrrep ***********************************************/
  /**
   * @fn     chrrep
   * @brief  replace one character within a string with a new character
   * @param  str     pointer to string
   * @param  oldchr  the old character to replace in the string
   * @param  newchr  the replacement value
   * @return none
   *
   * This function modifies the original string pointed to by *str.
   *
   */
  void Utilities::chrrep(char *str, char oldchr, char newchr) {
    char *p = str;
    int   i=0;
    while ( *p ) {
      if (*p == oldchr) {
        /**
         * special case for DEL character. move string over by one char
         */
        if (newchr == 127) {    // if newchr==DEL copy memory after that chr
          memmove(&str[i], &str[i+1], strlen(str)-i);
        }
        else {                  // otherwise just replace chr
          *p = newchr;
        }
      }
      ++p; i++;                 // increment pointer and byte counter
    }
  }
  /** Common::Utilities::chrrep ***********************************************/


  /** Common::Utilities::get_time_string **************************************/
  /**
   * @fn     get_time_string
   * @brief  
   * @param  
   * @return 
   *
   */
  std::string Utilities::get_time_string() {
    std::stringstream current_time;  // String to contain the time
    time_t t;                        // Container for system time
    struct timespec data;            // Time of day container
    struct tm gmt;                   // GMT time container

    // Get the system time, return a bad timestamp on error
    //
    if (clock_gettime(CLOCK_REALTIME, &data) != 0) return("9999-99-99T99:99:99.999");

    // Convert the time of day to GMT
    //
    t = data.tv_sec;
    if (gmtime_r(&t, &gmt) == NULL) return("9999-99-99T99:99:99.999");

    current_time.setf(std::ios_base::right);
    current_time << std::setfill('0') << std::setprecision(0)
                 << std::setw(4) << gmt.tm_year + 1900   << "-"
                 << std::setw(2) << gmt.tm_mon + 1 << "-"
                 << std::setw(2) << gmt.tm_mday    << "T"
                 << std::setw(2) << gmt.tm_hour  << ":"
                 << std::setw(2) << gmt.tm_min << ":"
                 << std::setw(2) << gmt.tm_sec
                 << "." << std::setw(3) << data.tv_nsec/1000000;

    return(current_time.str());
  }
  /** Common::Utilities::get_time_string **************************************/


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
    const char* function = "Common::FitsKeys::listkeys";
    fits_key_t::iterator keyit;
    for (keyit  = this->keydb.begin();
         keyit != this->keydb.end();
         keyit++) {
      Logf("(%s) %s = %s%s%s (%s)\n", function,
           keyit->second.keyword.c_str(),
           keyit->second.keyvalue.c_str(),
           keyit->second.keycomment.empty() ? "" : " // ",
           keyit->second.keycomment.empty() ? "" : keyit->second.keycomment.c_str(),
           keyit->second.keytype.c_str());
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
    const char* function = "Common::FitsKeys::addkey";
    Utilities util;
    std::vector<std::string> tokens;
    std::string keyword, keystring, keyvalue, keytype, keycomment;
    std::string comment_separator = "//";

    // There must be one equal '=' sign in the incoming string, so that will make two tokens here
    //
    util.Tokenize(arg, tokens, "=");
    if (tokens.size() != 2) {
      Logf("(%s) error: missing or too many '=': expected KEYWORD=VALUE//COMMENT (optional comment)\n", function);
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
        Logf("(%s) error: keyword %s not found\n", function, keyword.c_str());
      }
      else {
        this->keydb.erase(ii);
        Logf("(%s) keyword %s erased\n", function, keyword.c_str());
      }
      return(NO_ERROR);
    }

    // check for instances of the comment separator in keycomment
    //
    if (keycomment.find(comment_separator) != std::string::npos) {
      Logf("(%s) error: FITS comment delimiter: found too many instancces of %s in keycomment\n",
           function, comment_separator.c_str());
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
