#ifndef NEWLOGENTRY_H
#define NEWLOGENTRY_H

#include <fstream>
#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <ctime>
#include "utilities.h"

// where to save the logs
//
#define LOGPATH "/home/user/Logs"

// base name for log file name (date will be appended)
//
#define LOGNAME "archon"

long initlog();                                            // initialize the logging system
void closelog();                                           // close the log file stream
void logwrite(std::string function, std::string message);  // create a time-stamped log entry "message" from "function"
void create_new_log(int nextday_in);                       // thread to create a new log file each day

#endif
