/**
 * @file    server.cpp
 * @brief   this is the main server
 * @details spawns threads to handle requests, receives and parses commands
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "build_date.h"
#include "server.h"

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


int  main(int argc, char **argv);           // main thread (just gets things started)
void block_main(Network::TcpSocket sock);   // this thread handles requests on blocking port
void thread_main(Network::TcpSocket sock);  // this thread handles requests on non-blocking port
void doit(Network::TcpSocket sock);         // the worker thread


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

  initlog();                                         // required to initialize the logging system before use

  message << "this version built " << BUILD_DATE << " " << BUILD_TIME;
  logwrite(function, message.str());

  signal(SIGINT, signal_handler);
  signal(SIGPIPE, signal_handler);

  server.config.read_config(server.config);          // read configuration file

  server.get_config();                               // get needed values out of read-in configuration file

  // This will pre-thread N_THREADS threads.
  // The 0th thread is reserved for the blocking port, and the rest are for the non-blocking port.
  // Each thread gets a socket object. All of the socket objects are stored in a vector container.
  // The blocking thread socket object is of course unique.
  // For the non-blocking thread socket objects, create a listening socket with one object,
  // then the remaining objects are copies of the first.
  //
  // TcpSocket objects are instantiated with (PORT#, BLOCKING_STATE, POLL_TIMEOUT_MSEC, THREAD_ID#)
  //
  std::vector<Network::TcpSocket> socklist;          // create a vector container to hold N_THREADS TcpSocket objects
  socklist.reserve(N_THREADS);

  Network::TcpSocket s(BLKPORT, true, -1, 0);        // instantiate TcpSocket object with blocking port
  s.Listen();                                        // create a listening socket
  socklist.push_back(s);                             // add it to the socklist vector
  std::thread(block_main, socklist[0]).detach();     // spawn a thread to handle requests on this socket

  // pre-thread N_THREADS-1 detached threads to handle requests on the non-blocking port
  // thread #0 is reserved for the blocking port (above)
  //
  for (int i=1; i<N_THREADS; i++) {                  // create N_THREADS-1 non-blocking socket objects
    if (i==1) {                                      // first one only
      Network::TcpSocket s(NBPORT, false, CONN_TIMEOUT, i);   // instantiate TcpSocket object with non-blocking port
      s.Listen();                                    // create a listening socket
      socklist.push_back(s);
    }
    else {                                           // subsequent socket objects are copies of the first
      Network::TcpSocket s = socklist[1];            // copy the first one, which has a valid listening socket
      s.id = i;
      socklist.push_back(s);
    }
    std::thread(thread_main, socklist[i]).detach();  // spawn a thread to handle each non-blocking socket request
  }

  for (;;) pause();                                  // main thread suspends
  return 0;
}
/** main *********************************************************************/


/** block_main ***************************************************************/
/**
 * @fn     block_main
 * @brief  main function for blocking connection thread
 * @param  Network::TcpSocket sock, socket object
 * @return nothing
 *
 * accepts a socket connection and processes the request by
 * calling function doit()
 *
 * This thread never terminates.
 *
 */
void block_main(Network::TcpSocket sock) {
  while(1) {
    sock.Accept();
    doit(sock);                   // call function to do the work
    sock.Close();
  }
  return;
}
/** block_main ***************************************************************/


/** thread_main **************************************************************/
/**
 * @fn     thread_main
 * @brief  main function for all non-blocked threads
 * @param  Network::TcpSocket sock, socket object
 * @return nothing
 *
 * accepts a socket connection and processes the request by
 * calling function doit()
 *
 * There are N_THREADS-1 of these, one for each non-blocking connection.
 * These threads never terminate.
 *
 * This function differs from block_main only in that the call to Accept
 * is mutex-protected.
 *
 */
void thread_main(Network::TcpSocket sock) {
  while (1) {
    server.conn_mutex.lock();
    sock.Accept();
    server.conn_mutex.unlock();
    doit(sock);                // call function to do the work
    sock.Close();
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
void doit(Network::TcpSocket sock) {
  std::string function = "Archon::doit";
  char  buf[BUFSIZE+1], cmd[BUFSIZE+1];
  char *args=buf;
  char *c_ptr;
  int   i;
  long  ret;
  std::stringstream message;
  std::string scmd, sargs;        // arg string is everything after command
  std::vector<std::string> tokens;

  bool connection_open=true;

  while (connection_open) {
    memset(buf,  '\0', BUFSIZE);  // init buffers
    args=buf;

    // Wait (poll) connected socket for incoming data...
    //
    int pollret;
    if ( ( pollret=sock.Poll() ) <= 0 ) {
      if (pollret==0) {
        message.str(""); message << "Poll timeout on thread " << sock.id;
        logwrite(function, message.str());
      }
      if (pollret <0) {
        message.str(""); message << "Poll error on thread " << sock.id << ": " << strerror(errno);
        logwrite(function, message.str());
      }
      break;                      // this will close the connection
    }

    // Data available, now read from connected socket...
    //
    if ( sock.Read(buf, (size_t)BUFSIZE) <= 0 ) {
      message.str(""); message << "Read error: " << strerror(errno); logwrite(function, message.str());
      break;                      // close connection -- this probably means that
                                  // the client has terminated abnormally, having
                                  // sent FIN but not stuck around long enough
                                  // to accept CLOSE and give the LAST_ACK.
    }

    i=0; while (buf[i] != '\0') i++;

    try {
      STRIPCOMMAND(cmd, args);
      if (args == NULL) sargs = ""; else sargs = args;
      if (cmd  == NULL) scmd  = ""; else scmd  = cmd;

      if (scmd.empty()) continue;        // if no command then skip over everything

      sargs.erase(std::remove(sargs.begin(), sargs.end(), '\r' ), sargs.end());
      sargs.erase(std::remove(sargs.begin(), sargs.end(), '\n' ), sargs.end());

      message.str(""); message << "thread " << sock.id << " received command: " << cmd << " " << sargs;
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
                    sock.Write(imname);
                    sock.Write(" ");
                    }
    else
    if (MATCH(cmd, "imnum")) {
                    std::string imnum;   // string for the return value
                    ret = server.common.imnum(sargs, imnum);
                    if (!imnum.empty()) { sock.Write(imnum); sock.Write(" "); }
                    }
    else
    if (MATCH(cmd, "imdir")) {
                    std::string imdir;   // string for the return value
                    ret = server.common.imdir(sargs, imdir);
                    sock.Write(imdir);
                    sock.Write(" ");
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
                    std::string value;
                    ret = server.read_parameter(sargs, value);
                    if (!value.empty()) { sock.Write(value); sock.Write(" "); }
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
    else
    if (MATCH(cmd, "echo")) {
                    sock.Write(sargs);
                    sock.Write("\n");
                    }
    else {  // if no matching command found then assume it's a native command and send it straight to the controller
      ret = server.archon_native(buf);
    }

    if (ret != NOTHING) {
      std::string retstr=(ret==0?"DONE\n":"ERROR\n");
      if (sock.Write(retstr)<0) connection_open=false;
    }

    if (!sock.isblocking()) break;       // Non-blocking connection exits immediately.
                                         // Keep blocking connection open for interactive session.
  }

  sock.Close();
  return;
}
/** doit *********************************************************************/

