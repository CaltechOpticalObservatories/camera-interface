/** ---------------------------------------------------------------------------
 * @fn       logentry.c
 * @brief    logging functions to print time-stamped info to a file
 * @author   David Hale <dhale@caltech.edu>
 * @date     2008-10-06 created
 * @modified 2009-03-30 fixed initlogentry where filename wasn't printed right
 * @modified 2010-10-06 updated to use gmttime and be more general (for p3k)
 * @modified 2010-12-08 added SZ_BUF
 * @modified 2015-05-29 moved globals from logentry.h to logentry.c
 * @modified 2015-07-22 add func to close logfile, comment out create thread
 * @modified 2016-06-03 in Logf switch from time to timespec to get nanosecs
 *
 * Application requirements to use this API:
 *
 *  1) #include "logentry.h"
 *  2) call initlogentry(APPNAME) once upon initialization of the application,
 *     where APPNAME is a const char string.  This string will be used in 
 *     creating the log filename.
 *  3) call Logf(...) just like you would use printf(...) to print your
 *     timestamped string to the log file.
 *
 */

#include "logentry.h"

/** ---------------------------------------------------------------------------
 * globals
 */
FILE            *logfp;
char             app[SZ_APP];
pthread_mutex_t  loglock = PTHREAD_MUTEX_INITIALIZER;

/** ---------------------------------------------------------------------------
 * @fn     Logf
 * @brief  formatted, time-stamped output to a file
 * @param  variable length format string
 * @return nothing
 *
 * Logf works like standard printf, accepting a variable length format string
 * to specify how subsequent arguments are converted for output. The output
 * is timestamped and written (with timestamp) to the file opened in 
 * initlogentry.
 * 
 */
void Logf(const char *fmt, ...) {
  va_list    argp;
  struct tm *gmt;
  char       buf[SZ_BUF];
/*struct timespec ts;*/
  struct timeval  tv;

  pthread_mutex_lock(&loglock);

/*if (timespec_get(&ts, TIME_UTC)==0) perror("error getting time");*/
  gettimeofday(&tv, NULL);
/*gmt = gmtime(&ts.tv_sec);*/
  gmt = gmtime(&tv.tv_sec);
  fprintf(logfp, "%4d-%02d-%02dT%02d:%02d:%02d.%06ld  ",
          gmt->tm_year+1900, gmt->tm_mon+1, gmt->tm_mday, 
          gmt->tm_hour, gmt->tm_min, gmt->tm_sec, tv.tv_usec);
  fprintf(stdout, "%4d-%02d-%02dT%02d:%02d:%02d.%06ld  ",
          gmt->tm_year+1900, gmt->tm_mon+1, gmt->tm_mday, 
          gmt->tm_hour, gmt->tm_min, gmt->tm_sec, tv.tv_usec);

  va_start(argp, fmt);
  vsnprintf(buf, sizeof(buf), fmt, argp);
  fputs(buf, logfp);  fflush(logfp);   /* send it to the log file, and */
  fputs(buf, stdout); fflush(stdout);  /* send it to stdout            */
  va_end(argp);
  pthread_mutex_unlock(&loglock);
  }

/** ---------------------------------------------------------------------------
 * @fn     Logf_
 * @brief  formatted output to a file
 * @param  variable length format string
 * @return nothing
 *
 * Logf_ works just like Logf but omits the timestamp (useful for printing
 * subsequent lines related to some event).
 *
 */
void Logf_(const char *fmt, ...) {
  va_list    argp;
  char       buf[SZ_BUF];

  pthread_mutex_lock(&loglock);
  va_start(argp, fmt);
  vsnprintf(buf, sizeof(buf), fmt, argp);
  fputs(buf, logfp);  fflush(logfp);   /* send it to the log file, and */
  fputs(buf, stdout); fflush(stdout);  /* send it to stdout            */
  va_end(argp);
  pthread_mutex_unlock(&loglock);
  }

/** ---------------------------------------------------------------------------
 * @fn     close_logentry
 * @brief  closes the file opened by initlogentry
 * @param  none
 * @return 0 if OK, else EOF and sets errno
 *
 */
int close_logentry() {
  if (fclose(logfp) != 0) {
    perror("close_logentry");
    return(EOF);
    }
  return 0;
}

/** ---------------------------------------------------------------------------
 * @fn     initlogentry
 * @brief  initializes the logging functionality, opens log file
 * @param  const char string to represent application name
 * @return 0 if OK, else errno
 *
 * This function opens the log file for writing and spawns a separate thread
 * to create a new logfile on the next day. The const char string passed is
 * used to create the filename.
 *
 */
int initlogentry(const char *appname) {
/*void      *create_new_logfile(void *arg);*/
  char       logfile[128];
  time_t     t;
  struct tm *gmt;
/*
  int        nextday, newlogfile;

  pthread_t      logtid;
  pthread_attr_t logattr;

  pthread_mutex_lock(&loglock);
*/

  snprintf(app, SZ_APP, "%s", appname);

  if (time(&t)==((time_t)-1)) { perror("error getting time"); return(errno); }
  gmt = gmtime(&t);
/*
  nextday=(unsigned int)(86410-gmt->tm_hour*3600-gmt->tm_min*60-gmt->tm_sec);
*/

  sprintf(logfile, "%s/%s_%04d%02d%02d.log", LOGPATH, app, 
                   gmt->tm_year+1900, gmt->tm_mon+1, gmt->tm_mday);

/*if (access(logfile, F_OK) != 0) newlogfile=TRUE; else newlogfile=FALSE;*/
  logfp = fopen(logfile, "a");  //TODO this will fail if the path doesn't exist or can't be written
  if (logfp==NULL) fprintf(stderr, "ERROR: opening %s: %d: %s", logfile,
                                   errno, strerror(errno));

  pthread_mutex_unlock(&loglock);

/*
  if (newlogfile)
    Logf("*** logfile: %s_%04d%02d%02d.log\n", app,
         gmt->tm_year+1900, gmt->tm_mon+1, gmt->tm_mday);
  else
    Logf("will open new logfile in %d sec\n", nextday);

  pthread_attr_init(&logattr);
  pthread_attr_setdetachstate(&logattr, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&logtid, &logattr, create_new_logfile, &nextday) != 0)
    {
    fprintf(stderr, "ERROR: spawn create_new_logfile: %d: %s",
             errno, strerror(errno));
    return(errno);
    }

*/
  return(0);
  }

/** ---------------------------------------------------------------------------
 * @fn     create_new_logfile
 * @brief  thread to create new logfile each day
 * @param  seconds to sleep until next day
 * @return nothing
 *
 * This thread sleeps until the next day, at which point it calls
 * initlogentry to create a new logfile.
 *
 */
void *create_new_logfile(void *arg) {
  sleep((unsigned long)arg); 
  initlogentry(app);
  return (void *)0;
  }
