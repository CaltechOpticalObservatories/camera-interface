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
    this->msgref = 0;
    this->image_data = NULL;
    this->image_data_bytes = 0;
    this->image_data_allocated = 0;
    this->config.filename = CONFIG_FILE;  //!< default config file set in CMakeLists.txt

    // TODO I should change these to STL maps instead
    //
    this->frame.bufsample.resize( Archon::nbufs );
    this->frame.bufcomplete.resize( Archon::nbufs );
    this->frame.bufmode.resize( Archon::nbufs );
    this->frame.bufbase.resize( Archon::nbufs );
    this->frame.bufframe.resize( Archon::nbufs );
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


  /**************** Archon::Interface::get_config *****************************/
  /**
   * @fn     get_config
   * @brief  get needed values out of the read configuration files
   * @param  none
   * @return NO_ERROR if successful or ERROR on error
   *
   */
  long Interface::get_config() {
    std::string function = "Archon::Interface::get_config";

    // loop through the entries in the configuration file, stored in config class
    //
    for (int entry=0; entry < this->config.n_entries; entry++) {

      if (config.param[entry].compare(0, 9, "ARCHON_IP")==0) {
        this->camera_info.hostname = config.arg[entry];
        this->archon.sethost( config.arg[entry] );
      }

      if (config.param[entry].compare(0, 11, "ARCHON_PORT")==0) {
        int port;
        try {
          port = std::stoi( config.arg[entry] );
        }
        catch (std::invalid_argument ) {
          logwrite(function, "error invalid port number: unable to convert to integer");
          return(ERROR);
        }
        catch (std::out_of_range) {
          logwrite(function, "port number out of integer range");
          return(ERROR);
        }
        this->camera_info.port = port;
        this->archon.setport(port);
      }

      if (config.param[entry].compare(0, 12, "EXPOSE_PARAM")==0) {
        this->exposeparam = config.arg[entry];
      }

      if (config.param[entry].compare(0, 11, "DEFAULT_ACF")==0) {
        this->camera_info.configfilename = config.arg[entry];
      }

      if (config.param[entry].compare(0, 5, "IMDIR")==0) {
        this->common.imdir( config.arg[entry] );
      }

      if (config.param[entry].compare(0, 6, "IMNAME")==0) {
        this->common.imname( config.arg[entry] );
      }

    }
    logwrite(function, "successfully read config file");
    return NO_ERROR;
  }


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
        logwrite(function, "delete image_data");
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
      message.str(""); message << "error " << errno << " connecting to " << this->camera_info.hostname << ":" << this->camera_info.port;
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
      logwrite(function, "error disconnecting Archon camera!");
    }

    return(error);
  }
  /**************** Archon::Interface::disconnect_controller ******************/


  /**************** Archon::Interface::archon_native **************************/
  /**
   * @fn     archon_native
   * @brief  send native commands directly to Archon and log result
   * @param  std::string cmd
   * @return long ret from archon_cmd() call
   *
   * This function simply calls archon_cmd() and logs the reply
   * //TODO: communicate with future ASYNC port?
   *
   */
  long Interface::archon_native(std::string cmd) {
    std::string function = "Archon::Interface::archon_native";
    std::stringstream message;
    std::string reply;
    long ret = archon_cmd(cmd, reply);
    if (!reply.empty()) {
      message.str(""); message << "native reply: " << reply;
      logwrite(function, message.str());
    }
    return( ret );
  }
  /**************** Archon::Interface::archon_native **************************/


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
    char    buffer[1024];                       //!< temporary buffer for holding Archon replies
    int     error = NO_ERROR;

    if (!this->archon.isconnected()) {          // nothing to do if no connection open to controller
      logwrite(function, "error: connection not open to controller");
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
      logwrite(function, "error converting command to uppercase");
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
      logwrite(function, "error writing to camera socket");
    }

    // For the FETCH command we don't wait for a reply, but return immediately.
    // FETCH results in a binary response which is handled elsewhere (in read_data).
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
      memset(buffer, '\0', 1024);                    // init temporary buffer
      retval = this->archon.Read(buffer, 1024);      // read into temp buffer
      if (retval <= 0) {
        logwrite(function, "error reading Archon");
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
      message.str(""); message << "Archon controller returned error processing command: " << cmd;
      logwrite(function, message.str());
    }
    else
    if (reply.compare(0, 3, check)!=0) {             // First 3 bytes of reply must equal checksum else reply doesn't belong to command
      error = ERROR;
      std::string hdr = reply;
      message.str(""); message << "error: command-reply mismatch for command: " << scmd.erase(scmd.find('\n'));
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
      message.str(""); message << "error: parameter \"" << paramname << "\" not found";
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
      message << "error:  malformed reply: " << reply;
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
      message.str(""); message << "error writing parameter \"" << paramname << "=" << value << "\" to configuration memory";
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
      message.str(""); message << "error loading parameter \"" << paramname << "=" << value << "\" into Archon";
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
        message.str(""); message << "error " << retval << " calling FETCHLOG";
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


  /**************** Archon::Interface::load_config ****************************/
  /**
   * @fn     load_config
   * @brief
   * @param  none
   * @return 
   *
   * This function is overloaded. If no argument is passed then call the version
   * requiring an argument, using the default configfilename specified in the .cfg file.
   *
   */
  long Interface::load_config() {
    return( this->load_config( this->camera_info.configfilename ) );
  }
  long Interface::load_config(std::string acffile) {
    std::string function = "Archon::Interface::load_config";
    std::stringstream message;
    FILE    *fp;
    char    *line=NULL;  // if NULL, getline(3) will malloc/realloc space as required
    size_t   len=0;      // must set=0 for getline(3) to automatically malloc
    char    *lineptr, *keyptr, *valueptr, *paramnameptr, *paramvalptr;
    ssize_t  read;
    int      linecount;
    int      error=NO_ERROR;
    int      obsmode = -1;
    bool     begin_parsing=FALSE;

    if ( acffile.empty() )
      acffile = this->camera_info.configfilename;
    else
      this->camera_info.configfilename = acffile;

    logwrite(function, acffile);

    std::string keyword, keystring, keyvalue, keytype, keycomment;

    std::string modesection;
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
      message.str(""); message << "error opening Archon Config File: " << acffile << ": errno: " << errno;
      logwrite(function, message.str());
      return(ERROR);
    }

    /**
     * default all modes undefined,
     * set defined=TRUE only when entries are found in the .acf file
     * and remove all elements from the map containers
     */
    for (int modenum=0; modenum<NUM_OBS_MODES; modenum++) {
      this->modeinfo[modenum].defined = FALSE;
      this->modeinfo[modenum].rawenable = -1;    // -1 means undefined. Set in ACF.
      this->modeinfo[modenum].parammap.clear();
      this->modeinfo[modenum].configmap.clear();
      this->modeinfo[modenum].acfkeys.keydb.clear();
    }

    linecount = 0;
    while ((read=getline(&line, &len, fp)) != EOF) {
      /**
       * don't start parsing until [CONFIG] and stop on a newline or [SYSTEM]
       */
      if (strncasecmp(line, "[CONFIG]", 8)==0) { begin_parsing=TRUE;  continue; }
      if (strncasecmp(line, "\n",       1)==0) { begin_parsing=FALSE; continue; }
      if (strncasecmp(line, "[SYSTEM]", 8)==0) { begin_parsing=FALSE; continue; }
      /**
       * parse mode sections
       */
      if (strncasecmp(line, "[MODE_",   6)==0) { // this is a mode section
        chrrep(line, '[',  127);                 // remove [ bracket (replace with DEL)
        chrrep(line, ']',  127);                 // remove ] bracket (replace with DEL)
        chrrep(line, '\n', 127);                 // remove newline (replace with DEL)
        if (line!=NULL) modesection=line;        // create a string object out of the rest of the line

        // loop through all possible observing modes
        // to validate this mode
        //
        for (int modenum=0; modenum<NUM_OBS_MODES; modenum++) {
          if (modesection == Observing_mode_str[modenum]) {
            obsmode = modenum;  // here is the observing mode for this section
            message.str(""); message << "found section for mode " << obsmode << ": " << modesection;
            logwrite(function, message.str());
            begin_parsing=TRUE;
            continue;
          }
        }
        if (obsmode == -1) {
          message.str(""); message << "error: unrecognized observation mode section " << modesection << " found in .acf file";
          logwrite(function, message.str());
          if (line) free(line); fclose(fp);                               // getline() mallocs a buffer for line
          return(ERROR);
        }
      }

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
          this->modeinfo[obsmode].parammap[paramnameptr].name  = paramnameptr;
          this->modeinfo[obsmode].parammap[paramnameptr].value = lineptr;
          this->modeinfo[obsmode].defined = TRUE;                         // this mode is defined
        }
        /**
         * ...or we have any other CONFIG line of the form KEY=VALUE
         */
        else {
          if ( (keyptr = strsep(&lineptr, "=")) == NULL ) continue;       // keyptr: KEY, lineptr: VALUE
          // save the VALUE, indexed by KEY
          // don't need the line number
          this->modeinfo[obsmode].configmap[keyptr].value = lineptr;      //valueptr;
          this->modeinfo[obsmode].defined = TRUE;                         // this mode is defined
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

        this->modeinfo[obsmode].defined = TRUE;                           // this mode is defined

        std::string internal_key = keyptr;

        if (internal_key == "NUM_CCDS") {
          this->geometry[obsmode].num_ccds = atoi(lineptr);
        }
        else
        if (internal_key == "AMPS_PER_CCD_HORI") {
          this->geometry[obsmode].amps_per_ccd[0] = atoi(lineptr);
        }
        else
        if (internal_key == "AMPS_PER_CCD_VERT") {
          this->geometry[obsmode].amps_per_ccd[1] = atoi(lineptr);
        }
        else {
          message.str(""); message << "error: unrecognized internal parameter specified: "<< internal_key;
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
          logwrite(function, "error: token mismatch: expected KEYWORD=value//comment (found too many ='s)");
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
        this->modeinfo[obsmode].acfkeys.keydb[keyword].keyword    = keyword;
        this->modeinfo[obsmode].acfkeys.keydb[keyword].keytype    = this->camera_info.userkeys.get_keytype(keyvalue);
        this->modeinfo[obsmode].acfkeys.keydb[keyword].keyvalue   = keyvalue;
        this->modeinfo[obsmode].acfkeys.keydb[keyword].keycomment = keycomment;
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

    this->camera_info.region_of_interest[0]=0;
    this->camera_info.region_of_interest[1]=5;
    this->camera_info.region_of_interest[2]=0;
    this->camera_info.region_of_interest[3]=6;

    this->camera_info.binning[0]=1;
    this->camera_info.binning[1]=1;

    this->set_camera_mode(FRAME_IMAGE);

    return(error);
  }
  /**************** Archon::Interface::load_config ****************************/


  /**************** Archon::Interface::set_camera_mode ************************/
  /**
   * @fn     set_camera_mode
   * @brief  
   * @param  none
   * @return 
   *
   */
  long Interface::set_camera_mode(int mode) {
    std::string function = "Archon::Interface::set_camera_mode";
    std::stringstream message;
    bool configchanged = false;
    bool paramchanged = false;

    /**
     * first check is that mode is within allowable range
     */
    if (mode < 0 || mode >= NUM_OBS_MODES) {
      message.str(""); message << "invalid mode " << this->Observing_mode_str[mode] << " (" << mode << ")";
      logwrite(function, message.str());
      return(ERROR);
    }

    /**
     * there needs to have been a modes section defined in the .acf file
     * in order to switch to that mode, or else undesirable behavior may result
     */
    if (!this->modeinfo[mode].defined) {
      message.str(""); message << "error undefined mode " << this->Observing_mode_str[mode] << " in ACF file " << this->camera_info.configfilename;
      logwrite(function, message.str());
      return(ERROR);
    }

    this->camera_info.current_observing_mode = mode;

    message.str(""); message << "requested mode: " << mode << ": " << this->Observing_mode_str[mode];
    logwrite(function, message.str());

    int num_ccds = this->geometry[this->camera_info.current_observing_mode].num_ccds;

    /**
     * set internal variables based on new .acf values loaded
     */
    std::istringstream( this->configmap["LINECOUNT"].value  ) >> this->geometry[mode].linecount;
    std::istringstream( this->configmap["PIXELCOUNT"].value ) >> this->geometry[mode].pixelcount;
    std::istringstream( this->configmap["RAWSEL"].value     ) >> this->rawinfo.adchan;
    std::istringstream( this->configmap["RAWENABLE"].value  ) >> this->modeinfo[mode].rawenable;

    switch (this->camera_info.frame_type) {

      case FRAME_IMAGE:
        std::istringstream( this->configmap["PIXELCOUNT"].value ) >> this->camera_info.detector_pixels[0];
        std::istringstream( this->configmap["LINECOUNT"].value  ) >> this->camera_info.detector_pixels[1];
        this->camera_info.detector_pixels[0] *= this->geometry[this->camera_info.current_observing_mode].amps_per_ccd[0];
        this->camera_info.detector_pixels[1] *= this->geometry[this->camera_info.current_observing_mode].amps_per_ccd[1];
        break;

      case FRAME_RAW:
        std::istringstream( this->configmap["RAWSAMPLES"].value ) >> this->camera_info.detector_pixels[0];
        std::istringstream( this->configmap["RAWENDLINE"].value ) >> this->camera_info.detector_pixels[1]; 
        this->camera_info.detector_pixels[1]++;
        this->rawinfo.raw_samples = this->camera_info.detector_pixels[0];
        this->rawinfo.raw_lines   = this->camera_info.detector_pixels[1];
        break;

      default:
        this->camera_info.detector_pixels[0] = this->camera_info.detector_pixels[1] = 0;
        message.str(""); message << "error invalid frame type: " << this->camera_info.frame_type;
        logwrite(function, message.str());
        return(ERROR);
    }

    // The following are unique settings for each particular mode:
    //
    switch(mode) {

      case MODE_RAW:
        // frame_type will determine the bits per pixel and where the detector_axes come from
        this->camera_info.frame_type = FRAME_RAW;
        this->camera_info.region_of_interest[0] = 1;
        this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
        this->camera_info.region_of_interest[2] = 1;
        this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];
        // Binning factor (no binning)
        this->camera_info.binning[0] = 1;
        this->camera_info.binning[1] = 1;
        break;

      case MODE_DEFAULT:
        this->camera_info.frame_type = FRAME_IMAGE;
        // ROI is the full detector
        this->camera_info.region_of_interest[0] = 1;
        this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
        this->camera_info.region_of_interest[2] = 1;
        this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];
        // Binning factor (no binning)
        this->camera_info.binning[0] = 1;
        this->camera_info.binning[1] = 1;
        break;

      default:                           // Unspecified mode is an error
        message.str(""); message << "error invalid mode: "<< mode;
        logwrite(function, message.str());
        return(ERROR);
        break;
    }

    /**
     * set current number of Archon buffers and resize local memory
     */
    int bigbuf=-1;
    std::istringstream( this->configmap["BIGBUF"].value  ) >> bigbuf;  // get value of BIGBUF from loaded acf file
    this->camera_info.activebufs = (bigbuf==1) ? 2 : 3;                // set number of active buffers based on BIGBUF

    /**
     * set bitpix based on SAMPLEMODE
     */
    int samplemode=-1;
    std::istringstream( this->configmap["SAMPLEMODE"].value ) >> samplemode;  // SAMPLEMODE=0 for 16bpp, =1 for 32bpp
    if (samplemode < 0) {
      message.str(""); message << "bad or missing SAMPLEMODE from " << this->camera_info.configfilename;
      logwrite(function, message.str());
      return (ERROR);
    }
    this->camera_info.bitpix = (samplemode==0) ? 16 : 32;

    long error = NO_ERROR;
    if ((error == NO_ERROR) && paramchanged)  error = this->archon_cmd(LOADPARAMS);
    if ((error == NO_ERROR) && configchanged) error = this->archon_cmd(APPLYCDS);

    // Get the current frame buffer status
    if (error == NO_ERROR) error = this->get_frame_status();
    if (error != NO_ERROR) {
      logwrite(function, "error unable to get frame status");
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
        message.str(""); message << "error bad bitpix " << this->camera_info.bitpix;
        logwrite(function, message.str());
        return (ERROR);
      }
    }
    if (error != NO_ERROR) {
      logwrite(function, "error setting axes");
      return (ERROR);
    }

    message.str(""); message << "will use " << this->camera_info.bitpix << " bits per pixel";
    logwrite(function, message.str());

    // allocate image_data in blocks because the controller outputs data in units of blocks
    //
    this->image_data_bytes = (uint32_t) floor( ((this->camera_info.image_memory * num_ccds) + BLOCK_LEN - 1 ) / BLOCK_LEN ) * BLOCK_LEN;

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
  long Interface::load_mode_settings(int mode) {
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
    for (cfg_it  = this->modeinfo[mode].configmap.begin();
         cfg_it != this->modeinfo[mode].configmap.end();
         cfg_it++) {
      error = this->write_config_key( cfg_it->first.c_str(), cfg_it->second.value.c_str(), configchanged );
      if (error != NO_ERROR) {
        errstr  << "error writing config key:" << cfg_it->first << " value:" << cfg_it->second.value
                << " for mode " << this->Observing_mode_str[mode];
        break;
      }
    }

    /**
     * if no errors from writing config keys, then
     * iterate through the parammap, writing each parameter in the map
     */
    if (error == NO_ERROR) {
      for (param_it  = this->modeinfo[mode].parammap.begin();
           param_it != this->modeinfo[mode].parammap.end();
           param_it++) {
        error = this->write_parameter( param_it->first.c_str(), param_it->second.value.c_str(), paramchanged );
        message.str(""); message << "paramchanged=" << (paramchanged?"true":"false");
        logwrite(function, message.str());
        if (error != NO_ERROR) {
          errstr  << "error writing parameter key:" << param_it->first << " value:" << param_it->second.value
                  << " for mode " << this->Observing_mode_str[mode];
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
      message.str(""); message << "loaded mode: " << this->Observing_mode_str[mode];
      logwrite(function, message.str());
    }
    else {
      logwrite(function, errstr.str());
    }

    /**
     * read back some TAPLINE information
     */
    std::istringstream( this->configmap["TAPLINES"].value ) >> this->taplines;  // total number of taps

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
          logwrite(function, "error invalid AD number: unable to convert to integer");
          return(ERROR);
        }
        catch (std::out_of_range) {
          logwrite(function, "AD number out of integer range");
          return(ERROR);
        }
        if ( (adnum < 0) || (adnum > MAXADCHANS) ) {
          message.str(""); message << "error: ADC channel " << adnum << " outside range {0:" << MAXADCHANS << "}";
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
          logwrite(function, "error invalid GAIN and/or OFFSET: unable to convert to integer");
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
      logwrite(function, "error sending FRAME command");
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
        message << "error: invalid number of tokens (" << subtokens.size() << ") in FRAME message:";
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
          if (subtokens[0].substr(4)=="BASE") {                     // for "BUFnBASE=VALUE" the value is unit64
            lvalue  = std::stol( subtokens[1] );                    // this value will get assigned to the corresponding parameter
          }
          else                                                      // everything else is an int
            value  = std::stoi( subtokens[1] );                     // this value will get assigned to the corresponding parameter
        }
        catch (std::invalid_argument ) {
          logwrite(function, "error invalid buffer or value: unable to convert to integer");
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
          message.str(""); message << "error: buffer number " << bufnum << " outside range {1:" << Archon::nbufs << "}";
          logwrite(function, message.str());
          return(ERROR);
        }
        bufnum--;   // subtract 1 because it is 1-based in the message but need 0-based for the indexing
        if (subtokens[0].substr(4) == "SAMPLE")      this->frame.bufsample[bufnum]      =  value;
        if (subtokens[0].substr(4) == "COMPLETE")    this->frame.bufcomplete[bufnum]    =  value;
        if (subtokens[0].substr(4) == "MODE")        this->frame.bufmode[bufnum]        =  value;
        if (subtokens[0].substr(4) == "BASE")        this->frame.bufbase[bufnum]        = lvalue;
        if (subtokens[0].substr(4) == "FRAME")       this->frame.bufframe[bufnum]       =  value;
        if (subtokens[0].substr(4) == "WIDTH")       this->frame.bufwidth[bufnum]       =  value;
        if (subtokens[0].substr(4) == "HEIGHT")      this->frame.bufheight[bufnum]      =  value;
        if (subtokens[0].substr(4) == "PIXELS")      this->frame.bufpixels[bufnum]      =  value;
        if (subtokens[0].substr(4) == "LINES")       this->frame.buflines[bufnum]       =  value;
        if (subtokens[0].substr(4) == "RAWBLOCKS")   this->frame.bufrawblocks[bufnum]   =  value;
        if (subtokens[0].substr(4) == "RAWLINES")    this->frame.bufrawlines[bufnum]    =  value;
        if (subtokens[0].substr(4) == "RAWOFFSET")   this->frame.bufrawoffset[bufnum]   =  value;
        if (subtokens[0].substr(4) == "TIMESTAMP")   this->frame.buftimestamp[bufnum]   =  value;
        if (subtokens[0].substr(4) == "RETIMESTAMP") this->frame.bufretimestamp[bufnum] =  value;
        if (subtokens[0].substr(4) == "FETIMESTAMP") this->frame.buffetimestamp[bufnum] =  value;
      }
    }

    newestbuf   = this->frame.index;

    if (this->frame.index < (int)this->frame.bufframe.size()) {
      newestframe = this->frame.bufframe[this->frame.index];
    }
    else {
      message.str(""); message << "error: index " << this->frame.index << " exceeds number of buffers " << this->frame.bufframe.size();
      logwrite(function, message.str());
      return(ERROR);
    }

    // loop through the number of buffers
    //
    int num_zero = 0;
    for (int bc=0; bc<Archon::nbufs; bc++) {

      // look for special start-up case, when all frame buffers are zero
      //
      if ( this->frame.bufframe[bc] == 0 ) num_zero++;

      if ( (this->frame.bufframe[bc] > newestframe) &&
            this->frame.bufcomplete[bc] ) {
        newestframe = this->frame.bufframe[bc];
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
      message << std::setfill(' ') << std::setw(5) << std::dec << this->frame.bufframe[bufn] << " ";  // frame
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
      message.str(""); message << "error locking frame buffer " << buffer;
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
      message.str(""); message << "error unrecognized timer response: " << reply;
      logwrite(function, message.str());
      return(ERROR);
    }

    // Second token must be a hexidecimal string
    //
    std::string timer_str = tokens[1]; 
    timer_str.erase(timer_str.find('\n'));  // remove newline
    if (!std::all_of(timer_str.begin(), timer_str.end(), ::isxdigit)) {
      message.str(""); message << "error unrecognized timer value: " << timer_str;
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
      message.str(""); message << "error: requested address 0x" << std::hex << bufaddr << " outside range {0xA0000000:0x" << maxaddr << "}";
      logwrite(function, message.str());
      return(ERROR);
    }
    if (bufblocks > maxblocks) {
      message.str(""); message << "error: requested blocks 0x" << std::hex << bufblocks << " outside range {0:0x" << maxblocks << "}";
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
      logwrite(function, "error converting command to uppercase");
      return(ERROR);
    }

    if (this->archon_cmd(scmd) == ERROR) {
      logwrite(function, "error sending FETCH command. Aborting read.");
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
   * @return 
   *
   */
  long Interface::read_frame() {
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
    int num_ccds = this->geometry[this->camera_info.current_observing_mode].num_ccds;

    // Check that image buffer is prepared  //TODO should I call prepare_image_buffer() here, automatically?
    //
    if ( (this->image_data == NULL)    ||
         (this->image_data_bytes == 0) ||
         (this->image_data_allocated != this->image_data_bytes) ) {
      logwrite(function, "error: image buffer not ready");
      return(ERROR);
    }

    // Get the current frame buffer status
    error = this->get_frame_status();
    if (error != NO_ERROR) {
      logwrite(function, "error unable to get frame status");
      return(error);
    }

    // Archon buffer number of the last frame read into memory
    //
    bufready = this->frame.index + 1;

    if (bufready < 1 || bufready > this->camera_info.activebufs) {
      message.str(""); message << "error: invalid buffer " << bufready;
      logwrite(function, message.str());
      return(ERROR);
    }

    message.str(""); message << "will read " << (this->camera_info.frame_type == FRAME_RAW ? "raw" : "image")
                             << " data from Archon controller buffer " << bufready << " frame " << this->frame.frame;
    logwrite(function, message.str());

    // Lock the frame buffer before reading it
    //
    if ((error=this->lock_buffer(bufready)) == ERROR) return (error);

    // Send the FETCH command to read the memory buffer from the Archon backplane.
    // Archon replies with one binary response per requested block. Each response
    // has a message header.
    //
    switch (this->camera_info.frame_type) {
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
        logwrite(function, "error: unknown frame type specified");
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
          logwrite(function, "error timeout waiting for data from Archon");
          return(ERROR);
        }
      }

      // Check message header
      //
      SNPRINTF(check, "<%02X:", this->msgref);
      if ( (retval=this->archon.Read(header, 4)) != 4 ) {
        message.str(""); message << "error " << retval << " reading header";
        logwrite(function, message.str());
        error = ERROR;
        break;
      }
      if (header[0] == '?') {  // Archon retured an error
        message.str(""); message << "error reading " << (this->camera_info.frame_type==FRAME_RAW?"raw ":"image ") << " data";
        logwrite(function, message.str());
        this->fetchlog();      // check the Archon log for error messages
        error = ERROR;
        break;
      }
      else if (strncmp(header, check, 4) != 0) {
        message.str(""); message << "Archon command-reply mismatch reading " << (this->camera_info.frame_type==FRAME_RAW?"raw ":"image ")
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
      message.str(""); message << "error incomplete frame read " << std::dec 
                               << totalbytesread << " bytes: " << block << " of " << bufblocks << " 1024-byte blocks";
      logwrite(function, message.str());
    }

    // Unlock the frame buffer
    //
    if (error == NO_ERROR) error = this->archon_cmd(UNLOCK);

    // On success, write the value to the log and return
    //
    if (error == NO_ERROR) {
      message.str(""); message << "successfully read " << totalbytesread << (this->camera_info.frame_type==FRAME_RAW?" raw ":"")
                               << " bytes (0x" << std::uppercase << std::hex << bufblocks << " blocks) from Archon controller";
      logwrite(function, message.str());
    }
    // Throw an error for any other errors
    //
    else {
      logwrite(function, "error reading Archon camera data to memory!");
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
   * The camera_info class was copied into fits_info when the exposure was started,
   * so use fits_info from here on out.
   *
   */
  long Interface::write_frame() {
    std::string function = "Archon::Interface::write_frame";
    std::stringstream message;
    uint32_t   *cbuf32;                  //!< used to cast char buf into 32 bit int
    uint16_t   *cbuf16;                  //!< used to cast char buf into 16 bit int
    long        error;

    message.str(""); message << "writing " << this->fits_info.bitpix << "-bit data from memory to disk";
    logwrite(function, message.str());

    // The Archon sends four 8-bit numbers per pixel. To convert this into something usable,
    // cast the image buffer into integers. Handled differently depending on bits per pixel.
    //
    switch (this->fits_info.bitpix) {

      // convert four 8-bit values into a 32-bit value and scale by 2^16
      //
      case 32: {
        FITS_file <float> fits_file;                       // Instantiate a FITS_file object with the appropriate type
        cbuf32 = (uint32_t *)this->image_data;             // cast here to 32b
        float *fbuf = NULL;
        fbuf = new float[ this->fits_info.image_size ];    // allocate a float buffer of same number of pixels

        for (long pix=0; pix < this->fits_info.image_size; pix++) {
          fbuf[pix] = cbuf32[pix] / (float)65535;          // right shift 16 bits
        }

        // write the image and increment image_num on success
        //
        error = fits_file.write_image(fbuf, this->fits_info);
        if ( error == NO_ERROR ) {
          this->common.increment_imnum();
        }
        else {
          logwrite(function, "error writing image");
        }
        if (fbuf != NULL) {
          delete [] fbuf;
        }
        break;
      }

      // convert four 8-bit values into 16 bit values and no scaling necessary
      //
      case 16: {
        FITS_file <unsigned short> fits_file;              // Instantiate a FITS_file object with the appropriate type
        cbuf16 = (uint16_t *)this->image_data;             // cast to 16b
        error = fits_file.write_image(cbuf16, this->fits_info);
        break;
      }

      // shouldn't happen
      //
      default:
        message.str(""); message << "error unrecognized bits per pixel: " << this->fits_info.bitpix;
        logwrite(function, message.str());
        error = ERROR;
        break;
    }

    return(error);
  }
  /**************** Archon::Interface::write_frame ****************************/


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
      logwrite(function, "error: key|value cannot have NULL");
    }

    else

    if ( this->configmap.find(key) == this->configmap.end() ) {
      error = ERROR;
      message.str(""); message << "error: key " << key << " not found in configmap";
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
      message.str(""); message << "sending: archon_cmd(" << sscmd << ")";
      logwrite(function, message.str());
      error=this->archon_cmd((char *)sscmd.str().c_str());   // send the WCONFIG command here
      if (error==NO_ERROR) {
        this->configmap[key].value = newvalue;               // save newvalue in the STL map
        changed = true;
      }
      else {
        message.str(""); message << "error: config key=value: " << key << "=" << newvalue << " not written";
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
      logwrite(function, "error: paramname|value cannot have NULL");
    }

    else

    if ( this->parammap.find(paramname) == this->parammap.end() ) {
      error = ERROR;
      message.str(""); message << "error parameter \"" << paramname << "\" not found in parammap";
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


  /**************** Archon::Interface::expose *********************************/
  /**
   * @fn     expose
   * @brief  triggers an Archon exposure by setting the EXPOSE parameter = 1
   * @param  none
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::expose() {
    std::string function = "Archon::Interface::expose";
    std::stringstream message;
    long error;

    // exposeparam is set by the configuration file, CONFIG_FILE
    // check to make sure it was set, or else expose won't work
    //
    if (this->exposeparam.empty()) {
      message.str(""); message << "error EXPOSE_PARAM not set in configuration file " << CONFIG_FILE;
      logwrite(function, message.str());
      return(ERROR);
    }

    error = this->prep_parameter(this->exposeparam, "1");
    if (error == NO_ERROR) error = this->load_parameter(this->exposeparam, "1");

    // get system time and Archon's timer after exposure starts
    // start_time is used by the FITS writer ?? //TODO (maybe can remove)
    //
    if (error == NO_ERROR) {
      this->camera_info.start_time = get_time_string();             // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
      error = this->get_timer(&this->start_time);                   // Archon internal timer
    }

    if (error == NO_ERROR) {
      message.str(""); message << "exposure started at Archon time " << this->start_time;
      logwrite(function, message.str());
    }
    if (error == NO_ERROR) error = this->prepare_image_buffer();

    this->camera_info.fits_name = this->common.get_fitsname();      // assemble the FITS filenmae

    this->camera_info.userkeys.keydb = this->userkeys.keydb;        // copy the userkeys database object into camera_info

    // add any keys from the ACF file (from modeinfo[mode].acfkeys) into the camera_info.userkeys object
    //
    int mode = this->camera_info.current_observing_mode;
    Common::FitsKeys::fits_key_t::iterator keyit;
    for (keyit  = this->modeinfo[mode].acfkeys.keydb.begin();
         keyit != this->modeinfo[mode].acfkeys.keydb.end();
         keyit++) {
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyword    = keyit->second.keyword;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keytype    = keyit->second.keytype;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyvalue   = keyit->second.keyvalue;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keycomment = keyit->second.keycomment;
    }

    this->fits_info = this->camera_info;                            // copy the camera_info class, to be given to fits writer

    return (error);
  }
  /**************** Archon::Interface::expose *********************************/


}
