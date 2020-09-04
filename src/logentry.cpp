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

std::mutex newloglock;
std::ofstream filestream;      //!< IO stream class


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
long initlog() {
  std::tm *timenow;
  std::stringstream filename;

  std::lock_guard<std::mutex> lock(newloglock);

  timenow = get_timenow();                 // current calendar time (defined in utilities.h)
  if (timenow == NULL) return 1;           // error

  // assemble log file name from #define and current date
  //
  filename << LOGPATH << "/" << LOGNAME << "_" << std::setfill('0')
                                               << std::setw(4) << timenow->tm_year + 1900
                                               << std::setw(2) << timenow->tm_mon + 1
                                               << std::setw(2) << timenow->tm_mday << ".log";
  // number of seconds until a new day
  //
  int nextday = (unsigned int)(86410 - timenow->tm_hour*3600 - timenow->tm_min*60 - timenow->tm_sec);

  // open the log file stream for append
  //
  try {
    filestream.open(filename.str(), std::ios_base::app);
  }
  catch(...) {
    std::cerr << "(initlog) error opening log file " << filename.str() << ": " << std::strerror(errno) << "\n";
    return 1;
  }

  // spawn a thread to create a new logfile each day
  //
  std::thread(create_new_log, nextday).detach();

  return 0;
}
/** initlog ******************************************************************/


/** create_new_log ***********************************************************/
/**
 * @fn     create_new_log
 * @brief  close current and create new logfile
 * @param  nextday_in
 * @return none
 *
 * This runs in its own detached thread spawned by initlog, sleeps for the 
 * specified number of seconds (seconds until a new day) at which time it
 * closes the logfile and creates a new one.
 *
 */
void create_new_log(int nextday_in) {
  std::this_thread::sleep_for(std::chrono::seconds(nextday_in));
  closelog();
  initlog();
}
/** create_new_log ***********************************************************/


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
 */
void logwrite(std::string function, std::string message) {
  std::stringstream logmsg;
  std::string timestamp = get_time_string();     // get the current time (defined in utilities.h)

  std::lock_guard<std::mutex> lock(newloglock);  // lock mutex to protect from multiple access

  logmsg << timestamp << "  (" << function << ") " << message << "\n";

  filestream << logmsg.str();                    // send to the file stream
  filestream.flush();
  std::cerr << logmsg.str();                     // send to standard error
}
/** logwrite *****************************************************************/

