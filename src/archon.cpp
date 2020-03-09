/**
 * @file    archon.cpp
 * @brief   
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include "build_date.h"
#include "tcplinux.h"
#include "logentry.h"
#include <thread>

#define  N_THREADS    10
#define  NBPORT       2157
#define  BLKPORT      2158
#define  CONN_TIMEOUT 3000  //<! incoming (non-blocking) connection timeout in milliseconds

namespace Archon {

  Server::~Server() {
    close(nonblocking_socket);
    close(blocking_socket);
    Logf("(Archon::Server) closing sockets\n");
  }

  void Server::exit_cleanly(void) {
    Logf("(Archon::Server::exit_cleanly) server exiting\n");
    exit(0);
  }
}

Archon::Server arcserv;

/** signal_handler ***********************************************************/
/**
 * @fn     signal_handler
 * @brief  handles ctrl-C
 * @param  int signo
 * @return nothing
 *
 */
void signal_handler(int signo) {
  switch (signo) {
    case SIGINT:
      Logf("(Archon::signal_handler) received INT\n");
      arcserv.exit_cleanly();
      break;
    case SIGPIPE:
      Logf("(Archon::signal_handler) caught SIGPIPE\n");
      break;
    default:
      arcserv.exit_cleanly();
      break;
  }
  return;
}
/** signal_handler ***********************************************************/


void block_main(int threadnum);
int  main(int argc, char **argv);
void thread_main(int threadnum);
void doit(int threadnum);


/** main *********************************************************************/
/**
 * @fn     main
 * @brief  the main function
 * @param  int argc, char** argv
 * @return 0
 *
 */
int main(int argc, char **argv) {

  signal(SIGINT, signal_handler);
  signal(SIGPIPE, signal_handler);

  initlogentry("arcserv");

  Logf("(Archon::main) this version built %s %s\n", BUILD_DATE, BUILD_TIME);

  arcserv.nonblocking_socket = tcp_listen(NBPORT);   // initialize non-blocking TCP socket
  arcserv.blocking_socket = tcp_listen(BLKPORT);     // initialize blocking TCP socket
  
  // spawn thread for blocking port, detached from the main thread.
  // this will be thread_num = 0 and the non-blocking threads will
  // count from 1 to N_THREADS.
  //
  std::thread(block_main, 0).detach();

  // pre-thread up to N-1 detached threads. The Nth is saved for the blocking port.
  //
  for (int i=1; i<N_THREADS; i++) {
    std::thread(thread_main, i).detach();
  }

  for (;;) pause();                                  // main thread suspends

  return 0;
}
/** main *********************************************************************/


/** block_main ***************************************************************/
/**
 * @fn     block_main
 * @brief  main function for blocking connection thread
 * @param  void* arg, thread id
 * @return nothing
 *
 * accepts a socket connection and processes the request by
 * calling function doit() with port_type=BLOCK
 *
 * This thread never terminates.
 *
 */
void block_main(int threadnum) {
  int connfd;

  struct sockaddr_in cliaddr;
  socklen_t          len;
  char               buff[1024];

  if (threadnum != 0) {
    Logf("(Archon::block_main) WARNING: block_main shouldn't be called with thr %d\n", threadnum);
  }

  while(1) {
    len=sizeof(cliaddr);
    connfd=accept(arcserv.blocking_socket,(struct sockaddr *) &cliaddr, &len);

    sprintf(arcserv.conndata[threadnum].cliaddr_str, "%s",
            inet_ntop(AF_INET, &cliaddr.sin_addr, buff, sizeof(buff)));

    arcserv.conndata[threadnum].cliport    = ntohs(cliaddr.sin_port);
    arcserv.conndata[threadnum].connfd     = connfd;
    arcserv.conndata[threadnum].port_type  = arcserv.BLOCK;

    doit(threadnum);                         // call function to do the work

    close(arcserv.conndata[threadnum].connfd);
  }
  return ;
}
/** block_main ***************************************************************/


/** thread_main **************************************************************/
/**
 * @fn     thread_main
 * @brief  main function for all non-blocked threads
 * @param  void* arg, thread id
 * @return nothin
 *
 * accepts a socket connection and processes the request by
 * calling function doit()
 *
 * There are N_THREADS-1 of these, one for each nb connection.
 * These threads never terminate.
 *
 */
void thread_main(int threadnum) {
  int connfd;                                  /* connection file descriptor */

  struct sockaddr_in cliaddr;
  socklen_t          len=(socklen_t)sizeof(cliaddr);
  char               buff[1024];

  while(1) {
    arcserv.conn_mutex.lock();

    connfd = accept(arcserv.nonblocking_socket, (struct sockaddr *) &cliaddr, &len);

    arcserv.conndata[threadnum].cliport    = ntohs(cliaddr.sin_port);
    arcserv.conndata[threadnum].connfd     = connfd;
    arcserv.conndata[threadnum].port_type  = arcserv.NONBLOCK;
    sprintf(arcserv.conndata[threadnum].cliaddr_str, "%s", 
            inet_ntop(AF_INET, &cliaddr.sin_addr, buff, sizeof(buff)));

    arcserv.conn_mutex.unlock();

    doit(threadnum);                         // call function to do the work

    close(arcserv.conndata[threadnum].connfd);
  }
  return;
}
/** thread_main **************************************************************/


