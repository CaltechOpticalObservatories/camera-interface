/** ---------------------------------------------------------------------------
 * @file     include/build_date.h
 * @brief    build date for arcserv
 * @author   David Hale <dhale@astro.caltech.edu>
 * @date     2019-06-10
 * @modified 2019-06-10, DH
 *
 * When arcserv is built the Makefile will touch this file.
 * arcserv's main() will Logf the modification date/time of this file,
 * thereby logging the build date of arcserv.
 *
 */
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__
