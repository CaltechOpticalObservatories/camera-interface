/**
 * @file    camerad.cpp
 * @brief   this is the main camerad server
 * @details spawns threads to handle requests, receives and parses commands
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "build_date.h"
#include "camerad.h"
#include "daemonize.h"

Camera::Server server;
std::string logpath; 

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
    case SIGTERM:
    case SIGINT:
      logwrite(function, "received termination signal");
      server.camera.async.enqueue("exit");  // shutdown the async_main thread if running
      server.exit_cleanly();                // shutdown the server
      break;
    case SIGHUP:
      logwrite(function, "caught SIGHUP");
      server.configure_controller();
      break;
    case SIGPIPE:
      logwrite(function, "caught SIGPIPE");
      break;
    default:
      logwrite(function, "received unknown signal");
      server.camera.async.enqueue("exit");  // shutdown the async_main thread if running
      server.exit_cleanly();                // shutdown the server
      break;
  }
  return;
}
/** signal_handler ***********************************************************/


int  main(int argc, char **argv);           // main thread (just gets things started)
void new_log_day( bool logstderr );         // create a new log each day
void block_main(Network::TcpSocket sock);   // this thread handles requests on blocking port
void thread_main(Network::TcpSocket sock);  // this thread handles requests on non-blocking port
void async_main(Network::UdpSocket sock);   // this thread handles the asyncrhonous UDP message port
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
  long ret=NO_ERROR;
  std::string daemon_in;     // daemon setting read from config file
  bool start_daemon = false; // don't start as daemon unless specifically requested
  bool logstderr = true;     // log also to stderr

  // capture these signals
  //
  signal(SIGINT, signal_handler);
  signal(SIGPIPE, signal_handler);
  signal(SIGHUP, signal_handler);

  // check for "-f <filename>" command line option to specify config file
  //
  if ( cmdOptionExists( argv, argv+argc, "-f" ) ) {
    char* filename = getCmdOption( argv, argv+argc, "-f" );
    if ( filename ) {
      server.config.filename = std::string( filename );
    }
  }
  else

  // if no "-f <filename>" then as long as there's at least one arg,
  // assume that is the config file name.
  //
  if (argc>1) {
    server.config.filename = std::string( argv[1] );
  }
  else {
    logwrite(function, "ERROR: no configuration file specified");
    server.exit_cleanly();
  }

  if ( server.config.read_config(server.config) != NO_ERROR) {  // read configuration file specified on command line
    logwrite(function, "ERROR: unable to configure system");
    server.exit_cleanly();
  }

  // camerad would like a few configuration keys before the daemon starts up
  //
  for (int entry=0; entry < server.config.n_entries; entry++) {
    if (server.config.param[entry] == "LOGPATH") logpath = server.config.arg[entry];    // where to write log files
    if (server.config.param[entry] == "TM_ZONE") zone = server.config.arg[entry];       // time zone for time stamps
    if (server.config.param[entry] == "DAEMON")  daemon_in = server.config.arg[entry];  // am I starting as a daemon or not?
    if (server.config.param[entry] == "LOGSTDERR") {                                    // should I log also to stderr?
      std::string stderrstr = server.config.arg[entry];
      std::transform( stderrstr.begin(), stderrstr.end(), stderrstr.begin(), ::tolower );
      if ( stderrstr.empty() )    { continue; }          else
      if ( stderrstr == "true"  ) { logstderr = true;  } else
      if ( stderrstr == "false" ) { logstderr = false; } else {
        message.str(""); message << "unrecognized value for LOGSTDERR=" << stderrstr << ": expected {True:False}";
        logwrite( function, message.str() );
      }
    }
  }

  if (logpath.empty()) {
    std::cerr << get_timestamp() << "(" << function << ") ERROR: LOGPATH not specified in configuration file\n";
    server.exit_cleanly();
  }

  if ( ( init_log( logpath, Camera::DAEMON_NAME, logstderr ) != 0 ) ) {  // initialize the logging system
    std::cerr << get_timestamp() << "(" << function << ") ERROR: unable to initialize logging system\n";
    server.exit_cleanly();
  }

  if ( zone == "local" ) {
    logwrite( function, "using local time zone" );
    server.systemkeys.addkey( "TM_ZONE=local//time zone" );
  }
  else {
    logwrite( function, "using GMT time zone" );
    server.systemkeys.addkey( "TM_ZONE=GMT//time zone" );
  }

  if ( !daemon_in.empty() && daemon_in == "yes" ) start_daemon = true;
  else
  if ( !daemon_in.empty() && daemon_in == "no"  ) start_daemon = false;
  else
  if ( !daemon_in.empty() ) {
    message.str(""); message << "ERROR: unrecognized argument DAEMON=" << daemon_in << ", expected { yes | no }";
    logwrite( function, message.str() );
    server.exit_cleanly();
  }

  // check for "-d" command line option last so that the command line
  // can override the config file to start as daemon
  //
  if ( cmdOptionExists( argv, argv+argc, "-d" ) ) {
    start_daemon = true;
  }

  if ( start_daemon ) {
    logwrite( function, "starting daemon" );
    Daemon::daemonize( Camera::DAEMON_NAME, "/tmp", "", "", "" );
  }

  // log and add server build date to system keys db
  //
  message << "this version built " << BUILD_DATE << " " << BUILD_TIME;
  logwrite(function, message.str());

  message.str(""); message << "CAMD_VER=" << BUILD_DATE << " " << BUILD_TIME << " // camerad build date";
  server.systemkeys.addkey( message.str() );

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
      Network::TcpSocket s(server.nbport, false, CONN_TIMEOUT, i);   // instantiate TcpSocket object, non-blocking port, CONN_TIMEOUT timeout
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

  // Instantiate a multicast UDP object and spawn a thread to send asynchronous messages
  //
  Network::UdpSocket async(server.asyncport, server.asyncgroup);
  std::thread(async_main, async).detach();

  // thread to start a new logbook each day
  //
  std::thread( new_log_day, logstderr ).detach();

  for (;;) pause();                                  // main thread suspends
  return 0;
}
/** main *********************************************************************/


