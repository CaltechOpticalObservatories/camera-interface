#include "archon_interface.h"
#include <iostream>

namespace Camera {
  ArchonInterface::~ArchonInterface() = default;

  void ArchonInterface::myfunction() {
    std::cerr << "Archon's implementation of Camera::myfunction\n";
  }

  /**************** ArchonInterface::connect_controller *********************/
  /**
   * @fn     connect_controller
   * @brief
   * @param  none (devices_in here for future expansion)
   * @return 
   *
   */
  long ArchonInterface::connect_controller(std::string args, std::string &retstring) {
    // Example content
    retstring = "OK";
    return 0;
  }

  long ArchonInterface::connect_controller(const std::string& devices_in="") {
    std::string function = "ArchonInterface::connect_controller";
    std::stringstream message;
    int adchans=0;
    long   error = ERROR;

    if ( this->controller.connected) {
      logwrite(function, "camera connection already open");
      return NO_ERROR;
    }

    // Initialize the camera connection
    //
    logwrite(function, "opening a connection to the camera system");

    if ( this->controller.connected != 0 ) {
      message.str(""); message << "connecting to " << this->controller.archon_network_details.hostname << ":" << this->controller.archon_network_details.port << ": " << strerror(errno);
      logwrite( function, message.str() );
      return ERROR;
    }

    message.str("");
    message << "socket connection to " << this->controller.archon_network_details.hostname << ":" << this->controller.archon_network_details.port;
    logwrite(function, message.str());

    // Get the current system information for the installed modules
    //
    std::string reply;
    error = this->send_cmd( SYSTEM, reply );        // first the whole reply in one string

    std::vector<std::string> lines, tokens;
    Tokenize( reply, lines, " " );                    // then each line in a separate token "lines"

    for ( const auto& line : lines ) {
      Tokenize( line, tokens, "_=" );                 // finally break each line into tokens to get module, type and version
      if ( tokens.size() != 3 ) continue;             // need 3 tokens

      std::string version;
      int module=0;
      int type=0;

      // get the module number
      //
      if ( tokens[0].compare( 0, 9, "BACKPLANE" ) == 0 ) {
        if ( tokens[1] == "VERSION" ) this->controller.backplaneversion = tokens[2];
        continue;
      }

      // get the module and type of each module from MODn_TYPE
      //
      if ( ( tokens[0].compare( 0, 3, "MOD" ) == 0 ) && ( tokens[1] == "TYPE" ) ) {
        try {
          module = std::stoi( tokens[0].substr(3) );
          type   = std::stoi( tokens[2] );

        } catch (std::invalid_argument &) {
          message.str(""); message << "unable to convert module or type from " << tokens[0] << "=" << tokens[1] << " to integer";
          logwrite( function, message.str() );
          return ERROR;

        } catch (std::out_of_range &) {
          message.str(""); message << "module " << tokens[0].substr(3) << " or type " << tokens[1] << " out of range";
          logwrite( function, message.str() );
          return ERROR;
        }

      } else continue;

      // get the module version
      //
      if ( tokens[1] == "VERSION" ) version = tokens[2]; else version = "";

      // now store it permanently
      //
      if ( (module > 0) && (module <= nmods) ) {
        try {
          this->controller.modtype.at(module-1)    = type;       // store the type in a vector indexed by module
          this->controller.modversion.at(module-1) = version;    // store the type in a vector indexed by module

        } catch (std::out_of_range &) {
          message.str(""); message << "requested module " << module << " out of range {1:" << nmods;
          logwrite( function, message.str() );
        }

      } else {                                          // else should never happen
        message.str(""); message << "module " << module << " outside range {1:" << nmods << "}";
        logwrite( function, message.str() );
        return ERROR;
      }

      // Use the module type to resize the gain and offset vectors,
      // but always use the largest possible value allowed.
      //
      if ( type ==  2 ) adchans = ( adchans < MAXADCCHANS ? MAXADCCHANS : adchans );  // ADC module (type=2) found
      if ( type == 17 ) adchans = ( adchans < MAXADMCHANS ? MAXADMCHANS : adchans );  // ADM module (type=17) found
      this->controller.gain.resize( adchans );
      this->controller.offset.resize( adchans );

      // Check that the AD modules are installed in the correct slot
      //
      if ( ( type == 2 || type == 17 ) && ( module < 5 || module > 8 ) ) {
        message.str(""); message << "AD module (type=" << type << ") cannot be in slot " << module << ". Use slots 5-8";
        logwrite( function, message.str() );
        return ERROR;
      }

    } // end for ( auto line : lines )

    // empty the Archon log
    //
    error = this->fetchlog();

    return error;
  }
  /**************** ArchonInterface::connect_controller *********************/


