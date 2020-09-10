/** ---------------------------------------------------------------------------
 * @fn       logentry.h
 * @brief    include file for logging functions
 * @author   David Hale <dhale@astro.caltech.edu>
 *
 */
#ifndef LOGENTRY_H
#define LOGENTRY_H

#include <fstream>
#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <ctime>
#include "utilities.h"

long initlog(std::string logpath);                         // initialize the logging system
void closelog();                                           // close the log file stream
void logwrite(std::string function, std::string message);  // create a time-stamped log entry "message" from "function"
void create_new_log(int nextday_in, std::string logpath);  // thread to create a new log file each day

#endif
