/** ---------------------------------------------------------------------------
 * @fn       p3k/aoca/include/logentry.h
 * @brief    include file for Logf function
 * @author   David Hale <dhale@astro.caltech.edu>
 * @date     2010-08-11
 * @modified 2010-12-08, DH
 * @modified 2015-05-29, DH moved globals to logentry.c and changed LOGPATH
 * @modified 2015-07-22, DH moved prototype for create_new_logfile to here
 *
 */
#ifndef LOGENTRY_H
#define LOGENTRY_H
#ifdef __cplusplus
extern "C" {
#endif 
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

/*---------------------------------------------------------------------------
| Definitions
|----------------------------------------------------------------------------*/
#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif
#define LOGPATH "/tmp"
#define SZ_APP 32
#define SZ_BUF 2048

/*---------------------------------------------------------------------------
| prototypes
|----------------------------------------------------------------------------*/
void Logf(const char *fmt, ...);
void Logf_(const char *fmt, ...);
int  initlogentry(const char *app);
int  close_logentry(void);
void *create_new_logfile(void *arg);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* LOGENTRY_H */

