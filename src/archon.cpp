/**
 * @file    archon.cpp
 * @brief   common interface functions
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include "archon.h"

#include <sstream>   // for std::stringstream
#include <iomanip>   // for setfil, setw, etc.
#include <iostream>  // for hex, uppercase, etc.
#include <algorithm> 
#include <cctype>
#include <string>
#include <fstream>

namespace Archon {

  // Archon::Interface constructor
  //
  Interface::Interface() {
    this->archon_busy = false;
    this->modeselected = false;
    this->firmwareloaded = false;
    this->msgref = 0;
    this->lastframe = 0;
    this->frame.index = 0;
    this->abort = false;
    this->taplines = 0;
    this->image_data = NULL;
    this->image_data_bytes = 0;
    this->image_data_allocated = 0;

    // pre-size the modtype and modversion vectors to hold the max number of modules
    //
    this->modtype.resize( nmods );
    this->modversion.resize( nmods );

    // TODO I should change these to STL maps instead
    //
    this->frame.bufsample.resize( Archon::nbufs );
    this->frame.bufcomplete.resize( Archon::nbufs );
    this->frame.bufmode.resize( Archon::nbufs );
    this->frame.bufbase.resize( Archon::nbufs );
    this->frame.bufframen.resize( Archon::nbufs );
    this->frame.bufwidth.resize( Archon::nbufs );
    this->frame.bufheight.resize( Archon::nbufs );
    this->frame.bufpixels.resize( Archon::nbufs );
    this->frame.buflines.resize( Archon::nbufs );
    this->frame.bufrawblocks.resize( Archon::nbufs );
    this->frame.bufrawlines.resize( Archon::nbufs );
    this->frame.bufrawoffset.resize( Archon::nbufs );
    this->frame.buftimestamp.resize( Archon::nbufs );
    this->frame.bufretimestamp.resize( Archon::nbufs );
    this->frame.buffetimestamp.resize( Archon::nbufs );
  }

  // Archon::Interface deconstructor
  //
  Interface::~Interface() {
  }


  /**************** Archon::Interface::interface ******************************/
  long Interface::interface(std::string &iface) {
    std::string function = "Archon::Interface::interface";
    iface = "STA-Archon";
    logwrite(function, iface);
    return(0);
  }
  /**************** Archon::Interface::interface ******************************/


  /**************** Archon::Interface::configure_controller *******************/
  /**
   * @fn     configure_controller
   * @brief  get controller-specific values out of the configuration file
   * @param  none
   * @return NO_ERROR if successful or ERROR on error
   *
   */
  long Interface::configure_controller() {
    std::string function = "Archon::Interface::configure_controller";
    std::stringstream message;
    int applied=0;
    long error;

    // loop through the entries in the configuration file, stored in config class
    //
    for (int entry=0; entry < this->config.n_entries; entry++) {

      if (config.param[entry].compare(0, 9, "ARCHON_IP")==0) {
        this->camera_info.hostname = config.arg[entry];
        this->archon.sethost( config.arg[entry] );
        applied++;
      }

      if (config.param[entry].compare(0, 11, "ARCHON_PORT")==0) {
        int port;
        try {
          port = std::stoi( config.arg[entry] );
        }
        catch (std::invalid_argument &) {
          logwrite(function, "ERROR: unable to convert port number to integer");
          return(ERROR);
        }
        catch (std::out_of_range &) {
          logwrite(function, "ERROR: port number out of integer range");
          return(ERROR);
        }
        this->camera_info.port = port;
        this->archon.setport(port);
        applied++;
      }

      if (config.param[entry].compare(0, 12, "EXPOSE_PARAM")==0) {
        this->exposeparam = config.arg[entry];
        applied++;
      }

      // .firmware and .readout_time are STL maps but (for now) only one Archon per computer
      // so map always to 0
      //
      if (config.param[entry].compare(0, 16, "DEFAULT_FIRMWARE")==0) {
        this->common.firmware[0] = config.arg[entry];
        applied++;
      }

      if (config.param[entry].compare(0, 12, "READOUT_TIME")==0) {
        int readtime;
        try {
          readtime = std::stoi ( config.arg[entry] );
        }
        catch (std::invalid_argument &) {
          logwrite(function, "ERROR: unable to convert readout time to integer");
          return(ERROR);
        }
        catch (std::out_of_range &) {
          logwrite(function, "ERROR: readout time out of integer range");
          return(ERROR);
        }
        this->common.readout_time[0] = readtime;
        applied++;
      }

      if (config.param[entry].compare(0, 5, "IMDIR")==0) {
        this->common.imdir( config.arg[entry] );
        applied++;
      }

      if (config.param[entry].compare(0, 8, "BASENAME")==0) {
        this->common.basename( config.arg[entry] );
        applied++;
      }

    }

    message.str("");
    if (applied==0) {
      message << "ERROR: ";
      error = ERROR;
    }
    else {
      error = NO_ERROR;
    }
    message << "applied " << applied << " configuration lines to controller";
    logwrite(function, message.str());
    return error;
  }
  /**************** Archon::Interface::configure_controller *******************/


  /**************** Archon::Interface::prepare_image_buffer *******************/
  /**
   * @fn     prepare_image_buffer
   * @brief  prepare image_data buffer, allocating memory as needed
   * @param  none
   * @return NO_ERROR if successful or ERROR on error
   *
   */
  long Interface::prepare_image_buffer() {
    std::string function = "Archon::Interface::prepare_image_buffer";
    std::stringstream message;

    // If there is already a correctly-sized buffer allocated,
    // then don't do anything except initialize that space to zero.
    //
    if ( (this->image_data != NULL)     &&
         (this->image_data_bytes != 0) &&
         (this->image_data_allocated == this->image_data_bytes) ) {
      memset(this->image_data, 0, this->image_data_bytes);
      message.str(""); message << "initialized " << this->image_data_bytes << " bytes of image_data memory";
      logwrite(function, message.str());
    }

    // If memory needs to be re-allocated, delete the old buffer
    //
    else {
      if (this->image_data != NULL) {
        logwrite(function, "deleting old image_data buffer");
        delete [] this->image_data;
        this->image_data=NULL;
      }
      // Allocate new memory
      //
      if (this->image_data_bytes != 0) {
        this->image_data = new char[this->image_data_bytes];
        this->image_data_allocated=this->image_data_bytes;
        message.str(""); message << "allocated " << this->image_data_bytes << " bytes for image_data";
        logwrite(function, message.str());
      }
      else {
        logwrite(function, "cannot allocate zero-length image memory");
        return(ERROR);
      }
    }

    return(NO_ERROR);
  }
  /**************** Archon::Interface::prepare_image_buffer *******************/


  /**************** Archon::Interface::connect_controller *********************/
  /**
   * @fn     connect_controller
   * @brief
   * @param  none (devices_in here for future expansion)
   * @return 
   *
   */
  long Interface::connect_controller(std::string devices_in="") {
    std::string function = "Archon::Interface::connect_controller";
    std::stringstream message;
    int adchans=0;
    long   error = ERROR;

    if ( this->archon.isconnected() ) {
      logwrite(function, "camera connection already open");
      return(NO_ERROR);
    }

    // Initialize the camera connection
    //
    logwrite(function, "opening a connection to the camera system");

    if ( this->archon.Connect() != 0 ) {
      message.str(""); message << "ERROR: " << errno << " connecting to " << this->camera_info.hostname << ":" << this->camera_info.port;
      logwrite(function, message.str());
      return(ERROR);
    }

    message.str("");
    message << "socket connection to " << this->camera_info.hostname << ":" << this->camera_info.port << " "
            << "established on fd " << this->archon.getfd();;
    logwrite(function, message.str());

    // Get the current system information for the installed modules
    //
    std::string reply;
    error = this->archon_cmd( SYSTEM, reply );        // first the whole reply in one string

    std::vector<std::string> lines, tokens;
    Tokenize( reply, lines, " " );                    // then each line in a separate token "lines"

    for ( auto line : lines ) {
      Tokenize( line, tokens, "_=" );                 // finally break each line into tokens to get module, type and version
      if ( tokens.size() != 3 ) continue;             // need 3 tokens

      std::string version="";
      int module=0;
      int type=0;

      // get the module number
      //
      if ( tokens[0].compare( 0, 9, "BACKPLANE" ) == 0 ) {
        if ( tokens[1] == "VERSION" ) this->backplaneversion = tokens[2];
        continue;
      }

      // get the module and type of each module from MODn_TYPE
      //
      if ( ( tokens[0].compare( 0, 3, "MOD" ) == 0 ) && ( tokens[1] == "TYPE" ) ) {
        try {
          module = std::stoi( tokens[0].substr(3) );
          type   = std::stoi( tokens[2] );
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "ERROR: unable to convert module or type from " << tokens[0] << "=" << tokens[1] << " to integer";
          logwrite(function, message.str());
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "ERROR: module " << tokens[0].substr(3) << " or type " << tokens[1] << " out of range";
          logwrite(function, message.str());
          return(ERROR);
        }
      }
      else continue;

      // get the module version
      //
      if ( tokens[1] == "VERSION" ) version = tokens[2]; else version = "";

      // now store it permanently
      //
      if ( (module > 0) && (module <= nmods) ) {
        try {
          this->modtype.at(module-1)    = type;       // store the type in a vector indexed by module
          this->modversion.at(module-1) = version;    // store the type in a vector indexed by module
        }
        catch (std::out_of_range &) {
          message.str(""); message << "ERROR: requested module " << module << " out of range {1:" << nmods;
          logwrite(function, message.str());
        }
      }
      else {                                          // else should never happen
        message.str(""); message << "ERROR: module " << module << " outside range {1:" << nmods << "}";
        logwrite(function, message.str());
        return(ERROR);
      }

      // Use the module type to resize the gain and offset vectors,
      // but always use the largest possible value allowed.
      //
      if ( type ==  2 ) adchans = ( adchans < MAXADCCHANS ? MAXADCCHANS : adchans );  // ADC module (type=2) found
      if ( type == 17 ) adchans = ( adchans < MAXADMCHANS ? MAXADMCHANS : adchans );  // ADM module (type=17) found
      this->gain.resize( adchans );
      this->offset.resize( adchans );

      // Check that the AD modules are installed in the correct slot
      //
      if ( ( type == 2 || type == 17 ) && ( module < 5 || module > 8 ) ) {
        message.str(""); message << "ERROR: AD module (type=" << type << ") cannot be in slot " << module << ". Use slots 5-8";
        logwrite( function, message.str() );
        return( ERROR );
      }

    } // end for ( auto line : lines )

    // empty the Archon log
    //
    error = this->fetchlog();

    return(error);
  }
  /**************** Archon::Interface::connect_controller *********************/


  /**************** Archon::Interface::disconnect_controller ******************/
  /**
   * @fn     disconnect_controller
   * @brief
   * @param  none
   * @return 
   *
   */
  long Interface::disconnect_controller() {
    std::string function = "Archon::Interface::disconnect_controller";
    long error;
    if (!this->archon.isconnected()) {
      logwrite(function, "connection already closed");
      return (NO_ERROR);
    }
    // close the socket file descriptor to the Archon controller
    //
    error = this->archon.Close();

    // Free the memory
    //
    if (this->image_data != NULL) {
      logwrite(function, "releasing allocated device memory");
      delete [] this->image_data;
      this->image_data=NULL;
    }

    // On success, write the value to the log and return
    //
    if (error == NO_ERROR) {
      logwrite(function, "Archon connection terminated");
    }
    // Throw an error for any other errors
    //
    else {
      logwrite(function, "ERROR: disconnecting Archon camera");
    }

    return(error);
  }
  /**************** Archon::Interface::disconnect_controller ******************/


  /**************** Archon::Interface::native *********************************/
  /**
   * @fn     native
   * @brief  send native commands directly to Archon and log result
   * @param  std::string cmd
   * @return long ret from archon_cmd() call
   *
   * This function simply calls archon_cmd() then breaks the reply into
   * space-delimited tokens and puts each token into the asynchronous
   * message queue. The result is that the reply comes out one line at
   * a time on the async port.
   *
   */
  long Interface::native(std::string cmd) {
    std::string function = "Archon::Interface::native";
    std::stringstream message;
    std::string reply;
    std::vector<std::string> tokens;
    long ret = archon_cmd(cmd, reply);
    if (!reply.empty()) {
      // Tokenize the reply and put each non-empty token into the asynchronous message queue.
      // The reply message begins and ends with "CMD:BEGIN" and "CMD:END" and
      // each line of the reply is prepended with "CMD:" where CMD is the native command
      // which generated the message.
      //
      message << cmd << ":BEGIN";
      this->common.message.enqueue( message.str() );

      Tokenize(reply, tokens, " ");
      for (long unsigned int tok=0; tok < tokens.size(); tok++) {
        if ( ! tokens[tok].empty() && tokens[tok] != "\n" ) {
          message.str(""); message << cmd << ":" << tokens[tok];
          this->common.message.enqueue( message.str() );
        }
      }
      message.str(""); message << cmd << ":END";
      this->common.message.enqueue( message.str() );
    }
    return( ret );
  }
  /**************** Archon::Interface::native *********************************/


  /**************** Archon::Interface::archon_cmd *****************************/
  /**
   * @fn     archon_cmd
   * @brief  send a command to Archon
   * @param  cmd
   * @param  reply (optional)
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::archon_cmd(std::string cmd) { // use this form when the calling
    std::string reply;                          // function doesn't need to look at the reply
    return( archon_cmd(cmd, reply) );
  }
  long Interface::archon_cmd(std::string cmd, std::string &reply) {
    std::string function = "Archon::Interface::archon_cmd";
    std::stringstream message;
    int     retval;
    char    check[4];
    char    buffer[4096];                       //!< temporary buffer for holding Archon replies
    int     error = NO_ERROR;

    if (!this->archon.isconnected()) {          // nothing to do if no connection open to controller
      logwrite(function, "ERROR: connection not open to controller");
      return(ERROR);
    }

    if (this->archon_busy) {                    // only one command at a time
      message.str(""); message << "Archon busy: ignored command " << cmd;
      logwrite(function, message.str());
      return(BUSY);
    }

    /**
     * Hold a scoped lock for the duration of this function, 
     * to prevent multiple threads from accessing the Archon.
     */
    const std::lock_guard<std::mutex> lock(this->archon_mutex);
    this->archon_busy = true;

    // build command: ">xxCOMMAND\n" where xx=hex msgref and COMMAND=command
    //
    this->msgref = (this->msgref + 1) % 256;       // increment msgref for each new command sent
    std::stringstream ssprefix;
    ssprefix << ">"
             << std::setfill('0')
             << std::setw(2)
             << std::hex
             << this->msgref;
    std::string prefix=ssprefix.str();
    try {
      std::transform( prefix.begin(), prefix.end(), prefix.begin(), ::toupper );    // make uppercase
    }
    catch (...) {
      logwrite(function, "ERROR: converting command to uppercase");
      return(ERROR);
    }

    std::stringstream  sscmd;         // sscmd = stringstream, building command
    sscmd << prefix << cmd << "\n";
    std::string scmd = sscmd.str();   // scmd = string, command to send

    // build the command checksum: msgref used to check that reply matches command
    //
    SNPRINTF(check, "<%02X", this->msgref);

    // log the command as long as it's not a STATUS, TIMER, WCONFIG or FRAME command
    //
    if ( (cmd.compare(0,7,"WCONFIG") != 0) &&
         (cmd.compare(0,5,"TIMER") != 0)   &&
         (cmd.compare(0,6,"STATUS") != 0)  &&
         (cmd.compare(0,5,"FRAME") != 0) ) {
      // erase newline for logging purposes
      std::string fcmd = scmd; try { fcmd.erase(fcmd.find("\n"), 1); } catch(...) { }
      message.str(""); message << "sending command: " << fcmd;
      logwrite(function, message.str());
    }

    // send the command
    //
    if ( (this->archon.Write(scmd)) == -1) {
      logwrite(function, "ERROR: writing to camera socket");
    }

    // For the FETCH command we don't wait for a reply, but return immediately.
    // FETCH results in a binary response which is handled elsewhere (in read_frame).
    // Must also distinguish this from the FETCHLOG command, for which we do wait
    // for a normal reply.
    //
    // The scoped mutex lock will be released automatically upon return.
    //
    if ( (cmd.compare(0,5,"FETCH")==0) && (cmd.compare(0,8,"FETCHLOG")!=0) ) return (NO_ERROR);

    // For all other commands, receive the reply
    //
    reply.clear();                                   // zero reply buffer
    do {
      if ( (retval=this->archon.Poll()) <= 0) {
        if (retval==0) { logwrite(function, "Poll timeout"); error = TIMEOUT; }
        if (retval<0)  { logwrite(function, "Poll error");   error = ERROR;   }
        break;
      }
      memset(buffer, '\0', 2048);                    // init temporary buffer
      retval = this->archon.Read(buffer, 2048);      // read into temp buffer
      if (retval <= 0) {
        logwrite(function, "ERROR: reading Archon");
        break; 
      }
      reply.append(buffer);                          // append read buffer into the reply string
    } while(retval>0 && reply.find("\n") == std::string::npos);

    // The first three bytes of the reply should contain the msgref of the
    // command, which can be used as a check that the received reply belongs
    // to the command which was sent.
    //
    // Error processing command (no other information is provided by Archon)
    //
    if (reply.compare(0, 1, "?")==0) {               // "?" means Archon experienced an error processing command
      error = ERROR;
      message.str(""); message << "ERROR: Archon controller returned error processing command: " << cmd;
      logwrite(function, message.str());
    }
    else
    if (reply.compare(0, 3, check)!=0) {             // First 3 bytes of reply must equal checksum else reply doesn't belong to command
      error = ERROR;
      std::string hdr = reply;
      try { scmd.erase(scmd.find("\n"), 1); } catch(...) { }
      message.str(""); message << "ERROR: command-reply mismatch for command: " << scmd;
      logwrite(function, message.str());
      message.str(""); message << "ERROR: expected " << check << " but received " << reply;
      logwrite(function, message.str());
    }
    else {                                           // command and reply are a matched pair
      error = NO_ERROR;

      // log the command as long as it's not a STATUS, TIMER, WCONFIG or FRAME command
      if ( (cmd.compare(0,7,"WCONFIG") != 0) &&
           (cmd.compare(0,5,"TIMER") != 0)   &&
           (cmd.compare(0,6,"STATUS") != 0)  &&
           (cmd.compare(0,5,"FRAME") != 0) ) {
        message.str(""); message << "command 0x" << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << this->msgref << " success";
        logwrite(function, message.str());
      }

      reply.erase(0, 3);                             // strip off the msgref from the reply
    }

    // clear the semaphore (still had the mutex this entire function)
    //
    this->archon_busy = false;

    return(error);
  }
  /**************** Archon::Interface::archon_cmd *****************************/


  /**************** Archon::Interface::read_parameter *************************/
  /**
   * @fn     read_parameter
   * @brief  read a parameter from Archon configuration memory
   * @param  paramname  char pointer to name of paramter
   * @param  value  reference to string for return value
   * @return ERROR on error, NO_ERROR if okay.
   *
   * The string reference contains the value of the parameter
   * to be returned to the user.
   *
   * No direct calls to Archon -- this function uses archon_cmd()
   * which in turn handles all of the Archon in-use locking.
   *
   */
  long Interface::read_parameter(std::string paramname, std::string &value) {
    std::string function = "Archon::Interface::read_parameter";
    std::stringstream message;
    std::stringstream cmd;
    std::string reply;
    int   error   = NO_ERROR;

    if (this->parammap.find(paramname.c_str()) == this->parammap.end()) {
      message.str(""); message << "ERROR: parameter \"" << paramname << "\" not found";
      logwrite(function, message.str());
      return(ERROR);
    }

    // form the RCONFIG command to send to Archon
    //
    cmd.str("");
    cmd << "RCONFIG"
        << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
        << this->parammap[paramname.c_str()].line;
    error = this->archon_cmd(cmd.str(), reply);               // send RCONFIG command here
    try { reply.erase(reply.find("\n"), 1); } catch(...) { }  // strip newline

    // reply should now be of the form PARAMETERn=PARAMNAME=VALUE
    // and we want just the VALUE here
    //

    unsigned int loc;
    value = reply;
    if (value.compare(0, 9, "PARAMETER") == 0) {                                      // value: PARAMETERn=PARAMNAME=VALUE
      if ( (loc=value.find("=")) != std::string::npos ) value = value.substr(++loc);  // value: PARAMNAME=VALUE
      else {
        value="NaN";
        error = ERROR;
      }
      if ( (loc=value.find("=")) != std::string::npos ) value = value.substr(++loc);  // value: VALUE
      else {
        value="NaN";
        error = ERROR;
      }
    }
    else {
      value="NaN";
      error = ERROR;
    }

    if (error==ERROR) {
      message << "ERROR:  malformed reply: " << reply;
      logwrite(function, message.str());
    }
    else {
      message.str(""); message << paramname << " = " << value;
      logwrite(function, message.str());
    }
    return(error);
  }
  /**************** Archon::Interface::read_parameter *************************/


  /**************** Archon::Interface::prep_parameter *************************/
  /**
   * @fn     prep_parameter
   * @brief  
   * @param  
   * @return NO_ERROR or ERROR,  return value from archon_cmd call
   *
   */
  long Interface::prep_parameter(std::string paramname, std::string value) {
    std::string function = "Archon::Interface::prep_parameter";
    std::stringstream message;
    std::stringstream scmd;
    int error = NO_ERROR;

    // Prepare to apply it to the system -- will be loaded on next EXTLOAD signal
    //
    scmd << "FASTPREPPARAM " << paramname << " " << value;
    if (error == NO_ERROR) error = this->archon_cmd(scmd.str());

    if (error != NO_ERROR) {
      message.str(""); message << "ERROR: writing parameter \"" << paramname << "=" << value << "\" to configuration memory";
      logwrite(function, message.str());
    }
    else {
      message.str(""); message << "parameter: " << paramname << " written to configuration memory";
      logwrite(function, message.str());
    }

    return(error);
  }
  /**************** Archon::Interface::prep_parameter *************************/


  /**************** Archon::Interface::load_parameter *************************/
  /**
   * @fn     load_parameter
   * @brief  
   * @param  
   * @return NO_ERROR or ERROR,  return value from archon_cmd call
   *
   */
  long Interface::load_parameter(std::string paramname, std::string value) {
    std::string function = "Archon::Interface::load_parameter";
    std::stringstream message;
    std::stringstream scmd;
    int error = NO_ERROR;

    scmd << "FASTLOADPARAM " << paramname << " " << value;

    if (error == NO_ERROR) error = this->archon_cmd(scmd.str());
    if (error != NO_ERROR) {
      message.str(""); message << "ERROR: loading parameter \"" << paramname << "=" << value << "\" into Archon";
      logwrite(function, message.str());
    }
    else {
      message.str(""); message << "parameter \"" << paramname << "=" << value << "\" loaded into Archon";
      logwrite(function, message.str());
    }
    return(error);
  }
  /**************** Archon::Interface::load_parameter *************************/


  /**************** Archon::Interface::fetchlog *******************************/
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
  long Interface::fetchlog() {
    std::string function = "Archon::Interface::fetchlog";
    std::string reply;
    std::stringstream message;
    int  retval;

    // send FETCHLOG command while reply is not (null)
    //
    do {
      if ( (retval=this->archon_cmd(FETCHLOG, reply))!=NO_ERROR ) {          // send command here
        message.str(""); message << "ERROR: " << retval << " calling FETCHLOG";
        logwrite(function, message.str());
        return(retval);
      }
      if (reply != "(null)") {
        try { reply.erase(reply.find("\n"), 1); } catch(...) { }             // strip newline
        logwrite(function, reply);                                           // log reply here
      }
    } while (reply != "(null)");                                             // stop when reply is (null)

    return(retval);
  }
  /**************** Archon::Interface::fetchlog *******************************/


  /**************** Archon::Interface::load_timing ****************************/
  /**
   * @fn     load_timing
   * @brief  loads the ACF file and applies the timing script and parameters only
   * @param  acffile, specified ACF to load
   * @param  retstring, reference to string for return values // TODO not yet implemented
   * @return 
   *
   * This function is overloaded.
   *
   * This function loads the ACF file then sends the LOADTIMING command 
   * which parses and compiles only the timing script and parameters.  
   *
   */
  long Interface::load_timing(std::string acffile, std::string &retstring) {
    return( this->load_timing( acffile ) );
  }
  long Interface::load_timing(std::string acffile) {
    std::string function = "Archon::Interface::load_timing";

    // load the ACF file into configuration memory
    //
    long error = this->load_acf( acffile );

    // parse timing script and parameters and apply them to the system
    //
    if (error == NO_ERROR) error = this->archon_cmd(LOADTIMING);

    return( error );
  }
  /**************** Archon::Interface::load_timing ****************************/


  /**************** Archon::Interface::load_firmware **************************/
  /**
   * @fn     load_firmware
   * @brief  loads the ACF file and applies the complete system configuration
   * @param  none
   * @return 
   *
   * This function is overloaded.
   *
   * This version takes a single argument for the acf file to load.
   *
   * This function loads the ACF file and then sends an APPLYALL which
   * parses and applies the complete system configuration from the 
   * configuration memory just loaded. The detector power will be off.
   *
   */
  long Interface::load_firmware(std::string acffile) {
    // load the ACF file into configuration memory
    //
    long error = this->load_acf( acffile );

    // Parse and apply the complete system configuration from configuration memory.
    // Detector power will be off after this.
    //
    if (error == NO_ERROR) error = this->archon_cmd(APPLYALL);

    return( error );
  }
  /**************** Archon::Interface::load_firmware **************************/
  /**
   * @fn     load_firmware
   * @brief
   * @param  none
   * @return 
   *
   * This function is overloaded.
   *
   * This version is for future compatibility.
   * The multiple-controller version will pass a reference to a return string. //TODO
   *
   */
  long Interface::load_firmware(std::string acffile, std::string &retstring) {
logwrite("load_firmware", "two arg");
    return( this->load_firmware( acffile ) );
  }
  /**************** Archon::Interface::load_firmware **************************/


  /**************** Archon::Interface::load_acf *******************************/
  /**
   * @fn     load_acf
   * @brief  loads the ACF file into configuration memory (no APPLY!)
   * @param  acffile
   * @return ERROR or NO_ERROR
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
  long Interface::load_acf(std::string acffile) {
    std::string function = "Archon::Interface::load_acf";
    std::stringstream message;
    std::fstream filestream;  // I/O stream class
    std::string line;         // the line read from the acffile
    std::string mode;
    std::string keyword, keystring, keyvalue, keytype, keycomment;
    std::stringstream sscmd;
    std::string key, value;

    int      linecount;  // the Archon configuration line number is required for writing back to config memory
    int      error=NO_ERROR;
    bool     parse_config=false;

    // get the acf filename, either passed here or from loaded default
    //
    if ( acffile.empty() ) {
      acffile = this->common.firmware[0];
    }
    else {
      this->common.firmware[0] = acffile;
    }

    // try to open the file
    //
    try {
      filestream.open(acffile, std::ios_base::in);
    }
    catch(...) {
      message << "ERROR: opening acf file " << acffile << ": " << std::strerror(errno);
      logwrite(function, message.str());
      return ERROR;
    }
    if ( ! filestream.is_open() || ! filestream.good() ) {
      message << "ERROR: acf file " << acffile << " not open";
      logwrite(function, message.str());
      return ERROR;
    }

    logwrite(function, acffile);

    // The CPU in Archon is single threaded, so it checks for a network 
    // command, then does some background polling (reading bias voltages etc), 
    // then checks again for a network command.  "POLLOFF" disables this 
    // background checking, so network command responses are very fast.  
    // The downside is that bias voltages, temperatures, etc are not updated 
    // until you give a "POLLON". 
    //
    if (error == NO_ERROR) error = this->archon_cmd(POLLOFF);

    // clear configuration memory for this controller
    //
    if (error == NO_ERROR) error = this->archon_cmd(CLEARCONFIG);

    // Any failure after clearing the configuration memory will mean
    // no firmware is loaded.
    //
    this->firmwareloaded = false;

    modemap.clear();                             // file is open, clear all modes

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
          line.erase(line.find("["), 1);         // erase the opening [ bracket
          line.erase(line.find("]"), 1);         // erase the closing ] bracket
        }
        catch(...) {
          message.str(""); message << "ERROR: malformed mode section: " << savedline << ": expected [MODE_xxxx]";
          logwrite(function, message.str());
	  filestream.close();
          return ERROR;
        }
        if ( ! line.empty() ) {                  // What's remaining should be MODE_xxxxx
          mode = line.substr(5);                 // everything after "MODE_" is the mode name
          std::transform( mode.begin(), mode.end(), mode.begin(), ::toupper );    // make uppercase

          // got a mode, now check if one of this name has already been located
          // and put into the modemap
          //
          if ( this->modemap.find(mode) != this->modemap.end() ) {
            message.str(""); message << "ERROR: duplicate definition of mode: " << mode << ": load aborted";
            logwrite(function, message.str());
	    filestream.close();
            return ERROR;
          }
          else {
            parse_config = true;
            message.str(""); message << "detected mode: " << mode; logwrite(function, message.str());
            this->modemap[mode].rawenable=-1;    // initialize to -1, require it be set somewhere in the ACF
                                                 // this also ensures something is saved in the modemap for this mode
          }
        }
        else {                                   // somehow there's no xxx left after removing "[MODE_" and "]"
          message.str(""); message << "ERROR: malformed mode section: " << savedline << ": expected [MODE_xxxx]";
          logwrite(function, message.str());
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
      //
      try { line.erase( std::remove(line.begin(), line.end(), '"'), line.end() ); } catch(...) { }

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
        line = line.substr(4);                                            // strip off the "ACF:" portion
        std::string key, value;

        try {
          Tokenize(line, tokens, "=");                                    // separate into tokens by "="

          if (tokens.size() == 1) {                                       // KEY=, the VALUE is empty
            key   = tokens[0];
            value = "";
          }
          else
          if (tokens.size() == 2) {                                       // KEY=VALUE
            key   = tokens[0];
            value = tokens[1];
          }
          else {
            message.str(""); message << "ERROR: malformed ACF line: " << savedline << ": expected KEY=VALUE";
            logwrite(function, message.str());
	    filestream.close();
            return ERROR;
          }

          bool keymatch = false;

          // If this key is in the main parammap then store it in the modemap's parammap for this mode
          //
          if (this->parammap.find( key ) != this->parammap.end()) {
            this->modemap[mode].parammap[ key ].name  = key;
            this->modemap[mode].parammap[ key ].value = value;
            keymatch = true;
          }

          // If this key is in the main configmap, then store it in the modemap's configmap for this mode
          //
          if (this->configmap.find( key ) != this->configmap.end()) {
            this->modemap[mode].configmap[ key ].value = value;
            keymatch = true;
          }

          // If this key is neither in the parammap nor in the configmap then return an error
          //
          if ( ! keymatch ) {
            message.str("");
            message << "[MODE_" << mode << "] ACF directive: " << key << "=" << value << " is not a valid parameter or configuration key";
            logwrite(function, message.str());
            filestream.close();
            return ERROR;
          }
        }
        catch ( ... ) {
          message.str(""); message << "ERROR: extracting KEY=VALUE pair from ACF line: " << savedline;
          logwrite(function, message.str());
          filestream.close();
          return ERROR;
        }
      } // end if (line.compare(0,4,"ACF:")==0)

      // The "ARCH:" tag is for internal (Archon_interface) variables
      // using the KEY=VALUE format.
      //
      else
      if (line.compare(0,5,"ARCH:")==0) {
        std::vector<std::string> tokens;
        line = line.substr(5);                                            // strip off the "ARCH:" portion
        Tokenize(line, tokens, "=");                                      // separate into KEY, VALUE tokens
        if (tokens.size() != 2) {
          message.str(""); message << "ERROR: malformed ARCH line: " << savedline << ": expected ARCH:KEY=VALUE";
          logwrite(function, message.str());
	  filestream.close();
          return ERROR;
        }

        if ( tokens[0] == "NUM_CCDS" ) {
          this->modemap[mode].geometry.num_ccds = std::stoi( tokens[1] );
        }
        else
        if ( tokens[0] == "AMPS_PER_CCD_HORI" ) {
          this->modemap[mode].geometry.amps_per_ccd[0] = std::stoi( tokens[1] );
        }
        else
        if ( tokens[0] == "AMPS_PER_CCD_VERT" ) {
          this->modemap[mode].geometry.amps_per_ccd[1] = std::stoi( tokens[1] );
        }
        else {
          message.str(""); message << "ERROR: unrecognized internal parameter specified: "<< tokens[0];
          logwrite(function, message.str());
	  filestream.close();
          return(ERROR);
        }
      } // end if (line.compare(0,5,"ARCH:")==0)

      // the "FITS:" tag is used to write custom keyword entries of the form "FITS:KEYWORD=VALUE/COMMENT"
      //
      else
      if (line.compare(0,5,"FITS:")==0) {
        std::vector<std::string> tokens;
        line = line.substr(5);                                            // strip off the "FITS:" portion

        // First, tokenize on the equal sign "=".
        // The token left of "=" is the keyword. Immediate right is the value
        Tokenize(line, tokens, "=");
        if (tokens.size() != 2) {                                         // need at least two tokens at this point
          message.str(""); message << "ERROR: malformed FITS command: " << savedline << ": expected KEYWORD=value/comment";
          logwrite(function, message.str());
          filestream.close();
          return(ERROR);
        }
        keyword   = tokens[0].substr(0,8);                                // truncate keyword to 8 characters
        keystring = tokens[1];                                            // tokenize the rest in a moment
        keycomment = "";                                                  // initialize comment, assumed empty unless specified below

        // Next, tokenize on the slash "/".
        // The token left of "/" is the value. Anything to the right is a comment.
        //
        Tokenize(keystring, tokens, "/");

        if (tokens.size() == 0) {      // no tokens found means no "/" characeter which means no comment
          keyvalue = keystring;        // therefore the keyvalue is the entire string
        }

        if (tokens.size() > 0) {       // at least one token
          keyvalue = tokens[0];
        }

        if (tokens.size() == 2) {      // If there are two tokens here then the second is a comment
          keycomment = tokens[1];
        }

        if (tokens.size() > 2) {       // everything below this has been covered
          message.str(""); message << "ERROR: malformed FITS command: " << savedline << ": expected KEYWORD=VALUE/COMMENT";
          logwrite(function, message.str());
          message.str(""); message << "ERROR: too many \"/\" in comment string? " << keystring;
          logwrite(function, message.str());
          filestream.close();
          return(ERROR);
        }

        // Save all of the user keyword information in a map for later
        //
        this->modemap[mode].acfkeys.keydb[keyword].keyword    = keyword;
        this->modemap[mode].acfkeys.keydb[keyword].keytype    = this->camera_info.userkeys.get_keytype(keyvalue);
        this->modemap[mode].acfkeys.keydb[keyword].keyvalue   = keyvalue;
        this->modemap[mode].acfkeys.keydb[keyword].keycomment = keycomment;
      } // end if (line.compare(0,5,"FITS:")==0)

      //
      // ----- all done looking for "TAGS:" -----
      //

      // If this is a PARAMETERn=ParameterName=value KEY=VALUE pair...
      //
      else
      if ( (line.compare(0,11,"PARAMETERS=")!=0) &&   // not the "PARAMETERS=xx line
           (line.compare(0, 9,"PARAMETER"  )==0) ) {  // but must start with "PARAMETER"

        std::vector<std::string> tokens;
        Tokenize(line, tokens, "=");                  // separate into PARAMETERn, ParameterName, value tokens

        if (tokens.size() != 3) {
          message.str(""); message << "ERROR: malformed paramter line: " << savedline << ": expected PARAMETERn=Param=value";;
          logwrite(function, message.str());
          filestream.close();
          return ERROR;
        }

        // Tokenize broke everything up at the "=" and
        // we need all three parts but we also need a version containing the last
        // two parts together, "ParameterName=value" so re-assemble them here.
        //
        std::stringstream paramnamevalue;
        paramnamevalue << tokens[1] << "=" << tokens[2];             // reassemble ParameterName=value string

        // build an STL map "configmap" indexed on PARAMETERn, the part before the first "=" sign
        //
        this->configmap[ tokens[0] ].line  = linecount;              // configuration line number
        this->configmap[ tokens[0] ].value = paramnamevalue.str();   // configuration value for PARAMETERn

        // build an STL map "parammap" indexed on ParameterName so that we can lookup by the actual name
        //
        this->parammap[ tokens[1] ].key   = tokens[0];          // PARAMETERn
        this->parammap[ tokens[1] ].name  = tokens[1] ;         // ParameterName
        this->parammap[ tokens[1] ].value = tokens[2];          // value
        this->parammap[ tokens[1] ].line  = linecount;          // line number

        // assemble a KEY=VALUE pair used to form the WCONFIG command
        key   = tokens[0];                                      // PARAMETERn
        value = paramnamevalue.str();                           // ParameterName=value
      } // end If this is a PARAMETERn=ParameterName=value KEY=VALUE pair...

      // ...otherwise, for all other KEY=VALUE pairs, there is only the value and line number
      // to be indexed by the key. Some lines may be equal to blank, e.g. "CONSTANTx=" so that
      // only one token is made
      //
      else {
        std::vector<std::string> tokens;
        // Tokenize will return a size=1 even if there are no delimiters,
        // so work around this by first checking for delimiters
        // before calling Tokenize.
        //
        if (line.find_first_of("=", 0) == std::string::npos) {
          continue;
        }
        Tokenize(line, tokens, "=");                            // separate into KEY, VALUE tokens
        if (tokens.size() == 0) {
          continue;                                             // nothing to do here if no tokens (ie no "=")
        }
        if (tokens.size() > 0 ) {                               // at least one token is the key
          key   = tokens[0];                                    // KEY
          value = "";                                           // VALUE can be empty (e.g. labels not required)
          this->configmap[ tokens[0] ].line  = linecount;
          this->configmap[ tokens[0] ].value = value;     
        }
        if (tokens.size() > 1 ) {                               // if a second token then that's the value
          value = tokens[1];                                    // VALUE (there is a second token)
          this->configmap[ tokens[0] ].value = tokens[1];
        }
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
        if (error == NO_ERROR) error = this->archon_cmd(sscmd.str());
      } // end if ( !key.empty() && !value.empty() )
      linecount++;
    } // end while ( getline(filestream, line) )

    // re-enable background polling
    //
    if (error == NO_ERROR) error = this->archon_cmd(POLLON);

    filestream.close();
    if (error == NO_ERROR) {
      logwrite(function, "loaded Archon config file OK");
    }

    // If there was an Archon error then read the Archon error log
    //
    if (error != NO_ERROR) this->fetchlog();

    this->modeselected = false;           // require that a mode be selected after loading new firmware

    this->firmwareloaded = true;

    return(error);
  }
  /**************** Archon::Interface::load_acf *******************************/


  /**************** Archon::Interface::set_camera_mode ************************/
  /**
   * @fn     set_camera_mode
   * @brief  
   * @param  none
   * @return 
   *
   */
  long Interface::set_camera_mode(std::string mode) {
    std::string function = "Archon::Interface::set_camera_mode";
    std::stringstream message;
    bool configchanged = false;
    bool paramchanged = false;
    long error;

    // No point in trying anything if no firmware has been loaded yet
    //
    if ( ! this->firmwareloaded ) {
      logwrite(function, "ERROR: no firmware loaded");
      return(ERROR);
    }

    std::transform( mode.begin(), mode.end(), mode.begin(), ::toupper );    // make uppercase

    // The requested mode must have been read in the current ACF file
    // and put into the modemap...
    //
    if (this->modemap.find(mode) == this->modemap.end()) {
      message.str(""); message << "ERROR: undefined mode " << mode << " in ACF file " << this->common.firmware[0];
      logwrite(function, message.str());
      return(ERROR);
    }

    // load specific mode settings from .acf and apply to Archon
    //
    if ( load_mode_settings(mode) != NO_ERROR) {
      message.str(""); message << "ERROR: failed to load mode settings for mode: " << mode;
      logwrite(function, message.str());
      return(ERROR);
    }

    // set internal variables based on new .acf values loaded
    //
    error = NO_ERROR;
    if (error==NO_ERROR) error = get_configmap_value("LINECOUNT", this->modemap[mode].geometry.linecount);
    if (error==NO_ERROR) error = get_configmap_value("PIXELCOUNT", this->modemap[mode].geometry.pixelcount);
    if (error==NO_ERROR) error = get_configmap_value("RAWENABLE", this->modemap[mode].rawenable);
    if (error==NO_ERROR) error = get_configmap_value("RAWSEL", this->rawinfo.adchan);
    if (error==NO_ERROR) error = get_configmap_value("RAWSAMPLES", this->rawinfo.rawsamples);
    if (error==NO_ERROR) error = get_configmap_value("RAWENDLINE", this->rawinfo.rawlines);
#ifdef LOGLEVEL_DEBUG
    message.str(""); 
    message << "[DEBUG] mode=" << mode << " RAWENABLE=" << this->modemap[mode].rawenable 
            << " RAWSAMPLES=" << this->rawinfo.rawsamples << " RAWLINES=" << this->rawinfo.rawlines;
    logwrite(function, message.str());
#endif

    int num_ccds = this->modemap[mode].geometry.num_ccds;                 // for convenience

    // set current number of Archon buffers and resize local memory
    //
    int bigbuf=-1;
    if (error==NO_ERROR) error = get_configmap_value("BIGBUF", bigbuf);   // get value of BIGBUF from loaded acf file
    this->camera_info.activebufs = (bigbuf==1) ? 2 : 3;                   // set number of active buffers based on BIGBUF

    // There is one special reserved mode name, "RAW"
    //
    if (mode=="RAW") {
      this->camera_info.detector_pixels[0] = this->rawinfo.rawsamples;
      this->camera_info.detector_pixels[1] = this->rawinfo.rawlines; 
      this->camera_info.detector_pixels[1]++;
      // frame_type will determine the bits per pixel and where the detector_axes come from
      this->camera_info.frame_type = Common::FRAME_RAW;
      this->camera_info.region_of_interest[0] = 1;
      this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
      this->camera_info.region_of_interest[2] = 1;
      this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];
      // Binning factor (no binning)
      this->camera_info.binning[0] = 1;
      this->camera_info.binning[1] = 1;
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[0] (RAWSAMPLES) = " << this->camera_info.detector_pixels[0];
      logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[1] (RAWENDLINE) = " << this->camera_info.detector_pixels[1];
      logwrite(function, message.str());
#endif
    }

    // Any other mode falls under here
    //
    else {
      if (error==NO_ERROR) error = get_configmap_value("PIXELCOUNT", this->camera_info.detector_pixels[0]);
      if (error==NO_ERROR) error = get_configmap_value("LINECOUNT", this->camera_info.detector_pixels[1]);
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] mode=" << mode; logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[0] (PIXELCOUNT) = " << this->camera_info.detector_pixels[0]
                               << " amps_per_ccd[0] = " << this->modemap[mode].geometry.amps_per_ccd[0];
      logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[1] (LINECOUNT) = " << this->camera_info.detector_pixels[1]
                               << " amps_per_ccd[1] = " << this->modemap[mode].geometry.amps_per_ccd[1];
      logwrite(function, message.str());
