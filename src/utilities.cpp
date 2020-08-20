/**
 * @file    utilities.cpp
 * @brief   some handy utilities to use anywhere
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "utilities.h"


  /** parse_val ***************************************************************/
  /**
   * @fn     parse_val
   * @brief  returns an unsigned int from a string
   * @param  string
   * @return unsigned int
   *
   */
  unsigned int parse_val(const std::string& str) {
    std::stringstream v;
    unsigned int u=0;
    if ( (str.find("0x")!=std::string::npos) || (str.find("0X")!=std::string::npos))
      v << std::hex << str;
    else
      v << std::dec << str;
    v >> u;
    return u;
  }
  /** parse_val ***************************************************************/


  /** Tokenize ****************************************************************/
  /**
   * @fn     Tokenize
   * @brief  break a string into a vector
   * @param  str, input string
   * @param  tokens, vector of tokens
   * @param  delimiters, string
   * @return number of tokens
   *
   */
  int Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters) {
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
  /** Tokenize ****************************************************************/


  /** chrrep ******************************************************************/
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
  void chrrep(char *str, char oldchr, char newchr) {
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
  /** chrrep ******************************************************************/


  /** get_timenow *************************************************************/
  /**
   * @fn     get_timenow
   * @brief  
   * @param  
   * @return 
   *
   */
  std::tm* get_timenow() {
    std::tm *timenow; 
    std::time_t timer = std::time(nullptr);  // current calendar time encoded as a std::time_t object

    timenow = std::gmtime(&timer);           // convert time since epoch as std::time_t value into calendar time

    if (timenow == NULL) {
      std::perror("(get_timenow) converting time");
      return NULL;
    }
    return timenow;
  }
  /** get_timenow *************************************************************/


  /** get_time_string *********************************************************/
  /**
   * @fn     get_time_string
   * @brief  return current time in formatted string "YYYY-MM-DDTHH:MM:SS.ssssss"
   * @param  
   * @return 
   *
   */
  std::string get_time_string() {
    std::stringstream current_time;  // String to contain the time
    time_t t;                        // Container for system time
    struct timespec data;            // Time of day container
    struct tm gmt;                   // GMT time container

    // Get the system time, return a bad timestamp on error
    //
    if (clock_gettime(CLOCK_REALTIME, &data) != 0) return("9999-99-99T99:99:99.999999");

    // Convert the time of day to GMT
    //
    t = data.tv_sec;
    if (gmtime_r(&t, &gmt) == NULL) return("9999-99-99T99:99:99.999999");

    current_time.setf(std::ios_base::right);
    current_time << std::setfill('0') << std::setprecision(0)
                 << std::setw(4) << gmt.tm_year + 1900   << "-"
                 << std::setw(2) << gmt.tm_mon + 1 << "-"
                 << std::setw(2) << gmt.tm_mday    << "T"
                 << std::setw(2) << gmt.tm_hour  << ":"
                 << std::setw(2) << gmt.tm_min << ":"
                 << std::setw(2) << gmt.tm_sec
                 << "." << std::setw(6) << data.tv_nsec/1000;

    return(current_time.str());
  }
  /** get_time_string *********************************************************/


  /** get_clock_time **********************************************************/
  /**
   * @fn     get_clock_time
   * @brief  get the current clock time using REALTIME flag from the processor
   * @param  nonw
   * @return time in seconds
   *
   */
  double get_clock_time() {
    struct timespec data;  // Container for the current time
    if (clock_gettime(CLOCK_REALTIME, &data) != 0) return 0;
    return ( data.tv_sec + (data.tv_nsec / 1000000000.0) );
  }
  /** get_clock_time **********************************************************/


  /** timeout *****************************************************************/
  /**
   * @fn     timeout
   * @brief  
   * @param  
   * @return 
   *
   */
  void timeout(float seconds, bool next_sec) {
    if (seconds < 1 && seconds > 0) {
      usleep(seconds * 1000000);
    }
    else {
      // Sleep the number of seconds called for
      //
      if (seconds > 0) {
        sleep((int)seconds);
      }

      if (next_sec == true) {
        // Get the current time to the microsecond
        //
        struct timespec data;                // Time of day container
        if(clock_gettime(CLOCK_REALTIME, &data) != 0) return;

        // If the microseconds are less than 0.9999999, we sleep that long
        //
        if (data.tv_nsec/1000000000 < 0.999999999) {
          usleep((useconds_t)((999999999-data.tv_nsec)/1000));
        }
      }
    }
  }
