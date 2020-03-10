/**
 * @file    common.cpp
 * @brief   common interface functions
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include <iostream>
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

}