#endif
      this->camera_info.detector_pixels[0] *= this->modemap[mode].geometry.amps_per_ccd[0];
      this->camera_info.detector_pixels[1] *= this->modemap[mode].geometry.amps_per_ccd[1];
      this->camera_info.frame_type = Common::FRAME_IMAGE;
      // ROI is the full detector
      this->camera_info.region_of_interest[0] = 1;
      this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
      this->camera_info.region_of_interest[2] = 1;
      this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];
      // Binning factor (no binning)
      this->camera_info.binning[0] = 1;
      this->camera_info.binning[1] = 1;
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[0] (PIXELCOUNT) = " << this->camera_info.detector_pixels[0];
      logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[1] (LINECOUNT) = " << this->camera_info.detector_pixels[1];
      logwrite(function, message.str());
#endif
    }

    // set bitpix based on SAMPLEMODE
    //
    int samplemode=-1;
    if (error==NO_ERROR) error = get_configmap_value("SAMPLEMODE", samplemode); // SAMPLEMODE=0 for 16bpp, =1 for 32bpp
    if (samplemode < 0) {
      message.str(""); message << "ERROR: bad or missing SAMPLEMODE from " << this->common.firmware[0];
      logwrite(function, message.str());
      return (ERROR);
    }
    this->camera_info.bitpix = (samplemode==0) ? 16 : 32;

    // Load parameters and Apply CDS/Deint configuration if any of them changed
    //
    if ((error == NO_ERROR) && paramchanged)  error = this->archon_cmd(LOADPARAMS);
    if ((error == NO_ERROR) && configchanged) error = this->archon_cmd(APPLYCDS);

    // Get the current frame buffer status
    if (error == NO_ERROR) error = this->get_frame_status();
    if (error != NO_ERROR) {
      logwrite(function, "ERROR: unable to get frame status");
      return(error);
    }

    // Set axes, image dimensions, calculate image_memory, etc.
    // Raw will always be 16 bpp (USHORT).
    // Image can be 16 or 32 bpp depending on SAMPLEMODE setting in ACF.
    // Call set_axes(datatype) with the FITS data type needed, which will set the info.datatype variable.
    //
    if (this->camera_info.frame_type == Common::FRAME_RAW) {
      error = this->camera_info.set_axes(USHORT_IMG);                                     // 16 bit raw is unsigned short int
    }
    if (this->camera_info.frame_type == Common::FRAME_IMAGE) {
      if (this->camera_info.bitpix == 16) error = this->camera_info.set_axes(SHORT_IMG);  // 16 bit image is short int
      else
      if (this->camera_info.bitpix == 32) error = this->camera_info.set_axes(FLOAT_IMG);  // 32 bit image is float
      else {
        message.str(""); message << "ERROR: bad bitpix " << this->camera_info.bitpix;
        logwrite(function, message.str());
        return (ERROR);
      }
    }
    if (error != NO_ERROR) {
      logwrite(function, "ERROR: setting axes");
      return (ERROR);
    }

    // allocate image_data in blocks because the controller outputs data in units of blocks
    //
    this->image_data_bytes = (uint32_t) floor( ((this->camera_info.image_memory * num_ccds) + BLOCK_LEN - 1 ) / BLOCK_LEN ) * BLOCK_LEN;

    if (this->image_data_bytes == 0) {
      logwrite(function, "ERROR: image data size is zero! check NUM_CCDS, AMPS_PER_CCD_HORI, AMPS_PER_CCD_VERT in .acf file");
      error = ERROR;
    }

    this->camera_info.current_observing_mode = mode;       // identify the newly selected mode in the camera_info class object
    this->modeselected = true;                             // a valid mode has been selected

    message.str(""); message << "new mode: " << mode << " will use " << this->camera_info.bitpix << " bits per pixel";
    logwrite(function, message.str());

    return(error);
  }
  /**************** Archon::Interface::set_camera_mode ************************/


  /**************** Archon::Interface::load_mode_settings *********************/
  /**
   * @fn     load_mode_settings
   * @brief  load into Archon settings specified in mode section of .acf file
   * @param  camera mode
   * @return none
   *
   * At the end of the .acf file are optional sections for each camera
   * observing mode. These sections can contain any number of configuration
   * lines and parameters to set for the given mode. Those lines are read
   * when the configuration file is loaded. This function writes them to
   * the Archon controller.
   */
  long Interface::load_mode_settings(std::string mode) {
    std::string function = "Archon::Interface::load_mode_settings";
    std::stringstream message;

    long error=NO_ERROR;
    cfg_map_t::iterator   cfg_it;
    param_map_t::iterator param_it;
                     bool paramchanged  = false;
                     bool configchanged = false;

    std::stringstream errstr;

    /**
     * iterate through configmap, writing each config key in the map
     */
    for (cfg_it  = this->modemap[mode].configmap.begin();
         cfg_it != this->modemap[mode].configmap.end();
         cfg_it++) {
      error = this->write_config_key( cfg_it->first.c_str(), cfg_it->second.value.c_str(), configchanged );
      if (error != NO_ERROR) {
        errstr  << "ERROR: writing config key:" << cfg_it->first << " value:" << cfg_it->second.value << " for mode " << mode;
        break;
      }
    }

    /**
     * if no errors from writing config keys, then
     * iterate through the parammap, writing each parameter in the map
     */
    if (error == NO_ERROR) {
      for (param_it  = this->modemap[mode].parammap.begin();
           param_it != this->modemap[mode].parammap.end();
           param_it++) {
        error = this->write_parameter( param_it->first.c_str(), param_it->second.value.c_str(), paramchanged );
        message.str(""); message << "paramchanged=" << (paramchanged?"true":"false");
        logwrite(function, message.str());
        if (error != NO_ERROR) {
          errstr  << "ERROR: writing parameter key:" << param_it->first << " value:" << param_it->second.value << " for mode " << mode;
          break;
        }
      }
    }

    /**
     * apply the new settings to the system here, only if something changed
     */
    if ( (error == NO_ERROR) && paramchanged  ) error = this->archon_cmd(LOADPARAMS);
    if ( (error == NO_ERROR) && configchanged ) error = this->archon_cmd(APPLYCDS);

    if (error == NO_ERROR) {
      message.str(""); message << "loaded mode: " << mode;
      logwrite(function, message.str());
    }
    else {
      logwrite(function, errstr.str());
    }

    /**
     * read back some TAPLINE information
     */
    if (error==NO_ERROR) error = get_configmap_value("TAPLINES", this->taplines); // total number of taps

    std::vector<std::string> tokens;
    std::stringstream        tap;
    std::string              adchan;

    // Loop through every tap to get the offset for each
    //
    for (int i=0; i<this->taplines; i++) {
      tap.str("");
      tap << "TAPLINE" << i;  // used to find the tapline in the configmap

      // The value of TAPLINEn = ADxx,gain,offset --
      // tokenize by comma to separate out each parameter...
      //
      Tokenize(this->configmap[tap.str().c_str()].value, tokens, ",");

      // If all three tokens present (A?xx,gain,offset) then parse it,
      // otherwise it's an unused tap and we can skip it.
      //
      if (tokens.size() == 3) { // defined tap has three tokens
        adchan = tokens[0];     // AD channel is the first (0th) token
        char chars[] = "ADMLR"; // characters to remove in order to get just the AD channel number

        // Before removing these characters, set the max allowed AD number based on the tapline syntax.
        // "ADxx,gain,offset" is for ADC module
        // "AMxx,gain,offset" is for ADM module
        //
        int admax = 0;
        if ( adchan.find("AD") != std::string::npos ) admax = MAXADCCHANS;
        else
        if ( adchan.find("AM") != std::string::npos ) admax = MAXADMCHANS;
        else {
          message.str(""); message << "ERROR: bad tapline syntax. Expected ADn or AMn but got " << adchan;
          return( ERROR );
        }

        // remove AD, AM, L, R from the adchan string, to get just the AD channel number
        //
        for (unsigned int j = 0; j < strlen(chars); j++) {
          adchan.erase(std::remove(adchan.begin(), adchan.end(), chars[j]), adchan.end());
        }

        // AD# in TAPLINE is 1-based (numbered 1,2,3,...)
        // but convert here to 0-based (numbered 0,1,2,...) and check value before using
        //
        int adnum;
        try {
          adnum = std::stoi(adchan) - 1;
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "ERROR: unable to convert AD number \'" << adchan << "\' to integer";
          logwrite(function, message.str());
          return(ERROR);
        }
        catch (std::out_of_range &) {
          logwrite(function, "AD number out of integer range");
          return(ERROR);
        }
        if ( (adnum < 0) || (adnum > admax) ) {
          message.str(""); message << "ERROR: ADC channel " << adnum << " outside range {0:" << admax << "}";
          logwrite(function, message.str());
          return(ERROR);
        }
        // Now that adnum is OK, convert next two tokens to gain, offset
        //
        try {
          this->gain.at(adnum)   = std::stoi(tokens[1]);    // gain as function of AD channel
          this->offset.at(adnum) = std::stoi(tokens[2]);    // offset as function of AD channel
        }
        catch (std::invalid_argument &) {
          logwrite(function, "ERROR: unable to convert GAIN and/or OFFSET to integer");
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "ERROR: GAIN " << tokens[1] << ", OFFSET " << tokens[2] << " or AD# " << adnum << " outside range";
          logwrite(function, message.str());
          return(ERROR);
        }
      }
    }

    return(error);
  }
  /**************** Archon::Interface::load_mode_settings *********************/


  /**************** Archon::Interface::get_frame_status ***********************/
  /**
   * @fn     get_frame_status
   * @brief  get the current frame buffer status
   * @param  none
   * @return 
   *
   * Sends the "FRAME" command to Archon, reads back the reply, then parses the
   * reply and stores parameters into the framestatus structure 
   * (of type frame_data_t).
   *
   */
  long Interface::get_frame_status() {
    std::string function = "Archon::Interface::get_frame_status";
    std::string reply;
    std::stringstream message;
    int   newestframe, newestbuf;
    int   error=NO_ERROR;

    // send FRAME command to get frame buffer status
    //
    if ( (error = this->archon_cmd(FRAME, reply)) ) {
      logwrite(function, "ERROR: sending FRAME command");
      return(error);
    }

    // First Tokenize breaks the single, continuous string into vector of individual strings,
    // from "TIMER=xxxx RBUF=xxxx " to:
    //   tokens[0] : TIMER=xxxx
    //   tokens[1] : RBUF=xxxx
    //   tokens[2] : etc.
    //
    std::vector<std::string> tokens;
    Tokenize(reply, tokens, " ");

    for (size_t i=0; i<tokens.size(); i++) {

      // Second Tokenize separates the paramater from the value
      //
      std::vector<std::string> subtokens;
      subtokens.clear();
      Tokenize(tokens[i], subtokens, "=");

      // Each entry in the FRAME message must have two tokens, one for each side of the "=" equal sign
      // (in other words there must be two subtokens per token)
      //
      if (subtokens.size() != 2) {
        message.str("");
        message << "ERROR: invalid number of tokens (" << subtokens.size() << ") in FRAME message:";
        for (size_t i=0; i<subtokens.size(); i++) message << " " << subtokens[i];
        logwrite(function, message.str());
        return(ERROR);  // We could continue; but if one is bad then we could miss seeing a larger problem
      }

      int bufnum=0;
      int value=0;
      uint64_t lvalue=0;

      if (subtokens[0]=="TIMER") this->frame.timer = subtokens[1];  // timer is a string
      else {                                                        // everything else is going to be a number
        try {                                                       // use "try..catch" to catch exceptions converting strings to numbers
          if (subtokens[0].compare(0, 3, "BUF")==0) {               // for all "BUFnSOMETHING=VALUE" we want the bufnum "n"
            bufnum = std::stoi( subtokens[0].substr(3, 1) );        // extract the "n" here which is 1-based (1,2,3)
          }
          if (subtokens[0].substr(4)=="BASE" ) {                    // for "BUFnBASE=xxx" the value is uint64
            lvalue  = std::stol( subtokens[1] );                    // this value will get assigned to the corresponding parameter
          }
          else
          if (subtokens[0].find("TIMESTAMP")!=std::string::npos) {  // for any "xxxTIMESTAMPxxx" the value is uint64
            lvalue  = std::stol( subtokens[1] );                    // this value will get assigned to the corresponding parameter
          }
          else                                                      // everything else is an int
            value  = std::stoi( subtokens[1] );                     // this value will get assigned to the corresponding parameter
        }
        catch (std::invalid_argument &) {
          logwrite(function, "ERROR: unable to convert buffer or value to integer");
          return(ERROR);
        }
        catch (std::out_of_range &) {
          logwrite(function, "buffer or value out of integer range");
          return(ERROR);
        }
      }
      if (subtokens[0]=="RBUF")  this->frame.rbuf  = value;
      if (subtokens[0]=="WBUF")  this->frame.wbuf  = value;

      // The next group are BUFnSOMETHING=VALUE
      // Extract the "n" which must be a number from 1 to Archon::nbufs
      // After getting the buffer number we assign the corresponding value.
      //
      if (subtokens[0].compare(0, 3, "BUF")==0) {
        if (bufnum < 1 || bufnum > Archon::nbufs) {
          message.str(""); message << "ERROR: buffer number " << bufnum << " outside range {1:" << Archon::nbufs << "}";
          logwrite(function, message.str());
          return(ERROR);
        }
        bufnum--;   // subtract 1 because it is 1-based in the message but need 0-based for the indexing
        if (subtokens[0].substr(4) == "SAMPLE")      this->frame.bufsample[bufnum]      =  value;
        if (subtokens[0].substr(4) == "COMPLETE")    this->frame.bufcomplete[bufnum]    =  value;
        if (subtokens[0].substr(4) == "MODE")        this->frame.bufmode[bufnum]        =  value;
        if (subtokens[0].substr(4) == "BASE")        this->frame.bufbase[bufnum]        = lvalue;
        if (subtokens[0].substr(4) == "FRAME")       this->frame.bufframen[bufnum]      =  value;
        if (subtokens[0].substr(4) == "WIDTH")       this->frame.bufwidth[bufnum]       =  value;
        if (subtokens[0].substr(4) == "HEIGHT")      this->frame.bufheight[bufnum]      =  value;
        if (subtokens[0].substr(4) == "PIXELS")      this->frame.bufpixels[bufnum]      =  value;
        if (subtokens[0].substr(4) == "LINES")       this->frame.buflines[bufnum]       =  value;
        if (subtokens[0].substr(4) == "RAWBLOCKS")   this->frame.bufrawblocks[bufnum]   =  value;
        if (subtokens[0].substr(4) == "RAWLINES")    this->frame.bufrawlines[bufnum]    =  value;
        if (subtokens[0].substr(4) == "RAWOFFSET")   this->frame.bufrawoffset[bufnum]   =  value;
        if (subtokens[0].substr(4) == "TIMESTAMP")   this->frame.buftimestamp[bufnum]   = lvalue;
        if (subtokens[0].substr(4) == "RETIMESTAMP") this->frame.bufretimestamp[bufnum] = lvalue;
        if (subtokens[0].substr(4) == "FETIMESTAMP") this->frame.buffetimestamp[bufnum] = lvalue;
      }
    }

    newestbuf   = this->frame.index;

    if (this->frame.index < (int)this->frame.bufframen.size()) {
      newestframe = this->frame.bufframen[this->frame.index];
    }
    else {
      message.str(""); message << "ERROR: index " << this->frame.index << " exceeds number of buffers " << this->frame.bufframen.size();
      logwrite(function, message.str());
      return(ERROR);
    }

    // loop through the number of buffers
    //
    int num_zero = 0;
    for (int bc=0; bc<Archon::nbufs; bc++) {

      // look for special start-up case, when all frame buffers are zero
      //
      if ( this->frame.bufframen[bc] == 0 ) num_zero++;

      if ( (this->frame.bufframen[bc] > newestframe) &&
            this->frame.bufcomplete[bc] ) {
        newestframe = this->frame.bufframen[bc];
        newestbuf   = bc;
      }
    }

    // start-up case, all frame buffers are zero
    //
    if (num_zero == Archon::nbufs) {
      newestframe = 0;
      newestbuf   = 0;
    }

    /**
     * save index of newest buffer. From this we can find the newest frame, etc.
     */
    this->frame.index = newestbuf;
    this->frame.frame = newestframe;

    return(error);
  }
  /**************** Archon::Interface::get_frame_status ***********************/


  /**************** Archon::Interface::print_frame_status *********************/
  /**
   * @fn     print_frame_status
   * @brief  print the Archon frame buffer status
   * @param  none
   * @return none
   *
   * Writes the Archon frame buffer status to the log file,
   * I.E. information in this->frame structure, obtained from
   * the "FRAME" command. See Archon::Interface::get_frame_status()
   *
   */
  long Interface::print_frame_status() {
    std::string function = "Archon::Interface::print_frame_status";
    std::stringstream message;
    int bufn;
    int error = NO_ERROR;
    char statestr[Archon::nbufs][4];

    // write as log message
    //
    message.str(""); message << "    buf base       rawoff     frame ready lines rawlines rblks width height state";
    logwrite(function, message.str());
    message.str(""); message << "    --- ---------- ---------- ----- ----- ----- -------- ----- ----- ------ -----";
    logwrite(function, message.str());
    message.str("");
    for (bufn=0; bufn < Archon::nbufs; bufn++) {
      memset(statestr[bufn], '\0', 4);
      if ( (this->frame.rbuf-1) == bufn)   strcat(statestr[bufn], "R");
      if ( (this->frame.wbuf-1) == bufn)   strcat(statestr[bufn], "W");
      if ( this->frame.bufcomplete[bufn] ) strcat(statestr[bufn], "C");
    }
    for (bufn=0; bufn < Archon::nbufs; bufn++) {
      message << std::setw(3) << (bufn==this->frame.index ? "-->" : "") << " ";                       // buf
      message << std::setw(3) << bufn+1 << " ";
      message << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex
              << this->frame.bufbase[bufn] << " ";                                                    // base
      message << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex
              << this->frame.bufrawoffset[bufn] << " ";                                               // rawoff
      message << std::setfill(' ') << std::setw(5) << std::dec << this->frame.bufframen[bufn] << " "; // frame
      message << std::setw(5) << this->frame.bufcomplete[bufn] << " ";                                // ready
      message << std::setw(5) << this->frame.buflines[bufn] << " ";                                   // lines
      message << std::setw(8) << this->frame.bufrawlines[bufn] << " ";                                // rawlines
      message << std::setw(5) << this->frame.bufrawblocks[bufn] << " ";                               // rblks
      message << std::setw(5) << this->frame.bufwidth[bufn] << " ";                                   // width
      message << std::setw(6) << this->frame.bufheight[bufn] << " ";                                  // height
      message << std::setw(5) << statestr[bufn];                                                      // state
      logwrite(function, message.str());
      message.str("");
    }
    return(error);
  }
  /**************** Archon::Interface::print_frame_status *********************/


  /**************** Archon::Interface::lock_buffer ****************************/
  /**
   * @fn     lock_buffer
   * @brief  lock Archon frame buffer
   * @param  int frame buffer to lock
   * @return 
   *
   */
  long Interface::lock_buffer(int buffer) {
    std::string function = "Archon::Interface::lock_buffer";
    std::stringstream message;
    std::stringstream sscmd;

    sscmd.str("");
    sscmd << "LOCK" << buffer;
    if ( this->archon_cmd(sscmd.str()) ) {
      message.str(""); message << "ERROR: locking frame buffer " << buffer;
      logwrite(function, message.str());
      return(ERROR);
    }
    return (NO_ERROR);
  }
  /**************** Archon::Interface::lock_buffer ****************************/


  /**************** Archon::Interface::get_timer ******************************/
  /** 
   * @fn     get_timer
   * @brief  read the 64 bit interal timer from the Archon controller
   * @param  *timer pointer to type unsigned long int
   * @return errno on error, 0 if okay.
   *
   * Sends the "TIMER" command to Archon, reads back the reply, and stores the
   * value as (unsigned long int) in the argument variable pointed to by *timer.
   *
   * This is an internal 64 bit timer/counter. One tick of the counter is 10 ns.
   *
   */
  long Interface::get_timer(unsigned long int *timer) {
    std::string function = "Archon::Interface::get_timer";
    std::string reply;
    std::stringstream message, timer_ss;
    std::vector<std::string> tokens;
    int  error;

    // send TIMER command to get frame buffer status
    //
    if ( (error = this->archon_cmd(TIMER, reply)) != NO_ERROR ) {
      return(error);
    }

    Tokenize(reply, tokens, "=");                   // Tokenize the reply

    // Reponse should be "TIMER=xxxx\n" so there needs
    // to be two tokens
    //
    if (tokens.size() != 2) {
      message.str(""); message << "ERROR: unrecognized timer response: " << reply;
      logwrite(function, message.str());
      return(ERROR);
    }

    // Second token must be a hexidecimal string
    //
    std::string timer_str = tokens[1]; 
    try { timer_str.erase(timer_str.find("\n"), 1); } catch(...) { }  // remove newline
    if (!std::all_of(timer_str.begin(), timer_str.end(), ::isxdigit)) {
      message.str(""); message << "ERROR: unrecognized timer value: " << timer_str;
      logwrite(function, message.str());
      return(ERROR);
    }

    // convert from hex string to integer and save return value
    //
    timer_ss << std::hex << tokens[1];
    timer_ss >> *timer;
    return(NO_ERROR);
  }
  /**************** Archon::Interface::get_timer ******************************/


  /**************** Archon::Interface::fetch **********************************/
  /**
   * @fn     fetch
   * @brief  fetch Archon frame buffer
   * @param  int frame buffer to lock
   * @return 
   *
   */
  long Interface::fetch(uint64_t bufaddr, uint32_t bufblocks) {
    std::string function = "Archon::Interface::fetch";
    std::stringstream message;
    uint32_t maxblocks = (uint32_t)(1.5E9 / this->camera_info.activebufs / 1024 );
    uint64_t maxoffset = this->camera_info.activebufs==2 ? 0xD0000000 : 0xE0000000;
    uint64_t maxaddr = maxoffset + maxblocks;

    if (bufaddr < 0xA0000000 || bufaddr > maxaddr) {
      message.str(""); message << "ERROR: requested address 0x" << std::hex << bufaddr << " outside range {0xA0000000:0x" << maxaddr << "}";
      logwrite(function, message.str());
      return(ERROR);
    }
    if (bufblocks > maxblocks) {
      message.str(""); message << "ERROR: requested blocks 0x" << std::hex << bufblocks << " outside range {0:0x" << maxblocks << "}";
      logwrite(function, message.str());
      return(ERROR);
    }

    std::stringstream sscmd;
    sscmd << "FETCH"
          << std::setfill('0') << std::setw(8) << std::hex
          << bufaddr
          << std::setfill('0') << std::setw(8) << std::hex
          << bufblocks;
    std::string scmd = sscmd.str();
    try {
      std::transform( scmd.begin(), scmd.end(), scmd.begin(), ::toupper );  // make uppercase
    }
    catch (...) {
      logwrite(function, "ERROR: converting command to uppercase");
      return(ERROR);
    }

    if (this->archon_cmd(scmd) == ERROR) {
      logwrite(function, "ERROR: sending FETCH command. Aborting read.");
      this->archon_cmd(UNLOCK);                                             // unlock all buffers
      return(ERROR);
    }

    message.str(""); message << "reading " << (this->camera_info.frame_type==Common::FRAME_RAW?"raw":"image") << " with " << scmd;
    logwrite(function, message.str());
    return(NO_ERROR);
  }
  /**************** Archon::Interface::fetch **********************************/


  /**************** Archon::Interface::read_frame *****************************/
  /**
   * @fn     read_frame
   * @brief  read latest Archon frame buffer
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * This is function is overloaded.
   *
   * This version, with no parameter, is the one that is called by the server.
   * The decision is made here if the frame to be read is a RAW or an IMAGE 
   * frame based on this->camera_info.current_observing_mode, then the 
   * overloaded version of read_frame(frame_type) is called with the appropriate 
   * frame type of IMAGE or RAW.
   *
   * This function WILL call write_frame(...) to write data after reading it.
   *
   */
  long Interface::read_frame() {
    std::string function = "Archon::Interface::read_frame";
    std::stringstream message;
    long error = NO_ERROR;

    if ( ! this->modeselected ) {
      logwrite(function, "ERROR: no mode selected");
      return ERROR;
    }

    int rawenable = this->modemap[this->camera_info.current_observing_mode].rawenable;

    if (rawenable == -1) {
      logwrite(function, "ERROR: RAWENABLE is undefined");
      return ERROR;
    }

    // RAW-only
    //
    if (this->camera_info.current_observing_mode == "RAW") {              // "RAW" is the only reserved mode name

      // the RAWENABLE parameter must be set in the ACF file, in order to read RAW data
      //
      if (rawenable==0) {
        logwrite(function, "ERROR: observing mode is RAW but RAWENABLE=0 -- change mode or set RAWENABLE?");
        return ERROR;
      }
      else {
        if (error == NO_ERROR) error = this->read_frame(Common::FRAME_RAW);       // read raw frame
        if (error == NO_ERROR) error = this->write_frame();                       // write raw frame
      }
    }

    // IMAGE, or IMAGE+RAW
    // datacube was already set = true in the expose function
    //
    else {
      if (error == NO_ERROR) error = this->read_frame(Common::FRAME_IMAGE);       // read image frame
      if (error == NO_ERROR) error = this->write_frame();                         // write image frame

      // If mode is not RAW but RAWENABLE=1, then we will first read an image
      // frame (just done above) and then a raw frame (below). To do that we
      // must switch to raw mode then read the raw frame. Afterwards, switch back
      // to the original mode, for any subsequent exposures..
      //
      if (rawenable == 1) {
#ifdef LOGLEVEL_DEBUG
        logwrite(function, "[DEBUG] rawenable is set -- IMAGE+RAW file will be saved");
        logwrite(function, "[DEBUG] switching to mode=RAW");
#endif
        std::string orig_mode = this->camera_info.current_observing_mode; // save the original mode so we can come back to it
        if (error == NO_ERROR) error = this->set_camera_mode("raw");      // switch to raw mode

#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "error=" << error << "[DEBUG] calling read_frame(Common::FRAME_RAW) if error=0"; logwrite(function, message.str());
#endif
        if (error == NO_ERROR) error = this->read_frame(Common::FRAME_RAW);       // read raw frame
/////   message.str(""); message << "[DEBUG] error=" << error << " calling write_raw() if error=0"; logwrite(function, message.str());
/////   if (error == NO_ERROR) error = this->write_raw();                 // write raw frame
#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "error=" << error << "[DEBUG] calling write_frame() for raw data if error=0"; logwrite(function, message.str());
#endif
        if (error == NO_ERROR) error = this->write_frame();                 // write raw frame
#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "error=" << error << "[DEBUG] switching back to original mode if error=0"; logwrite(function, message.str());
#endif
        if (error == NO_ERROR) error = this->set_camera_mode(orig_mode);  // switch back to the original mode
      }
    }

    return error;
  }
  /**************** Archon::Interface::read_frame *****************************/


  /**************** Archon::Interface::read_frame *****************************/
  /**
   * @fn     read_frame
   * @brief  read latest Archon frame buffer
   * @param  frame_type
   * @return ERROR or NO_ERROR
   *
   * This is the overloaded read_frame function which accepts the frame_type argument.
   * This is called only by this->read_frame() to perform the actual read of the
   * selected frame type.
   *
   * No write takes place here!
   *
   */
  long Interface::read_frame(Common::frame_type_t frame_type) {
    std::string function = "Archon::Interface::read_frame";
    std::stringstream message;
    int retval;
    int bufready;
    char check[5], header[5];
    char *ptr_image;
    int bytesread, totalbytesread, toread;
    uint64_t bufaddr;
    unsigned int block, bufblocks=0;
    long error = ERROR;
    int num_ccds = this->modemap[this->camera_info.current_observing_mode].geometry.num_ccds;

    this->camera_info.frame_type = frame_type;

/***
    // Check that image buffer is prepared  //TODO should I call prepare_image_buffer() here, automatically?
    //
    if ( (this->image_data == NULL)    ||
         (this->image_data_bytes == 0) ) {
      logwrite(function, "ERROR: image buffer not ready");
//    return(ERROR);
    }

    if ( this->image_data_allocated != this->image_data_bytes ) {
      message.str(""); message << "ERROR: incorrect image buffer size: " 
                               << this->image_data_allocated << " bytes allocated but " << this->image_data_bytes << " needed";
      logwrite(function, message.str());
//    return(ERROR);
    }
***/

    error = this->prepare_image_buffer();
    if (error == ERROR) {
      logwrite(function, "ERROR: unable to allocate an image buffer");
      return(ERROR);
    }

// TODO removed 2021-Jun-09
// This shouldn't be needed since wait_for_readout() was called previously.
//  // Get the current frame buffer status
//  //
//  error = this->get_frame_status();
//
//  if (error != NO_ERROR) {
//    logwrite(function, "ERROR: unable to get frame status");
//    return(error);
//  }

    // Archon buffer number of the last frame read into memory
    //
    bufready = this->frame.index + 1;

    if (bufready < 1 || bufready > this->camera_info.activebufs) {
      message.str(""); message << "ERROR: invalid buffer " << bufready;
      logwrite(function, message.str());
      return(ERROR);
    }

    message.str(""); message << "will read " << (frame_type == Common::FRAME_RAW ? "raw" : "image")
                             << " data from Archon controller buffer " << bufready << " frame " << this->frame.frame;
    logwrite(function, message.str());

    // Lock the frame buffer before reading it
    //
    if ((error=this->lock_buffer(bufready)) == ERROR) return (error);

    // Send the FETCH command to read the memory buffer from the Archon backplane.
    // Archon replies with one binary response per requested block. Each response
    // has a message header.
    //
    switch (frame_type) {
      case Common::FRAME_RAW:
        // Archon buffer base address
        bufaddr   = this->frame.bufbase[this->frame.index] + this->frame.bufrawoffset[this->frame.index];

        // Calculate the number of blocks expected. image_memory is bytes per CCD
        bufblocks = (unsigned int) floor( (this->camera_info.image_memory + BLOCK_LEN - 1 ) / BLOCK_LEN );
        break;

      case Common::FRAME_IMAGE:
        // Archon buffer base address
        bufaddr   = this->frame.bufbase[this->frame.index];

        // Calculate the number of blocks expected. image_memory is bytes per CCD
        bufblocks =
        (unsigned int) floor( ((this->camera_info.image_memory * num_ccds) + BLOCK_LEN - 1 ) / BLOCK_LEN );
        break;

      default:
        logwrite(function, "ERROR: unknown frame type specified");
        return(ERROR);
        break;
    }

    message.str(""); message << "will read " << std::dec << this->camera_info.image_memory << " bytes "
                             << "0x" << std::uppercase << std::hex << bufblocks << " blocks from bufaddr=0x" << bufaddr;
    logwrite(function, message.str());

    // send the FETCH command.
    // This will take the archon_busy semaphore, but not release it -- must release in this function!
    //
    error = this->fetch(bufaddr, bufblocks);

    // Read the data from the connected socket into memory, one block at a time
    //
    ptr_image = this->image_data;
    totalbytesread = 0;
    std::cerr << "reading bytes: ";
    for (block=0; block<bufblocks; block++) {

      // Are there data to read?
      if ( (retval=this->archon.Poll()) <= 0) {
        if (retval==0) logwrite(function, "Poll timeout");
        if (retval<0)  logwrite(function, "Poll error");
        error = ERROR;
        break;
      }

      // Wait for a block+header Bytes to be available
      // (but don't wait more than 1 second -- this should be tens of microseconds or less)
      //
      auto start = std::chrono::steady_clock::now();             // start a timer now

      while ( this->archon.Bytes_ready() < (BLOCK_LEN+4) ) {
        auto now = std::chrono::steady_clock::now();             // check the time again
        std::chrono::duration<double> diff = now-start;          // calculate the duration
        if (diff.count() > 1) {                                  // break while loop if duration > 1 second
          std::cerr << "\n";
          logwrite(function, "ERROR: timeout waiting for data from Archon");
          error = ERROR;
          break;
        }
      }

      // Check message header
      //
      SNPRINTF(check, "<%02X:", this->msgref);
      if ( (retval=this->archon.Read(header, 4)) != 4 ) {
        message.str(""); message << "ERROR: " << retval << " reading header";
        logwrite(function, message.str());
        error = ERROR;
        break;
      }
      if (header[0] == '?') {  // Archon retured an error
        message.str(""); message << "ERROR: reading " << (frame_type==Common::FRAME_RAW?"raw ":"image ") << " data";
        logwrite(function, message.str());
        this->fetchlog();      // check the Archon log for error messages
        error = ERROR;
        break;
      }
      else if (strncmp(header, check, 4) != 0) {
        message.str(""); message << "Archon command-reply mismatch reading " << (frame_type==Common::FRAME_RAW?"raw ":"image ")
                                 << " data. header=" << header << " check=" << check;
        logwrite(function, message.str());
        error = ERROR;
        break;
      }

      // Read the frame contents
      //
      bytesread = 0;
      do {
        toread = BLOCK_LEN - bytesread;
        if ( (retval=this->archon.Read(ptr_image, (size_t)toread)) > 0 ) {
          bytesread += retval;         // this will get zeroed after each block
          totalbytesread += retval;    // this won't (used only for info purposes)
          std::cerr << std::setw(10) << totalbytesread << "\b\b\b\b\b\b\b\b\b\b";
          ptr_image += retval;         // advance pointer
        }
      } while (bytesread < BLOCK_LEN);

    } // end of loop: for (block=0; block<bufblocks; block++)

    // give back the archon_busy semaphore to allow other threads to access the Archon now
    //
    const std::unique_lock<std::mutex> lock(this->archon_mutex);
    this->archon_busy = false;
    this->archon_mutex.unlock();

    std::cerr << std::setw(10) << totalbytesread << " complete\n";   // display progress on same line of std err

    if (block < bufblocks) {
      message.str(""); message << "ERROR: incomplete frame read " << std::dec 
                               << totalbytesread << " bytes: " << block << " of " << bufblocks << " 1024-byte blocks";
      logwrite(function, message.str());
    }

    // Unlock the frame buffer
    //
    if (error == NO_ERROR) error = this->archon_cmd(UNLOCK);

    // On success, write the value to the log and return
    //
    if (error == NO_ERROR) {
      message.str(""); message << "successfully read " << std::dec << totalbytesread << (frame_type==Common::FRAME_RAW?" raw":" image")
                               << " bytes (0x" << std::uppercase << std::hex << bufblocks << " blocks) from Archon controller";
      logwrite(function, message.str());
    }
    // Throw an error for any other errors
    //
    else {
      logwrite(function, "ERROR: reading Archon camera data to memory!");
    }
    return(error);
  }
  /**************** Archon::Interface::read_frame *****************************/


  /**************** Archon::Interface::write_frame ****************************/
  /**
   * @fn     write_frame
   * @brief  creates a FITS_file object to write the image_data buffer to disk
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * A FITS_file object is created here to write the data. This object MUST remain
   * valid while any (all) threads are writing data, so the write_data function
   * will keep track of threads so that it doesn't terminate until all of its 
   * threads terminate.
   *
   * The camera_info class was copied into fits_info when the exposure was started,  //TODO I've un-done this.
   * so use fits_info from here on out.                                              //TODO Don't use fits_info right now.
   *                                                                                 //TODO Only using camera_info
   */
  long Interface::write_frame() {
    std::string function = "Archon::Interface::write_frame";
    std::stringstream message;
    uint32_t   *cbuf32;                  //!< used to cast char buf into 32 bit int
    uint16_t   *cbuf16;                  //!< used to cast char buf into 16 bit int
    int16_t    *cbuf16s;                 //!< used to cast char buf into 16 bit int
    long        error;

    if ( ! this->modeselected ) {
      logwrite(function, "ERROR: no mode selected");
      return ERROR;
    }

//  message.str(""); message << "writing " << this->fits_info.bitpix << "-bit data from memory to disk";  //TODO
    message.str(""); message << "writing " << this->camera_info.bitpix << "-bit data from memory to disk";
    logwrite(function, message.str());

    // The Archon sends four 8-bit numbers per pixel. To convert this into something usable,
    // cast the image buffer into integers. Handled differently depending on bits per pixel.
    //
    switch (this->camera_info.bitpix) {

      // convert four 8-bit values into a 32-bit value and scale by 2^16
      //
      case 32: {
        cbuf32 = (uint32_t *)this->image_data;                  // cast here to 32b
        float *fbuf = NULL;
//      fbuf = new float[ this->fits_info.image_size ];         // allocate a float buffer of same number of pixels for scaling  //TODO
        fbuf = new float[ this->camera_info.image_size ];       // allocate a float buffer of same number of pixels for scaling

//      for (long pix=0; pix < this->fits_info.image_size; pix++)   //TODO
        for (long pix=0; pix < this->camera_info.image_size; pix++) {
          fbuf[pix] = cbuf32[pix] / (float)65535;               // right shift 16 bits
        }

//      error = fits_file.write_image(fbuf, this->fits_info);   // write the image to disk //TODO
        error = this->fits_file.write_image(fbuf, this->camera_info); // write the image to disk
        if (fbuf != NULL) {
          delete [] fbuf;
        }
        break;
      }

      // convert four 8-bit values into 16 bit values
      //
      case 16: {
        if (this->camera_info.datatype == USHORT_IMG) {                   // raw
          cbuf16 = (uint16_t *)this->image_data;                          // cast to 16b unsigned int
//        error = fits_file.write_image(cbuf16, this->fits_info);         // write the image to disk //TODO
          error = this->fits_file.write_image(cbuf16, this->camera_info); // write the image to disk
        }
        else
        if (this->camera_info.datatype == SHORT_IMG) {
          cbuf16s = (int16_t *)this->image_data;                          // cast to 16b signed int
          int16_t *ibuf = NULL;
          ibuf = new int16_t[ this->camera_info.image_size ];
          for (long pix=0; pix < this->camera_info.image_size; pix++) {
            ibuf[pix] = cbuf16s[pix] - 32768;                             // subtract 2^15 from every pixel
          }
          error = this->fits_file.write_image(ibuf, this->camera_info);   // write the image to disk
          if (ibuf != NULL) { delete [] ibuf; }
        }
        else {
          message.str(""); message << "ERROR: unsupported 16 bit datatype " << this->camera_info.datatype;
          logwrite(function, message.str());
          error = ERROR;
        }
        break;
      }

      // shouldn't happen
      //
      default:
//      message.str(""); message << "ERROR: unrecognized bits per pixel: " << this->fits_info.bitpix; //TODO 
        message.str(""); message << "ERROR: unrecognized bits per pixel: " << this->camera_info.bitpix;
        logwrite(function, message.str());
        error = ERROR;
        break;
    }

    // Things to do after successful write
    //
    if ( error == NO_ERROR ) {
      this->common.increment_imnum();                                 // increment image_num when fitsnaming == "number"
      if (this->common.datacube()) this->camera_info.extension++;     // increment extension for cubes
      logwrite(function, "frame write complete");
    }
    else {
      logwrite(function, "ERROR: writing image");
    }

    return(error);
  }
  /**************** Archon::Interface::write_frame ****************************/


  /**************** Archon::Interface::write_raw ******************************/
  /**
   * @fn     write_raw
   * @brief  write raw 16 bit data to a FITS file
   * @param  none
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::write_raw() {
    std::string function = "Archon::Interface::write_raw";
    std::stringstream message;

    unsigned short *cbuf16;              //!< used to cast char buf into 16 bit int
             int    error = NO_ERROR;

    // Cast the image buffer of chars into integers to convert four 8-bit values 
    // into a 16-bit value
    //
    cbuf16 = (unsigned short *)this->image_data;

    fitsfile *FP       = NULL;
    int       status   = 0;
    int       naxes    = 2;
    long      axes[2];
    long      firstele = 1;
    long      nelements;

    axes[0] = this->camera_info.axes[0];
    axes[1] = this->camera_info.axes[1];

    nelements = axes[0] * axes[1];

    // create fits file
    //
    if (this->camera_info.extension == 0) {
#ifdef LOGLEVEL_DEBUG
      logwrite(function, "[DEBUG] creating fits file with cfitsio");
#endif
      if (fits_create_file( &FP, this->camera_info.fits_name.c_str(), &status ) ) {
        message.str("");
        message << "cfitsio error " << status << " creating FITS file " << this->camera_info.fits_name;
        logwrite(function, message.str());
        error = ERROR;
      }
    }
    else {
#ifdef LOGLEVEL_DEBUG
      logwrite(function, "[DEBUG] opening fits file with cfitsio");
      message.str(""); message << "[DEBUG] file=" << this->camera_info.fits_name << " extension=" << this->camera_info.extension
                               << " bitpix=" << this->camera_info.bitpix;
      logwrite(function, message.str());
#endif
      if (fits_open_file( &FP, this->camera_info.fits_name.c_str(), READWRITE, &status ) ) {
        message.str("");
        message << "cfitsio error " << status << " opening FITS file " << this->camera_info.fits_name;
        logwrite(function, message.str());
        error = ERROR;
      }
    }

    // create image
    //
    logwrite(function, "create image");
    message.str(""); message << "axes=" << axes[0] << " " << axes[1];
    logwrite(function, message.str());
    if ( fits_create_img( FP, USHORT_IMG, naxes, axes, &status) ) {
      message.str("");
      message << "fitsio error " << status << " creating FITS image for " << this->camera_info.fits_name;
      logwrite(function, message.str());
      error = ERROR;
    }

    // supplemental header keywords
    //
    fits_write_key( FP, TSTRING,    "MODE", &this->camera_info.current_observing_mode, "observing mode", &status );

    // write HDU
    //
    logwrite(function, "write HDU");
    if ( fits_write_img( FP, TUSHORT, firstele, nelements, cbuf16, &status) ) {
      message.str("");
      message << "fitsio error " << status << " writing FITS image HDU to " << this->camera_info.fits_name;
      logwrite(function, message.str());
      error = ERROR;
    }

    // close file
    //
    logwrite(function, "close file");
    if ( fits_close_file( FP, &status ) ) {
      message.str("");
      message << "fitsio error " << status << " closing fits file " << this->camera_info.fits_name;
      logwrite(function, message.str());
      error = ERROR;
    }

    return error;
  }
  /**************** Archon::Interface::write_raw ******************************/


  /**************** Archon::Interface::write_config_key ***********************/
  /**
   * @fn     write_config_key
   * @brief  write a configuration KEY=VALUE pair to the Archon controller
   * @param  key
   * @param  newvalue
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::write_config_key( const char *key, const char *newvalue, bool &changed ) {
    std::string function = "Archon::Interface::write_config_key";
    std::stringstream message, sscmd;
    int error=NO_ERROR;

    if ( key==NULL || newvalue==NULL ) {
      error = ERROR;
      logwrite(function, "ERROR: key|value cannot have NULL");
    }

    else

    if ( this->configmap.find(key) == this->configmap.end() ) {
      error = ERROR;
      message.str(""); message << "ERROR: key " << key << " not found in configmap";
      logwrite(function, message.str());
    }

    else

    /**
     * If no change in value then don't send the command
     */
    if ( this->configmap[key].value == newvalue ) {
      error = NO_ERROR;
      message.str(""); message << "config key " << key << "=" << newvalue << " not written: no change in value";
      logwrite(function, message.str());
    }

    else

    /**
     * Format and send the Archon WCONFIG command
     * to write the KEY=VALUE pair to controller memory
     */
    {
      sscmd << "WCONFIG"
            << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
            << this->configmap[key].line
            << key
            << "="
            << newvalue;
      message.str(""); message << "sending: archon_cmd(" << sscmd.str() << ")";
      logwrite(function, message.str());
      error=this->archon_cmd((char *)sscmd.str().c_str());   // send the WCONFIG command here
      if (error==NO_ERROR) {
        this->configmap[key].value = newvalue;               // save newvalue in the STL map
        changed = true;
      }
      else {
        message.str(""); message << "ERROR: config key=value: " << key << "=" << newvalue << " not written";
        logwrite(function, message.str());
      }
    }
    return(error);
  }

  long Interface::write_config_key( const char *key, int newvalue, bool &changed ) {
    std::stringstream newvaluestr;
    newvaluestr << newvalue;
    return ( write_config_key(key, newvaluestr.str().c_str(), changed) );
  }
  /**************** Archon::Interface::write_config_key ***********************/


  /**************** Archon::Interface::write_parameter ************************/
  /**
   * @fn     write_parameter
   * @brief  write a parameter to the Archon controller
   * @param  paramname
   * @param  newvalue
   * @return NO_ERROR or ERROR
   *
   */
  long Interface::write_parameter( const char *paramname, const char *newvalue, bool &changed ) {
    std::string function = "Archon::Interface::write_parameter";
    std::stringstream message, sscmd;
    int error=NO_ERROR;

    if ( paramname==NULL || newvalue==NULL ) {
      error = ERROR;
      logwrite(function, "ERROR: paramname|value cannot have NULL");
    }

    else

    if ( this->parammap.find(paramname) == this->parammap.end() ) {
      error = ERROR;
      message.str(""); message << "ERROR: parameter \"" << paramname << "\" not found in parammap";
      logwrite(function, message.str());
    }

    /**
     * If no change in value then don't send the command
     */
    if ( this->parammap[paramname].value == newvalue ) {
      error = NO_ERROR;
      message.str(""); message << "parameter " << paramname << "=" << newvalue << " not written: no change in value";
      logwrite(function, message.str());
    }

    else

    /**
     * Format and send the Archon command WCONFIGxxxxttt...ttt
     * which writes the text ttt...ttt to configuration line xxx (hex)
     * to controller memory.
     */
    if (error==NO_ERROR) {
      sscmd << "WCONFIG" 
            << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
            << this->parammap[paramname].line
            << this->parammap[paramname].key
            << "="
            << this->parammap[paramname].name
            << "="
            << newvalue;
      message.str(""); message << "sending archon_cmd(" << sscmd.str() << ")";
      logwrite(function, message.str());
      error=this->archon_cmd((char *)sscmd.str().c_str());   // send the WCONFIG command here
      this->parammap[paramname].value = newvalue;            // save newvalue in the STL map
      changed = true;
    } 
    
    return(error);
  } 
  
  long Interface::write_parameter( const char *paramname, int newvalue, bool &changed ) {
    std::stringstream newvaluestr;
    newvaluestr << newvalue;
    return ( write_parameter(paramname, newvaluestr.str().c_str(), changed) );
  }

  long Interface::write_parameter( const char *paramname, const char *newvalue ) {
    bool dontcare = false;
    return( write_parameter(paramname, newvalue, dontcare) );
  }

  long Interface::write_parameter( const char *paramname, int newvalue ) {
    bool dontcare = false;
    std::stringstream newvaluestr;
    newvaluestr << newvalue;
    return ( write_parameter(paramname, newvaluestr.str().c_str(), dontcare) );
  }
  /**************** Archon::Interface::write_parameter ************************/


  /**************** Archon::Interface::get_configmap_value ********************/
  /**
   * @fn     get_configmap_value
   * @brief  get the VALUE from configmap for a givenn KEY and assign to a variable
   * @param  string key_in is the KEY
   * @param  T& value_out reference to variable to contain the VALUE
   * @return ERROR or NO_ERROR
   *
   * This is a template class function so the &value_out reference can be any type.
   * If the key_in KEY is not found then an error message is logged and ERROR is
   * returned, otherwise the VALUE associated with key_in is assigned to &value_out, 
   * and NO_ERROR is returned.
   *
   */
  template <class T>
  long Interface::get_configmap_value(std::string key_in, T& value_out) {
    std::string function = "Archon::Interface::get_configmap_value";
    std::stringstream message;

    if ( this->configmap.find(key_in) != this->configmap.end() ) {
      std::istringstream( this->configmap[key_in].value  ) >> value_out;
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] key=" << key_in << " value=" << value_out << " line=" << this->configmap[key_in].line;
      logwrite(function, message.str());
#endif
      return NO_ERROR;
    }
    else {
      message.str("");
      message << "ERROR: key not found in configmap: " << key_in;
      logwrite(function, message.str());
      return ERROR;
    }
  }
  /**************** Archon::Interface::get_configmap_value ********************/


  /**************** Archon::Interface::expose *********************************/
  /**
   * @fn     expose
   * @brief  initiate an exposure
   * @param  nseq_in string, if set becomes the number of sequences
   * @return ERROR or NO_ERROR
   *
   * This function does the following before returning successful completion:
   *  1) trigger an Archon exposure by setting the EXPOSE parameter = nseq_in
   *  2) wait for exposure delay
   *  3) wait for readout into Archon frame buffer
   *  4) read frame buffer from Archon to host
   *  5) write frame to disk
   *
   * Note that this assumes that the Archon ACF has been programmed to automatically
   * read out the detector into the frame buffer after an exposure.
   *
   */
  long Interface::expose(std::string nseq_in) {
    std::string function = "Archon::Interface::expose";
    std::stringstream message;
    long error;
    std::string nseqstr;
    int nseq;

    std::string mode = this->camera_info.current_observing_mode;            // local copy for convenience

    if ( ! this->modeselected ) {
      logwrite(function, "ERROR: no mode selected");
      return ERROR;
    }

    // exposeparam is set by the configuration file
    // check to make sure it was set, or else expose won't work
    //
    if (this->exposeparam.empty()) {
      message.str(""); message << "ERROR: EXPOSE_PARAM not defined in configuration file " << this->config.filename;
      logwrite(function, message.str());
      return(ERROR);
    }

    // If nseq_in is not supplied then set nseq to 1
    //
    if ( nseq_in.empty() ) {
      nseqstr = "1";
      nseq=1;
    }
    else {                                                          // sequence argument passed in
      try {
        nseq = std::stoi( nseq_in );                                // test that nseq_in is an integer
        nseqstr = nseq_in;                                          // before trying to use it
      }
      catch (std::invalid_argument &) {
        message.str(""); message << "ERROR: unable to convert sequences: " << nseq_in << " to integer";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch (std::out_of_range &) {
        message.str(""); message << "ERROR: sequences " << nseq_in << " outside integer range";
        logwrite(function, message.str());
        return(ERROR);
      }
    }

    // Always initialize the extension number because someone could
    // set datacube true and then send "expose" without a number.
    //
    this->camera_info.extension = 0;

    error = this->get_frame_status();  // TODO is this needed here?

    if (error != NO_ERROR) {
      logwrite(function, "ERROR: unable to get frame status");
      return(ERROR);
    }
    this->lastframe = this->frame.bufframen[this->frame.index];     // save the last frame number acquired (wait_for_readout will need this)

    // initiate the exposure here
    //
    error = this->prep_parameter(this->exposeparam, nseqstr);
    if (error == NO_ERROR) error = this->load_parameter(this->exposeparam, nseqstr);

    // get system time and Archon's timer after exposure starts
    // start_timer is used to determine when the exposure has ended, in wait_for_exposure()
    //
    if (error == NO_ERROR) {
      this->camera_info.start_time = get_system_time();             // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
      error = this->get_timer(&this->start_timer);                  // Archon internal timer (one tick=10 nsec)
      this->common.set_fitstime(this->camera_info.start_time);      // sets common.fitstime (YYYYMMDDHHMMSS) used for filename
      error=this->common.get_fitsname(this->camera_info.fits_name); // assemble the FITS filename
    }

    if (error == NO_ERROR) logwrite(function, "exposure started");

    // Copy the userkeys database object into camera_info
    //
    this->camera_info.userkeys.keydb = this->userkeys.keydb;

    // add any keys from the ACF file (from modemap[mode].acfkeys) into the camera_info.userkeys object
    //
    Common::FitsKeys::fits_key_t::iterator keyit;
    for (keyit  = this->modemap[mode].acfkeys.keydb.begin();
         keyit != this->modemap[mode].acfkeys.keydb.end();
         keyit++) {
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyword    = keyit->second.keyword;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keytype    = keyit->second.keytype;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyvalue   = keyit->second.keyvalue;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keycomment = keyit->second.keycomment;
    }

/***
    // add the internal system keys into the camera_info.userkeys object
    //
    for (keyit  = this->systemkeys.keydb.begin();
         keyit != this->systemkeys.keydb.end();
         keyit++) {
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyword    = keyit->second.keyword;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keytype    = keyit->second.keytype;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyvalue   = keyit->second.keyvalue;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keycomment = keyit->second.keycomment;
    }
***/

    // If mode is not "RAW" but RAWENABLE is set then we're going to require a multi-extension data cube,
    // one extension for the image and a separate extension for raw data.
    //
    if ( (error == NO_ERROR) && (mode != "RAW") && (this->modemap[mode].rawenable) ) {
      if ( !this->common.datacube() ) {                   // if datacube not already set then it must be overridden here
        this->common.message.enqueue( "datacube=TRUE" );  // let everyone know
        logwrite(function, "NOTICE: override datacube=true");
        this->common.datacube(true);
      }
      this->camera_info.extension = 0;
    }

    // Save the datacube state in camera_info so that the FITS writer can know about it
    //
    this->camera_info.iscube = this->common.datacube();

    // Open the FITS file now for cubes
    //
    if ( this->common.datacube() ) error = this->fits_file.open_file(this->camera_info);

//  //TODO only use camera_info -- don't use fits_info -- is this OK? TO BE CONFIRMED
//  this->fits_info = this->camera_info;                            // copy the camera_info class, to be given to fits writer  //TODO

//  this->lastframe = this->frame.bufframen[this->frame.index];     // save the last frame number acquired (wait_for_readout will need this)

    if (nseq > 1) {
      message.str(""); message << "starting sequence of " << nseq << " frames. lastframe=" << this->lastframe;
      logwrite(function, message.str());
    }

    // If not RAW mode then wait for Archon frame buffer to be ready,
    // then read the latest ready frame buffer to the host. If this
    // is a squence, then loop over all expected frames.
    //
    if ( (error == NO_ERROR) && (mode != "RAW") ) {                 // If not raw mode then
      while (nseq-- > 0) {

        // Open a new FITS file for each frame when not using datacubes
        //
        if ( !this->common.datacube() ) {
          this->camera_info.start_time = get_system_time();             // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
          this->get_timer(&this->start_timer);                          // Archon internal timer (one tick=10 nsec)
          this->common.set_fitstime(this->camera_info.start_time);      // sets common.fitstime (YYYYMMDDHHMMSS) used for filename
          error=this->common.get_fitsname(this->camera_info.fits_name); // Assemble the FITS filename
          error = this->fits_file.open_file( this->camera_info );
        }

        if (error==NO_ERROR && this->camera_info.exposure_time != 0) {  // wait for the exposure delay to complete (if there is one)
          error = this->wait_for_exposure();
        }

        if (error==NO_ERROR) error = this->wait_for_readout();      // Wait for the readout into frame buffer,
        if (error==NO_ERROR) error = read_frame();                  // then read the frame buffer to host (and write file) when frame ready.
        if (error==NO_ERROR && !this->common.datacube()) {
          this->fits_file.close_file();                             // close the file when not using datacubes
        }
        if (error != NO_ERROR) break;                               // don't try additional sequences if there were errors
      }
    }
    else if ( (error == NO_ERROR) && (mode == "RAW") ) {
      error = this->get_frame_status();                             // Get the current frame buffer status
      if (error==NO_ERROR) error = read_frame();                    // For raw mode just read immediately
    }

    logwrite( function, (error==ERROR ? "ERROR" : "complete") );

    // for cubes, close the FITS file now that they've all been written
    // (or any time there is an error)
    //
    if ( this->common.datacube() || (error==ERROR) ) this->fits_file.close_file();

    return (error);
  }
  /**************** Archon::Interface::expose *********************************/


  /**************** Archon::Interface::wait_for_exposure **********************/
  /**
   * @fn     wait_for_exposure
   * @brief  creates a wait until the exposure delay has completed
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * This is not the actual exposure delay, nor does it accurately time the exposure
   * delay. This function merely creates a reasonably accurate wait on the host to
   * allow time for the Archon to complete its exposure delay. This is done by using
   * the exposure time given to the Archon and by using the Archon's internal timer,
   * which is queried here. There is no sense in polling the Archon's timer for the
   * entire exposure time, so this function waits internally for about 80% of the
   * exposure time, then only starts polling the Archon for the remaining time.
   *
   * A prediction is made of what the Archon's timer will be at the end, in order
   * to provide an estimate of completion.
   *
   */
  long Interface::wait_for_exposure() {
    std::string function = "Archon::Interface::wait_for_exposure";
    std::stringstream message;
    long error = NO_ERROR;

    int exposure_timeout_time;  // Time to wait for the exposure delay to time out
    unsigned long int timer, increment=0;

    // waittime is an integral number of msec below 80% of the exposure time
    // and will be used to keep track of elapsed time, for timeout errors
    //
    int waittime = (int)floor(0.8 * this->camera_info.exposure_time);  // in msec

    // Wait, (don't sleep) for approx 80% of the exposure time.
    // This is a period that could be aborted by setting the this->abort flag. //TODO not yet implemented?
    //
    double start_time = get_clock_time();
    double now = get_clock_time();

    // prediction is the predicted finish_timer, used to compute exposure time progress
    //
    unsigned long int prediction = this->start_timer + this->camera_info.exposure_time*1e5;

    std::cerr << "exposure progress: ";
    while ( (now - (waittime/1000. + start_time) < 0) && this->abort == false ) {
      timeout(0.010);  // sleep 10 msec = 1e6 Archon ticks
      increment += 1000000;
      now = get_clock_time();
      this->camera_info.exposure_progress = (double)increment / (double)(prediction - this->start_timer);
      if (this->camera_info.exposure_progress < 0 || this->camera_info.exposure_progress > 1) this->camera_info.exposure_progress=1;
      std::cerr << std::setw(3) << (int)(this->camera_info.exposure_progress*100) << "\b\b\b";
    }

    if (this->abort) {
      std::cerr << "\n";
      logwrite(function, "exposure aborted");
      return NO_ERROR;
    }

    // Set the time out value. If the exposure time is less than a second, set
    // the timeout to 1 second. Otherwise, set it to the exposure time plus
    // 1 second.
    //
    if (this->camera_info.exposure_time < 1){
      exposure_timeout_time = 1000; //ms
    }
    else {
      exposure_timeout_time = (this->camera_info.exposure_time) + 1000;
    }

    bool done = false;
    while (done == false && this->abort == false) {
      // Poll Archon's internal timer
      //
      if ( (error=this->get_timer(&timer)) == ERROR ) {
        std::cerr << "\n";
        logwrite(function, "ERROR: getting timer");
        break;
      }

      // update progress
      //
      this->camera_info.exposure_progress = (double)(timer - this->start_timer) / (double)(prediction - this->start_timer);
      if (this->camera_info.exposure_progress < 0 || this->camera_info.exposure_progress > 1) this->camera_info.exposure_progress=1;

      std::cerr << std::setw(3) << (int)(this->camera_info.exposure_progress*100) << "\b\b\b";  // send to stderr in case anyone is watching

      // exposure_time in msec (1e-3s), Archon timer ticks are in 10 nsec (1e-8s)
      // so when comparing timer ticks to exposure time there the difference is a factor of 1e-5
      //
      if ( ((timer - this->start_timer)*1e-5) >= this->camera_info.exposure_time ) {
        this->finish_timer = timer;
        done  = true;
        break;
      }

      timeout( 0.001 );      // a little pause to slow down the requests to Archon

      // Added protection against infinite loops, probably never will be invoked
      // because an Archon error getting the timer would exit the loop.
      // exposure_timeout_time is in msec and it's a little more than 1 msec to get
      // through this loop. If this is decremented each time through then it should
      // never hit zero before the exposure is finished, unless there is a serious
      // problem.
      //
      if (--exposure_timeout_time < 0) {
        logwrite(function, "ERROR: timeout waiting for exposure");
        return ERROR;
      }
    }  // end while (done == false && this->abort == false)

    if (this->abort) {
      std::cerr << "\n";
      logwrite(function, "exposure aborted");
      return NO_ERROR;
    }
    std::cerr << "\n";

    // On success, write the value to the log and return
    //
    if (error == NO_ERROR && this->abort == false){
      return(NO_ERROR);
    }
    // If the wait was stopped, log a message and return NO_ERROR
    //
    else if (this->abort == true) {
      logwrite(function, "ERROR: waiting for exposure stopped by external signal");
      return(NO_ERROR);
    }
    // Throw an error for any other errors
    //
    else {
      logwrite(function, "ERROR: waiting for Archon camera data");
      return(error);
    }
  }
  /**************** Archon::Interface::wait_for_exposure **********************/


  /**************** Archon::Interface::wait_for_readout ***********************/
  /**
   * @fn     wait_for_readout
   * @brief  creates a wait until the next frame buffer is ready
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * This function polls the Archon frame status until a new frame is ready.
   *
   */
  long Interface::wait_for_readout() {
    std::string function = "Archon::Interface::wait_for_readout";
    std::stringstream message;
    long error = NO_ERROR;
    int currentframe=this->lastframe;
    bool done = false;

    message.str("");
    message << "waiting for new frame: lastframe=" << this->lastframe << " frame.index=" << this->frame.index;
    logwrite(function, message.str());

    // waittime is 10% over the specified readout time
    // and will be used to keep track of timeout errors
    //
    double waittime;
    try {
      waittime = this->common.readout_time.at(0) * 1.1;        // this is in msec
    }
    catch(std::out_of_range &) {
      message.str(""); message << "ERROR: readout time for Archon not found from config file";
      logwrite(function, message.str());
      return(ERROR);
    }

    double clock_now     = get_clock_time();                   // get_clock_time returns seconds
    double clock_timeout = clock_now + waittime/1000.;         // must receive frame by this time

    // Poll frame status until current frame is not the last frame and the buffer is ready to read.
    // The last frame was recorded before the readout was triggered in get_frame().
    //
    while (done == false && this->abort == false) {

      error = this->get_frame_status();

      if (error != NO_ERROR) {
        logwrite(function, "unable to get frame status");
        break;
      }
      currentframe = this->frame.bufframen[this->frame.index];

      if ( (currentframe != this->lastframe) && (this->frame.bufcomplete[this->frame.index]==1) ) {
        done  = true;
        error = NO_ERROR;
        break;
      }  // end if ( (currentframe != this->lastframe) && (this->frame.bufcomplete[this->frame.index]==1) )

      // If the frame isn't done by the predicted time then
      // enough time has passed to trigger a timeout error.
      //
      if (clock_now > clock_timeout) {
        done = true;
        error = ERROR;
        message.str(""); message << "ERROR: timeout waiting for new frame. lastframe = " << this->lastframe;
        logwrite( function, message.str() );
      }
      clock_now = get_clock_time();
    } // end while (done == false && this->abort == false)

#ifdef LOGLEVEL_DEBUG
    message.str(""); 
    message << "[DEBUG] lastframe=" << this->lastframe 
            << " currentframe=" << currentframe 
            << " bufcomplete=" << this->frame.bufcomplete[this->frame.index];
    logwrite(function, message.str());
#endif
    this->lastframe = currentframe;

    // On success, write the value to the log and return
    //
    if (error == NO_ERROR && this->abort == false) {
      message.str("");
      message << "received currentframe: " << currentframe;
      logwrite(function, message.str());
      return(NO_ERROR);
    }
    // If the wait was stopped, log a message and return NO_ERROR
    //
    else
    if (this->abort == true) {
      logwrite(function, "wait for readout stopped by external signal");
      return(NO_ERROR);
    }
    // Throw an error for any other errors
    //
    else {
      logwrite(function, "ERROR: waiting for readout");
      return(error);
    }
  }
  /**************** Archon::Interface::wait_for_readout ***********************/


  /**************** Archon::Interface::get_parameter **************************/
  /**
   * @fn     get_parameter
   * @brief  get parameter using read_parameter()
   * @param  string
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::get_parameter(std::string parameter, std::string &retstring) {
    std::string function = "Archon::Interface::get_parameter";

    return this->read_parameter(parameter, retstring);
  }
  /**************** Archon::Interface::get_parameter **************************/


  /**************** Archon::Interface::set_parameter **************************/
  /**
   * @fn     set_parameter
   * @brief  set an Archon parameter
   * @param  string
   * @return ERROR or NO_ERROR
   *
   * This function calls "prep_parameter()" and "load_parameter()"
   *
   */
  long Interface::set_parameter(std::string parameter) {
    std::string function = "Archon::Interface::set_parameter";
    std::stringstream message;
    long ret=ERROR;
    std::vector<std::string> tokens;

    Tokenize(parameter, tokens, " ");

    if (tokens.size() != 2) {
      message.str(""); message << "ERROR: param expected 2 arguments (paramname and value) but got " << tokens.size();
      logwrite(function, message.str());
      ret=ERROR;
    }
    else {
      ret = this->prep_parameter(tokens[0], tokens[1]);
      if (ret == NO_ERROR) ret = this->load_parameter(tokens[0], tokens[1]);
    }
    return(ret);
  }
  /**************** Archon::Interface::set_parameter **************************/


  /**************** Archon::Interface::exptime ********************************/
  /**
   * @fn     exptime
   * @brief  set/get the exposure time
   * @param  string
   * @return ERROR or NO_ERROR
   *
   * This function calls "set_parameter()" and "get_parameter()" using
   * the "exptime" parameter (which must already be defined in the ACF file).
   *
   */
  long Interface::exptime(std::string exptime_in, std::string &retstring) {
    std::string function = "Archon::Interface::exptime";
    std::stringstream message;
    long ret=NO_ERROR;

    if ( !exptime_in.empty() ) {
      std::stringstream cmd;
      cmd << "exptime " << exptime_in;
      ret = this->set_parameter( cmd.str() );
      if (ret != ERROR) {
        try {
          this->camera_info.exposure_time = std::stoi( exptime_in );
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "ERROR: unable to convert exposure time: " << exptime_in << " to integer";
          logwrite(function, message.str());
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "ERROR: exposure time " << exptime_in << " outside integer range";
          logwrite(function, message.str());
          return(ERROR);
        }
      }
    }
    retstring = std::to_string( this->camera_info.exposure_time );
    message.str(""); message << "exposure time is " << retstring << " msec";
    logwrite(function, message.str());
    return(ret);
  }
  /**************** Archon::Interface::exptime ********************************/


  /**************** Archon::Interface::heater *********************************/
  /**
   * @fn     heater
   * @brief  heater control, set/get state, target, PID, ramp
   * @param  args contains various allowable strings (see full descsription)
   * @return ERROR or NO_ERROR
   *
   * valid args format,
   * to set or get the enable state and target for heater A or B on the specified module:
   *   <module> < A | B > [ <on | off> <target> ]
   *   possible args: 2 (get) or 3 or 4
   *
   * to set or get the PID parameters for heater A or B on the specified module:
   *   <module> < A | B > PID [ <p> <i> <d> ]
   *   possible args: 3 (get) or 6 (set)
   *
   * to set or get the ramp and ramprate for heater A or B on the specified module:
   *   <module> < A | B > RAMP [ <on | off> [ramprate] ]
   *   possible args: 3 (get) or 5 (set)
   *
   */
  long Interface::heater(std::string args, std::string &retstring) {
    std::string function = "Archon::Interface::heater";
    std::stringstream message;
    std::vector<std::string> tokens;
    int module;
    std::string heaterid;                   //!< A|B
    bool readonly=false;
    float pid_p, pid_i, pid_d;              //!< requested PID values
    int ramprate;                           //!< requested ramp rate value
    float target;                           //!< requested heater target value
    std::vector<std::string> heaterconfig;  //!< vector of configuration lines to read or write
    std::vector<std::string> heatervalue;   //!< vector of values associated with config lines (for write)

    // must have loaded firmware // TODO implement a command to read the configuration 
    //                           //      memory from Archon, in order to remove this restriction.
    //
    if ( ! this->firmwareloaded ) {
      logwrite( function, "ERROR: firmware not loaded" );
      return( ERROR );
    }

    std::transform( args.begin(), args.end(), args.begin(), ::toupper );  // make uppercase

    // RAMP requires a minimum backplane version
    //
    int ret = compare_versions( this->backplaneversion, REV_RAMP );
    if ( ret < 0 ) {
      if ( ret == -999 ) {
        message << "ERROR: comparing backplane version " << this->backplaneversion << " to " << REV_RAMP;
      }
      else {
        message << "ERROR: requires backplane version " << REV_RAMP << " or newer. ("
                << this->backplaneversion << " detected)";
      }
      logwrite( function, message.str() );
      return( ERROR );
    }

    Tokenize(args, tokens, " ");

    // At minimum there must be two tokens, <module> A|B
    //
    if ( tokens.size() < 2 ) {
      logwrite( function, "ERROR: expected at least two arguments: <module> A|B" );
      return ERROR;
    }

    // As long as there are at least two tokens, get the module and heaterid
    // which will be common to everything.
    //
    try {
      module   = std::stoi( tokens[0] );
      heaterid = tokens[1];
      if ( heaterid != "A" && heaterid != "B" ) {
        message.str(""); message << "ERROR: invalid heater " << heaterid << ": expected <module> A|B";
        logwrite(function, message.str());
        return(ERROR);
      }
    }
    catch (std::invalid_argument &) {
      message.str(""); message << "ERROR: first argument must be <module> but unable to convert " << tokens[0] << " to integer";
      logwrite(function, message.str());
      return(ERROR);
    }
    catch (std::out_of_range &) {
      message.str(""); message << "ERROR: first argument must be <module> but " << tokens[0] << " is outside integer range";
      logwrite(function, message.str());
      return(ERROR);
    }

    // check that requested module is valid
    //
    switch ( this->modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "ERROR: module " << module << " not installed";
        logwrite(function, message.str());
        return(ERROR);
      case 5:  // Heater
      case 11: // HeaterX
        break;
      default:
        message.str(""); message << "ERROR: module " << module << " not a heater board";
        logwrite(function, message.str());
        return(ERROR);
    }

    // Now that we've passed the basic tests (firmware OK, basic syntax OK, and requested module is a heater)
    // go ahead and set up some needed variables.
    //

    // heater target min/max depends on backplane version
    //
    ret = compare_versions( this->backplaneversion, REV_HEATERTARGET );
    if ( ret == -999 ) {
      message.str(""); message << "ERROR: comparing backplane version " << this->backplaneversion << " to " << REV_HEATERTARGET;
      logwrite( function, message.str() );
      return( ERROR );
    }
    else if ( ret == -1 ) {
      this->heater_target_min = -150.0;
      this->heater_target_max =   50.0;
    }
    else {
      this->heater_target_min = -250.0;
      this->heater_target_max =   50.0;
    }

    heaterconfig.clear();
    heatervalue.clear();
    std::stringstream ss;

    // Any one heater command can require reading or writing multiple configuration lines.
    // The code will look at each case and push each heater configuration line that needs
    // to be read or written into a vector (heaterconfig). For those that need to be written,
    // the value to write will be pushed into a separate vector (heatervalue). After these
    // vectors have been built up, the code will loop through each element of the vectors,
    // reading and/or writing each.

    // Start looking at the heater arguments passed, which have been tokenized.
    // There can be only 2, 3, 4, 5, or 6 tokens allowed.

    // If there are exactly two (2) tokens then we have received only:
    // <module> A|B
    // which means we're reading the state and target.
    //
    if ( tokens.size() == 2 ) {        // no args reads ENABLE, TARGET
      readonly = true;
      ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "ENABLE"; heaterconfig.push_back( ss.str() );
      ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "TARGET"; heaterconfig.push_back( ss.str() );
    }
    else
    // If there are three (3) tokens then the 3rd must be one of the following:
    // ON | OFF (for ENABLE), <target>, PID, RAMP
    //
    if ( tokens.size() == 3 ) {
      if ( tokens[2] == "ON" ) {       // ON
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "ENABLE"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( "1" );
      }
      else
      if ( tokens[2] == "OFF" ) {      // OFF
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "ENABLE"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( "0" );
      }
      else
      if ( tokens[2] == "RAMP" ) {     // RAMP
        readonly = true;
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMP";     heaterconfig.push_back( ss.str() );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMPRATE"; heaterconfig.push_back( ss.str() );
      }
      else
      if ( tokens[2] == "PID" ) {      // PID
        readonly = true;
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "P"; heaterconfig.push_back( ss.str() );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "I"; heaterconfig.push_back( ss.str() );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "D"; heaterconfig.push_back( ss.str() );
      }
      else {                           // <target>
        // first check that the requested target is a valid number and within range...
        //
        try {
          target = std::stof( tokens[2] );
         if ( target < this->heater_target_min || target > this->heater_target_max ) {
           message.str(""); message << "ERROR: requested target " << target << " outside range {" 
                                    << this->heater_target_min << ":" << this->heater_target_max << "}";
            logwrite( function, message.str() );
            return( ERROR );
          }
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "ERROR: unable to convert <target>=" << tokens[2] << " to float";
          logwrite(function, message.str());
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "ERROR: <target>=" << tokens[2] << " outside range of float";
          logwrite(function, message.str());
          return(ERROR);
        }
        // ...if target is OK then push into the config, value vectors
        //
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "TARGET"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[2] );
      }
    }
    else
    // If there are four (4) tokens then the 4th must be one of the following:
    // <target> (for enable), <ramprate> | ON | OFF (for RAMP)
    //
    if ( tokens.size() == 4 ) {

      if ( tokens[2] == "ON" ) {       // ON <target>
        // first check that the requested target is a valid number and within range...
        //
        try {
          target = std::stof( tokens[3] );
         if ( target < this->heater_target_min || target > this->heater_target_max ) {
           message.str(""); message << "ERROR: requested target " << target << " outside range {" 
                                    << this->heater_target_min << ":" << this->heater_target_max << "}";
            logwrite( function, message.str() );
            return( ERROR );
          }
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "ERROR: unable to convert <target>=" << tokens[3] << " to float";
          logwrite(function, message.str());
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "ERROR: <target>=" << tokens[3] << " outside range of float";
          logwrite(function, message.str());
          return(ERROR);
        }
        // ...if target is OK then push into the config, value vectors
        //
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "ENABLE"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( "1" );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "TARGET"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[3] );
      }
      else
      if ( tokens[2] == "RAMP" ) {     // RAMP x

        if ( tokens[3] == "ON" || tokens[3] == "OFF" ) {   // RAMP ON|OFF
          ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMP";     heaterconfig.push_back( ss.str() );
          std::string state = ( tokens[3]=="ON" ? "1" : "0" );
          heatervalue.push_back( state );
        }
        else {                                             // RAMP <ramprate>
          try {
            ramprate = std::stoi( tokens[3] );
            if ( ramprate < 1 || ramprate > 32767 ) {
              message.str(""); message << "ERROR: ramprate " << ramprate << " outside range {1:32767}";
              return( ERROR );
            }
          }
          catch (std::invalid_argument &) {
            message.str(""); message << "ERROR: expected RAMP <ramprate> but unable to convert <ramprate>=" << tokens[3] << " to integer";
            logwrite(function, message.str());
            return(ERROR);
          }
          catch (std::out_of_range &) {
            message.str(""); message << "ERROR: expected RAMP <ramprate> but <ramprate>=" << tokens[3] << " outside range of integer";
            logwrite(function, message.str());
            return(ERROR);
          }
          ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMPRATE"; heaterconfig.push_back( ss.str() );
          heatervalue.push_back( tokens[3] );
        }
      }
      else {
        message.str(""); message << "ERROR: expected ON | RAMP for 3rd argument but got " << tokens[2];
        logwrite(function, message.str());
        return( ERROR );
      }
    }
    else
    // If there are five (5) tokens then they must be
    // <module> A|B RAMP ON <ramprate>
    //
    if ( tokens.size() == 5 ) {        // RAMP ON <ramprate>
      if ( tokens[2] != "RAMP" && tokens[3] != "ON" ) {
        message.str(""); message << "ERROR: expected RAMP ON <ramprate> but got"; for (int i=2; i<5; i++) message << " " << tokens[i];
        logwrite(function, message.str());
        return( ERROR );
      }
      else {  // got "<module> A|B RAMP ON" now check that the last (5th) token is a number
        try {
          ramprate = std::stoi( tokens[4] );
          if ( ramprate < 1 || ramprate > 32767 ) {
            message.str(""); message << "ERROR: ramprate " << ramprate << " outside range {1:32767}";
            return( ERROR );
          }
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "ERROR: expected RAMP ON <ramprate> but unable to convert <ramprate>=" << tokens[4] << " to integer";
          logwrite(function, message.str());
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "ERROR: expected RAMP ON <ramprate> but <ramprate>=" << tokens[4] << " outside range of integer";
          logwrite(function, message.str());
          return(ERROR);
        }
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMP";     heaterconfig.push_back( ss.str() );
        heatervalue.push_back( "1" );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMPRATE"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[4] );
      }
    }
    else
    // If there are six (6) tokens then they must be
    // <module> A|B PID <p> <i> <d>
    //
    if ( tokens.size() == 6 ) {
      if ( tokens[2] != "PID" ) {
        message.str(""); message << "ERROR: expected PID <p> <i> <d> but got"; for (int i=2; i<6; i++) message << " " << tokens[i];
        logwrite(function, message.str());
        return( ERROR );
      }
      else {  // got "<module> A|B PID <p> <i> <d>" now check that the last 3 tokens are numbers
        // Fractional PID requires a minimum backplane version
        //
        bool fractionalpid_ok = false;
        ret = compare_versions( this->backplaneversion, REV_FRACTIONALPID );
        if ( ret == -999 ) {
          message.str(""); message << "ERROR: comparing backplane version " << this->backplaneversion << " to " << REV_FRACTIONALPID;
          logwrite( function, message.str() );
          return( ERROR );
        }
        else if ( ret == -1 ) fractionalpid_ok = false;
        else fractionalpid_ok = true;
        try {
          // If using backplane where fractional PID was not allowed, check for decimal points
          //
          if ( (!fractionalpid_ok) &&
               ( ( tokens[3].find(".") != std::string::npos ) ||
                 ( tokens[4].find(".") != std::string::npos ) ||
                 ( tokens[5].find(".") != std::string::npos ) ) ) {
            fesetround(FE_TONEAREST);  // round to value nearest X. halfway cases rounded away from zero
            tokens[3] = std::to_string( std::lrint( std::stof( tokens[3] ) ) ); // replace token with rounded integer
            tokens[4] = std::to_string( std::lrint( std::stof( tokens[4] ) ) ); // replace token with rounded integer
            tokens[5] = std::to_string( std::lrint( std::stof( tokens[5] ) ) ); // replace token with rounded integer

            message.str(""); message << "NOTICE: fractional PID requires backplane version " << REV_FRACTIONALPID << " or newer";
            logwrite( function, message.str() );
            message.str(""); message << "NOTICE: backplane version " << this->backplaneversion << " detected";
            logwrite( function, message.str() );
            message.str(""); message << "NOTICE: PIDs converted to: " << tokens[3] << " " << tokens[4] << " " << tokens[5];
            logwrite( function, message.str() );
          }
          pid_p = std::stof( tokens[3] );
          pid_i = std::stof( tokens[4] );
          pid_d = std::stof( tokens[5] );
          if ( pid_p < 0 || pid_p > 10000 || pid_i < 0 || pid_i > 10000 || pid_d < 0 || pid_d > 10000 ) {
            message.str(""); message << "ERROR: one or more PID values outside range {0:10000}";
            logwrite( function, message.str() );
            return( ERROR );
          }
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "ERROR: expected PID <p> <i> <d> but unable to convert one or more values to numbers:";
          for (int i=3; i<6; i++) message << " " << tokens[i];
          logwrite(function, message.str());
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "ERROR: expected PID <p> <i> <d> but one or more values outside range:";
          for (int i=3; i<6; i++) message << " " << tokens[i];
          logwrite(function, message.str());
          return(ERROR);
        }
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "P"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[3] );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "I"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[4] );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "D"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[5] );
      }
    }
    // Otherwise we have an invalid number of tokens
    //
    else {
      message.str(""); message << "ERROR: received " << tokens.size() << " arguments but expected 2, 3, 4, 5, or 6";
      logwrite(function, message.str());
      return( ERROR );
    }

    long error;

    if ( ! readonly ) {

      // For writing, loop through the heaterconfig and heatervalue vectors.
      // They MUST be the same size! This should be impossible.
      //
      if ( heaterconfig.size() != heatervalue.size() ) {
        message.str("");
        message << "BUG DETECTED: heaterconfig (" << heaterconfig.size() 
                << ") - heatervalue (" << heatervalue.size() << ") vector size mismatch";
        logwrite( function, message.str() );
        return( ERROR );
      }

      // Write the configuration lines
      //
      bool changed = false;
      size_t error_count = 0;
      for ( size_t i=0; i < heaterconfig.size(); i++ ) {
        error = this->write_config_key( heaterconfig[i].c_str(), heatervalue[i].c_str(), changed );
        message.str("");
        if ( error != NO_ERROR ) {
          message << "ERROR; writing configuration " << heaterconfig[i] << "=" << heatervalue[i];
          error_count++;  // this counter will be checked before the APPLYMOD command
        }
        else if ( !changed ) {
          message << "heater configuration: " << heaterconfig[i] << "=" << heatervalue[i] << " unchanged";
        }
        else {
          message << "updated heater configuration: " << heaterconfig[i] << "=" << heatervalue[i];
        }
        logwrite( function, message.str() );
      }

      // send the APPLMODxx command which parses and applies the configuration for module xx
      // The APPLYMOD is sent even if an error occured writing the config key(s) because
      // it's possible that one of the config keys was written.
      //
      // If error_count is the same as the number of configuration lines, then they all failed
      // to write, in which case do not send APPLYMOD. But if even one config key was written
      // then the APPLYMOD command must be sent.
      //
      if ( error_count == heaterconfig.size() ) {
        return( ERROR );
      }

      std::stringstream applystr;
      applystr << "APPLYMOD"
               << std::setfill('0')
               << std::setw(2)
               << std::hex
               << (module-1);

      error = this->archon_cmd( applystr.str() );

      if ( error != NO_ERROR ) {
        logwrite( function, "ERROR: applying heater configuration" );
      }
    }

    // Now read the configuration line(s).
    // For multiple lines, concatenate all values into one space-delimited string.

    // loop through the vector of heaterconfig keys,
    // getting the value for each, and putting them into retss
    //
    std::string value;
    std::stringstream retss;
    for ( auto key : heaterconfig ) {

      error = this->get_configmap_value( key, value );

      if ( error != NO_ERROR ) {
        message << "ERROR: reading heater configuration " << key;
        logwrite( function, message.str() );
        return( error );
      }
      else {
        // If key ends with "ENABLE" or "RAMP"
        // then convert the values 0,1 to OFF,ON, respectively
        //
        if ( key.substr( key.length()-6 ) == "ENABLE" ||
             key.substr( key.length()-4 ) == "RAMP" ) {
          if ( value == "0" ) value = "OFF";
          if ( value == "1" ) value = "ON";
        }
        retss << value << " ";
        message.str(""); message << key << "=" << value;
        logwrite( function, message.str() );
      }
    }
    retstring = retss.str();  // return value to calling function, passed by reference

    return ( error );
  }
  /**************** Archon::Interface::heater *********************************/


  /**************** Archon::Interface::sensor *********************************/
  /**
   * @fn     sensor
   * @brief  
   * @param  args contains various allowable strings (see full descsription)
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::sensor(std::string args, std::string &retstring) {
    std::string function = "Archon::Interface::sensor";
    std::stringstream message;
    std::vector<std::string> tokens;
    std::string sensorid;                   //!< A | B | C
    int module;                             //!< integer module number
    float current;                          //!< requested current
    bool readonly=true;                     //!< true is reading, not writing current
    long error;

    // must have loaded firmware // TODO implement a command to read the configuration 
    //                           //      memory from Archon, in order to remove this restriction.
    //
    if ( ! this->firmwareloaded ) {
      logwrite( function, "ERROR: firmware not loaded" );
      return( ERROR );
    }

    // requires a minimum backplane version
    //
    int ret = compare_versions( this->backplaneversion, REV_SENSORCURRENT );
    if ( ret < 0 ) {
      if ( ret == -999 ) {
        message << "ERROR: comparing backplane version " << this->backplaneversion << " to " << REV_SENSORCURRENT;
      }
      else {
        message << "ERROR: requires backplane version " << REV_SENSORCURRENT << " or newer. ("
                << this->backplaneversion << " detected)";
      }
      logwrite( function, message.str() );
      return( ERROR );
    }

    std::transform( args.begin(), args.end(), args.begin(), ::toupper );  // make uppercase

    Tokenize( args, tokens, " " );

    // At minimum there must be two tokens, <module> <sensorid>
    //
    if ( tokens.size() == 2 ) {
      readonly = true;
    }
    else if ( tokens.size() == 3 ) {
      readonly = false;
    }
    else {
      message.str(""); message << "ERROR: incorrect number of arguments: " << args << ": expected <module#> <A|B|C> [current]";
      logwrite( function, message.str() );
      return( ERROR );
    }

    // Get the module and sensorid
    //
    try {
      module   = std::stoi( tokens.at(0) );
      sensorid = tokens.at(1);

      if ( !readonly ) {
        current = std::stof( tokens.at(2) );
        if ( current > 1600000. ) {
          message.str(""); message << "ERROR: requested current " << current << " exceeds maximum (1600000 nA)";
          logwrite( function, message.str() );
          return( ERROR );
        }
      }

      if ( sensorid != "A" && sensorid != "B" && sensorid != "C" ) {
        message.str(""); message << "ERROR: invalid sensor " << sensorid << ": expected <module#> <A|B|C> [ current ]";
        logwrite( function, message.str() );
        return( ERROR );
      }
    }
    catch ( std::invalid_argument & ) {
      message.str(""); message << "ERROR: invalid argument in " << args << ": expected <module#> <A|B|C> [ current ]";
      logwrite( function, message.str() );
      return( ERROR );
    }
    catch ( std::out_of_range & ) {
      message.str(""); message << "ERROR: outside range in " << args << ": expected <module#> <A|B|C> [ current ]";
      logwrite( function, message.str() );
      return( ERROR );
    }

    // check that the requested module is valid
    //
    switch ( this->modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "ERROR: module " << module << " not installed";
        logwrite( function, message.str() );
        return( ERROR );
      case 5:  // Heater
      case 11: // HeaterX
        break;
      default:
        message.str(""); message << "ERROR: module " << module << " is not a heater board";
        logwrite( function, message.str() );
        return( ERROR );
    }

    std::stringstream sensorconfig;
    sensorconfig << "MOD" << module << "/SENSOR" << sensorid << "CURRENT";

    std::string key   = sensorconfig.str();
    std::string value = std::to_string(current);
    bool changed      = false;

    // If no current was supplied (readonly) then just read the configuration and exit
    //
    if ( readonly ) {
      message.str("");
      error = this->get_configmap_value( key, current );
      if ( error != NO_ERROR ) {
        message << "ERROR: reading current " << key;
      }
      else {
        retstring = std::to_string( current );
        message << "module " << module << " sensor " << sensorid << " current=" << current;
      }
      logwrite( function, message.str() );
      return ( error );
    }

    // Write the config line to update the sensor current
    //
    error = this->write_config_key( key.c_str(), value.c_str(), changed );

    // Now send the APPLYMODx command
    //
    std::stringstream applystr;
    applystr << "APPLYMOD"
             << std::setfill('0')
             << std::setw(2)
             << std::hex
             << (module-1);

    if ( error == NO_ERROR ) error = this->archon_cmd( applystr.str() );

    if ( error != NO_ERROR ) {
      message << "ERROR: writing sensor current configuration: " << key << "=" << value;
    }
    else if ( !changed ) {
      message << "sensor current configuration: " << key << "=" << value << " unchanged";
    }
    else {
      message << "updated sensor current configuration: " << key << "=" << value;
    }

    logwrite( function, message.str() );

    return ( error );
  }
  /**************** Archon::Interface::sensor *********************************/


  /**************** Archon::Interface::bias ***********************************/
  /**
   * @fn     bias
   * @brief  set a bias
   * @param  args contains: module, channel, bias
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::bias(std::string args, std::string &retstring) {
    std::string function = "Archon::Interface::bias";
    std::stringstream message;
    std::vector<std::string> tokens;
    std::stringstream biasconfig;
    int module;
    int channel;
    float voltage;
    float vmin, vmax;
    bool readonly=true;

    // must have loaded firmware
    //
    if ( ! this->firmwareloaded ) {
      logwrite( function, "ERROR: firmware not loaded" );
      return( ERROR );
    }

    Tokenize(args, tokens, " ");

    if (tokens.size() == 2) {
      readonly = true;
    }
    else if (tokens.size() == 3) {
      readonly = false;
    }
    else {
      message.str(""); message << "ERROR: incorrect number of arguments: " << args << ": expected module channel [voltage]";
      logwrite(function, message.str());
      return ERROR;
    }

    std::transform( args.begin(), args.end(), args.begin(), ::toupper );  // make uppercase

    try {
      module  = std::stoi( tokens[0] );
      channel = std::stoi( tokens[1] );
      if (!readonly) voltage = std::stof( tokens[2] );
    }
    catch (std::invalid_argument &) {
      message.str(""); message << "ERROR: unable to convert one or more arguments: " << args;
      logwrite(function, message.str());
      return(ERROR);
    }
    catch (std::out_of_range &) {
      message.str(""); message << "ERROR: one or more arguments outside range: " << args;
      logwrite(function, message.str());
      return(ERROR);
    }

    // Check that the module number is valid
    //
    if ( (module < 0) || (module > nmods) ) {
      message.str(""); message << "ERROR: module " << module << ": outside range {0:" << nmods << "}";
      logwrite(function, message.str());
      return(ERROR);
    }

    // Use the module type to get LV or HV Bias
    // and start building the bias configuration string.
    //
    switch ( this->modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "ERROR: module " << module << " not installed";
        logwrite(function, message.str());
        return(ERROR);
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
        message.str(""); message << "ERROR: module " << module << " not a bias board";
        logwrite(function, message.str());
        return(ERROR);
        break;
    }

    // Check that the channel number is valid
    // and add it to the bias configuration string.
    //
    if ( (channel < 1) || (channel > 30) ) {
      message.str(""); message << "ERROR: channel " << module << ": outside range {1:30}";
      logwrite(function, message.str());
      return(ERROR);
    }
    if ( (channel > 0) && (channel < 25) ) {
      biasconfig << "LC_V" << channel;
    }
    if ( (channel > 24) && (channel < 31) ) {
      channel -= 24;
      biasconfig << "HC_V" << channel;
    }

    if ( (voltage < vmin) || (voltage > vmax) ) {
      message.str(""); message << "ERROR: voltage " << voltage << ": outside range {" << vmin << ":" << vmax << "}";
      logwrite(function, message.str());
      return(ERROR);
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
        message << "ERROR: reading bias " << key;
      }
      else {
        retstring = std::to_string(voltage);
        message << "read bias " << key << "=" << voltage;
      }
      logwrite(function, message.str());
      return error;
    }

    // Write the config line to update the bias voltage
    //
    error = this->write_config_key(key.c_str(), value.c_str(), changed);

    // Now send the APPLYMODx command
    //
    std::stringstream applystr;
    applystr << "APPLYMOD" 
             << std::setfill('0')
             << std::setw(2)
             << std::hex
             << (module-1);

    if (error == NO_ERROR) error = this->archon_cmd(applystr.str());

    if (error != NO_ERROR) {
      message << "ERROR: writing bias configuration: " << key << "=" << value;
    }
    else if (!changed) {
      message << "bias configuration: " << key << "=" << value <<" unchanged";
    }
    else {
      message << "updated bias configuration: " << key << "=" << value;
    }

    logwrite(function, message.str());

    return error;
  }
  /**************** Archon::Interface::bias ***********************************/


  /**************** Archon::Interface::cds ************************************/
  /**
   * @fn     cds
   * @brief  set / get CDS parameters
   * @param  
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::cds(std::string args, std::string &retstring) {
    std::string function = "Archon::Interface::cds";
    std::vector<std::string> tokens;
    std::string key, value;
    bool changed;
    long error = ERROR;

    if ( args.empty() ) {
      logwrite(function, "ERROR: no argument");
      return( ERROR );
    }

    try {
      Tokenize(args, tokens, " ");

      // One token --
      // get the configuration key value
      //
      if ( tokens.size() == 1 ) {
        key   = tokens.at(0);
        std::transform( key.begin(), key.end(), key.begin(), ::toupper );          // make uppercase
        error = this->get_configmap_value(key, retstring);                         // read
      }
      else

      // Two tokens --
      // set the configuration key to value, send APPLYCDS, then read back the config key
      //
      if ( tokens.size() == 2 ) {
        key   = tokens.at(0);
        std::transform( key.begin(), key.end(), key.begin(), ::toupper );          // make uppercase
        value = tokens.at(1);
        error = this->write_config_key( key.c_str(), value.c_str(), changed );     // set
        if (error == NO_ERROR) error = this->archon_cmd(APPLYCDS);                 // apply
        if (error == NO_ERROR) error = this->get_configmap_value(key, retstring);  // read back
      }

      // More than two tokens is an error
      //
      else {
        logwrite(function, "ERROR: Too many arguments. Expected <configkey> [ value ]");
        return( ERROR );
      }
    }
    catch(std::out_of_range &) {
      logwrite(function, "ERROR: tokenizing arguments");
      return( ERROR );
    }
    catch(...) {
      logwrite(function, "ERROR: unknown error processing  arguments");
      return( ERROR );
    }

    return( error );
  }
  /**************** Archon::Interface::cds ************************************/


  /**************** Archon::Interface::test ***********************************/
  /**
   * @fn     test
   * @brief  test routines
   * @param  string args contains test name and arguments
   * @param  reference to retstring for any return values
   * @return ERROR or NO_ERROR
   *
   * This is the place to put various debugging and system testing tools.
   * It is placed here, rather than in common, to allow for controller-
   * specific tests. This means some common tests may need to be duplicated
   * for each controller.
   *
   * These are kept out of server.cpp so that I don't need a separate,
   * potentially conflicting command for each test I come up with, and also
   * reduces the chance of someone accidentally, inadvertently entering one
   * of these test commands.
   *
   * The server command is "test", the next parameter is the test name,
   * and any parameters needed for the particular test are extracted as
   * tokens from the args string passed in.
   *
   * The input args string is tokenized and tests are separated by a simple
   * series of if..else.. conditionals.
   *
   * current tests are:
   *   fitsname  - show what the fitsname will look like
   *   async     - queue an asynchronous status message
   *   modules   - Log all installed modules and their types
   *   parammap  - Log all parammap entries found in the ACF file
   *   configmap - Log all configmap entries found in the ACF file
   *   bw        - tests the exposure sequence bandwidth by running a sequence of exposures
   *   timer     - test Archon time against system time
   */
  long Interface::test(std::string args, std::string &retstring) {
    std::string function = "Archon::Interface::test";
    std::stringstream message;
    std::vector<std::string> tokens;
    long error;

    Tokenize(args, tokens, " ");

    if (tokens.size() < 1) {
      logwrite(function, "no test name provided");
      return ERROR;
    }

    std::string testname = tokens[0];                                // the first token is the test name

    // ----------------------------------------------------
    // fitsname
    // ----------------------------------------------------
    // Show what the fitsname will look like.
    // This is a "test" rather than a regular command so that it doesn't get mistaken
    // for returning a real, usable filename. When using fitsnaming=time, the filename
    // has to be generated at the moment the file is opened.
    //
    if (testname == "fitsname") {
      std::string msg;
      this->common.set_fitstime( get_system_time() );                // must set common.fitstime first
      error = this->common.get_fitsname(msg);                        // get the fitsname (by reference)
      this->common.message.enqueue( msg );                           // queue the fitsname
      logwrite(function, msg);                                       // log the fitsname
    } // end if (testname == fitsname)

    // ----------------------------------------------------
    // async [message]
    // ----------------------------------------------------
    // queue an asynchronous message
    // The [message] param is optional. If not provided then "test" is queued.
    //
    else
    if (testname == "async") {
      if (tokens.size() > 1) {
        if (tokens.size() > 2) {
          logwrite(function, "NOTICE: received multiple strings -- only the first will be queued");
        }
        logwrite( function, tokens[1] );
        this->common.message.enqueue( tokens[1] );
      }
      else {                                // if no string passed then queue a simple test message
        logwrite(function, "test");
        this->common.message.enqueue("test");
      }
      error = NO_ERROR;
    } // end if (testname == async)

    // ----------------------------------------------------
    // modules
    // ----------------------------------------------------
    // Log all installed modules
    //
    else
    if (testname == "modules") {
      logwrite( function, "installed module types: " );
      message.str("");
      for ( auto mod : this->modtype ) {
        message << mod << " ";
      }
      logwrite( function, message.str() );
      retstring = message.str();
      error = NO_ERROR;
    }

    // ----------------------------------------------------
    // parammap
    // ----------------------------------------------------
    // Log all parammap entries found in the ACF file
    //
    else
    if (testname == "parammap") {

      // loop through the modes
      //
      logwrite(function, "parammap entries by mode section:");
      for (auto mode_it = this->modemap.begin(); mode_it != this->modemap.end(); ++mode_it) {
        std::string mode = mode_it->first;
        message.str(""); message << "found mode section " << mode;
        logwrite(function, message.str());
        for (auto param_it = this->modemap[mode].parammap.begin(); param_it != this->modemap[mode].parammap.end(); ++param_it) {
          message.str(""); message << "MODE_" << mode << ": " << param_it->first << "=" << param_it->second.value;
          logwrite(function, message.str());
        }
      }

      logwrite(function, "ALL parammap entries in ACF:");
      int keycount=0;
      for (auto param_it = this->parammap.begin(); param_it != this->parammap.end(); ++param_it) {
        keycount++;
        message.str(""); message << param_it->first << "=" << param_it->second.value;
        logwrite(function, message.str());
      }
      message.str(""); message << "found " << keycount << " parammap entries";
      logwrite(function, message.str());
      error = NO_ERROR;
    } // end if (testname == parammap)

    // ----------------------------------------------------
    // configmap
    // ----------------------------------------------------
    // Log all configmap entries found in the ACF file
    //
    else
    if (testname == "configmap") {
      error = NO_ERROR;
      logwrite(function, "configmap entries by mode section:");
      for (auto mode_it=this->modemap.begin(); mode_it!=this->modemap.end(); ++mode_it) {
        std::string mode = mode_it->first;
        message.str(""); message << "found mode section " << mode;
        logwrite(function, message.str());
        for (auto config_it = this->modemap[mode].configmap.begin(); config_it != this->modemap[mode].configmap.end(); ++config_it) {
          message.str(""); message << "MODE_" << mode << ": " << config_it->first << "=" << config_it->second.value;
          logwrite(function, message.str());
        }
      }

      // if a second argument was passed then this is a config key
      // try to read it
      //
      if ( tokens.size() == 2 ) {
        std::string configkey = tokens[1];
        error = this->get_configmap_value(configkey, retstring);
      }

      // if a third argument was passed then set this configkey
      //
      if ( tokens.size() == 3 ) {
        std::string key = tokens[1];
        std::string value = tokens[2];
        bool configchanged;
        error = this->write_config_key( key.c_str(), value.c_str(), configchanged );
        if (error == NO_ERROR) error = this->archon_cmd(APPLYCDS);
      }

      int keycount=0;
      for (auto config_it = this->configmap.begin(); config_it != this->configmap.end(); ++config_it) {
        keycount++;
      }
      message.str(""); message << "found " << keycount << " configmap entries";
      logwrite(function, message.str());
    } // end if (testname == configmap)

    // ----------------------------------------------------
    // bw <nseq>
    // ----------------------------------------------------
    // Bandwidth test
    // This tests the exposure sequence bandwidth by running a sequence
    // of exposures, including reading the frame buffer -- everything except
    // for the fits file writing.
    //
    else
    if (testname == "bw") {

      if ( ! this->modeselected ) {
        logwrite(function, "ERROR: no mode selected");
        return ERROR;
      }

      std::string nseqstr;
      int nseq;
      bool ro=false;  // read only
      bool rw=false;  // read and write

      if (tokens.size() > 1) {
        nseqstr = tokens[1];
      }
      else {
        logwrite(function, "usage: test bw <nseq> [ rw | ro ]");
        return ERROR;
      }

      if (tokens.size() > 2) {
        if (tokens[2] == "rw") rw=true; else rw=false;
        if (tokens[2] == "ro") ro=true; else ro=false;
      }

      try {
        nseq = std::stoi( nseqstr );                                // test that nseqstr is an integer before trying to use it
      }
      catch (std::invalid_argument &) {
        message.str(""); message << "ERROR: unable to convert sequences: " << nseqstr << " to integer";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch (std::out_of_range &) {
        message.str(""); message << "ERROR: sequences " << nseqstr << " outside integer range";
        logwrite(function, message.str());
        return(ERROR);
      }

      // exposeparam is set by the configuration file
      // check to make sure it was set, or else expose won't work
      //
      if (this->exposeparam.empty()) {
        message.str(""); message << "ERROR: EXPOSE_PARAM not defined in configuration file " << this->config.filename;
        logwrite(function, message.str());
        return(ERROR);
      }
      error = this->get_frame_status();  // TODO is this needed here?

      if (error != NO_ERROR) {
        logwrite(function, "ERROR: unable to get frame status");
        return(ERROR);
      }
      this->lastframe = this->frame.bufframen[this->frame.index];     // save the last frame number acquired (wait_for_readout will need this)

      // initiate the exposure here
      //
      error = this->prep_parameter(this->exposeparam, nseqstr);
      if (error == NO_ERROR) error = this->load_parameter(this->exposeparam, nseqstr);

      // get system time and Archon's timer after exposure starts
      // start_timer is used to determine when the exposure has ended, in wait_for_exposure()
      //
      if (error == NO_ERROR) {
        this->camera_info.start_time = get_system_time();             // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
        error = this->get_timer(&this->start_timer);                  // Archon internal timer (one tick=10 nsec)
        this->common.set_fitstime(this->camera_info.start_time);      // sets common.fitstime (YYYYMMDDHHMMSS) used for filename
        // If read-write selected then need to do some FITS stuff
        //
        if ( rw ) {
          this->camera_info.extension = 0;                            // always initialize extension
          error = this->common.get_fitsname( this->camera_info.fits_name ); // assemble the FITS filename if rw selected
          Common::FitsKeys::fits_key_t::iterator keyit;               // add keys from the ACF file 
          for (keyit  = this->modemap[this->camera_info.current_observing_mode].acfkeys.keydb.begin();
               keyit != this->modemap[this->camera_info.current_observing_mode].acfkeys.keydb.end();
               keyit++) {
            this->camera_info.userkeys.keydb[keyit->second.keyword].keyword    = keyit->second.keyword;
            this->camera_info.userkeys.keydb[keyit->second.keyword].keytype    = keyit->second.keytype;
            this->camera_info.userkeys.keydb[keyit->second.keyword].keyvalue   = keyit->second.keyvalue;
            this->camera_info.userkeys.keydb[keyit->second.keyword].keycomment = keyit->second.keycomment;
          }
          this->camera_info.iscube = this->common.datacube();
          if ( this->common.datacube() ) error = this->fits_file.open_file(this->camera_info);
        }
      }

      if (error == NO_ERROR) logwrite(function, "exposure started");

      long frames_read = 0;

      // Wait for Archon frame buffer to be ready, then read the latest ready frame buffer to the host.
      // Loop over all expected frames.
      //
      while (nseq-- > 0) {

        // If read-write selected,
        // Open a new FITS file for each frame when not using datacubes
        //
        if ( rw && !this->common.datacube() ) {
          this->camera_info.start_time = get_system_time();             // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
          this->get_timer(&this->start_timer);                          // Archon internal timer (one tick=10 nsec)
          this->common.set_fitstime(this->camera_info.start_time);      // sets common.fitstime (YYYYMMDDHHMMSS) used for filename
          error=this->common.get_fitsname(this->camera_info.fits_name); // Assemble the FITS filename
          error = this->fits_file.open_file( this->camera_info );
        }

        if (this->camera_info.exposure_time != 0) {                 // wait for the exposure delay to complete (if there is one)
          error = this->wait_for_exposure();
          if (error==ERROR) {
            logwrite(function, "exposure delay error");
            break;
          }
          else {
            logwrite(function, "exposure delay complete");
          }
        }

        if (error==NO_ERROR) error = this->wait_for_readout();                     // wait for the readout into frame buffer,
        if (error==NO_ERROR && ro) error = this->read_frame(Common::FRAME_IMAGE);  // read image frame directly with no write
        if (error==NO_ERROR && rw) error = this->read_frame();                     // read image frame directly with no write
        if (error==NO_ERROR && rw && !this->common.datacube()) {
          this->fits_file.close_file();
        }
        if (error==NO_ERROR) frames_read++;
      }
      retstring = std::to_string( frames_read );

      // for cubes, close the FITS file now that they've all been written
      // (or any time there is an error)
      //
      if ( rw && ( this->common.datacube() || (error==ERROR) ) ) this->fits_file.close_file();


      logwrite( function, (error==ERROR ? "ERROR" : "complete") );

      message.str(""); message << "frames read = " << frames_read;
      logwrite(function, message.str());

    } // end if (testname==bw)

    // ----------------------------------------------------
    // timer
    // ----------------------------------------------------
    // test Archon time against system time
    //
    else
    if (testname == "timer") {

      int nseq;
      int sleepus;
      double systime1, systime2;
      unsigned long int archontime1, archontime2;
      std::vector<int> deltatime;
      int delta_archon, delta_system;

      if (tokens.size() < 3) {
        logwrite(function, "ERROR: expected test timer <cycles> <sleepus>");
        return ERROR;
      }

      try {
        nseq    = std::stoi( tokens[1] );
        sleepus = std::stoi( tokens[2] );
      }
      catch (std::invalid_argument &) {
        message.str(""); message << "ERROR: unable to convert " << tokens[1] << " or " << tokens[2] << " to an integer";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch (std::out_of_range &) {
        message.str(""); message << "ERROR: " << tokens[1] << " or " << tokens[2] << " outside integer range";
        logwrite(function, message.str());
        return(ERROR);
      }

      error = NO_ERROR;

      // turn off background polling while doing the timing test
      //
      if (error == NO_ERROR) error = this->archon_cmd(POLLOFF);

      // send the Archon TIMER command here, nseq times, with sleepus delay between each
      //
      int nseqsave = nseq;
      while ( error==NO_ERROR && nseq-- > 0 ) {
        // get Archon timer [ns] and system time [s], twice
        //
        error = get_timer(&archontime1);
        systime1 = get_clock_time();
        error = get_timer(&archontime2);
        systime2 = get_clock_time();

        // difference between two calls, converted to µsec
        //
        delta_archon = (int)((archontime2 - archontime1) / 100.);  // archon time was in 10 nsec
        delta_system = (int)((systime2 - systime1) * 1000000.);    // system time was in sec

        // enque each line to the async message port
        //
        message.str(""); message << "TEST_TIMER: " << nseqsave-nseq << ", " << delta_archon << ", " << delta_system;
        this->common.message.enqueue( message.str() );

        // save the difference between 
        //
        deltatime.push_back( abs( delta_archon - delta_system ) );

        usleep( sleepus );
      }

      // background polling back on
      //
      if (error == NO_ERROR) error = this->archon_cmd(POLLON);

      // calculate the average and standard deviation of the difference
      // between system and archon
      //
      double sum = std::accumulate(std::begin(deltatime), std::end(deltatime), 0.0);
      double m =  sum / deltatime.size();

      double accum = 0.0;
      std::for_each (std::begin(deltatime), std::end(deltatime), [&](const double d) {
          accum += (d - m) * (d - m);
      });

      double stdev = sqrt(accum / (deltatime.size()-1));

      message.str(""); message << "average delta=" << m << " stddev=" << stdev;
      logwrite(function, message.str());

      retstring = std::to_string( stdev );

    } // end if (testname==timer)

    // ----------------------------------------------------
    // invalid test name
    // ----------------------------------------------------
    //
    else {
      message.str(""); message << "ERROR: unknown test: " << testname;
      logwrite(function, message.str());
      error = ERROR;
    }

    return error;
  }
  /**************** Archon::Interface::test ***********************************/

}
