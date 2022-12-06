/**
 * @file    daemonize.h
 * @brief   include file to daemonize a program
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#ifndef DAEMONIZE_H
#define DAEMONIZE_H

#include <iostream>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>    // printf(3)
#include <stdlib.h>   // exit(3)
#include <unistd.h>   // fork(3), chdir(3), sysconf(3)
#include <signal.h>   // signal(3)
#include <sys/stat.h> // umask(3)
#include <syslog.h>   // syslog(3), openlog(3), closelog(3)


namespace Daemon {

  int daemonize( std::string name, std::string path, std::string outfile, std::string errfile, std::string infile ) {
    if ( path.empty() )    { path = "/tmp"; }
    if ( name.empty() )    { name = "mydaemon"; }
    if ( infile.empty() )  { infile = "/dev/null"; }
    if ( outfile.empty() ) { outfile = "/dev/null"; }
    if ( errfile.empty() ) { errfile = "/dev/null"; }

    // fork, detach from process group leader
    //
    pid_t child = fork();

    if ( child < 0 ) {    // failed fork
      fprintf( stderr, "ERROR: failed fork\n" );
      exit( EXIT_FAILURE );
    }
    if ( child > 0 ) {    // parent
      exit( EXIT_SUCCESS );
    }
    if( setsid() < 0 ) {  // failed to become session leader
      fprintf( stderr, "error: failed setsid\n" );
      exit( EXIT_FAILURE );
    }

    // catch/ignore signals
    //
    signal( SIGCHLD, SIG_IGN );

    // fork second time
    //
    if ( ( child = fork() ) < 0 ) {    //failed fork
      fprintf( stderr, "error: failed fork\n" );
      exit( EXIT_FAILURE );
    }
    if( child > 0 ) { //parent
      exit( EXIT_SUCCESS );
    }

    umask( 0 );              // new file permissions

    if ( chdir( path.c_str() ) < 0 ) {   // change to path directory
      fprintf( stderr, "error: failed chdir: %s\n", strerror(errno) );
      exit( EXIT_FAILURE );
    }

    // Close all open file descriptors
    //
    for( int fd = sysconf( _SC_OPEN_MAX ); fd >= 0; fd-- ) close( fd );

    // reopen stdin, stdout, stderr
    //
    stdin  = fopen( infile.c_str(), "r" );    // fd=0
    stdout = fopen( outfile.c_str(), "w+" );  // fd=1
    stderr = fopen( errfile.c_str(), "w+" );  // fd=2

    // open syslog
    //
    openlog( name.c_str(), LOG_PID, LOG_DAEMON );
    return( 0 );
  }

}
#endif
