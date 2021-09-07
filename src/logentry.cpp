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
std::ofstream filestream;      //!< IO stream class
unsigned int nextday = 86410;  //!< number of seconds until a new day


/** initlog ******************************************************************/
/**
 * @fn     initlog
 * @brief  initializes the logging
 * @param  none
 * @return 0 on success, 1 on error
 *
 * Opens an ofstream to the specified logfile, "LOGPATH/LOGNAME_YYYYMMDD.log"
 * LOGPATH and LOGNAME are defined in logentry.h
 *
 */
long initlog(std::string logpath) {
  std::string function = "initlog";
  std::stringstream filename;
  std::stringstream message;
  int year, mon, mday, hour, min, sec, usec;
  long error = 0;

  if ( ( error = get_time( year, mon, mday, hour, min, sec, usec ) ) ) return error;

  // assemble log file name from #define and current date
  //
  filename << logpath << "/cameraserver_" << std::setfill('0')
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
/** initlog ******************************************************************/


/** closelog *****************************************************************/
/**
 * @fn     closelog
 * @brief  closes the logfile stream
 * @param  none
 * @return none
 *
 * Call this from the main class deconstructor to clean up the log file.
 *
 */
void closelog() {
  if (filestream.is_open() == true) {
    filestream.flush();
    filestream.close();
  }
}
/** closelog *****************************************************************/


/** logwrite *****************************************************************/
/**
 * @fn     logwrite
 * @brief  create a log file entry
 * @param  function
 * @param  message
 * @return none
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
  std::string timestamp = get_timestamp();       // get the current time (defined in utilities.h)

  std::lock_guard<std::mutex> lock(loglock);     // lock mutex to protect from multiple access

  logmsg << timestamp << "  (" << function << ") " << message << "\n";

  if (filestream.is_open()) {
    filestream << logmsg.str();                  // send to the file stream (if open)
    filestream.flush();
  }
  std::cerr << logmsg.str();                     // send to standard error
}
/** logwrite *****************************************************************/

