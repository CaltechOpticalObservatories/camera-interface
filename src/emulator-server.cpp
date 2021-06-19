/**
 * @file    server.cpp
 * @brief   this is the main server
 * @details spawns threads to handle requests, receives and parses commands
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "emulator-server.h"
#include "emulator-archon.h"

Emulator::Server server;

/** signal_handler ***********************************************************/
/**
 * @fn     signal_handler
 * @brief  handles ctrl-C
 * @param  int signo
 * @return nothing
 *
 */
void signal_handler(int signo) {
  std::string function = "Emulator::signal_handler";
  switch (signo) {
    case SIGINT:
      std::cerr << "received INT\n";
      server.exit_cleanly();                  // shutdown the server
      break;
    case SIGPIPE:
      std::cerr << "caught SIGPIPE\n";
      break;
    default:
      server.exit_cleanly();                  // shutdown the server
      break;
  }
  return;
}
/** signal_handler ***********************************************************/


int  main(int argc, char **argv);           // main thread (just gets things started)
void block_main(Network::TcpSocket sock);   // this thread handles requests on blocking port
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
  std::string function = "Emulator::main";
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
    std::cerr << "ERROR: no configuration file specified\n";
    server.exit_cleanly();
  }

  std::cerr << server.config.n_entries << " lines read from " << server.config.filename << "\n";

  if (ret==NO_ERROR) ret=server.configure_server();      // get needed values out of read-in configuration file for the server

  if (ret != NO_ERROR) {
    std::cerr << "ERROR: unable to configure system\n";
    server.exit_cleanly();
  }

  if ( server.emulatorport == -1 ) {
    std::cerr << "ERROR: emulator server port not configured\n";
    server.exit_cleanly();
  }

  // create a thread for a single listening port
  // The TcpSocket object is instantiated with (PORT#, BLOCKING_STATE, POLL_TIMEOUT_MSEC, THREAD_ID#)
  //

  Network::TcpSocket s(server.emulatorport, true, -1, 0); // instantiate TcpSocket object
  s.Listen();                                             // create a listening socket
  std::thread(block_main, s).detach();                    // spawn a thread to handle requests on this socket

  for (;;) pause();                                       // main thread suspends
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
  std::string function = "(Emulator::doit) ";
  char  buf[BUFSIZE+1];
  long  ret;
  std::stringstream message;
  std::string cmd, ref, retstr;
  std::vector<std::string> tokens;

  bool connection_open=true;

  while (connection_open) {
    memset(buf,  '\0', BUFSIZE);  // init buffers

    // Wait (poll) connected socket for incoming data...
    //
    int pollret;
    if ( ( pollret=sock.Poll() ) <= 0 ) {
      if (pollret==0) {
        std::cerr << "Poll timeout on thread " << sock.id << "\n";
      }
      if (pollret <0) {
        std::cerr << "Poll error on thread " << sock.id << ": " << strerror(errno) << "\n";
      }
      break;                      // this will close the connection
    }

    // Data available, now read from connected socket...
    //
    if ( (ret=sock.Read(buf, (size_t)BUFSIZE)) <= 0 ) {
      if (ret<0) {                // could be an actual read error
        std::cerr << "Read error: " << strerror(errno) << "\n";
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
      // ignore empty commands or commands that don't begin with '>'
      //
      if ( ( sbuf.size() == 0 ) || ( (sbuf.size() > 0) && (sbuf.at(0) != '>') ) ) continue;

      // extract the command reference for the checksum
      //
      if ( sbuf.size() >= 3 ) {
        ref = sbuf.substr(1,2);
        cmd = sbuf.substr(3);
      }
    }
    catch ( std::runtime_error &e ) {
      std::stringstream errstream; errstream << e.what();
      std::cerr << "error parsing arguments: " << errstream.str() << "\n";
    }
    catch ( ... ) {
      std::cerr << "unknown error parsing buffer: " << sbuf << "\n";
    }

    /**
     * process commands here
     */

    if (cmd.compare("exit")==0) {
                    server.exit_cleanly();                  // shutdown the server
                    }
    else
    if (cmd.compare("SYSTEM")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare("STATUS")==0) {
                    ;
                    }
    else
    if (cmd.compare("TIMER")==0) {
                    ;
                    }
    else
    if (cmd.compare("FRAME")==0) {
                    ;
                    }
    else
    if (cmd.compare("FETCHLOG")==0) {
                    retstr = "<" + ref + "(null)";
                    }
    else
    if (cmd.compare(0,4,"LOCK")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare(0,5,"FETCH")==0) {
                    ;
                    }
    else
    if (cmd.compare(0,7,"WCONFIG")==0) {
                    ret = server.wconfig( cmd );
                    if ( ret == NO_ERROR ) retstr = "<" + ref;
/*
                    if ( ( cmd.length() < 14 ) ||                    // too short
                         ( cmd.find('=') == std::string::npos ) ) {  // or no equal sign
                      retstr = "?" + ref;                            // is an error
                    }
                    else {
                      try {
                        std::string linecount = cmd.substr(7,4);     // the line number
                        std::string line      = cmd.substr(11);      // the configuration line
                        server.configmap["43"].line=867;
                      }
                      catch( ... ) {
                        retstr = "?" + ref;
                      }
server.wconfig(cmd);
                    }
*/
                    }
    else
    if (cmd.compare(0,7,"RCONFIG")==0) {
                    std::string retstring;
                    ret = server.rconfig( cmd, retstring );
                    if ( ret == NO_ERROR ) retstr = "<" + ref + retstring;
                    std::cerr << function << "retstr=" << retstr << "\n";
                    }
    else
    if (cmd.compare("CLEARCONFIG")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare("APPLYALL")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare("POWERON")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare("POWEROFF")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare("LOADTIMING")==0) {
                    ;
                    }
    else
    if (cmd.compare("LOADPARAMS")==0) {
                    ;
                    }
    else
    if (cmd.compare("LOADPARAM")==0) {
                    ;
                    }
    else
    if (cmd.compare("PREPPARAM")==0) {
                    ;
                    }
    else
    if (cmd.compare("FASTLOADPARAM")==0) {
                    ;
                    }
    else
    if (cmd.compare("FASTPREPPARAM")==0) {
                    ;
                    }
    else
    if (cmd.compare("RESETTIMING")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare("HOLDTIMING")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare("RELEASETIMING")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare(0,8,"APPLYMOD")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare(0,8,"APPLYDIO")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare("APPLYCDS")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare("POLLOFF")==0) {
                    retstr = "<" + ref;
                    }
    else
    if (cmd.compare("POLLON")==0) {
                    retstr = "<" + ref;
                    }
    else {  // Archon simply ignores things it doesn't understand
    }

    if ( ! retstr.empty() ) {
      retstr += "\n";
      if (sock.Write(retstr)<0) connection_open=false;
    }

  }

  sock.Close();
  return;
}
/** doit *********************************************************************/