/** new_log_day **************************************************************/
/**
 * @brief      creates a new logbook each day
 * @param[in]  logstderr  true to also log to stderr
 *
 * This thread is started by main and never terminates.
 * It sleeps for the number of seconds that logentry determines
 * are remaining in the day, then closes and re-inits a new log file.
 *
 * The number of seconds until the next day "nextday" is a global which
 * is set by init_log.
 *
 */
void new_log_day( bool logstderr ) { 
  while (1) {
    std::this_thread::sleep_for( std::chrono::seconds( nextday ) );
    close_log();
    init_log( logpath, Camera::DAEMON_NAME, logstderr );
  }
}
/** new_log_day **************************************************************/


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


/** async_main ***************************************************************/
/**
 * @fn     async_main
 * @brief  asynchronous message sending thread
 * @param  Network::UdpSocket sock, socket object
 * @return nothing
 *
 * Loops forever, when a message arrives in the status message queue it is
 * sent out via multi-cast UDP datagram.
 *
 */
void async_main(Network::UdpSocket sock) {
  std::string function = "Camera::async_main";
  int retval;

  retval = sock.Create();                                   // create the UDP socket
  if (retval < 0) {
    logwrite(function, "error creating UDP multicast socket for asynchronous messages");
    server.exit_cleanly();                                  // do not continue on error
  }
  if (retval==1) {                                          // exit this thread but continue with server
    logwrite(function, "asyncrhonous message port disabled by request");
  }

  while (1) {
    std::string message = server.camera.async.dequeue();    // get the latest message from the queue (blocks)
    retval = sock.Send(message);                            // transmit the message
    if (retval < 0) {
      std::stringstream errstm;
      errstm << "error sending UDP message: " << message;
      logwrite(function, errstm.str());
    }
    if (message=="exit") {                                  // terminate this thread
      sock.Close();
      return;
    }
  }
  return;
}
/** async_main ***************************************************************/


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

  message.str(""); message << "thread " << sock.id << " accepted connection on fd " << sock.getfd();
  logwrite( function, message.str() );

  while (connection_open) {
    memset(buf,  '\0', BUFSIZE);  // init buffers

    // Wait (poll) connected socket for incoming data...
    //
    int pollret;
    if ( ( pollret=sock.Poll() ) <= 0 ) {
      if (pollret==0) {
        message.str(""); message << "Poll timeout on fd " << sock.getfd() << " thread " << sock.id;
        logwrite(function, message.str());
      }
      if (pollret <0) {
        message.str(""); message << "Poll error on fd " << sock.getfd() << " thread " << sock.id << ": " << strerror(errno);
        logwrite(function, message.str());
      }
      break;                      // this will close the connection
    }

    // Data available, now read from connected socket...
    //
    std::string sbuf;
    char delim='\n';
    if ( ( ret=sock.Read( sbuf, delim ) ) <= 0 ) {
      if (ret<0) {                // could be an actual read error
        message.str(""); message << "Read error on fd " << sock.getfd() << ": " << strerror(errno); logwrite(function, message.str());
      }
      if (ret==0) {
        message.str(""); message << "timeout reading from fd " << sock.getfd();
        logwrite( function, message.str() );
      }
      break;                      // Breaking out of the while loop will close the connection.
                                  // This probably means that the client has terminated abruptly, 
                                  // having sent FIN but not stuck around long enough
                                  // to accept CLOSE and give the LAST_ACK.
    }

    // convert the input buffer into a string and remove any trailing linefeed
    // and carriage return
    //
    sbuf.erase(std::remove(sbuf.begin(), sbuf.end(), '\r' ), sbuf.end());
    sbuf.erase(std::remove(sbuf.begin(), sbuf.end(), '\n' ), sbuf.end());

    if (sbuf.empty()) {sock.Write("\n"); continue;}  // acknowledge empty command so client doesn't time out

    try {
      std::size_t cmd_sep = sbuf.find_first_of(" "); // find the first space, which separates command from argument list

      cmd = sbuf.substr(0, cmd_sep);                 // cmd is everything up until that space

      if (cmd.empty()) {sock.Write("\n"); continue;} // acknowledge empty command so client doesn't time out

      if (cmd_sep == std::string::npos) {            // If no space was found,
        args="";                                     // then the arg list is empty,
      }
      else {
        args= sbuf.substr(cmd_sep+1);                // otherwise args is everything after that space.
      }

      message.str(""); message << "thread " << sock.id << " received command on fd " << sock.getfd() << ": " << cmd << " " << args;
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
    std::string retstring="";                               // string for return the value (where needed)

    if (cmd.compare("exit")==0) {
                    server.camera.async.enqueue("exit");    // shutdown the async message thread if running
                    server.exit_cleanly();                  // shutdown the server
                    }
    else
    if (cmd.compare("config")==0) {                         // report the config file used for camerad
                    std::stringstream cfg;
                    cfg << "CONFIG:" << server.config.filename;
                    server.camera.async.enqueue( cfg.str() );
                    sock.Write( server.config.filename );
                    sock.Write( " " );
                    ret = NO_ERROR;
                    }
    else
    if (cmd.compare("open")==0) {
                    ret = server.connect_controller(args);
                    }
    else
    if (cmd.compare("close")==0) {
                    ret = server.disconnect_controller();
                    }
    else
    if (cmd.compare("load")==0) {
                    if (args.empty()) ret = server.load_firmware(retstring);
                    else              ret = server.load_firmware(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("basename")==0) {
                    ret = server.camera.basename(args, retstring);
                    sock.Write(retstring);
                    sock.Write(" ");
                    }
    else
    if (cmd.compare("imnum")==0) {
                    ret = server.camera.imnum(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("imdir")==0) {
                    ret = server.camera.imdir(args, retstring);
                    sock.Write(retstring);
                    sock.Write(" ");
                    }
    else
    if (cmd.compare("autodir")==0) {
                    ret = server.camera.autodir(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("datacube")==0) {
                    ret = server.camera.datacube(args, retstring);
                    sock.Write(retstring);
                    sock.Write(" ");
                    }
    else
    if (cmd.compare("longerror")==0) {
                    ret = server.camera.longerror(args, retstring);
                    sock.Write(retstring);
                    sock.Write(" ");
                    }
    else
    if (cmd.compare("preexposures")==0) {
                    ret = server.camera_info.pre_exposures( args, retstring );
                    sock.Write( retstring );
                    sock.Write( " " );
                    }
    else
    if (cmd.compare("cubeamps")==0) {
                    ret = server.camera.cubeamps(args, retstring);
                    sock.Write(retstring);
                    sock.Write(" ");
                    }
    else
    if (cmd.compare("fitsnaming")==0) {
                    ret = server.camera.fitsnaming(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("shutter")==0) {
                    ret = server.shutter(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("writekeys")==0) {
                    ret = server.camera.writekeys(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("key")==0) {
                    if (args.compare(0, 4, "list")==0) {
                      logwrite( function, "systemkeys:" ); ret = server.systemkeys.listkeys();
                      logwrite( function, "userkeys:" );   ret = server.userkeys.listkeys();
                    }
                    else {
                      ret = server.userkeys.addkey(args);
                      if ( ret != NO_ERROR ) server.camera.log_error( function, "bad syntax" );
                    }
                    }
    else
    if (cmd.compare("abort")==0) {
                    server.camera.abort();
                    ret = 0;
                    }
#ifdef ASTROCAM
    else
    if (cmd.compare("isopen")==0) {
                    ret = server.is_connected( retstring );
                    sock.Write(retstring);
                    sock.Write(" ");
                    }
    else
    if (cmd.compare("useframes")==0) {
                    ret = server.access_useframes(args);
                    if (!args.empty()) { sock.Write(args); sock.Write(" "); }
                    }
    else
    if (cmd.compare("geometry")==0) {
                    ret = server.geometry(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("buffer")==0) {
                    ret = server.buffer(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("readout")==0) {
                    ret = server.readout(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
#endif
#ifdef STA_ARCHON
    else
    if (cmd.compare("nlines")==0) {
        ret = server.nlines(args, retstring);
        if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
    }
    else
    if (cmd.compare("video")==0) {
        ret = server.video();
    }
    else
    if (cmd.compare("hexpose")==0) {
        ret = server.hexpose(args);
    }
    else
    if (cmd.compare("roi")==0) {
                    ret = server.region_of_interest( args, retstring );
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("isloaded")==0) {
                    retstring = server.firmwareloaded ? "true" : "false";
                    sock.Write(retstring);
                    sock.Write(" ");
                    ret = NO_ERROR;
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
    if (cmd.compare("getp")==0) {
                    ret = server.get_parameter(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("setp")==0) {
                    ret = server.set_parameter(args);
                    }
    else
    if (cmd.compare("loadtiming")==0) {
                    if (args.empty()) ret = server.load_timing(retstring);
                    else              ret = server.load_timing(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("inreg")==0) {
                    ret = server.inreg(args);
                    }
    else
    if (cmd.compare("printstatus")==0) {
                    ret = server.get_frame_status();
                    if (ret==NO_ERROR) server.print_frame_status();
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
    if (cmd.compare("cds")==0) {
                    ret = server.cds(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("heater")==0) {
                    ret = server.heater(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("sensor")==0) {
                    ret = server.sensor(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("longexposure")==0) {
                    ret = server.longexposure(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("hdrshift")==0) {
                    ret = server.hdrshift(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("trigin")==0) {
                    ret = server.trigin(args);
                    }
#endif
    else
    if (cmd.compare("expose")==0) {
                    ret = server.expose(args);
                    }
    else
    if (cmd.compare("exptime")==0) {
                    // Neither controller allows fractional exposure times
                    // so catch that here.
                    //
                    if ( args.find(".") != std::string::npos ) {
                      ret = ERROR;
                      logwrite(function, "ERROR: fractional exposure times not allowed");
                      // empty the args string so that a call to exptime returns the current exptime
                      //
                      args="";
                      server.exptime(args, retstring);
                    }
                    else { ret = server.exptime(args, retstring); }
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("bias")==0) {
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
                    ret = server.interface(retstring);
                    sock.Write(retstring);
                    sock.Write(" ");
                    }
    else
    if (cmd.compare("test")==0) {
                    ret = server.test(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
                    }
    else
    if (cmd.compare("native")==0) {
                    try {
                      std::transform( args.begin(), args.end(), args.begin(), ::toupper );    // make uppercase
                    }
                    catch (...) {
                      logwrite(function, "error converting command to uppercase");
                      ret=ERROR;
                    }
#ifdef ASTROCAM
                    ret = server.native(args, retstring);
                    if (!retstring.empty()) { sock.Write(retstring); sock.Write(" "); }
#endif
#ifdef STA_ARCHON
                    ret = server.native(args);
#endif
                  }

    // if no matching command found
    //
    else {        message.str(""); message << "ERROR unrecognized command: " << cmd;
                  logwrite( function, message.str() );
                  ret=ERROR;
                  }

    if (ret != NOTHING) {
      std::string retstr=(ret==0?"DONE\n":"ERROR\n");
      if ( ret==0 ) retstr="DONE\n"; else retstr="ERROR" + server.camera.get_longerror() + "\n";
      if (sock.Write(retstr)<0) connection_open=false;
    }

    if (!sock.isblocking()) break;       // Non-blocking connection exits immediately.
                                         // Keep blocking connection open for interactive session.
  }

  sock.Close();
  return;
}
/** doit *********************************************************************/