/** doit *********************************************************************/
/**
 * @fn     doit
 * @brief  the workhorse of each thread connetion
 * @param  int thr
 * @return nothin
 *
 * stays open until closed by client
 *
 * commands come in the form: 
 * <device> [all|<app>] [_BLOCK_] <command> [<arg>]
 *
 */
void doit(int threadnum) {
  const char* function = "Archon::doit";
  char  buf[BUFSIZE+1], cmd[BUFSIZE+1];
  char *args=buf;
  char *c_ptr;
  int   i;
  unsigned long  ret=0;
  char  retstr[BUFSIZE];
  std::string paramname;          // paramname after stripping the command used for set_param, get_param

  bool connection_open=true;

  while (connection_open) {
    memset(buf,  '\0', BUFSIZE);  // init buffers
    args=buf;

    if ( Poll(arcserv.conndata[threadnum].connfd, 
             (arcserv.conndata[threadnum].port_type==arcserv.NONBLOCK) ? CONN_TIMEOUT : -1) <= 0 ) 
      break;
    if ( read(arcserv.conndata[threadnum].connfd, buf, (size_t)BUFSIZE) <= 0 )
      break;                      // close connection -- this probably means that
                                  // the client has terminated abnormally, having
                                  // sent FIN but not stuck around long enough
                                  // to accept CLOSE and give the LAST_ACK.

    i=0; while (buf[i] != '\0') i++;

    std::string argstr = args;
    try {
      STRIPCOMMAND(cmd, args);
      if (args == NULL) paramname = ""; else paramname = args;

      argstr.erase(std::remove(argstr.begin(), argstr.end(), '\r' ), argstr.end());
      argstr.erase(std::remove(argstr.begin(), argstr.end(), '\n' ), argstr.end());
      Logf("(%s) thread %d received command: %s\n", function, threadnum, argstr.c_str());
    }
    catch ( std::runtime_error &e ) {
      std::stringstream errstr; errstr << e.what();
      Logf("(%s) error parsing arguments: %s\n", function, errstr.str().c_str());
      ret = -1;
    }
    catch ( ... ) {
      Logf("() unknown error parsing arguments: %s\n", function, argstr.c_str());
      ret = -1;
    }

    /**
     * process commands here
     */
    if (MATCH(buf, "exit")) {
                    arcserv.exit_cleanly();
                    }
    else
    if (MATCH(buf, "open")) {
                    if (!arcserv.is_driver_open()) {       // API should, but can't handle two opens
                      if ( arcserv.open_driver(argstr) == ARC_STATUS_ERROR ) {
                        arcserv.log_last_error();
                      }
                    }
                    if ( arcserv.is_driver_open() ) ret=0; else ret=1;  // return 0 if open, 1 otherwise
                    }
    else
    if (MATCH(buf, "isopen")) {
                    ret = arcserv.is_driver_open();
                    }
    else
    if (MATCH(buf, "close")) {
                    ret = arcserv.close_driver();
                    }
    else
    if (MATCH(buf, "load")) {
                    ret = arcserv.load_file(argstr);
                    }
    else
    if (MATCH(buf, "setup")) {
                    ret = arcserv.setup_controller(argstr);
                    }
    else
    if (MATCH(buf, "expose")) {
                    ret = arcserv.start_exposure();
                    }
    else
    if (MATCH(buf, "clear_fitskeys")) {
                    ret = arcserv.fitskey.clear_fitskeys();
                    }
    else
    if (MATCH(buf, "get")) {
                    ret = arcserv.get_param(paramname);
                    }
    else
    if (MATCH(buf, "set")) {
                    ret = arcserv.set_param(paramname);
                    }
    else {
      // convert buf to std::string and remove any newline and carriage returns
      //
      std::string bufstr = buf;
      try {
        bufstr.erase(std::remove(bufstr.begin(), bufstr.end(), '\r' ), bufstr.end());
        bufstr.erase(std::remove(bufstr.begin(), bufstr.end(), '\n' ), bufstr.end());
        if (bufstr.length() < 3) {
          Logf("() ERROR: command length too short: %d (expected at least 3)\n", function, bufstr.length());
          ret = -1;
          continue;
        }
        for (i=0; i<3; i++) bufstr.at(i)=toupper(bufstr.at(i));
        ret = arcserv.command(bufstr);
        }
      catch ( std::runtime_error &e ) {
        std::stringstream errstr; errstr << e.what();
        Logf("(%s) error processing command: %s\n", function, errstr.str().c_str());
        ret = -1;
      }
      catch ( ... ) {
        Logf("(%s) unknown error processing command: %s\n", function, bufstr.c_str());
        ret = -1;
      }
    }

    if (ret == 0x444F4E ) ret = 0;  // 'DON'

    snprintf(retstr, sizeof(retstr), "%ld\n", ret);

    if (sock_rbputs(arcserv.conndata[threadnum].connfd, retstr)<0) connection_open=false;

    // non-blocking connection exits immediately.
    // keep blocking connection open for interactive session.
    // 
    if (arcserv.conndata[threadnum].port_type == arcserv.NONBLOCK) break;

    /**
     * write back a character indicating success or error
    if (err) { if (sock_rbputs(arcserv.conndata[threadnum].connfd,"? \0")<0) conn=0; }
    else     { if (sock_rbputs(arcserv.conndata[threadnum].connfd,"# \0")<0) conn=0; }
     */
  }

  close(arcserv.conndata[threadnum].connfd);
  return;
}
/** doit *********************************************************************/

