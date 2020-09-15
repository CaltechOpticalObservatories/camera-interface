/**
 * @file    server.cpp
 * @brief   this is the main server
 * @details spawns threads to handle requests, receives and parses commands
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "build_date.h"
#include "server.h"

Camera::Server server;

/** signal_handler ***********************************************************/
/**
 * @fn     signal_handler
 * @brief  handles ctrl-C
 * @param  int signo
 * @return nothing
 *
 */
void signal_handler(int signo) {
  std::string function = "Camera::signal_handler";
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
  std::string function = "Camera::main";
  std::stringstream message;

  signal(SIGINT, signal_handler);
  signal(SIGPIPE, signal_handler);

  // get the configuration file from the command line
  //
  long ret;
  if (argc>1) {
    server.config.filename = std::string( argv[1] );
    ret = server.config.read_config(server.config);      // read configuration file specified on command line
  }
  else {
    logwrite(function, "ERROR: no configuration file specified");
    server.exit_cleanly();
  }

  std::string logpath;
  for (int entry=0; entry < server.config.n_entries; entry++) {
    if (server.config.param[entry] == "LOGPATH") logpath = server.config.arg[entry];
  }
  if (logpath.empty()) {
    logwrite(function, "ERROR: LOGPATH not specified in configuration file");
    server.exit_cleanly();
  }

  if ( (initlog(logpath) != 0) ) {                       // initialize the logging system
    logwrite(function, "ERROR: unable to initialize logging system");
    server.exit_cleanly();
  }

  message << "this version built " << BUILD_DATE << " " << BUILD_TIME;
  logwrite(function, message.str());

  message.str(""); message << server.config.n_entries << " lines read from " << server.config.filename;
  logwrite(function, message.str());

  if (ret==NO_ERROR) ret=server.configure_server();      // get needed values out of read-in configuration file for the server
  if (ret==NO_ERROR) ret=server.configure_controller();  // get needed values out of read-in configuration file for the controller

  if (ret != NO_ERROR) {
    logwrite(function, "ERROR: unable to configure system");
    server.exit_cleanly();
  }

  if (server.nbport == -1 || server.blkport == -1) {
    logwrite(function, "ERROR: server ports not configured");
    server.exit_cleanly();
  }

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

  Network::TcpSocket s(server.blkport, true, -1, 0); // instantiate TcpSocket object with blocking port
  s.Listen();                                        // create a listening socket
  socklist.push_back(s);                             // add it to the socklist vector
  std::thread(block_main, socklist[0]).detach();     // spawn a thread to handle requests on this socket

  // pre-thread N_THREADS-1 detached threads to handle requests on the non-blocking port
  // thread #0 is reserved for the blocking port (above)
  //
  for (int i=1; i<N_THREADS; i++) {                  // create N_THREADS-1 non-blocking socket objects
    if (i==1) {                                      // first one only
      Network::TcpSocket s(server.nbport, false, CONN_TIMEOUT, i);   // instantiate TcpSocket object with non-blocking port
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
  std::string function = "Camera::doit";
  char  buf[BUFSIZE+1];
  long  ret;
  std::stringstream message;
  std::string cmd, args;        // arg string is everything after command
  std::vector<std::string> tokens;

  bool connection_open=true;

  while (connection_open) {
    memset(buf,  '\0', BUFSIZE);  // init buffers

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
    if ( (ret=sock.Read(buf, (size_t)BUFSIZE)) <= 0 ) {
      if (ret<0) {                // could be an actual read error
        message.str(""); message << "Read error: " << strerror(errno); logwrite(function, message.str());
      }
      break;                      // Breaking out of the while loop will close the connection.
                                  // This probably means that the client has terminated abruptly, 
                                  // having sent FIN but not stuck around long enough
                                  // to accept CLOSE and give the LAST_ACK.
    }

    // convert the input buffer into a string and remove any trailing linefeed
    // and carriage return
    //
    std::string sbuf = buf;
    sbuf.erase(std::remove(sbuf.begin(), sbuf.end(), '\r' ), sbuf.end());
    sbuf.erase(std::remove(sbuf.begin(), sbuf.end(), '\n' ), sbuf.end());

    try {
      std::size_t cmd_sep = sbuf.find_first_of(" "); // find the first space, which separates command from argument list

      cmd = sbuf.substr(0, cmd_sep);                 // cmd is everything up until that space

      if (cmd.empty()) continue;                     // If no command then skip over everything.

      if (cmd_sep == string::npos) {                 // If no space was found,
        args="";                                     // then the arg list is empty,
      }
      else {
        args= sbuf.substr(cmd_sep+1);                // otherwise args is everything after that space.
      }

      message.str(""); message << "thread " << sock.id << " received command: " << cmd << " " << args;
      logwrite(function, message.str());
    }
    catch ( std::runtime_error &e ) {
      std::stringstream errstream; errstream << e.what();
      message.str(""); message << "error parsing arguments: " << errstream.str();
      logwrite(function, message.str());
      ret = -1;
    }
    catch ( ... ) {
      message.str(""); message << "unknown error parsing arguments: " << args;
      logwrite(function, message.str());
      ret = -1;
    }

    /**
     * process commands here
     */
    ret = NOTHING;

    if (cmd.compare("exit")==0) {
                    server.exit_cleanly();
                    }
    else
    if (cmd.compare("open")==0) {
                    ret = server.connect_controller();
                    }
    else
    if (cmd.compare("close")==0) {
                    ret = server.disconnect_controller();
                    }
    else
    if (cmd.compare("load")==0) {
                    ret = server.load_firmware(args);
                    }
    else
    if (cmd.compare("mode")==0) {
                    if (args.empty()) {     // no argument means asking for current mode
                      if (server.modeselected) {
                        ret=NO_ERROR;
                        sock.Write(server.camera_info.current_observing_mode); sock.Write(" ");
                      }
                      else ret=ERROR;       // no mode selected returns an error
                    }
                    else ret = server.set_camera_mode(args);
                    }
    else
    if (cmd.compare("basename")==0) {
                    std::string retstring;  // string for the return value
                    ret = server.common.basename(args, retstring);
                    sock.Write(retstring);
                    sock.Write(" ");
                    }
    else
    if (cmd.compare("imnum")==0) {
                    std::string retstring;   // string for the return value
                    ret = server.common.imnum(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("imdir")==0) {
                    std::string retstring;   // string for the return value
                    ret = server.common.imdir(args, retstring);
                    sock.Write(retstring);
                    sock.Write(" ");
                    }
    else
    if (cmd.compare("fitsnaming")==0) {
                    std::string retstring;
                    ret = server.common.fitsnaming(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("key")==0) {
                    if (args.compare(0, 4, "list")==0)
                      ret = server.userkeys.listkeys();
                    else
                      ret = server.userkeys.addkey(args);
                    }
    else
    if (cmd.compare("getp")==0) {
                    std::string retstring;
                    ret = server.get_parameter(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("setp")==0) {
                    ret = server.set_parameter(args);
                    }
    else
    if (cmd.compare("printstatus")==0) {
                    ret = server.get_frame_status();
                    if (ret==NO_ERROR) ret = server.print_frame_status();
                    }
    else
    if (cmd.compare("readframe")==0) {
                    ret = server.read_frame();
                    }
    else
    if (cmd.compare("writeframe")==0) {
                    ret = server.write_frame();
                    }
    else
    if (cmd.compare("expose")==0) {
                    ret = server.expose(args);
                    }
    else
    if (cmd.compare("exptime")==0) {
                    std::string retstring;
                    ret = server.exptime(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("bias")==0) {
                    std::string retstring;
                    ret = server.bias(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("echo")==0) {
                    sock.Write(args);
                    sock.Write("\n");
                    }
    else
    if (cmd.compare("interface")==0) {
                    std::string retstring;   // string for the return value
                    ret = server.interface(retstring);
                    sock.Write(retstring);
                    sock.Write(" ");
                    }
    else {  // if no matching command found then assume it's a native command and send it straight to the controller
      try {
        std::transform( sbuf.begin(), sbuf.end(), sbuf.begin(), ::toupper );    // make uppercase
      }
      catch (...) {
        logwrite(function, "error converting command to uppercase");
        ret=ERROR;
      }
      ret = server.native(sbuf);
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

