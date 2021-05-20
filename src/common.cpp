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
    this->is_datacube = false;
    this->image_dir = "/tmp";
    this->base_name = "image";
    this->image_num = 0;
    this->fits_naming = "time";
    this->fitstime = "";
    this->abortstate = false;
    this->_abortstate = false;
  }


  /** Common::Common::abort ***************************************************/
  /**
   * @fn     abort
   * @brief  abort the current operation (exposure, readout, etc.)
   * @param  none
   * @return none
   *
   */
  void Common::abort() {
    std::string function = "Common::Common::abort";
    std::stringstream message;
    this->abortstate = true;
    logwrite(function, "received abort");
    return;
  }
  /** Common::Common::abort ***************************************************/

void Common::set_abortstate(bool state) {
  this->abort_mutex.lock();
  this->abortstate = state;
  this->_abortstate = state;
  this->abort_mutex.unlock();
}

bool Common::get_abortstate() {
  bool state;
  this->abort_mutex.lock();
  state = this->abortstate;
  this->abort_mutex.unlock();
  return( state );
}

  /** Common::Common::fitsnaming **********************************************/
  /**
   * @fn     fitsnaming
   * @brief  set or get the fits naming type
   * @param  std::string naming_in
   * @param  std::string& naming_out
   * @return ERROR or NO_ERROR
   *
   */
  long Common::fitsnaming(std::string naming_in, std::string& naming_out) {
    std::string function = "Common::Common::fitsnaming";
    std::stringstream message;
    long error;

    message.str(""); message << "fits naming: " << this->fits_naming;

    if (naming_in.empty()) {           // no string passed so this is a request. do nothing but will return the current value
      error = NO_ERROR;
    }
    else
    if ( (naming_in.compare("time")==0) || (naming_in.compare("number")==0) ) {
      this->fits_naming = naming_in;   // set new value
      error = NO_ERROR;
    }
    else {
      message.str(""); message << "error: invalid naming type: " << naming_in << ". Must be \"time\" or \"number\".";
      error = ERROR;
    }

    logwrite(function, message.str());
    naming_out = this->fits_naming;    // return the current value
    return error;
  }
  /** Common::Common::fitsnaming **********************************************/


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
      catch (std::invalid_argument &) {
        logwrite(function, "error invalid number: unable to convert to integer");
        return(ERROR);
      }
      catch (std::out_of_range &) {
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


  /** Common::Common::basename ************************************************/
  /**
   * @fn     basename
   * @brief  set or get the base_name member
   * @param  std::string name_in
   * @param  std::string& name_out
   * @return NO_ERROR
   *
   * This function is overloaded with a form that doesn't use a return value.
   *
   */
  long Common::basename(std::string name_in) {
    std::string dontcare;
    return( basename(name_in, dontcare) );
  }
  long Common::basename(std::string name_in, std::string& name_out) {
    std::string function = "Common::Common::basename";
    std::stringstream message;

    // If no string is passed then this is a request; return the current value.
    //
    if (name_in.empty()) {
      message.str(""); message << "base name is " << this->base_name;
      logwrite(function, message.str());
      name_out = this->base_name;
    }
    else {                             // Otherwise set the image name
      this->base_name = name_in;
      name_out = name_in;
    }
    return(NO_ERROR);
  }
  /** Common::Common::basename ************************************************/


  /** Common::Common::imdir ***************************************************/
  /**
   * @fn     imdir
   * @brief  set or get the image_dir base directory
   * @param  std::string dir_in
   * @param  std::string& dir_out (pass reference for return value)
   * @return ERROR or NO_ERROR
   *
   * This function is overloaded with a form that doesn't use a return value reference.
   *
   * The base directory for images is this->image_dir. It is set (or read) here. It
   * is not created; it must already exist. The date subdirectory is added later, in
   * the get_fitsname() function.
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
      // Use stat to check that the requested dir_in is a directory
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
        dir_out = this->image_dir;                         // and the return var
        return(NO_ERROR);                                  // return success
      }
      else {
        message.str(""); message << "error requested image directory " << dir_in << " is not a directory";
        logwrite(function, message.str());
        dir_out = this->image_dir;                         // no changes, return the current image_dir
        return(ERROR);
      }
    }
    else {
      message.str(""); message << "error requested image directory " << dir_in << " does not exist";
      dir_out = this->image_dir;
      return(ERROR);
    }
  }
  /** Common::Common::imdir ***************************************************/


  /** Common::Common:set_fitstime *********************************************/
  /**
   * @fn     set_fitstime
   * @brief  set the "fitstime" variable used for the filename
   * @param  string formatted as "YYYY-MM-DDTHH:MM:SS.ssssss"
   * @return std::string
   *
   * The Common class has a public string variable "fitstime" which is
   * to be used for the FITS filename, when the time-format is selected.
   * This time should be the whole-second of the time that the exposure
   * was started, so that time is passed in here. This function strips
   * that string down to just the numerals for use in the filename.
   *
   */
  void Common::set_fitstime(std::string time_in) {
    std::string function = "Common::Common::set_fitstime";
    std::stringstream message;

    if ( time_in.length() != 26 ) {  // wrong number of characters, input can't be formatted correctly
      message.str(""); message << "error: bad input time: " << time_in;
      logwrite(function, message.str());
      this->fitstime = "99999999999999";
      return;
    }

    this->fitstime = time_in.substr(0,4)     // YYYY
                   + time_in.substr(5,2)     // MM
                   + time_in.substr(8,2)     // DD
                   + time_in.substr(11,2)    // HH
                   + time_in.substr(14,2)    // MM
                   + time_in.substr(17,2);   // SS

    return;
  }
  /** Common::Common:set_fitstime *********************************************/


  /** Common::Common:get_fitsname *********************************************/
  /**
   * @fn     get_fitsname
   * @brief  assemble the FITS filename
   * @param  std::string controllerid (optional, due to overloading)
   * @param  std::string &name_out reference for name
   * @return ERROR or NO_ERROR
   *
   * This function assembles the fully qualified path to the output FITS filename
   * using the parts (dir, basename, time or number) stored in the Common::Common class.
   * If the filename already exists then a -number is inserted, incrementing that
   * number until a unique name is achieved.
   *
   * This function is overloaded, to allow passing a controller id to include in the filename.
   *
   */
  long Common::get_fitsname(std::string &name_out) {
    return ( this->get_fitsname("", name_out) );
  }
  long Common::get_fitsname(std::string controllerid, std::string &name_out) {
    std::string function = "Common::Common::get_fitsname";
    std::stringstream message;
    std::stringstream fn, fitsname;

    // image_dir is the requested base directory and now add on the date directory
    //
    std::stringstream basedir_datedir;
    basedir_datedir << this->image_dir << "/" << get_system_date();

    // Make sure the directory exists
    //
    DIR *dirp;                                             // pointer to the directory
    if ( (dirp = opendir(basedir_datedir.str().c_str())) == NULL ) {
      // If directory doesn't exist then try to create it.
      // Note that this only creates the bottom-level directory, the added date part.
      // The base directory has to exist.
      //
      if ( (mkdir(basedir_datedir.str().c_str(), S_IRUSR | S_IWUSR | S_IXUSR)) == 0 ) {
        message.str(""); message << "created date subdirectory " << basedir_datedir.str();
        logwrite(function, message.str());
      }
      else {                                               // error creating date subdirectory
        message.str("");
        message << "error " << errno << " creating date subdirectory " << basedir_datedir.str() << ": " << strerror(errno);
        logwrite(function, message.str());
        // a common error might be that the base directory doesn't exist
        //
        if (errno==ENOENT) {
          message.str("");
          message << "requested base directory " << basedir_datedir.str() << " does not exist";
          logwrite(function, message.str());
        }
        return(ERROR);
      }
    }
    else {
      closedir(dirp);                                      // directory already existed so close it
    }

    // start building the filename with directory/basename_
    // where "basedir_datedir" is "image_dir/date" which was just assembled above
    //
    fitsname.str("");
    fitsname << basedir_datedir.str() << "/" << this->base_name << "_";

    // add the controllerid if one is given
    //
    if ( ! controllerid.empty() ) {
      fitsname << controllerid << "_";
    }

    // add the time or number suffix
    //
    if (this->fits_naming.compare("time")==0) {
      fitsname << this->fitstime;
    }
    else
    if (this->fits_naming.compare("number")==0) {
      // width of image_num portion of the filename is at least 4 digits, and grows as needed
      //
      int width = (this->image_num < 10000 ? 4 :   
                  (this->image_num < 100000 ? 5 :   
                  (this->image_num < 1000000 ? 6 :   
                  (this->image_num < 10000000 ? 7 :  
                  (this->image_num < 100000000 ? 8 :  
                  (this->image_num < 1000000000 ? 9 : 10)))))); 
      fitsname << std::setfill('0') << std::setw(width) << this->image_num;
    }

    // Check if file exists and include a -# to set apart duplicates.
    //
    struct stat st;
    int dupnumber=1;
    fn.str(""); fn << fitsname.str();
    fn << ".fits";
    while (stat(fn.str().c_str(), &st) == 0) {
      fn.str(""); fn << fitsname.str();
      fn << "-" << dupnumber << ".fits";
      dupnumber++;  // keep incrementing until we have a unique filename
    }

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] fits_naming=" << this->fits_naming 
                             << " controllerid=" << controllerid
                             << " will write to file: " << fn.str();
    logwrite(function, message.str());
#endif

    name_out = fn.str();
    return(NO_ERROR);
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


  /** Common::Common::datacube ************************************************/
  /**
   * @fn     datacube
   * @brief  set or get the datacube state
   * @param  std::string state_in
   * @return true or false
   *
   * The state_in string should be "True" or "False", case-insensitive.
   *
   * This function is overloaded.
   *
   */
  void Common::datacube(bool state_in) {                                  // write-only boolean
    std::string dontcare;
    this->datacube( (state_in ? "true" : "false"), dontcare );
  }
  bool Common::datacube() {                                               // read-only boolean
    return ( this->is_datacube );
  }
  long Common::datacube(std::string state_in, std::string &state_out) {   // read-write string, called from server
    std::string function = "Common::Common::datacube";
    std::stringstream message;
    int error = NO_ERROR;

    // If something is passed then try to use it to set the datacube state
    //
    if ( !state_in.empty() ) {
      try {
        std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::toupper );    // make uppercase
        if (state_in == "FALSE" ) this->is_datacube = false;
        else
        if (state_in == "TRUE"  ) this->is_datacube = true;
        else {
          message.str(""); message << "ERROR: " << state_in << " is invalid. Expecting true or false";
          logwrite(function, message.str());
          error = ERROR;
        }
      }
      catch (...) {
        logwrite(function, "error converting state_in to uppercase");
        error = ERROR;
      }
    }

    // error or not, the state reported is whatever was last successfully set
    //
    state_out = (this->is_datacube ? "true" : "false");
    message.str(""); message << "datacube=" << state_out;
    logwrite(function, message.str());
    this->message.enqueue( message.str() );

    // and this lets the server know if it was set or not
    //
    return( error );
  }
  /** Common::Common::datacube ************************************************/


  /** Common::Queue::enqueue **************************************************/
  /**
   * @fn     enqueue
   * @brief  puts a message into the queue
   * @param  std::string message
   * @return none
   *
   */
  void Queue::enqueue(std::string message) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    message_queue.push(message);
    notifier.notify_one();
  }
  /** Common::Queue::enqueue **************************************************/


  /** Common::Queue::dequeue **************************************************/
  /**
   * @fn     dequeue
   * @brief  pops the first message off the queue
   * @param  none
   * @return std::string message
   *
   * Get the "front"-element.
   * If the queue is empty, wait untill an element is avaiable.
   *
   */
  std::string Queue::dequeue(void) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    while(message_queue.empty()) {
      notifier.wait(lock);   // release lock as long as the wait and reaquire it afterwards.
    }
    std::string message = message_queue.front();
    message_queue.pop();
    return message;
  }
  /** Common::Queue::dequeue **************************************************/

}
