/**
 * @file    archon_interface.cpp
 * @brief   implementation of Archon Interface
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "archon_interface.h"
#include "archon_controller.h"

namespace Camera {

  ArchonInterface::ArchonInterface() {
    controller.set_interface(this);
  }
  ArchonInterface::~ArchonInterface() {
  }


  /***** Camera::ArchonInterface::abort ***************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::abort( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::abort");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::ArchonInterface::abort ***************************************/


  /***** Camera::ArchonInterface::autodir *************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::autodir( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::autodir");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::ArchonInterface::autodir *************************************/


  /***** Camera::ArchonInterface::basename ************************************/
  /**
   * @brief      set or get the image basename
   * @param[in]  args       requested base name
   * @param[out] retstring  current base name
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::basename( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::basename");
    long error=NO_ERROR;

    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_BASENAME;
      retstring.append( " [ <name> ]\n" );
      retstring.append( "  set or get image basename\n" );
      return HELP;
    }

    // Base name cannot contain a "/" because that would be a subdirectory,
    // and subdirectories are not checked here, only by imdir command.
    //
    if ( args.find('/') != std::string::npos ) {
      logwrite( function, "ERROR basename cannot contain '/' character" );
      error = ERROR;
    }
    // if name is supplied the set the image name
    else if ( !args.empty() ) {
      this->camera_info.base_name = args;
      error = NO_ERROR;
    }

    // In any case, log and return the current value.
    //
    logwrite(function, "base name is "+std::string(this->camera_info.base_name));
    retstring = this->camera_info.base_name;

    return error;
  }
  /***** Camera::ArchonInterface::basename ************************************/


  /***** Camera::ArchonInterface::bias ****************************************/
  /**
   * @brief      set a bias voltage
   * @param[in]  args       contains <module> <chan> <voltage>
   * @param[out] retstring  
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::bias( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::bias");
    std::stringstream message;
    std::vector<std::string> tokens;
    std::stringstream biasconfig;
    int module;
    int channel;
    float voltage;
    float vmin, vmax;
    bool readonly=true;

    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_BIAS;
      retstring.append( " <module> <chan> <voltage>\n" );
      retstring.append( "  set a bias voltage\n" );
      return HELP;
    }

    // must have loaded firmware
    //
    if ( ! this->controller.is_firmwareloaded ) {
      logwrite( function, "ERROR firmware not loaded" );
      return ERROR;
    }

    Tokenize(args, tokens, " ");

    if (tokens.size() == 2) {
      readonly = true;

    } else if (tokens.size() == 3) {
      readonly = false;

    } else {
      message.str(""); message << "incorrect number of arguments: " << args << ": expected module channel [voltage]";
      logwrite( function, message.str() );
      return ERROR;
    }

    try {
      module  = std::stoi( tokens[0] );
      channel = std::stoi( tokens[1] );
      if (!readonly) voltage = std::stof( tokens[2] );
    }
    catch (const std::exception &e) {
      message.str(""); message << "ERROR parsing \"" << args << "\": expected <module> <channel> [ voltage ]: "
                               << e.what();
      logwrite( function, message.str() );
      return ERROR;

    }

    // Check that the module number is valid
    //
    if ( (module < 0) || (module > nmods) ) {
      message.str(""); message << "module " << module << ": outside range {0:" << nmods << "}";
      logwrite( function, message.str() );
      return ERROR;
    }

    // Use the module type to get LV or HV Bias
    // and start building the bias configuration string.
    //
    switch ( this->controller.modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "module " << module << " not installed";
        logwrite( function, message.str() );
        return ERROR;
        break;
      case 3:  // LVBias
      case 9:  // LVXBias
        biasconfig << "MOD" << module << "/LV";
        vmin = -14.0;
        vmax = +14.0;
        break;
      case 4:  // HVBias
      case 8:  // HVXBias
        biasconfig << "MOD" << module << "/HV";
        vmin =   0.0;
        vmax = +31.0;
        break;
      default:
        message.str(""); message << "module " << module << " not a bias board";
        logwrite( function, message.str() );
        return ERROR;
        break;
    }

    // Check that the channel number is valid
    // and add it to the bias configuration string.
    //
    if ( (channel < 1) || (channel > 30) ) {
      message.str(""); message << "bias channel " << module << ": outside range {1:30}";
      logwrite( function, message.str() );
      return ERROR;
    }
    if ( (channel > 0) && (channel < 25) ) {
      biasconfig << "LC_V" << channel;
    }
    if ( (channel > 24) && (channel < 31) ) {
      channel -= 24;
      biasconfig << "HC_V" << channel;
    }

    if ( (voltage < vmin) || (voltage > vmax) ) {
      message.str(""); message << "bias voltage " << voltage << ": outside range {" << vmin << ":" << vmax << "}";
      logwrite( function, message.str() );
      return ERROR;
    }

    // Locate this line in the configuration so that it can be written to the Archon
    //
    std::string key   = biasconfig.str();
    std::string value = std::to_string(voltage);
    bool changed      = false;
    long error;

    // If no voltage suppled (readonly) then just read the configuration and exit
    //
    if (readonly) {
      message.str("");
      error = this->get_configmap_value(key, voltage);
      if (error != NO_ERROR) {
        message << "reading bias " << key;

      } else {
        retstring = std::to_string(voltage);
        message << "read bias " << key << "=" << voltage;
      }
      logwrite( function, message.str() );
      return error;
    }

    // Write the config line to update the bias voltage
    //
    error = this->controller.write_config_key(key.c_str(), value.c_str(), changed);

    // Now send the APPLYMODx command
    //
    std::stringstream applystr;
    applystr << "APPLYMOD"
             << std::setfill('0')
             << std::setw(2)
             << std::hex
             << (module-1);

    if (error == NO_ERROR) error = this->send_cmd(applystr.str());

    if (error != NO_ERROR) {
      message << "writing bias configuration: " << key << "=" << value;

    } else if (!changed) {
      message << "bias configuration: " << key << "=" << value <<" unchanged";

    } else {
      message << "updated bias configuration: " << key << "=" << value;
    }

    logwrite(function, message.str());

    return error;
  }
  /***** Camera::ArchonInterface::bias ****************************************/


  /***** Camera::ArchonInterface::bin *****************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::bin( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::bin");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::ArchonInterface::bin *****************************************/


  /***** Camera::ArchonInterface::connect_controller **************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::connect_controller( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::connect_controller");
    logwrite(function, "not yet implemented");
    // Example content
    retstring = "OK";
    return NO_ERROR;
  }

  long ArchonInterface::connect_controller(const std::string& devices_in="") {
    std::string function = "ArchonInterface::connect_controller";
    std::stringstream message;
    int adchans=0;
    long   error = ERROR;

    if ( this->controller.is_connected) {
      logwrite(function, "camera connection already open");
      return NO_ERROR;
    }

    // Initialize the camera connection
    //
    logwrite(function, "opening a connection to the camera system");

    if ( !this->controller.is_connected ) {
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
  /***** Camera::ArchonInterface::connect_controller **************************/


  /***** Camera::ArchonInterface::get_configmap_value *************************/
  /**
   * @brief      get the VALUE from configmap for a givenn KEY and assign to a variable
   * @param[in]  key_in is the KEY
   * @param[out] value_out reference to variable to contain the VALUE
   * @return     ERROR | NO_ERROR
   *
   * This is a template class function so the &value_out reference can be any type.
   * If the key_in KEY is not found then an error message is logged and ERROR is
   * returned, otherwise the VALUE associated with key_in is assigned to &value_out, 
   * and NO_ERROR is returned.
   *
   */
  template <class T>
  long ArchonInterface::get_configmap_value(std::string key_in, T& value_out) {
    const std::string function("Camera::ArchonInterface::get_configmap_value");
    std::stringstream message;

    if ( this->controller.configmap.find(key_in) != this->controller.configmap.end() ) {
      std::istringstream( this->controller.configmap[key_in].value  ) >> value_out;
      return NO_ERROR;
    }
    else {
      message.str(""); message << "ERROR requested key: " << key_in << " not found in configuration";
      logwrite( function, message.str() );
      return ERROR;
    }
  }
  /***** Camera::ArchonInterface::get_configmap_value *************************/


  /***** Camera::ArchonInterface::disconnect_controller ***********************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::disconnect_controller(const std::string args, std::string &retstring) {
    const std::string function("Camera::ArchonInterface::disconnect_controller");
    long error = NO_ERROR;

    if (!this->controller.is_connected) {
      logwrite(function, "connection already closed");
      return NO_ERROR;
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
  /***** Camera::ArchonInterface::disconnect_controller ***********************/


  /***** Camera::ArchonInterface::exptime *************************************/
  /**
   * @brief      set/get the exposure time
   * @details    exposure time is in the units set by longexposure() or the
   *             unit can be optionally set here
   * @param[in]  exptime_in  "<time> [ s | ms ]" exposure time in current units
   * @param[out] retstring   return string
   * @return     ERROR | NO_ERROR
   *
   * This function calls "set_parameter()" and "get_parameter()" using
   * the "exptime" parameter (which must already be defined in the ACF file).
   *
   */
  long ArchonInterface::exptime( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::exptime");
    std::stringstream message;
    logwrite(function, "not yet implemented");
    long ret=NO_ERROR;
/***
    if ( !exptime_in.empty() ) {
      if ( exptime_in.find(".") != std::string::npos ) {
      logwrite( function, "must be a whole number" );
      retstring="invalid_argument";
      return ERROR;
      }
      std::vector<std::string> tokens;
      Tokenize( exptime_in, tokens, " " );
      try {
        uint32_t exptime=0;
        std::string unit;
        if ( tokens.size() > 0 ) exptime = static_cast<uint32_t>( std::stoul( tokens.at(0) ) );
        if ( tokens.size() > 1 ) unit    = tokens.at(1);
        if ( tokens.size() < 1 || tokens.size() > 2 ) {
          logwrite( function, "expected <exptime> [ s | ms ]" );
          retstring="invalid_argument";
          return ERROR;
        }

        if ( exptime < 0 || exptime > Archon::MAX_EXPTIME ) throw std::out_of_range("value out of range");

        // If a unit was supplied then call longexposure() to handle that
        //
        if ( ! unit.empty() ) {
          if ( unit=="s" )  ret = this->longexposure( "true", retstring );
          else
          if ( unit=="ms" ) ret = this->longexposure( "false", retstring );
          else {
            logwrite( function, "expected <exptime> [ s | ms ]" );
            return ERROR;
          }
          // longexposure() could fail if there's an error talking to Archon
          // or if longexposure is not supported by the ACF.
          //
          if ( ret != NO_ERROR ) {
            logwrite( function, "unable to set selected unit. exptime not set." );
            retstring="invalid_argument";
            return ERROR;
          }
        }

        // set the parameter on the Archon
        //
        std::stringstream cmd;
        cmd << "exptime " << tokens.at(0);
        ret = this->set_parameter( cmd.str() );

        // If Archon was updated then update the class
        //
        if ( ret == NO_ERROR ) {
          if ( ! unit.empty() ) this->camera_info.exposure_time.unit( unit );
          this->camera_info.exposure_time.value( exptime );
        }
      }
      catch (std::exception &e) {
        message.str(""); message << "parsing exposure time: " << exptime_in << ": " << e.what();
        logwrite( function, message.str() );
        return ERROR;
      }
    }

    // add exposure time to system keys db
    //
    message.str(""); message << "EXPTIME=" << this->camera_info.exposure_time.value()
                             << " // exposure time in " << this->camera_info.exposure_time.unit();
    this->systemkeys.addkey( message.str() );

    // prepare the return value
    //
    message.str(""); message << this->camera_info.exposure_time.value() << " " << this->camera_info.exposure_time.unit();
    retstring = message.str();

    message.str(""); message << "exposure time is " << retstring;
    logwrite(function, message.str());

    return ret;
***/
    return ERROR;
  }
  /***** Camera::ArchonInterface::exptime *************************************/


  /***** Camera::ArchonInterface::send_cmd ************************************/
  /**
   * @brief      send a command to Archon
   * @details    This function is overloaded.
   *             Use this form when the calling function doesn't need a reply.
   * @param[in]  cmd    command to send
   * @return     ERROR | BUSY | NO_ERROR
   *
   */
  long ArchonInterface::send_cmd(std::string cmd) {
    std::string reply;
    return( send_cmd(cmd, reply) );
  }
  /***** Camera::ArchonInterface::send_cmd ************************************/
  /**
   * @brief      send a command to Archon
   * @details    This function is overloaded.
   *             Use this form when the calling function doesn't need a reply.
   * @param[in]  cmd    command to send
   * @param[out] reply  string contains reply
   * @return     ERROR | BUSY | NO_ERROR
   *
   */
  long ArchonInterface::send_cmd(std::string cmd, std::string &reply) {
    std::string function = "ArchonInterface::send_cmd";
    std::stringstream message;
    int     retval;
    char    check[4];
    char    buffer[4096];                       //!< temporary buffer for holding Archon replies
    int     error = NO_ERROR;

    if (!this->controller.is_connected) {       // nothing to do if no connection open to controller
      logwrite( function, "connection not open to controller" );
      return ERROR;
    }

    if (this->controller.is_busy) {                    // only one command at a time
      message.str(""); message << "Archon busy: ignored command " << cmd;
      logwrite( function, message.str() );
      return BUSY;
    }

    // Hold a scoped lock for the duration of this function,
    // to prevent multiple threads from accessing the Archon.
    //
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
    }
    catch (const std::exception &e) {
      message.str(""); message << "ERROR converting command " << prefix << " to uppercase: " << e.what();
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
      memset(buffer, '\0', 2048);                         // init temporary buffer
      retval = this->controller.sock.Read(buffer, 2048);  // read into temp buffer
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
  /***** Camera::ArchonInterface::send_cmd ************************************/


  /***** Camera::ArchonInterface::fetchlog ************************************/
  /**
   * @brief  fetch the archon log entry and log the response
   * @return NO_ERROR or ERROR,  return value from send_cmd call
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
  /***** Camera::ArchonInterface::fetchlog ************************************/


  /***** Camera::ArchonInterface::test ****************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::test( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::test");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::ArchonInterface::test ****************************************/
}
