/**
 * @file    logentry.cpp
 * @brief   logentry functions
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * Write time-stamped entries to log file using logwrite(function, message)
 * where function is a string representing the calling function and
 * message is an arbitrary string.
 *
 */

#include "logentry.h"

std::mutex loglock;
std::mutex debuglock;
std::ofstream filestream;      /// IO stream class
std::ofstream debugstream;     /// IO stream class
unsigned int nextday = 86410;  /// number of seconds until a new day
bool to_stderr = true;         /// write to stderr
std::string tmzone_log="";     /// optional time zone for logging


/** init_log *****************************************************************/
/**
 * @fn         init_log
 * @brief      initializes the logging
 * @param[in]  name       name of the log file in logpath
 * @param[in]  logpath    path for log file
 * @param[in]  logstderr  should I also print to stderr?
 * @param[in]  logtmzone  optional time zone for logging
 * @return     0 on success, 1 on error
 *
 * Opens an ofstream to the specified logfile, "logpath/name_YYYYMMDD.log"
 * where logpath and name are passed in as parameters.
 *
 */
long init_log( std::string name, std::string logpath, std::string logstderr, std::string logtmzone ) {
  std::string function = "init_log";
  std::stringstream filename;
  std::stringstream debugfile;
  std::stringstream message;
  int year, mon, mday, hour, min, sec, usec;
  long error = 0;

  to_stderr = ( logstderr=="false" ? false : true );

  tmzone_log = logtmzone;

  if ( ( error = get_time( tmzone_log, year, mon, mday, hour, min, sec, usec ) ) ) return error;

  // assemble log file name from the passed-in arguments and current date
  //
  filename << logpath << "/" << name << "_" << std::setfill('0')
                      << std::setw(4) << year
                      << std::setw(2) << mon
                      << std::setw(2) << mday << ".log";
  debugfile << logpath << "/debug_" << std::setfill('0')
                       << std::setw(4) << year
                       << std::setw(2) << mon
                       << std::setw(2) << mday << ".log";

  // number of seconds until a new day
  //
  nextday = (unsigned int)(86410 - hour*3600 - min*60 - sec);

  // open the log file stream for append
  //
  try {
    filestream.open(filename.str(), std::ios_base::app);
    debugstream.open(debugfile.str(), std::ios_base::app);
  }
  catch(...) {
    message << "ERROR: opening log file " << filename.str() << ": " << std::strerror(errno);
    logwrite(function, message.str());
    return 1;
  }
  if ( ! filestream.is_open() ) {
    message << "ERROR: log file " << filename.str() << " not open";
    logwrite(function, message.str());
    return 1;
  }

  return 0;
}
/** init_log *****************************************************************/


/** close_log ****************************************************************/
/**
 * @fn         close_log
 * @brief      closes the logfile stream
 * @param[in]  none
 * @return     none
 *
 * Call this from the main class deconstructor to clean up the log file.
 *
 */
void close_log() {
  if (filestream.is_open() == true) {
    filestream.flush(); debugstream.flush();
    filestream.close(); debugstream.close();
  }
}
/** close_log ****************************************************************/


/** logwrite *****************************************************************/
/**
 * @fn         logwrite
 * @brief      create a log file entry
 * @param[in]  function
 * @param[in]  message
 * @return     none
 *
 * Create a time-stamped entry in the log file in the form of:
 * YYYY-MM-DDTHH:MM:SS.ssssss (function) message\n
 *
 * This function can also be used for logging to stderr, even if the
 * log filestream isn't open.
 *
 */
void logwrite(std::string function, std::string message) {
  std::stringstream logmsg;
  std::string timestamp = get_timestamp(tmzone_log); // get the current time (defined in utilities.h)

  std::lock_guard<std::mutex> lock(loglock);     // lock mutex to protect from multiple access

  logmsg << timestamp << "  (" << function << ") " << message << "\n";

  if (filestream.is_open()) {
    filestream << logmsg.str();                  // send to the file stream (if open)
    filestream.flush();
  }
  if ( to_stderr ) std::cerr << logmsg.str();    // send to standard error unless restricted
}
/** logwrite *****************************************************************/

void debug(const std::string &message) {
  auto now = std::chrono::system_clock::now();
  auto t   = std::chrono::system_clock::to_time_t(now);
  auto usec = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
  std::tm mytime;
  localtime_r( &t, &mytime );

  std::lock_guard<std::mutex> lock(debuglock);
  debugstream << std::put_time( &mytime, "%Y-%m-%d %H:%M:%S" ) << "." << std::setfill('0') << std::setw(6) << usec.count()
              << " " << message << "\n";
  debugstream.flush();
}

