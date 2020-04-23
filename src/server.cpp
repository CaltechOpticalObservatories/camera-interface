/**
 * @file    server.cpp
 * @brief   
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <thread>

#include "build_date.h"
#include "tcplinux.h"
#include "server.h"
#include "common.h"
#include "config.h"
#include "utilities.h"

#define  N_THREADS    10
#define  BUFSIZE      1024  //!<
#define  NBPORT       3030
#define  BLKPORT      3031
#define  CONN_TIMEOUT 3000  //<! incoming (non-blocking) connection timeout in milliseconds

Archon::Server server;

/** signal_handler ***********************************************************/
/**
 * @fn     signal_handler
 * @brief  handles ctrl-C
 * @param  int signo
 * @return nothing
 *
 */
void signal_handler(int signo) {
  std::string function = "Archon::signal_handler";
  switch (signo) {
    case SIGINT:
      logwrite(function, "received INT");
      server.exit_cleanly();
      break;
    case SIGPIPE:
      logwrite(function, "caught SIGPIPE");
      break;
    default:
      server.exit_cleanly();
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
  std::string function = "Archon::main";
  std::stringstream message;

  initlog();

  message << "this version built " << BUILD_DATE << " " << BUILD_TIME;
  logwrite(function, message.str());

  signal(SIGINT, signal_handler);
  signal(SIGPIPE, signal_handler);

  server.config.read_config(server.config);          // read configuration file

  server.get_config();

  server.nonblocking_socket = tcp_listen(NBPORT);    // initialize non-blocking TCP socket
  server.blocking_socket = tcp_listen(BLKPORT);      // initialize blocking TCP socket
  
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
  std::string function = "Archon::block_main";
  std::stringstream message;
  int connfd;

  struct sockaddr_in cliaddr;
  socklen_t          len;
  char               buff[1024];

  if (threadnum != 0) {
    message.str(""); message << "WARNING: block_main shouldn't be called with thread " << threadnum;
    logwrite(function, message.str());
  }

  while(1) {
    len=sizeof(cliaddr);
    connfd=accept(server.blocking_socket,(struct sockaddr *) &cliaddr, &len);

    sprintf(server.conndata[threadnum].cliaddr_str, "%s",
            inet_ntop(AF_INET, &cliaddr.sin_addr, buff, sizeof(buff)));

    server.conndata[threadnum].cliport    = ntohs(cliaddr.sin_port);
    server.conndata[threadnum].connfd     = connfd;
    server.conndata[threadnum].port_type  = server.BLOCK;

    doit(threadnum);                         // call function to do the work

    close(server.conndata[threadnum].connfd);
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
    server.conn_mutex.lock();

    connfd = accept(server.nonblocking_socket, (struct sockaddr *) &cliaddr, &len);

    server.conndata[threadnum].cliport    = ntohs(cliaddr.sin_port);
    server.conndata[threadnum].connfd     = connfd;
    server.conndata[threadnum].port_type  = server.NONBLOCK;
    sprintf(server.conndata[threadnum].cliaddr_str, "%s", 
            inet_ntop(AF_INET, &cliaddr.sin_addr, buff, sizeof(buff)));

    server.conn_mutex.unlock();

    doit(threadnum);                         // call function to do the work

    close(server.conndata[threadnum].connfd);
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
  std::string function = "Archon::doit";
  char  buf[BUFSIZE+1], cmd[BUFSIZE+1];
  char *args=buf;
  char *c_ptr;
  int   i;
  long  ret;
  char  retstr[BUFSIZE];
  std::stringstream message;
  std::string scmd, sargs;        // arg string is everything after command
  std::vector<std::string> tokens;

  bool connection_open=true;

  while (connection_open) {
    memset(buf,  '\0', BUFSIZE);  // init buffers
    args=buf;

    if ( Poll(server.conndata[threadnum].connfd, 
             (server.conndata[threadnum].port_type==server.NONBLOCK) ? CONN_TIMEOUT : -1) <= 0 ) 
      break;
    if ( read(server.conndata[threadnum].connfd, buf, (size_t)BUFSIZE) <= 0 )
      break;                      // close connection -- this probably means that
                                  // the client has terminated abnormally, having
                                  // sent FIN but not stuck around long enough
                                  // to accept CLOSE and give the LAST_ACK.

    i=0; while (buf[i] != '\0') i++;

    try {
      STRIPCOMMAND(cmd, args);
      if (args == NULL) sargs = ""; else sargs = args;
      if (cmd  == NULL) scmd  = ""; else scmd  = cmd;

      if (scmd.empty()) continue;        // if no command then skip over everything

      sargs.erase(std::remove(sargs.begin(), sargs.end(), '\r' ), sargs.end());
      sargs.erase(std::remove(sargs.begin(), sargs.end(), '\n' ), sargs.end());

      message.str(""); message << "thread " << threadnum << " received command: " << cmd << " " << sargs;
      logwrite(function, message.str());
    }
    catch ( std::runtime_error &e ) {
      std::stringstream errstr; errstr << e.what();
      message.str(""); message << "error parsing arguments: " << errstr;
      logwrite(function, message.str());
      ret = -1;
    }
    catch ( ... ) {
      message.str(""); message << "unknown error parsing arguments: " << sargs;
      logwrite(function, message.str());
      ret = -1;
    }

    /**
     * process commands here
     */
    ret = NOTHING;

    if (MATCH(cmd, "exit")) {
                    server.exit_cleanly();
                    }
    else
    if (MATCH(cmd, "open")) {
                    ret = server.connect_controller();
                    }
    else
    if (MATCH(cmd, "close")) {
                    ret = server.disconnect_controller();
                    }
    else
    if (MATCH(cmd, "load")) {
                    ret = server.load_config(sargs);
                    if (ret==ERROR) server.fetchlog();
                    }
    else
    if (MATCH(cmd, "imname")) {
                    std::string imname;  // string for the return value
                    ret = server.common.imname(sargs, imname);
                    sock_rbputs(server.conndata[threadnum].connfd, (char*)imname.c_str());
                    sock_rbputs(server.conndata[threadnum].connfd, (char*)" ");
                    }
    else
    if (MATCH(cmd, "imnum")) {
                    std::string imnum;   // string for the return value
                    ret = server.common.imnum(sargs, imnum);
                    sock_rbputs(server.conndata[threadnum].connfd, (char*)imnum.c_str());
                    sock_rbputs(server.conndata[threadnum].connfd, (char*)" ");
                    }
    else
    if (MATCH(cmd, "imdir")) {
                    std::string imdir;   // string for the return value
                    ret = server.common.imdir(sargs, imdir);
                    sock_rbputs(server.conndata[threadnum].connfd, (char*)imdir.c_str());
                    sock_rbputs(server.conndata[threadnum].connfd, (char*)" ");
                    }
    else
    if (MATCH(cmd, "key")) {
                    if (sargs.compare(0, 4, "list")==0)
                      ret = server.userkeys.listkeys();
                    else
                      ret = server.userkeys.addkey(sargs);
                    }
    else
    if (MATCH(cmd, "getp")) {
                    std::string valstring;
                    ret = server.read_parameter(sargs, valstring);
                    logwrite(function, valstring);
                    }
    else
    if (MATCH(cmd, "setp")) {
                    Tokenize(sargs, tokens, " ");
                    if (tokens.size() != 2) {
                      ret = ERROR;
                      message.str(""); message << "error: expected 2 arguments, got " << tokens.size();
                      logwrite(function, message.str());
                    }
                    else {
                      ret = server.prep_parameter(tokens[0], tokens[1]);
                      if (ret == NO_ERROR) ret = server.load_parameter(tokens[0], tokens[1]);
                    }
                    }
    else
    if (MATCH(cmd, "printstatus")) {
                    ret = server.get_frame_status();
                    if (ret==NO_ERROR) ret = server.print_frame_status();
                    }
    else
    if (MATCH(cmd, "readframe")) {
                    ret = server.read_frame();
                    }
    else
    if (MATCH(cmd, "writeframe")) {
                    ret = server.write_frame();
                    }
    else
    if (MATCH(cmd, "expose")) {
                    ret = server.expose();
                    }
    else {  // if no matching command found then assume it's a native command and send it straight to the controller
      ret = server.archon_native(buf);
    }

    if (ret != NOTHING) {
      snprintf(retstr, sizeof(retstr), "%s\n", ret==0?"DONE":"ERROR");
      if (sock_rbputs(server.conndata[threadnum].connfd, retstr)<0) connection_open=false;
    }

    // non-blocking connection exits immediately.
    // keep blocking connection open for interactive session.
    // 
    if (server.conndata[threadnum].port_type == server.NONBLOCK) break;
  }

  close(server.conndata[threadnum].connfd);
  return;
}
/** doit *********************************************************************/

