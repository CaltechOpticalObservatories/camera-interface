/**
 * @file    archon.cpp
 * @brief   common interface functions
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include "archon.h"
#include "fits.h"

#include <sstream>   // for std::stringstream
#include <iomanip>   // for setfil, setw, etc.
#include <iostream>  // for hex, uppercase, etc.
#include <algorithm> 
#include <cctype>
#include <string>

namespace Archon {

  // Archon::Interface constructor
  //
  Interface::Interface() {
    this->archon_busy = false;
    this->modeselected = false;
    this->msgref = 0;
    this->lastframe = 0;
    this->frame.index = 0;
    this->abort = false;
    this->taplines = 0;
    this->image_data = NULL;
    this->image_data_bytes = 0;
    this->image_data_allocated = 0;
//  this->config.filename = CONFIG_FILE;  //!< default config file set in CMakeLists.txt

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
        catch (std::invalid_argument ) {
          logwrite(function, "ERROR: invalid port number: unable to convert to integer");
          return(ERROR);
        }
        catch (std::out_of_range) {
          logwrite(function, "port number out of integer range");
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

      if (config.param[entry].compare(0, 16, "DEFAULT_FIRMWARE")==0) {
        this->camera_info.configfilename = config.arg[entry];
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
   * @param  none
   * @return 
   *
   */
  long Interface::connect_controller() {
    std::string function = "Archon::Interface::connect_controller";
    std::stringstream message;
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
   * This function simply calls archon_cmd() and logs the reply
   * //TODO: communicate with future ASYNC port?
   *
   */
  long Interface::native(std::string cmd) {
    std::string function = "Archon::Interface::native";
    std::stringstream message;
    std::string reply;
    long ret = archon_cmd(cmd, reply);
    if (!reply.empty()) {
      message.str(""); message << "native reply: " << reply;
      logwrite(function, message.str());
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
    char    buffer[2048];                       //!< temporary buffer for holding Archon replies
    int     error = NO_ERROR;

    if (!this->archon.isconnected()) {          // nothing to do if no connection open to controller
      logwrite(function, "ERROR: connection not open to controller");
      return(ERROR);
    }

    if (this->archon_busy) return(BUSY);        // only one command at a time

    /**
     * Hold a scoped lock for the duration of this function, 
     * to prevent multiple threads from accessing the Archon.
     */
    const std::lock_guard<std::mutex> lock(this->archon_mutex);
    this->archon_busy = true;

    // build command: ">xxCOMMAND\n" where xx=hex msgref and COMMAND=command
    //
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
      std::string fcmd = scmd; fcmd.erase(fcmd.find('\n'));                 // remove newline for logging
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
    reply="";                                        // zero reply buffer
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
      message.str(""); message << "ERROR: command-reply mismatch for command: " << scmd.erase(scmd.find('\n'));
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
      this->msgref = (this->msgref + 1) % 256;       // increment msgref
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
    error = this->archon_cmd(cmd.str(), reply);          // send RCONFIG command here
    reply.erase(reply.find('\n'));                       // strip newline

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
        reply.erase(reply.find("\n"));                                       // strip newline
        logwrite(function, reply);                                           // log reply here
      }
    } while (reply != "(null)");                                             // stop when reply is (null)

    return(retval);
  }
  /**************** Archon::Interface::fetchlog *******************************/


  /**************** Archon::Interface::load_firmware **************************/
  /**
   * @fn     load_firmware
   * @brief
   * @param  none
   * @return 
   *
   * This function is overloaded. If no argument is passed then call the version
   * requiring an argument, using the default configfilename specified in the .cfg file.
   *
   */
  long Interface::load_firmware() {
    long ret = this->load_firmware( this->camera_info.configfilename );
    if (ret == ERROR) this->fetchlog();
    return(ret);
  }
  long Interface::load_firmware(std::string acffile) {
    std::string function = "Archon::Interface::load_firmware";
    std::stringstream message;
    FILE    *fp;
    char    *line=NULL;  // if NULL, getline(3) will malloc/realloc space as required
    size_t   len=0;      // must set=0 for getline(3) to automatically malloc
    char    *lineptr, *keyptr, *valueptr, *paramnameptr, *paramvalptr;
    ssize_t  read;
    int      linecount;
    int      error=NO_ERROR;
    bool     begin_parsing=false;

    if ( acffile.empty() )
      acffile = this->camera_info.configfilename;
    else
      this->camera_info.configfilename = acffile;

    logwrite(function, acffile);

    std::string keyword, keystring, keyvalue, keytype, keycomment;

    std::string modesection;
    std::string mode;
    std::stringstream sscmd;
    std::stringstream key, value;

    /**
     * The CPU in Archon is single threaded, so it checks for a network 
     * command, then does some background polling (reading bias voltages etc), 
     * then checks again for a network command.  "POLLOFF" disables this 
     * background checking, so network command responses are very fast.  
     * The downside is that bias voltages, temperatures, etc are not updated 
     * until you give a "POLLON". 
     */
    if (error == NO_ERROR) error = this->archon_cmd(POLLOFF);

    /**
     * clear configuration memory for this controller
     */
    if (error == NO_ERROR) error = this->archon_cmd(CLEARCONFIG);

    /**
     * open configuration file for reading, then parse it line-by-line
     */
    if ( (fp = fopen(acffile.c_str(), "r")) == NULL ) {
      message.str(""); message << "ERROR: opening Archon Config File: " << acffile << ": errno: " << errno;
      logwrite(function, message.str());
      return(ERROR);
    }

    modemap.clear();                             // file is open, clear all modes

    linecount = 0;
    while ((read=getline(&line, &len, fp)) != EOF) {
      /**
       * don't start parsing until [CONFIG] and stop on a newline or [SYSTEM]
       */
      if (strncasecmp(line, "[CONFIG]", 8)==0) { begin_parsing=true;  continue; }
      if (strncasecmp(line, "\n",       1)==0) { begin_parsing=false; continue; }
      if (strncasecmp(line, "[SYSTEM]", 8)==0) { begin_parsing=false; continue; }
      /**
       * parse mode sections
       */
      if (strncasecmp(line, "[MODE_",   6)==0) { // this is a mode section
        chrrep(line, '[',  127);                 // remove [ bracket (replace with DEL)
        chrrep(line, ']',  127);                 // remove ] bracket (replace with DEL)
        chrrep(line, '\n', 127);                 // remove newline (replace with DEL)
        if (line!=NULL) {
          modesection=line;                      // create a string object out of the rest of the line
          mode=modesection.substr(5);            // everything after "MODE_" is the mode name
          std::transform( mode.begin(), mode.end(), mode.begin(), ::toupper );    // make uppercase
          if ( this->modemap.find(mode) != this->modemap.end() ) {
            message.str(""); message << "ERROR: duplicate definition of mode: " << mode << ": load aborted";
            logwrite(function, message.str());
            return ERROR;
          }
          else {
            begin_parsing = true;
            message.str(""); message << "detected mode: " << mode; logwrite(function, message.str());
            this->modemap[mode].rawenable=-1;    // initialize to -1, require it be set somewhere in the ACF
          }
        }
      }

      // Not one of the above [....] tags and not parsing, so skip to the next line
      //
      if (!begin_parsing) continue;

      /**
       * Convert the key (everything before first equal sign) to upper case
       */
      int linenum=0;
      while (line[linenum] != '=' && line[linenum] != 0) {
        line[linenum]=toupper(line[linenum]);
        linenum++;
      }

      /**
       * A tab takes up two characters in a line, "\t" which need to be replaced by two spaces "  "
       * because Archon requires we replace all of the backslashes with forward slashes. Before we
       * do that we have to get rid of the tabs (which contain backslashes).
       */
      const char *tab,               // location of tab
                 *orig_ptr,          // copy of the original line
                 *line_ptr = line;   // working copy of the line 

      // loop through the line looking for tabs
      //
      for ( (orig_ptr = line);
            (tab      = strstr(orig_ptr, (const char *)"\\t"));
            (orig_ptr = tab+2) ) {
        const size_t skiplen = tab-orig_ptr;              // location of the first tab
        strncpy((char*)line_ptr, orig_ptr, skiplen);      // copy the original line from the start up to the first tab
        line_ptr += skiplen;
        strncpy((char*)line_ptr, (const char *)"  ", 2);  // copy the two spaces into the line, in place of the tab
        line_ptr += 2;
      }
      strcpy((char*)line_ptr, orig_ptr);                  // copy the remainder of the line to the end

      chrrep(line, '\\', '/');                            // replace backslash with forward slash
      chrrep(line, '\"', 127);                            // remove all quotes (replace with DEL)

      /** ************************************************************
       * Store actual Archon parameters in their own STL map IN ADDITION to the map
       * in which all other keywords are store, so that they can be accessed in
       * a different way.  Archon PARAMETER KEY=VALUE paris are formatted as:
       * PARAMETERn=parametername=value
       * where "PARAMETERn" is the key and "parametername=value" is the value.
       * However, it is logical to access them by parametername only. That is what the
       * parammap is for, hence the need for this STL map indexed on only the "parametername"
       * portion of the value. Conversely, the configmap is indexed by the key.
       *
       * In order to modify these keywords in Archon, the entire above phrase
       * (KEY=VALUE pair) must be preserved along with the line number on which it 
       * occurs in the config file.
       * ************************************************************ */

      key.str(""); value.str("");   // initialize KEY=VALUE strings used to form command

      lineptr = line;
      if ( (valueptr = strstr(lineptr, "=")) == NULL ) continue;  // find the first equal sign
      valueptr++;                                                 // skip over that equal sign

      // now lineptr  is the entire line, 
      // and valueptr is everything after the first equal sign

      /**
       * Look for TAGS: in the .acf file
       *
       * If tag is "ACF:" then it's a .acf line (could be a parameter or configuration)
       */
      if (strncmp(lineptr, "ACF:",  4)==0) {

        if (strsep(&lineptr, ":") == NULL) continue;                      // strip off the "ACF:" portion
        chrrep(lineptr, '\n', 127);                                       // remove newline (replace with DEL)

        /**
         * We either hava a PARAMETER of the form: PARAMETERn=PARAMNAME=VALUE
         */
        if (strncmp(lineptr, "PARAMETER", 9) == 0) {                      // lineptr: PARAMETERn=PARAMNAME=VALUE
          if ( strsep(&lineptr, "=") == NULL ) continue;                  // lineptr: PARAMNAME=VALUE
          if ( (paramnameptr = strsep(&lineptr, "=")) == NULL ) continue; // paramnameptr: PARAMNAME, lineptr: VALUE
          // save the parametername and value
          // don't care about PARAMETERn key or line number,
          // we'll look those up in this->parammap when we need them
          this->modemap[mode].parammap[paramnameptr].name  = paramnameptr;
          this->modemap[mode].parammap[paramnameptr].value = lineptr;
        }
        /**
         * ...or we have any other CONFIG line of the form KEY=VALUE
         */
        else {
          if ( (keyptr = strsep(&lineptr, "=")) == NULL ) continue;       // keyptr: KEY, lineptr: VALUE
          // save the VALUE, indexed by KEY
          // don't need the line number
          this->modemap[mode].configmap[keyptr].value = lineptr;          //valueptr;
        }
      } // end if (strncmp(lineptr, "ACF:",  4)==0)

      /**
       * The "ARCH:" tag is for internal (Archon_interface) variables
       * using the KEY=VALUE format.
       */
      else
      if (strncmp(lineptr, "ARCH:", 5)==0) {
        if (strsep(&lineptr, ":") == NULL) continue;                      // strip off the "ARCH:" portion
        chrrep(lineptr, '\n', 127);                                       // remove newline (replace with DEL)
        if ( (keyptr = strsep(&lineptr, "=")) == NULL ) continue;         // keyptr: KEY, lineptr: VALUE

        std::string internal_key = keyptr;

        if (internal_key == "NUM_CCDS") {
          this->modemap[mode].geometry.num_ccds = atoi(lineptr);
        }
        else
        if (internal_key == "AMPS_PER_CCD_HORI") {
          this->modemap[mode].geometry.amps_per_ccd[0] = atoi(lineptr);
          message.str(""); message << "[DEBUG] loaded amps_per_ccd[0]=" << this->modemap[mode].geometry.amps_per_ccd[0] << " for mode " << mode;
          logwrite(function, message.str());
        }
        else
        if (internal_key == "AMPS_PER_CCD_VERT") {
          this->modemap[mode].geometry.amps_per_ccd[1] = atoi(lineptr);
          message.str(""); message << "[DEBUG] loaded amps_per_ccd[1]=" << this->modemap[mode].geometry.amps_per_ccd[1] << " for mode " << mode;
          logwrite(function, message.str());
        }
        else {
          message.str(""); message << "ERROR: unrecognized internal parameter specified: "<< internal_key;
          logwrite(function, message.str());
          if (line) free(line); fclose(fp);                               // getline() mallocs a buffer for line
          return(ERROR);
        }
      } // end if (strncmp(lineptr, "ARCH:", 5)==0)

      /**
       * the "FITS:" tag is used to write custom keyword entries
       */
      else
      if (strncmp(lineptr, "FITS:", 5)==0) {
        std::vector<std::string> tokens;
        if (strsep(&lineptr, ":") == NULL) continue;                      // strip off the "FITS:" portion
        chrrep(lineptr, '\n', 127);                                       // remove newline (replace with DEL)

        // First, tokenize on the equal sign "=".
        // The token left of "=" is the keyword. Immediate right is the value
        Tokenize(lineptr, tokens, "=");
        if (tokens.size() != 2) {                                         // need at least two tokens at this point
          logwrite(function, "ERROR: token mismatch: expected KEYWORD=value//comment (found too many ='s)");
          if (line) free(line); fclose(fp);                               // getline() mallocs a buffer for line
          return(ERROR);
        }
        keyword   = tokens[0].substr(0,8);                                // truncate keyword to 8 characters
        keystring = tokens[1];                                            // tokenize the rest in a moment

        // Next, tokenize on the slash "/".
        // The token left of "/" is the value. Anything to the right is a comment.
        Tokenize(keystring, tokens, "/");
        keyvalue   = tokens[0];

        if (tokens.size() == 2) {      // If there are two tokens then the second is a comment,
          keycomment = tokens[1];
        } else {                       // but the comment is optional.
          keycomment = "";
        }

        // Save all of the user keyword information in a map for later
        //
        this->modemap[mode].acfkeys.keydb[keyword].keyword    = keyword;
        this->modemap[mode].acfkeys.keydb[keyword].keytype    = this->camera_info.userkeys.get_keytype(keyvalue);
        this->modemap[mode].acfkeys.keydb[keyword].keyvalue   = keyvalue;
        this->modemap[mode].acfkeys.keydb[keyword].keycomment = keycomment;
      } // end if (strncmp(lineptr, "FITS:", 5)==0)

      /**
       * ----- all done looking for "TAGS:" -----
       */

      /**
       * If this is a PARAMETERn=parametername=value KEY=VALUE pair...
       */
      else
      if ( (strstr(lineptr, "PARAMETERS")==NULL) &&   /* ignore the PARAMETERS=nn line    */
           (strncmp(lineptr, "PARAMETER", 9)==0) ) {  /* line must start with PARAMETER   */
                                                      /* (don't parse mode sections here) */
        /**
         * this first call to strsep divides the "PARAMETERn" from the "parametername=value"
         */
        if ( (keyptr = strsep(&lineptr, "=")) == NULL ) continue;

        // now keyptr   is everything before the first equal sign
        // and valueptr is everything after the first equal sign (as is lineptr)

        this->configmap[keyptr].line  = linecount;
        this->configmap[keyptr].value = valueptr;

        if ( (paramvalptr = strstr(lineptr, "=")) == NULL ) continue; // find the next equal sign
        paramvalptr++;                                                // skip over that equal sign

        // now paramvalptr is everything after the second equal sign (actual value of parameter)
        // and lineptr     is everything after the first equal sign (the "parametername=value" part)

        /**
         * this second call to strsep divides the "parametername" from the "value"
         */
        if ( (paramnameptr = strsep(&lineptr, "=")) == NULL ) continue;

        // now paramnameptr is the part before the second equal (the "parametername" part)
        // and lineptr      is the part after the second equal  (the "value" part)

        this->parammap[paramnameptr].key   = keyptr;        // PARAMETERn
        this->parammap[paramnameptr].name  = paramnameptr;  // parametername
        this->parammap[paramnameptr].value = lineptr;       // value
        this->parammap[paramnameptr].line  = linecount;     // line number
        // assemble a KEY=VALUE pair used to form the WCONFIG command
        key   << keyptr;                                    // PARAMETERn
        value << paramnameptr << "=" << lineptr;            // parametername=value
      } // end If this is a PARAMETERn=parametername=value KEY=VALUE pair...

      /**
       * ...otherwise, for all other KEY=VALUE pairs, there is only the value and line number
       * to be indexed by the key. 
       * keyptr    is the part before the first equal sign
       * valueptr  is the part after the first equal sign
       */
      else {
        if ( (keyptr = strsep(&lineptr, "=")) == NULL ) continue;
        this->configmap[keyptr].line  = linecount;
        this->configmap[keyptr].value = valueptr;
        // assemble a KEY=VALUE pair used to form the WCONFIG command
        key   << keyptr;                                    // KEY
        value << valueptr;                                  // VALUE
      } // end else

      /**
       * Form the WCONFIG command to Archon and
       * write the config line to the controller memory.
       */
      if (key!=NULL && value !=NULL) {
        sscmd.str("");
        sscmd << "WCONFIG"
              << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
              << linecount
              << key.str() << "=" << value.str() ;
        // send the WCONFIG command here
        if (error == NO_ERROR) error = this->archon_cmd(sscmd.str());
      } // end if (key!=NULL && value !=NULL)
      linecount++;
    } // end while ((read=getline(&line, &len, fp)) != EOF)

    /**
     * re-enable background polling
     */
    if (error == NO_ERROR) error = this->archon_cmd(POLLON);

    /**
     * apply the configuration just loaded into memory, and turn on power
     */
    if (error == NO_ERROR) error = this->archon_cmd(APPLYALL);

    if (line) free(line);                 // getline() mallocs a buffer for line
    fclose(fp);
    if (error == NO_ERROR) {
      logwrite(function, "loaded Archon config file OK");
    }

    // If there was an Archon error then read the Archon error log
    //
    if (error != NO_ERROR) this->fetchlog();

    this->camera_info.region_of_interest[0]=0;
    this->camera_info.region_of_interest[1]=5;
    this->camera_info.region_of_interest[2]=0;
    this->camera_info.region_of_interest[3]=6;

    this->camera_info.binning[0]=1;
    this->camera_info.binning[1]=1;

    this->modeselected = false;           // require that a mode be selected after loading new firmware

    return(error);
  }
  /**************** Archon::Interface::load_firmware **************************/


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

    std::transform( mode.begin(), mode.end(), mode.begin(), ::toupper );    // make uppercase

    // The requested mode must have been read in the current ACF file
    // and put into the modemap...
    //
    if (this->modemap.find(mode) == this->modemap.end()) {
      message.str(""); message << "ERROR: undefined mode " << mode << " in ACF file " << this->camera_info.configfilename;
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

    message.str(""); 
    message << "[DEBUG] mode=" << mode << " RAWENABLE=" << this->modemap[mode].rawenable 
            << " RAWSAMPLES=" << this->rawinfo.rawsamples << " RAWLINES=" << this->rawinfo.rawlines;
    logwrite(function, message.str());

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
      this->camera_info.frame_type = FRAME_RAW;
      this->camera_info.region_of_interest[0] = 1;
      this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
      this->camera_info.region_of_interest[2] = 1;
      this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];
      // Binning factor (no binning)
      this->camera_info.binning[0] = 1;
      this->camera_info.binning[1] = 1;
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[0] (RAWSAMPLES) = " << this->camera_info.detector_pixels[0];
      logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[1] (RAWENDLINE) = " << this->camera_info.detector_pixels[1];
      logwrite(function, message.str());
    }

    // Any other mode falls under here
    //
    else {
      if (error==NO_ERROR) error = get_configmap_value("PIXELCOUNT", this->camera_info.detector_pixels[0]);
      if (error==NO_ERROR) error = get_configmap_value("LINECOUNT", this->camera_info.detector_pixels[1]);
      message.str(""); message << "[DEBUG] mode=" << mode; logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[0] (PIXELCOUNT) = " << this->camera_info.detector_pixels[0]
                               << " amps_per_ccd[0] = " << this->modemap[mode].geometry.amps_per_ccd[0];
      logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[1] (LINECOUNT) = " << this->camera_info.detector_pixels[1]
                               << " amps_per_ccd[1] = " << this->modemap[mode].geometry.amps_per_ccd[1];
      logwrite(function, message.str());
      this->camera_info.detector_pixels[0] *= this->modemap[mode].geometry.amps_per_ccd[0];
      this->camera_info.detector_pixels[1] *= this->modemap[mode].geometry.amps_per_ccd[1];
      this->camera_info.frame_type = FRAME_IMAGE;
      // ROI is the full detector
      this->camera_info.region_of_interest[0] = 1;
      this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
      this->camera_info.region_of_interest[2] = 1;
      this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];
      // Binning factor (no binning)
      this->camera_info.binning[0] = 1;
      this->camera_info.binning[1] = 1;
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[0] (PIXELCOUNT) = " << this->camera_info.detector_pixels[0];
      logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[1] (LINECOUNT) = " << this->camera_info.detector_pixels[1];
      logwrite(function, message.str());
    }

    // set bitpix based on SAMPLEMODE
    //
    int samplemode=-1;
    if (error==NO_ERROR) error = get_configmap_value("SAMPLEMODE", samplemode); // SAMPLEMODE=0 for 16bpp, =1 for 32bpp
    if (samplemode < 0) {
      message.str(""); message << "bad or missing SAMPLEMODE from " << this->camera_info.configfilename;
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
    // Call set_axes(datatype) with the FITS data type needed.
    //
    if (this->camera_info.frame_type == FRAME_RAW) {
      error = this->camera_info.set_axes(USHORT_IMG);
    }
    if (this->camera_info.frame_type == FRAME_IMAGE) {
      if (this->camera_info.bitpix == 16) error = this->camera_info.set_axes(USHORT_IMG);
      else
      if (this->camera_info.bitpix == 32) error = this->camera_info.set_axes(FLOAT_IMG);
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
    message.str(""); message << "[DEBUG] image_data_bytes = " << image_data_bytes; logwrite(function, message.str());

    this->camera_info.current_observing_mode = mode;       // identify the newly selected mode in the camera_info class object
    this->modeselected = true;                             // a valid mode has been selected

    message.str(""); message << "new mode: " << mode << " will use " << this->camera_info.bitpix << " bits per pixel";
    logwrite(function, message.str());

    return(NO_ERROR);
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

      // If all three tokens present (ADxx,gain,offset) then parse it,
      // otherwise it's an unused tap and we can skip it.
      //
      if (tokens.size() == 3) { // defined tap has three tokens
        adchan = tokens[0];     // AD channel is the first (0th) token
        char chars[] = "ADLR";  // characters to remove in order to get just the AD channel number

        // remove AD, L, R from the adchan string, to get just the AD channel number
        //
        for (unsigned int j = 0; j < strlen(chars); j++) {
          adchan.erase(std::remove(adchan.begin(), adchan.end(), chars[j]), adchan.end());
        }

        // AD# in TAPLINE is 1-based (numbered 1-16)
        // but convert here to 0-based (numbered 0-15) and check value before using
        //
        int adnum;
        try {
          adnum = std::stoi(adchan) - 1;
        }
        catch (std::invalid_argument ) {
          logwrite(function, "ERROR: invalid AD number: unable to convert to integer");
          return(ERROR);
        }
        catch (std::out_of_range) {
          logwrite(function, "AD number out of integer range");
          return(ERROR);
        }
        if ( (adnum < 0) || (adnum > MAXADCHANS) ) {
          message.str(""); message << "ERROR: ADC channel " << adnum << " outside range {0:" << MAXADCHANS << "}";
          logwrite(function, message.str());
          return(ERROR);
        }
        // Now that adnum is OK, convert next two tokens to gain, offset
        //
        try {
          this->gain  [ adnum ] = std::stoi(tokens[1]);    // gain as function of AD channel
          this->offset[ adnum ] = std::stoi(tokens[2]);    // offset as function of AD channel
        }
        catch (std::invalid_argument ) {
          logwrite(function, "ERROR: invalid GAIN and/or OFFSET: unable to convert to integer");
          return(ERROR);
        }
        catch (std::out_of_range) {
          logwrite(function, "GAIN and/or OFFSET out of integer range");
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
        catch (std::invalid_argument ) {
          logwrite(function, "ERROR: invalid buffer or value: unable to convert to integer");
          return(ERROR);
        }
        catch (std::out_of_range) {
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
    timer_str.erase(timer_str.find('\n'));  // remove newline
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

    message.str(""); message << "reading " << (this->camera_info.frame_type==FRAME_RAW?"raw":"image") << " with " << scmd;
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
    message.str(""); message << "[DEBUG] rawenable=" << rawenable; logwrite(function, message.str());

    if (rawenable == -1) {
      logwrite(function, "ERROR: RAWENABLE is undefined");
      return ERROR;
    }

    // RAW-only
    //
    if (this->camera_info.current_observing_mode == "RAW") {              // "RAW" is the only reserved mode name
      logwrite(function, "[DEBUG] observing mode is RAW");

      // the RAWENABLE parameter must be set in the ACF file, in order to read RAW data
      //
      if (rawenable==0) {
        logwrite(function, "ERROR: observing mode is RAW but RAWENABLE=0 -- change mode or set RAWENABLE?");
        return ERROR;
      }
      else {
        logwrite(function, "[DEBUG] raw-only calling read_frame(FRAME_RAW)");
        if (error == NO_ERROR) error = this->read_frame(FRAME_RAW);       // read raw frame
        logwrite(function, "[DEBUG] raw-only calling write_frame()");
        if (error == NO_ERROR) error = this->write_frame();               // write raw frame
      }
    }

    // IMAGE, or IMAGE+RAW
    //
    else {
      if (rawenable == 1) {                                               // rawenable can still be set even if mode != RAW
        this->camera_info.data_cube = true;                               // in which case we should write a data cube
        this->camera_info.extension = 0;
      }

      logwrite(function, "[DEBUG] calling read_frame(FRAME_IMAGE)");
      if (error == NO_ERROR) error = this->read_frame(FRAME_IMAGE);       // read image frame
      logwrite(function, "[DEBUG] calling write_frame()");
      if (error == NO_ERROR) error = this->write_frame();                 // write image frame

      // If mode is not RAW but RAWENABLE=1, then we will first read an image
      // frame (just done above) and then a raw frame (below). To do that we
      // must switch to raw mode then read the raw frame. Afterwards, switch back
      // to the original mode, for any subsequent exposures..
      //
      if (rawenable == 1) {
        logwrite(function, "[DEBUG] rawenable is set -- IMAGE+RAW file will be saved (I hope)");
        logwrite(function, "[DEBUG] switching to mode=RAW");
        std::string orig_mode = this->camera_info.current_observing_mode; // save the original mode so we can come back to it
        if (error == NO_ERROR) error = this->set_camera_mode("raw");      // switch to raw mode

        message.str(""); message << "[DEBUG] error=" << error << " calling read_frame(FRAME_RAW) if error=0"; logwrite(function, message.str());
        if (error == NO_ERROR) error = this->read_frame(FRAME_RAW);       // read raw frame
        message.str(""); message << "[DEBUG] error=" << error << " calling write_raw() if error=0"; logwrite(function, message.str());
        if (error == NO_ERROR) error = this->write_raw();                 // write raw frame
        message.str(""); message << "[DEBUG] error=" << error << " switching back to original mode if error=0"; logwrite(function, message.str());
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
   */
  long Interface::read_frame(frame_type_t frame_type) {
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

    // Get the current frame buffer status
    //
    error = this->get_frame_status();

    if (error != NO_ERROR) {
      logwrite(function, "ERROR: unable to get frame status");
      return(error);
    }

    // Archon buffer number of the last frame read into memory
    //
    bufready = this->frame.index + 1;

    if (bufready < 1 || bufready > this->camera_info.activebufs) {
      message.str(""); message << "ERROR: invalid buffer " << bufready;
      logwrite(function, message.str());
      return(ERROR);
    }

    message.str(""); message << "will read " << (frame_type == FRAME_RAW ? "raw" : "image")
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
      case FRAME_RAW:
        // Archon buffer base address
        bufaddr   = this->frame.bufbase[this->frame.index] + this->frame.bufrawoffset[this->frame.index];

        // Calculate the number of blocks expected. image_memory is bytes per CCD
        bufblocks = (unsigned int) floor( (this->camera_info.image_memory + BLOCK_LEN - 1 ) / BLOCK_LEN );
        break;

      case FRAME_IMAGE:
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
      auto start = std::chrono::high_resolution_clock::now();    // start a timer now

      while ( this->archon.Bytes_ready() < (BLOCK_LEN+4) ) {
        auto now = std::chrono::high_resolution_clock::now();    // check the time again
        std::chrono::duration<double> diff = now-start;          // calculate the duration
        if (diff.count() > 1) {                                  // break while loop if duration > 1 second
          logwrite(function, "ERROR: timeout waiting for data from Archon");
          return(ERROR);
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
        message.str(""); message << "ERROR: reading " << (frame_type==FRAME_RAW?"raw ":"image ") << " data";
        logwrite(function, message.str());
        this->fetchlog();      // check the Archon log for error messages
        error = ERROR;
        break;
      }
      else if (strncmp(header, check, 4) != 0) {
        message.str(""); message << "Archon command-reply mismatch reading " << (frame_type==FRAME_RAW?"raw ":"image ")
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
      message.str(""); message << "successfully read " << std::dec << totalbytesread << (frame_type==FRAME_RAW?" raw":" image")
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
        FITS_file <float> fits_file;                            // Instantiate a FITS_file object with the appropriate type
        cbuf32 = (uint32_t *)this->image_data;                  // cast here to 32b
        float *fbuf = NULL;
//      fbuf = new float[ this->fits_info.image_size ];         // allocate a float buffer of same number of pixels for scaling  //TODO
        fbuf = new float[ this->camera_info.image_size ];       // allocate a float buffer of same number of pixels for scaling

//      for (long pix=0; pix < this->fits_info.image_size; pix++)   //TODO
        for (long pix=0; pix < this->camera_info.image_size; pix++) {
          fbuf[pix] = cbuf32[pix] / (float)65535;               // right shift 16 bits
        }

//      error = fits_file.write_image(fbuf, this->fits_info);   // write the image to disk //TODO
        error = fits_file.write_image(fbuf, this->camera_info); // write the image to disk
        if (fbuf != NULL) {
          delete [] fbuf;
        }
        break;
      }

      // convert four 8-bit values into 16 bit values and no scaling necessary
      //
      case 16: {
        FITS_file <unsigned short> fits_file;                   // Instantiate a FITS_file object with the appropriate type
        cbuf16 = (uint16_t *)this->image_data;                  // cast to 16b
//      error = fits_file.write_image(cbuf16, this->fits_info); // write the image to disk //TODO
        error = fits_file.write_image(cbuf16, this->camera_info); // write the image to disk
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
      this->common.increment_imnum();                             // increment image_num when fitsnaming == "number"
      if (this->camera_info.data_cube) this->camera_info.extension++;  // increment extension for cubes
      message.str(""); 
      message << "[DEBUG] this->camera_info.extension = " << this->camera_info.extension; 
      logwrite(function, message.str());
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
      logwrite(function, "[DEBUG] creating fits file with cfitsio");
      if (fits_create_file( &FP, this->camera_info.fits_name.c_str(), &status ) ) {
        message.str("");
        message << "cfitsio error " << status << " creating FITS file " << this->camera_info.fits_name;
        logwrite(function, message.str());
        error = ERROR;
      }
    }
    else {
      logwrite(function, "[DEBUG] opening fits file with cfitsio");
      message.str(""); message << "[DEBUG] file=" << this->camera_info.fits_name << " extension=" << this->camera_info.extension
                               << " bitpix=" << this->camera_info.bitpix;
      logwrite(function, message.str());
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
      message.str(""); message << "sending archon_cmd(" << sscmd << ")";
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
      message.str(""); message << "[DEBUG] key_in=" << key_in << " value_out=" << value_out; logwrite(function, message.str());
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
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * This function really does three things before returning successful completion:
   *  1) trigger an Archon exposure by setting the EXPOSE parameter = 1
   *  2) wait for exposure delay
   *  3) wait for readout into Archon frame buffer
   *
   * Note that this assumes that the Archon ACF has been programmed to automatically
   * read out the detector into the frame buffer after an exposure.
   *
   */
  long Interface::expose() {
    std::string function = "Archon::Interface::expose";
    std::stringstream message;
    long error;

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

    error = this->prep_parameter(this->exposeparam, "1");
    if (error == NO_ERROR) error = this->load_parameter(this->exposeparam, "1");

    // get system time and Archon's timer after exposure starts
    // start_timer is used to determine when the exposure has ended, in wait_for_exposure()
    //
    if (error == NO_ERROR) {
      this->camera_info.start_time = get_time_string();             // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
      error = this->get_timer(&this->start_timer);                  // Archon internal timer (one tick=10 nsec)
      this->common.set_fitstime(this->camera_info.start_time);      // sets common.fitstime (YYYYMMDDHHMMSS) used for filename
    }

    if (error == NO_ERROR) logwrite(function, "exposure started");
    message.str(""); 
    message << "[DEBUG] mode " << this->camera_info.current_observing_mode;
    logwrite(function, message.str());

    this->camera_info.fits_name = this->common.get_fitsname();      // assemble the FITS filenmae

    this->camera_info.userkeys.keydb = this->userkeys.keydb;        // copy the userkeys database object into camera_info

    // add any keys from the ACF file (from modemap[mode].acfkeys) into the camera_info.userkeys object
    //
    Common::FitsKeys::fits_key_t::iterator keyit;
    for (keyit  = this->modemap[this->camera_info.current_observing_mode].acfkeys.keydb.begin();
         keyit != this->modemap[this->camera_info.current_observing_mode].acfkeys.keydb.end();
         keyit++) {
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyword    = keyit->second.keyword;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keytype    = keyit->second.keytype;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyvalue   = keyit->second.keyvalue;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keycomment = keyit->second.keycomment;
    }

    // add the internal system keys into the camera_info.userkeys object
    //
/*
    for (keyit  = this->systemkeys.keydb.begin();
         keyit != this->systemkeys.keydb.end();
         keyit++) {
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyword    = keyit->second.keyword;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keytype    = keyit->second.keytype;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyvalue   = keyit->second.keyvalue;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keycomment = keyit->second.keycomment;
    }
*/

    // If mode is not "RAW" but RAWENABLE is set then we're going to require a multi-extension data cube,
    // one extension each for CDS and RAW data
    //
    if (this->camera_info.current_observing_mode!="RAW" && this->modemap[this->camera_info.current_observing_mode].rawenable) {
      this->camera_info.data_cube = true;
      this->camera_info.extension = 0;
    }
    else {
      this->camera_info.data_cube = false;
      this->camera_info.extension = 0;
    }

    message.str(""); message << "[DEBUG] data_cube = " << (this->camera_info.data_cube?"True":"False") << " extension=" << this->camera_info.extension;
    logwrite(function, message.str());

//  //TODO only use camera_info -- don't use fits_info -- is this OK? TO BE CONFIRMED
//  this->fits_info = this->camera_info;                            // copy the camera_info class, to be given to fits writer  //TODO

    this->lastframe = this->frame.bufframen[this->frame.index];     // save the last frame number acquired (wait_for_readout will need this)

    error = this->wait_for_exposure();                              // wait for the exposure delay to complete

    if (error==ERROR) {
      logwrite(function, "exposure delay error");
      return(error);
    }
    else {
      logwrite(function, "exposure delay complete");
    }

    if (this->camera_info.current_observing_mode != "RAW") {        // If not raw mode then
      error = this->wait_for_readout();                             // wait for the readout into frame buffer.
    }

    if (error==ERROR) {
      logwrite(function, "readout error");
      return(error);
    }
    else {
      logwrite(function, "readout complete");
    }

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

    // waittime is an integral number of milliseconds 20% over the readout time
    // and will be used to keep track of elapsed time, for timeout errors
    //
    int waittime = (int)ceil(CCD_READOUT_TIME * 120 / 100);

    double clock_timeout = get_clock_time()*1000 + waittime;  // get_clock_time returns seconds, and I'm counting msec
    double clock_now = get_clock_time()*1000;

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

      // Enough time has passed to trigger a timeout error.
      //
      if (clock_now > clock_timeout) {
        done = true;
        error = ERROR;
        logwrite(function, "ERROR: timeout waiting for readout");
      }
      timeout( 0.001);
      clock_now = get_clock_time()*1000;
    } // end while (done == false && this->abort == false)

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
      if (ret != ERROR) this->camera_info.exposure_time = std::stoi( exptime_in );
    }
    retstring = std::to_string( this->camera_info.exposure_time );
    message.str(""); message << "exposure time is " << retstring << " msec";
    logwrite(function, message.str());
    return(ret);
  }
  /**************** Archon::Interface::exptime ********************************/

}
