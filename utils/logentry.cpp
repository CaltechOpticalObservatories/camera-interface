/**
 * @file    logentry.cpp
 * @brief   logentry functions
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * Write time-stamped entries to log file using logwrite(function, message)
 * where function is a string representing the calling function and
 * message is an arbitrary string.
 *
 */

#include "logentry.h"

std::mutex loglock; /// mutex to protect from multiple access
std::ofstream filestream; /// IO stream class
unsigned int nextday = 86410; /// number of seconds until a new day
bool to_stderr = true; /// write to stderr by default
std::string tmzone_log; /// optional time zone for logging

std::queue<std::string> log_queue;
std::condition_variable log_cv;
std::atomic<bool> logger_running{true};
std::thread logger_thread;


void logger_worker() {
    while (logger_running || !log_queue.empty()) {
        std::unique_lock<std::mutex> lock(loglock);
        log_cv.wait(lock, [] { return !log_queue.empty() || !logger_running; });

        while (!log_queue.empty()) {
            std::string msg = std::move(log_queue.front());
            log_queue.pop();
            lock.unlock();

            if (filestream.is_open()) filestream << msg;
            if (to_stderr) std::cerr << msg;

            lock.lock();
        }
        if (filestream.is_open()) filestream.flush();
    }
}


/***** init_log ***************************************************************/
/**
 * @brief      initializes the logging
 * @param[in]  name       name of the log file in logpath
 * @param[in]  logpath    path for log file
 * @param[in]  logstderr  should logwrite also print to stderr?
 * @param[in]  logtmzone  optional time zone for logging
 * @return     0 on success, 1 on error
 *
 * Opens an ofstream to the specified logfile, "logpath/name_YYYYMMDD.log"
 * where logpath and name are passed in as parameters.
 *
 */
long init_log(std::string name, std::string logpath, std::string logstderr, std::string logtmzone) {
    const std::string function = "init_log";
    std::stringstream filename;
    std::stringstream message;
    int year, mon, mday, hour, min, sec, usec;
    long error = 0;

    to_stderr = (logstderr != "false");
    tmzone_log = logtmzone;

    if ((error = get_time(tmzone_log, year, mon, mday, hour, min, sec, usec))) return error;

    filename << logpath << "/" << name << "_" << std::setfill('0')
             << std::setw(4) << year << std::setw(2) << mon << std::setw(2) << mday << ".log";

    nextday = static_cast<unsigned int>(86410 - hour * 3600 - min * 60 - sec);

    try {
        filestream.open(filename.str(), std::ios_base::app);
    } catch (...) {
        std::cerr << "ERROR: opening log file failed\n";
        return 1;
    }

    if (is_owner(filename.str())) {
        try {
            std::filesystem::permissions(filename.str(), std::filesystem::perms::all,
                                         std::filesystem::perm_options::remove);
            std::filesystem::permissions(filename.str(),
                                         std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write |
                                         std::filesystem::perms::group_read |
                                         std::filesystem::perms::group_write |
                                         std::filesystem::perms::others_read,
                                         std::filesystem::perm_options::add);
        } catch (...) {
            std::cerr << "ERROR: setting permissions failed\n";
            return 1;
        }
    }

    if (!has_write_permission(filename.str()) || !filestream.is_open()) {
        std::cerr << "ERROR: no write permission or file not open\n";
        return 1;
    }

    logger_running = true;
    logger_thread = std::thread(logger_worker);
    return 0;
}

/***** init_log ***************************************************************/


/***** close_log **************************************************************/
/**
 * @brief      closes the logfile stream
 * @details    Call this from the main class deconstructor to clean up the log file.
 *
 */
void close_log() {
    logger_running = false;
    log_cv.notify_one();
    if (logger_thread.joinable()) logger_thread.join();
    if (filestream.is_open()) filestream.close();
}

/***** close_log **************************************************************/


/***** logwrite ***************************************************************/
/**
 * @brief      create a log file entry
 * @param[in]  function  string containing the Namespace::Class::function
 * @param[in]  message   string to log
 *
 * Create a time-stamped entry in the log file in the form of:
 * YYYY-MM-DDTHH:MM:SS.ssssss (function) message\n
 *
 * This function can also be used for logging to stderr, even if the
 * log filestream isn't open.
 *
 */
void logwrite(const std::string &function, const std::string &message) {
    std::stringstream logmsg;
    std::string timestamp = get_timestamp(tmzone_log);
    logmsg << timestamp << "  (" << function << ") " << message << "\n";

    {
        std::lock_guard<std::mutex> lock(loglock);
        log_queue.push(logmsg.str());
    }
    log_cv.notify_one();
}

/***** logwrite ***************************************************************/
