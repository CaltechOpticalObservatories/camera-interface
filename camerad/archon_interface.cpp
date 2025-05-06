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


  /***** Camera::ArchonInterface::expose **************************************/
  /**
   * @brief      
   * @param      
   * @param      
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::expose( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::expose");
    logwrite(function, "not yet implemented");

    // Help
    //
    if (args.empty() || args=="?" || args=="help") {
      retstring = CAMERAD_EXPOSE;
      retstring.append( " <tbd>\n" );
      retstring.append( "  TBD\n" );
      return HELP;
    }
    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::expose **************************************/


  /***** Camera::ArchonInterface::get_status_key ******************************/
  /**
   * @brief      get value for the indicated key from the Archon "STATUS" string
   * @param[in]  key    key to extract from STATUS
   * @param[out] value  value of key
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::get_status_key( std::string key, std::string &value ) {
    const std::string function("Camera::ArchonInterface::get_status_key");
    std::stringstream message;
    std::string reply;

    long error = this->send_cmd( STATUS, reply );  // first the whole reply in one string

    if ( error != NO_ERROR ) return error;

    std::vector<std::string> lines, tokens;
    Tokenize( reply, lines, " " );                 // then each line in a separate token "lines"

    for ( auto line : lines ) {
      Tokenize( line, tokens, "=" );               // break each line into tokens to get KEY=value
      if ( tokens.size() != 2 ) continue;          // need 2 tokens
      try {
        if ( tokens.at(0) == key ) {               // looking for the KEY
          value = tokens.at(1);                    // found the KEY= status here
          break;                                   // done looking
        } else continue;
      }
      catch (const std::exception &e) {            // should be impossible
        logwrite(function, "ERROR: "+std::string(e.what()));
        return ERROR;
      }
    }
    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::get_status_key ******************************/


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
    std::stringstream check;
    int     retval;
    char    buffer[4096];                       //!< temporary buffer for holding Archon replies
    int     error = NO_ERROR;

    if (!this->controller.is_connected) {       // nothing to do if no connection open to controller
      logwrite( function, "ERROR connection not open to controller" );
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
    check << "<" << std::setfill('0') << std::setw(2) << std::hex << this->controller.msgref;

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
    }
    else
    // First 3 bytes of reply must equal checksum else reply doesn't belong to command
    if (reply.substr(0,3) != check.str()) {
      error = ERROR;
      scmd.erase( std::remove( scmd.begin(), scmd.end(), '\n' ), scmd.end() );
      message.str(""); message << "ERROR command-reply mismatch for " + scmd + ": expected " + check.str() + " but received " + reply ;
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


  /***** Camera::ArchonInterface::load_acf ************************************/
  /**
   * @brief      loads the ACF file into configuration memory (no APPLY!)
   * @param[in]  acffile
   * @return     ERROR or NO_ERROR
   *
   * This function loads the specfied file into configuration memory.
   * While the ACF is being read, an internal database (STL map) is being
   * created to allow lookup access to the ACF file or parameters.
   *
   * The [MODE_XXX] sections are also parsed and parameters as a function
   * of mode are saved in their own database.
   *
   * This function only loads (WCONFIGxxx) the configuration memory; it does
   * not apply it to the system. Therefore, this function must be followed
   * with a LOADTIMING or APPLYALL command, for example.
   *
   */
  long ArchonInterface::load_acf(std::string acffile) {
    const std::string function("Camera::ArchonInterface::load_acf");
    std::stringstream message;
    std::fstream filestream;  // I/O stream class
    std::string line;         // the line read from the acffile
    std::string mode;
    std::string keyword, keystring, keyvalue, keytype, keycomment;
    std::stringstream sscmd;
    std::string key, value;

    int      linecount;  // the Archon configuration line number is required for writing back to config memory
    long     error=NO_ERROR;
    bool     parse_config=false;

    // get the acf filename, either passed here or from loaded default
    //
    if ( acffile.empty() ) {
      acffile = this->camera_info.firmware[0];

    } else {
      this->camera_info.firmware[0] = acffile;
    }

    // try to open the file
    //
    try {
      filestream.open(acffile, std::ios_base::in);
    }
    catch(const std::exception &e) {
      logwrite( function, "ERROR opening "+acffile+": "+std::string(e.what()));
      return ERROR;
    }

    if ( ! filestream.is_open() || ! filestream.good() ) {
      logwrite(function, "ERROR opening "+acffile);
      return ERROR;
    }

    logwrite(function, acffile);

    // The CPU in Archon is single threaded, so it checks for a network
    // command, then does some background polling (reading bias voltages etc.),
    // then checks again for a network command.  "POLLOFF" disables this
    // background checking, so network command responses are very fast.
    // The downside is that bias voltages, temperatures, etc. are not updated
    // until you give a "POLLON".
    //
    error = this->send_cmd(POLLOFF);

    // clear configuration memory for this controller
    //
    if (error == NO_ERROR) error = this->send_cmd(CLEARCONFIG);

    if ( error != NO_ERROR ) {
        logwrite( function, "ERROR: could not prepare Archon for new ACF" );
        return error;
    }

    // Any failure after clearing the configuration memory will mean
    // no firmware is loaded.
    //
    this->controller.is_firmwareloaded=false;

    this->controller.modemap.clear();            // file is open, clear all modes

    linecount = 0;                               // init Archon configuration line number

    while ( getline(filestream, line) ) {        // note that getline discards the newline "\n" character

      // don't start parsing until [CONFIG] and stop on a newline or [SYSTEM]
      //
      if (line == "[CONFIG]") { parse_config=true;  continue; }
      if (line == "\n"      ) { parse_config=false; continue; }
      if (line == "[SYSTEM]") { parse_config=false; continue; }

      std::string savedline = line;              // save un-edited line for error reporting

      // parse mode sections, looking for "[MODE_xxxxx]"
      //
      if (line.substr(0,6)=="[MODE_") {          // this is a mode section
        try {
          line.erase(line.find('['), 1);         // erase the opening square bracket
          line.erase(line.find(']'), 1);         // erase the closing square bracket
        }
        catch(const std::exception &e) {
          logwrite(function, "ERROR parsing \""+savedline+"\": "+std::string(e.what())+": expected [MODE_xxxx]");
          filestream.close();
          return ERROR;
        }
        if ( ! line.empty() ) {                  // What's remaining should be MODE_xxxxx
          mode = line.substr(5);                 // everything after "MODE_" is the mode name
          std::transform( mode.begin(), mode.end(), mode.begin(), ::toupper );    // make uppercase

          // got a mode, now check if one of this name has already been located
          // and put into the modemap
          //
          if ( this->controller.modemap.find(mode) != this->controller.modemap.end() ) {
            logwrite(function, "ERROR duplicate definition of mode "+mode+": load aborted");
	          filestream.close();
            return ERROR;
          }
          else {
            parse_config = true;
            message.str(""); message << "detected mode: " << mode; logwrite(function, message.str());
            this->controller.modemap[mode].rawenable=-1;    // initialize to -1, require it be set somewhere in the ACF
                                                 // this also ensures something is saved in the modemap for this mode
          }

        }
        else {                                   // somehow there's no xxx left after removing "[MODE_" and "]"
          logwrite(function, "ERROR malformed mode section \""+savedline+"\": expected [MODE_xxxx]");
          filestream.close();
          return ERROR;
        }
      }

      // Everything else is for parsing configuration lines so if we didn't get [CONFIG] then
      // skip to the next line.
      //
      if (!parse_config) continue;

      // replace any TAB characters with a space
      //
      string_replace_char(line, "\t", " ");

      // replace any backslash characters with a forward slash
      //
      string_replace_char(line, "\\", "/");

      // erase all quotes
      try {
          line.erase( std::remove(line.begin(), line.end(), '"'), line.end() );
      } catch(...) { }

      // Initialize key, value strings used to form WCONFIG KEY=VALUE command.
      // As long as key stays empty then the WCONFIG command is not written to the Archon.
      // This is what keeps TAGS: in the [MODE_xxxx] sections from being written to Archon,
      // because these do not populate key.
      //
      key="";
      value="";

      //  ************************************************************
      // Store actual Archon parameters in their own STL map IN ADDITION to the map
      // in which all other keywords are store, so that they can be accessed in
      // a different way.  Archon PARAMETER KEY=VALUE paris are formatted as:
      // PARAMETERn=ParameterName=value
      // where "PARAMETERn" is the key and "ParameterName=value" is the value.
      // However, it is logical to access them by ParameterName only. That is what the
      // parammap is for, hence the need for this STL map indexed on only the "ParameterName"
      // portion of the value. Conversely, the configmap is indexed by the key.
      //
      // parammap stores ONLY the parameters, which are identified as PARAMETERxx="paramname=value"
      // configmap stores every configuration line sent to Archon (which includes parameters)
      //
      // In order to modify these keywords in Archon, the entire above phrase
      // (KEY=VALUE pair) must be preserved along with the line number on which it
      // occurs in the config file.
      // ************************************************************

      // Look for TAGS: in the .acf file mode sections
      //
      // If tag is "ACF:" then it's a .acf line (could be a parameter or configuration)
      //
      if (line.compare(0,4,"ACF:")==0) {
        std::vector<std::string> tokens;
        line = line.substr(4);              // strip off the "ACF:" portion
        std::string acf_key, acf_value;

        try {
          Tokenize(line, tokens, "="); // separate into tokens by "="

          if (tokens.size() == 1) {                    // KEY=, the VALUE is empty
            acf_key   = tokens[0];
            acf_value = "";

          } else if (tokens.size() == 2) {             // KEY=VALUE
                acf_key   = tokens[0];
                acf_value = tokens[1];

          } else {
                logwrite(function, "ERROR malformed ACF line \""+savedline+": expected KEY=VALUE");
	              filestream.close();
                return ERROR;
          }

          bool keymatch = false;

          // If this key is in the main parammap then store it in the modemap's parammap for this mode
          if (this->controller.parammap.find( acf_key ) != this->controller.parammap.end()) {
            this->controller.modemap[mode].parammap[ acf_key ].name  = acf_key;
            this->controller.modemap[mode].parammap[ acf_key ].value = acf_value;
            keymatch = true;
          }

          // If this key is in the main configmap, then store it in the modemap's configmap for this mode
          //
          if (this->controller.configmap.find( acf_key ) != this->controller.configmap.end()) {
            this->controller.modemap[mode].configmap[ acf_key ].value = acf_value;
            keymatch = true;
          }

          // If this key is neither in the parammap nor in the configmap then return an error
          //
          if ( ! keymatch ) {
            message.str("");
            message << "[MODE_" << mode << "] ACF directive: " << acf_key << "=" << acf_value << " is not a valid parameter or configuration key";
            logwrite(function, message.str());
            filestream.close();
            return ERROR;
          }

        } catch (const std::exception &e) {
          logwrite(function, "ERROR extracting KEY=VAUE pair from \""+savedline+"\": "+std::string(e.what()));
          filestream.close();
          return ERROR;
        }
        // end if (line.compare(0,4,"ACF:")==0)

      } else if (line.compare(0,5,"ARCH:")==0) {
          // The "ARCH:" tag is for internal (Archon_interface) variables
          // using the KEY=VALUE format.
          //
          std::vector<std::string> tokens;
          line = line.substr(5);                                            // strip off the "ARCH:" portion
          Tokenize(line, tokens, "=");                                      // separate into KEY, VALUE tokens
          if (tokens.size() != 2) {
            logwrite(function, "ERROR malformed ARCH line \""+savedline+"\": expected ARCH:KEY=VALUE");
	          filestream.close();
            return ERROR;
          }
          if ( tokens[0] == "NUM_DETECT" ) {
            this->controller.modemap[mode].geometry.num_detect = std::stoi( tokens[1] );

          } else if ( tokens[0] == "HORI_AMPS" ) {
            this->controller.modemap[mode].geometry.amps[0] = std::stoi( tokens[1] );

          } else if ( tokens[0] == "VERT_AMPS" ) {
            this->controller.modemap[mode].geometry.amps[1] = std::stoi( tokens[1] );

          } else {
            logwrite(function, "ERROR unrecognized internal parameter "+tokens[0]);
	          filestream.close();
            return ERROR;
          }
          // end else if (line.compare(0,5,"ARCH:")==0)

      } else if (line.compare(0,5,"FITS:")==0) {
          // the "FITS:" tag is used to write custom keyword entries of the form "FITS:KEYWORD=VALUE/COMMENT"
          //
          std::vector<std::string> tokens;
          line = line.substr(5);           // strip off the "FITS:" portion

          // First, tokenize on the equal sign "=".
          // The token left of "=" is the keyword. Immediate right is the value
          Tokenize(line, tokens, "=");
          if (tokens.size() != 2) {    // need at least two tokens at this point
            logwrite(function, "ERROR malformed FITS command "+savedline+": expected KEYWORD=VALUE/COMMENT");
            filestream.close();
            return ERROR;
          }
          keyword   = tokens[0].substr(0,8); // truncate keyword to 8 characters
          keystring = tokens[1];                    // tokenize the rest in a moment
          keycomment = "";                          // initialize comment, assumed empty unless specified below

          // Next, tokenize on the slash "/".
          // The token left of "/" is the value. Anything to the right is a comment.
          //
          Tokenize(keystring, tokens, "/");

          if (tokens.empty()) {          // no tokens found means no "/" characeter which means no comment
            keyvalue = keystring;        // therefore the keyvalue is the entire string
          }

          if (not tokens.empty()) {         // at least one token
            keyvalue = tokens[0];
          }

          if (tokens.size() == 2) {      // If there are two tokens here then the second is a comment
            keycomment = tokens[1];
          }

          if (tokens.size() > 2) {       // everything below this has been covered
            logwrite(function, "ERROR malformed FITS command "+savedline+": expected KEYWORD=VALUE/COMMENT");
            logwrite(function, "ERROR too many \"/\" in comment string? "+keystring);
            filestream.close();
            return ERROR;
          }

          // Save all the user keyword information in a map for later
          this->controller.modemap[mode].acfkeys.keydb[keyword].keyword    = keyword;
          this->controller.modemap[mode].acfkeys.keydb[keyword].keytype    = this->camera_info.userkeys.get_keytype(keyvalue);
          this->controller.modemap[mode].acfkeys.keydb[keyword].keyvalue   = keyvalue;
          this->controller.modemap[mode].acfkeys.keydb[keyword].keycomment = keycomment;
          // end if (line.compare(0,5,"FITS:")==0)
          //
          // ----- all done looking for "TAGS:" -----
          //

      } else if ( (line.compare(0,11,"PARAMETERS=")!=0) &&   // not the "PARAMETERS=xx line
            (line.compare(0, 9,"PARAMETER"  )==0) ) {  // but must start with "PARAMETER"
          // If this is a PARAMETERn=ParameterName=value KEY=VALUE pair...
          //

          std::vector<std::string> tokens;
          Tokenize(line, tokens, "=");                  // separate into PARAMETERn, ParameterName, value tokens

          if (tokens.size() != 3) {
            logwrite(function, "ERROR malformed parameter line "+savedline+": expected PARAMETERn=Param=value");
            filestream.close();
            return ERROR;
          }

          // Tokenize broke everything up at the "=" and
          // we need all three parts, but we also need a version containing the last
          // two parts together, "ParameterName=value" so re-assemble them here.
          //
          std::stringstream paramnamevalue;
          paramnamevalue << tokens[1] << "=" << tokens[2];             // reassemble ParameterName=value string

          // build an STL map "configmap" indexed on PARAMETERn, the part before the first "=" sign
          //
          this->controller.configmap[ tokens[0] ].line  = linecount;              // configuration line number
          this->controller.configmap[ tokens[0] ].value = paramnamevalue.str();   // configuration value for PARAMETERn

          // build an STL map "parammap" indexed on ParameterName so that we can look up by the actual name
          //
          this->controller.parammap[ tokens[1] ].key   = tokens[0];    // PARAMETERn
          this->controller.parammap[ tokens[1] ].name  = tokens[1];    // ParameterName
          this->controller.parammap[ tokens[1] ].value = tokens[2];    // value
          this->controller.parammap[ tokens[1] ].line  = linecount;    // line number

          // assemble a KEY=VALUE pair used to form the WCONFIG command
          key   = tokens[0];                                      // PARAMETERn
          value = paramnamevalue.str();                           // ParameterName=value
          // end If this is a PARAMETERn=ParameterName=value KEY=VALUE pair...

      } else {
          // ...otherwise, for all other KEY=VALUE pairs, there is only the value and line number
          // to be indexed by the key. Some lines may be equal to blank, e.g. "CONSTANTx=" so that
          // only one token is made
          //
          std::vector<std::string> tokens;
          // Tokenize will return a size=1 even if there are no delimiters,
          // so work around this by first checking for delimiters
          // before calling Tokenize.
          //
          if (line.find_first_of('=', 0) == std::string::npos) {
            continue;
          }

          Tokenize(line, tokens, "=");                            // separate into KEY, VALUE tokens
          if (tokens.empty()) {
            continue;                                             // nothing to do here if no tokens (ie no "=")
          }

          key = tokens[0];                                        // not empty so at least one token is the KEY
          value.clear();                                          // VALUE can be empty (e.g. labels not required)

          if ( tokens.size() > 1 ) {                              // at least one more token is the value
            value = tokens[1];                                    // VALUE (there is a second token)
          }

          if ( tokens.size() > 2 ) {                              // more tokens are possible
            for ( size_t i=2; i<tokens.size(); ++i ) {            // loop through remaining tokens
              value += "=" + tokens[i];                           // and put them back together as the VALUE
            }
          }

          this->controller.configmap[ tokens[0] ].line  = linecount;
          this->controller.configmap[ tokens[0] ].value = value;
      } // end else

      // Form the WCONFIG command to Archon and
      // write the config line to the controller memory (if key is not empty).
      //
      if ( !key.empty() ) {                                     // value can be empty but key cannot
        sscmd.str("");
        sscmd << "WCONFIG"
              << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
              << linecount
              << key << "=" << value << "\n";
        // send the WCONFIG command here
        if (error == NO_ERROR) error = this->send_cmd(sscmd.str());
        linecount++;
      } // end if ( !key.empty() && !value.empty() )
    } // end while ( getline(filestream, line) )

    this->controller.configlines = linecount;  // save the number of configuration lines

    // re-enable background polling
    //
    if (error == NO_ERROR) error = this->send_cmd(POLLON);

    filestream.close();
    if (error == NO_ERROR) {
      logwrite(function, "loaded Archon config file OK");
      this->controller.is_firmwareloaded = true;

      // add to systemkeys keyword database
      //
      std::stringstream keystr;
      keystr << "FIRMWARE=" << acffile << "// controller firmware";
      this->systemkeys.addkey( keystr.str() );
    }

    // If there was an Archon error then read the Archon error log
    //
    if (error != NO_ERROR) error = this->fetchlog();

    this->controller.is_camera_mode = false;         // require that a mode be selected after loading new firmware

    return error;
  }
  /***** Camera::ArchonInterface::load_acf ************************************/


  /***** Camera::ArchonInterface::load_firmware *******************************/
  /**
   * @brief      send native commands directly to Archon and log result
   * @details    sends the command directly to Archon but does not parse any
   *
   */
  long ArchonInterface::load_firmware( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::load_firmware");
    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::load_firmware *******************************/


  /***** Camera::ArchonInterface::load_timing *********************************/
  /**
   * @brief      loads the ACF file and applies the timing script and parameters only
   * @param      acffile, specified ACF to load
   * @param      retstring, reference to string for return values
   * @return     ERROR | NO_ERROR | HELP
   *
   * This function is overloaded.
   *
   * This function loads the ACF file then sends the LOADTIMING command
   * which parses and compiles only the timing script and parameters.
   *
   */
  long ArchonInterface::load_timing( const std::string args, std::string &retstring ) {
    // Help
    //
    if (args.empty() || args=="?" || args=="help") {
      retstring = CAMERAD_LOADTIMING;
      retstring.append( " <timing.acf>\n" );
      retstring.append( "  Loads <timing.acf> file into Archon, then sends the LOADTIMING command\n" );
      retstring.append( "  which parses and compiles only the timing script and parameters.\n" );
      return HELP;
    }

    return( this->load_timing(args) );
  }
  /***** Camera::ArchonInterface::load_timing *********************************/
  /**
   * @brief      loads the ACF file and applies the timing script and parameters only
   * @param      acffile, specified ACF to load
   * @return     ERROR | NO_ERROR
   *
   * This function is overloaded.
   *
   * This function loads the ACF file then sends the LOADTIMING command
   * which parses and compiles only the timing script and parameters.
   *
   */
  long ArchonInterface::load_timing( const std::string args ) {
    // load the ACF file into configuration memory
    //
    long error = this->load_acf( args );

    // parse timing script and parameters and apply them to the system
    //
    if (error == NO_ERROR) error = this->send_cmd(LOADTIMING);

    return error;
  }
  /***** Camera::ArchonInterface::load_timing *********************************/


  /***** Camera::ArchonInterface::native **************************************/
  /**
   * @brief      send native commands directly to Archon and log result
   * @details    sends the command directly to Archon but does not parse any
   *             reply. @TODO publish the reply?
   * @param[in]  cmd  command to send to Archon
   * @return     ERROR|NO_ERROR|BUSY, return from send_cmd() call
   *
   */
  long ArchonInterface::native( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::native");
    std::string reply;

    // Help
    //
    if (args.empty() || args=="?" || args=="help") {
      retstring = CAMERAD_NATIVE;
      retstring.append( " <cmd>\n" );
      retstring.append( "  Sends <cmd> directly to Archon without parsing the reply, other than\n" );
      retstring.append( "  to confirm that it did reply.\n" );
      return HELP;
    }

    long error = send_cmd(args, reply);

    if (!reply.empty()) {
      logwrite(function, reply);
      // TODO publish the reply?
    }

    return error;
  }
  /***** Camera::ArchonInterface::native **************************************/


  /***** Camera::ArchonInterface::power ***************************************/
  /**
   * @brief      turn on controller bias power supplies
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::power( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::power");
    std::stringstream message;
    long error=NO_ERROR;

    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_POWER;
      retstring.append( " [ on | off ]\n" );
      retstring.append( "  Turn on|off Archon bias power supplies. If no arg supplied then\n" );
      retstring.append( "  return current state.\n" );
      return HELP;
    }

    if (!this->controller.is_connected) {       // nothing to do if no connection open to controller
      logwrite( function, "ERROR connection not open to controller" );
      return ERROR;
    }


    // set the Archon power state as requested
    //
    if ( !args.empty() ) {
      if ( caseCompareString(args, "on") ) {
        // send POWERON command to Archon and wait 2s to ensure stable
        if ( (error=this->send_cmd( POWERON )) == NO_ERROR ) {
          std::this_thread::sleep_for( std::chrono::seconds(2) );
        }
      }
      else
      if ( caseCompareString(args, "off") ) {
        // send POWEROFF command to Archon and wait 200ms to ensure off
        if ( (error=this->send_cmd( POWEROFF )) == NO_ERROR ) {
          std::this_thread::sleep_for( std::chrono::milliseconds(200) );
        }
      }
      else {
        logwrite(function, "ERROR expected {ON|OFF}");
        return ERROR;
      }
      if ( error != NO_ERROR ) {
        logwrite( function, "ERROR setting Archon power "+args);
        return ERROR;
      }
    }

    // Read the Archon power state directly from Archon
    //
    std::string power;
    error = get_status_key( "POWER", power );

    if ( error != NO_ERROR ) return ERROR;

    int status=-1;

    try { status = std::stoi( power ); }
    catch (const std::exception &e) {
      logwrite(function, "ERROR: "+std::string(e.what()));
      return ERROR;
    }

    // set the power status (or not) depending on the value extracted from the STATUS message
    //
    switch( status ) {
      case -1:                                                  // no POWER token found in status message
        logwrite(function, "ERROR finding power in Archon status message" );
        return ERROR;
      case  0:                                                  // usually an internal error
        this->controller.power_status = "UNKNOWN";
        break;
      case  1:                                                  // no configuration applied
        this->controller.power_status = "NOT_CONFIGURED";
        break;
      case  2:                                                  // power is off
        this->controller.power_status = "OFF";
        break;
      case  3:                                                  // some modules powered, some not
        this->controller.power_status = "INTERMEDIATE";
        break;
      case  4:                                                  // power is on
        this->controller.power_status = "ON";
        break;
      case  5:                                                  // system is in standby
        this->controller.power_status = "STANDBY";
        break;
      default:                                                  // should be impossible
        logwrite(function, "ERROR unknown power status: "+std::to_string(status));
        return ERROR;
    }

    message.str(""); message << "POWER:" << this->controller.power_status;
//  this->camera.async.enqueue( message.str() );

    retstring = this->controller.power_status;

    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::power ***************************************/


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

    // initialize the exposure mode to Expose_CCD and call that expose
    //
    logwrite(function, "calling exposure_mode->expose() for Expose_CCD");
    this->exposure_mode = std::make_unique<Expose_CCD>(this);
    if (this->exposure_mode) this->exposure_mode->expose();

    // initialize the exposure mode to Expose_RXRV and call that expose
    //
    logwrite(function, "calling exposure_mode->expose() for Expose_RXRV");
    this->exposure_mode = std::make_unique<Expose_RXRV>(this);
    if (this->exposure_mode) this->exposure_mode->expose();

    if (!this->exposure_mode) {
      logwrite(function, "ERROR exposure mode undefined!");
      return ERROR;
    }

    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::test ****************************************/
}