  /**************** ArchonInterface::disconnect_controller ******************/
  /**
   * @fn     disconnect_controller
   * @brief
   * @param  none
   * @return 
   *
   */
  long ArchonInterface::disconnect_controller(const std::string args, std::string &retstring) {
    std::string function = "ArchonInterface::disconnect_controller";
    long error = NO_ERROR;
    if (!this->controller.connected) {
      logwrite(function, "connection already closed");
      return (NO_ERROR);
    }

    // On success, write the value to the log and return
    //
    if (error == NO_ERROR) {
      logwrite(function, "Archon connection terminated");

    } else {
        // Throw an error for any other errors
      logwrite( function, "disconnecting Archon camera" );
    }

    return error;
  }
  /**************** ArchonInterface::disconnect_controller ******************/

  /**************** ArchonInterface::archon_cmd *****************************/
  /**
   * @fn     archon_cmd
   * @brief  send a command to Archon
   * @param  cmd
   * @param  reply (optional)
   * @return ERROR, BUSY or NO_ERROR
   *
   */
  long ArchonInterface::send_cmd(std::string cmd) { // use this form when the calling
    std::string reply;                          // function doesn't need to look at the reply
    return( send_cmd(cmd, reply) );
  }
  long ArchonInterface::send_cmd(std::string cmd, std::string &reply) {
    std::string function = "ArchonInterface::archon_cmd";
    std::stringstream message;
    int     retval;
    char    check[4];
    char    buffer[4096];                       //!< temporary buffer for holding Archon replies
    int     error = NO_ERROR;

    if (!this->controller.connected) {          // nothing to do if no connection open to controller
      logwrite( function, "connection not open to controller" );
      return ERROR;
    }

    if (this->controller.is_busy) {                    // only one command at a time
      message.str(""); message << "Archon busy: ignored command " << cmd;
      logwrite( function, message.str() );
      return BUSY;
    }

    /**
     * Hold a scoped lock for the duration of this function, 
     * to prevent multiple threads from accessing the Archon.
     */
    const std::lock_guard<std::mutex> lock(this->controller.archon_mutex);
    this->controller.is_busy = true;

    // build command: ">xxCOMMAND\n" where xx=hex msgref and COMMAND=command
    //
    this->controller.msgref = (this->controller.msgref + 1) % 256;       // increment msgref for each new command sent
    std::stringstream ssprefix;
    ssprefix << ">"
             << std::setfill('0')
             << std::setw(2)
             << std::hex
             << this->controller.msgref;
    std::string prefix=ssprefix.str();
    try {
      std::transform( prefix.begin(), prefix.end(), prefix.begin(), ::toupper );    // make uppercase

    } catch (...) {
      message.str(""); message << "converting Archon command: " << prefix << " to uppercase";
      logwrite( function, message.str() );
      return ERROR;
    }

    // This allows sending commands that don't get logged,
    // by prepending QUIET, which gets removed here if present.
    //
    bool quiet=false;
    if ( cmd.find(QUIET)==0 ) {
      cmd.erase(0, QUIET.length());
      quiet=true;
    }

    std::stringstream  sscmd;         // sscmd = stringstream, building command
    sscmd << prefix << cmd << "\n";
    std::string scmd = sscmd.str();   // scmd = string, command to send

    // build the command checksum: msgref used to check that reply matches command
    //
    SNPRINTF(check, "<%02X", this->controller.msgref)

    // log the command as long as it's not a STATUS, TIMER, WCONFIG or FRAME command
    //
    if ( !quiet && (cmd.compare(0,7,"WCONFIG") != 0) &&
                   (cmd.compare(0,5,"TIMER") != 0)   &&
                   (cmd.compare(0,6,"STATUS") != 0)  &&
                   (cmd.compare(0,5,"FRAME") != 0) ) {
      // erase newline for logging purposes
      std::string fcmd = scmd;
      try {
          fcmd.erase(fcmd.find('\n'), 1);
      } catch(...) { }
      message.str(""); message << "sending command: " << fcmd;
      logwrite(function, message.str());
    }

    // send the command
    //
    if ( (this->controller.sock.Write(scmd)) == -1) {
      logwrite( function, "writing to camera socket");
    }

    // For the FETCH command we don't wait for a reply, but return immediately.
    // FETCH results in a binary response which is handled elsewhere (in read_frame).
    // Must also distinguish this from the FETCHLOG command, for which we do wait
    // for a normal reply.
    //
    // The scoped mutex lock will be released automatically upon return.
    //
    if ( (cmd.compare(0,5,"FETCH")==0)
        && (cmd.compare(0,8,"FETCHLOG")!=0) ) return (NO_ERROR);

    // For all other commands, receive the reply
    //
    reply.clear();                                   // zero reply buffer
    do {
      if ( (retval=this->controller.sock.Poll()) <= 0) {
        if (retval==0) {
            message.str("");
            message << "Poll timeout waiting for response from Archon command (maybe unrecognized command?)";
            error = TIMEOUT;
        }
        if (retval<0)  {
            message.str("");
            message << "Poll error waiting for response from Archon command";
            error = ERROR;
        }
        if ( error != NO_ERROR ) {
          logwrite( function, message.str() );
        }
        break;
      }
      memset(buffer, '\0', 2048);                    // init temporary buffer
      retval = this->controller.sock.Read(buffer, 2048);      // read into temp buffer
      if (retval <= 0) {
        logwrite( function, "reading Archon" );
        break; 
      }
      reply.append(buffer);                          // append read buffer into the reply string
    } while(retval>0 && reply.find('\n') == std::string::npos);

    // If there was an Archon error then clear the busy flag and get out now
    //
    if ( error != NO_ERROR ) {
        this->controller.is_busy = false;
        return error;
    }

    // The first three bytes of the reply should contain the msgref of the
    // command, which can be used as a check that the received reply belongs
    // to the command which was sent.
    //
    // Error processing command (no other information is provided by Archon)
    //
    if (reply.compare(0, 1, "?")==0) {  // "?" means Archon experienced an error processing command
      error = ERROR;
      message.str(""); message << "Archon controller returned error processing command: " << cmd;
      logwrite( function, message.str() );

    } else if (reply.compare(0, 3, check)!=0) {  // First 3 bytes of reply must equal checksum else reply doesn't belong to command
        error = ERROR;
        // std::string hdr = reply;
        try {
          scmd.erase(scmd.find('\n'), 1);
        } catch(...) { }
        message.str(""); message << "command-reply mismatch for command: " + scmd + ": expected " + check + " but received " + reply ;
        logwrite( function, message.str() );
    } else {                                           // command and reply are a matched pair
      error = NO_ERROR;

      // log the command as long as it's not a STATUS, TIMER, WCONFIG or FRAME command
      if ( !quiet && (cmd.compare(0,7,"WCONFIG") != 0) &&
                     (cmd.compare(0,5,"TIMER") != 0)   &&
                     (cmd.compare(0,6,"STATUS") != 0)  &&
                     (cmd.compare(0,5,"FRAME") != 0) ) {
        message.str("");
        message << "command 0x" << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << this->controller.msgref << " success";
        logwrite(function, message.str());
      }

      reply.erase(0, 3);                             // strip off the msgref from the reply
    }

    // clear the semaphore (still had the mutex this entire function)
    //
    this->controller.is_busy = false;

    return error;
  }
  /**************** ArchonInterface::archon_cmd *****************************/

  /**************** ArchonInterface::fetchlog *******************************/
  /**
   * @fn     fetchlog
   * @brief  fetch the archon log entry and log the response
   * @param  none
   * @return NO_ERROR or ERROR,  return value from archon_cmd call
   *
   * Send the FETCHLOG command to, then read the reply from Archon.
   * Fetch until the log is empty. Log the response.
   *
   */
  long ArchonInterface::fetchlog() {
    std::string function = "ArchonInterface::fetchlog";
    std::string reply;
    std::stringstream message;
    long  retval;

    // send FETCHLOG command while reply is not (null)
    //
    do {
      if ( (retval=this->send_cmd(FETCHLOG, reply)) != NO_ERROR ) {          // send command here
        logwrite( function, "ERROR: calling FETCHLOG" );
        return retval;
      }
      if (reply != "(null)") {
        try {
            reply.erase(reply.find('\n'), 1);
        } catch(...) { }             // strip newline
        logwrite(function, reply);                                           // log reply here
      }
    } while (reply != "(null)");                                             // stop when reply is (null)

    return retval;
  }
  /**************** ArchonInterface::fetchlog *******************************/

}
