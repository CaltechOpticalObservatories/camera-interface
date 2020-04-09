/**
 * @file    archon.cpp
 * @brief   common interface functions
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include "logentry.h"
#include "common.h"
#include "archon.h"
#include "tcplinux.h"
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
    this->connection_open = false;
    this->current_state = "uninitialized";
    this->sockfd = -1;
    this->msgref = 0;
    this->camera_info.hostname = "192.168.1.2";
//  this->camera_info.hostname = "10.0.0.2";
    this->camera_info.port=4242;
    this->image_data = NULL;
    this->image_data_bytes = 0;
    this->image_data_allocated = 0;
  }

  // Archon::Interface deconstructor
  //
  Interface::~Interface() {
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
    const char* function = "Archon::Interface::prepare_image_buffer";

    // If there is already a correctly-sized buffer allocated,
    // then don't do anything except initialize that space to zero.
    //
    if ( (this->image_data != NULL)     &&
         (this->image_data_bytes != 0) &&
         (this->image_data_allocated == this->image_data_bytes) ) {
      memset(this->image_data, 0, this->image_data_bytes);
      Logf("(%s) initialized %d bytes of image_data memory\n", function, this->image_data_bytes);
    }

    // If memory needs to be re-allocated, delete the old buffer
    //
    else {
      if (this->image_data != NULL) {
        Logf("(%s) delete image_data\n", function);
        delete [] this->image_data;
        this->image_data=NULL;
      }
      // Allocate new memory
      //
      if (this->image_data_bytes != 0) {
        this->image_data = new char[this->image_data_bytes];
        this->image_data_allocated=this->image_data_bytes;
        Logf("(%s) allocated %d bytes for image_data\n", function, this->image_data_bytes);
      }
      else {
        Logf("(%s) cannot allocate zero-length image memory\n", function);
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
    const char* function = "Archon::Interface::connect_controller";
    long   error = ERROR;

    if (this->connection_open == true) {
      Logf("(%s) camera connection already open\n", function);
      return(NO_ERROR);
    }

    // Initialize the camera connection
    //
    Logf("(%s) opening a connection to the camera system\n", function);

    this->current_state = "initializing";

    // open socket connection to camera controller
    //
    if ( (this->sockfd = connect_to_server( this->camera_info.hostname.c_str(), this->camera_info.port ) ) < 0 ) {
      this->current_state = "uninitialized";
      Logf("(%s) error %d connecting to %s:%d\n", function, errno, this->camera_info.hostname.c_str(), this->camera_info.port);
      return(ERROR);
    }
    this->connection_open = true;

    // this->sockfd contains the socket file descriptor to the new conncection
    //
    Logf("(%s) socket connection to %s:%d established on fd %d\n", function, 
         this->camera_info.hostname.c_str(), this->camera_info.port, this->sockfd);

    // empty the Archon log
    //
    error = this->fetchlog();
    this->current_state = "initialized";

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
    const char* function = "Archon::Interface::disconnect_controller";
    long error;
    if (!this->connection_open) {
      Logf("(%s) connection already closed\n", function);
    }
    // close the socket file descriptor to the Archon controller
    //
    if (this->sockfd) {
      if (close(this->sockfd) == 0) {
        this->connection_open = false;
        this->current_state = "uninitialized";
        error = NO_ERROR;
      }
      else {
        error = ERROR;
      }
    }
    else {
      error = NO_ERROR;
    }

    // Free the memory
    //
    Logf("(%s) releasing allocated device memory\n", function);
    if (this->image_data != NULL) {
      delete [] this->image_data;
      this->image_data=NULL;
    }

    // On success, write the value to the log and return
    //
    if (error == NO_ERROR) {
      Logf("(%s) Archon connection terminated\n", function);
    }
    // Throw an error for any other errors
    //
    else {
      Logf("(%s) error disconnecting Archon camera!\n", function);
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
  long Interface::archon_native(std::string cmd) { // use this form when the calling
    char reply[REPLY_LEN];                         // function doesn't need to look at the reply
    long ret = archon_cmd(cmd, reply);
    if (strlen(reply) > 0) {
      Logf("native reply: %s\n", reply);
    }
    return( ret );
  }
  /**************** Archon::Interface::archon_native **************************/


  /**************** Archon::Interface::archon_cmd *****************************/
  /**
   * @fn     archon_cmd
   * @brief
   * @param  none
   * @return 
   *
   */
  long Interface::archon_cmd(std::string cmd) { // use this form when the calling
    char reply[REPLY_LEN];                      // function doesn't need to look at the reply
    return( archon_cmd(cmd, reply) );
  }
  long Interface::archon_cmd(std::string cmd, char *reply) {
    const char* function = "Archon::Interface::archon_cmd";
    int     retval;
    char    check[4];
    char    buffer[BLOCK_LEN];                  //!< temporary buffer for holding Archon replies
    int     error = NO_ERROR;

    unsigned long bufcount;

    if (!this->connection_open) {               // nothing to do if no connection open to controller
      Logf("(%s) error: connection not open to controller\n", function);;
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
    std::transform( prefix.begin(), prefix.end(), prefix.begin(), ::toupper );    // make uppercase

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
      Logf("(%s) sending command: %s\n", function, fcmd.c_str());
    }

    // send the command
    //
    if ( (sock_write(this->sockfd, (char *)scmd.c_str())) == -1) {
      Logf("(%s) error writing to camera socket\n", function);
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
    memset(reply, '\0', REPLY_LEN);                  // zero reply buffer
    bufcount = 0;                                    // zero bytes read counter
    while (1) {
      memset(buffer, '\0', BLOCK_LEN);               // temporary buffer

      if ( (retval=Poll(this->sockfd, POLLTIMEOUT)) <= 0) {
        if (retval==0) { Logf("(%s) Poll timeout\n", function); error = TIMEOUT; }
        if (retval<0)  { Logf("(%s) Poll error\n", function);   error = ERROR;   }
        break;
      }
      if ( (retval = read(this->sockfd, buffer, BLOCK_LEN)) <= 0) {  // read into temp buffer
        break;
      }

      bufcount += retval;                            // keep track of the total bytes read

      if (bufcount < REPLY_LEN) {                    // make sure not to overflow buffer with strncat
        strncat(reply, buffer, (size_t)retval);      // cat into reply buffer
      }
      else {
        error = ERROR;
        Logf("(%s) error: buffer overflow: %d bytes in response to command: %s\n", function, bufcount, cmd.c_str());
        break;
      }
      if (strstr(reply, "\n")) {
        break;
      }
    } // end while(1)

    // The first three bytes of the reply should contain the msgref of the
    // command, which can be used as a check that the received reply belongs
    // to the command which was sent.
    //
    // Error processing command (no other information is provided by Archon)
    //
    if (strncmp(reply, "?", 1) == 0) {
      error = ERROR;
      Logf("(%s) Archon controller returned error processing command: %s\n", function, cmd.c_str());
    }
    else
    if (strncmp(reply, check, 3) != 0) {              // Command-Reply mismatch
      error = ERROR;
      std::string hdr = reply;
      Logf("(%s) error: command-reply mismatch for command %s: received header:%s\n", 
           function, cmd.c_str(), hdr.substr(0,3).c_str());
    }
    else {                                            // command and reply are a matched pair
      error = NO_ERROR;

      // log the command as long as it's not a STATUS, TIMER, WCONFIG or FRAME command
      if ( (cmd.compare(0,7,"WCONFIG") != 0) &&
           (cmd.compare(0,5,"TIMER") != 0)   &&
           (cmd.compare(0,6,"STATUS") != 0)  &&
           (cmd.compare(0,5,"FRAME") != 0) ) {
        Logf("(%s) command 0x%02X success\n", function, this->msgref);
      }

      memmove( reply, reply+3, strlen(reply) );       // strip off the msgref from the reply
      this->msgref = (this->msgref + 1) % 256;        // increment msgref
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
   * @param  valstring  reference to string for return value
   * @return ERROR on error, NO_ERROR if okay.
   *
   * The string reference contains the value of the parameter
   * to be returned to the user.
   *
   * No direct calls to Archon -- this function uses archon_cmd()
   * which in turn handles all of the Archon in-use locking.
   *
   */
  long Interface::read_parameter(std::string paramname, std::string &valstring) {
    const char* function = "Archon::Interface::read_parameter";
    std::stringstream cmd;
    int   error   = NO_ERROR;
    char *lineptr = NULL;
    char  reply[REPLY_LEN];

    if (this->parammap.find(paramname.c_str()) == this->parammap.end()) {
      Logf("(%s) error: parameter \"%s\" not found\n", function, paramname.c_str());
      return(ERROR);
    }

    // form the RCONFIG command to send to Archon
    //
    cmd.str("");
    cmd << "RCONFIG"
        << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
        << this->parammap[paramname.c_str()].line;
    error = this->archon_cmd(cmd.str(), reply);          // send RCONFIG command here
    reply[strlen(reply)-1]=0;                            // strip newline

    // reply should be of the form PARAMETERn=PARAMNAME=VALUE
    // and we want just the VALUE here
    //
    lineptr = reply;

    if (strncmp(lineptr, "PARAMETER", 9) == 0) {         // lineptr: PARAMETERn=PARAMNAME=VALUE
      if ( strsep(&lineptr, "=") == NULL ) error=ERROR;  // lineptr: PARAMNAME=VALUE
      if ( strsep(&lineptr, "=") == NULL ) error=ERROR;  // lineptr: VALUE
    }
    else {
      error = ERROR;
    }

    if (error != NO_ERROR) {
      Logf("(%s) error:  malformed reply: %s\n", function, reply);
      valstring = "NaN";                                 // return "NaN" for parameter value on error
      error = ERROR;
    }
    else {
      valstring = lineptr;                               // return parameter value
      Logf("(%s) %s = %s\n", function, paramname.c_str(), valstring.c_str());
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
    const char* function = "Archon::Interface::prep_parameter";
    std::stringstream scmd;
    int error = NO_ERROR;

    // Prepare to apply it to the system -- will be loaded on next EXTLOAD signal
    //
    scmd << "FASTPREPPARAM " << paramname << " " << value;
    if (error == NO_ERROR) error = this->archon_cmd(scmd.str());

    if (error != NO_ERROR) {
      Logf("(%s) error writing parameter \"%s=%s\" to configuration memory\n", function, paramname.c_str(), value.c_str());
    }
    else {
      Logf("(%s) parameter: %s written to configuration memory\n", function, paramname.c_str());
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
    const char* function = "Archon::Interface::load_parameter";
    std::stringstream scmd;
    int error = NO_ERROR;

    scmd << "FASTLOADPARAM " << paramname << " " << value;

    if (error == NO_ERROR) error = this->archon_cmd(scmd.str());
    if (error != NO_ERROR) {
      Logf("(%s) error loading parameter \"%s=%s\" into Archon\n", function, paramname.c_str(), value.c_str());
    }
    else {
      Logf("(%s) parameter \"%s=%s\" loaded into Archon\n", function, paramname.c_str(), value.c_str());
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
    int  retval;
    char reply[REPLY_LEN];
    const char* function = "Archon::Interface::fetchlog";

    // send FETCHLOG command while reply is not (null)
    //
    do {
      if ( (retval=this->archon_cmd(FETCHLOG, reply))!=NO_ERROR ) {          // send command here
        Logf("(%s) error %d calling FETCHLOG\n", function, retval);
        return(retval);
      }
      if (strncmp(reply, "(null)", strlen("(null)"))!=0) {
        reply[strlen(reply)-1]=0;                                            // strip newline
        Logf("(%s) %s\n", function, reply);                                  // log reply here
      }
    } while (strncmp(reply, "(null)", strlen("(null)")) != 0);               // stop when reply is (null)

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
   */
  long Interface::load_config(std::string cfgfile) {
    const char* function = "Archon::Interface::load_config";
    FILE    *fp;
    char    *line=NULL;  // if NULL, getline(3) will malloc/realloc space as required
    size_t   len=0;      // must set=0 for getline(3) to automatically malloc
    char    *lineptr, *keyptr, *valueptr, *paramnameptr, *paramvalptr;
    ssize_t  read;
    int      linecount;
    int      error=NO_ERROR;
    int      obsmode = -1;
    bool     begin_parsing=FALSE;

    this->camera_info.configfilename = cfgfile;

    Logf("(%s) %s\n", function, cfgfile.c_str());

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
    if ( (fp = fopen(cfgfile.c_str(), "r")) == NULL ) {
      Logf("(%s) opening Archon Config File: %s: errno: %d\n", function, cfgfile.c_str(), errno);
      return(ERROR);
    }

    /**
     * default all modes undefined,
     * set defined=TRUE only when entries are found in the .acf file
     * and remove all elements from the map containers
     */
    for (int modenum=0; modenum<NUM_OBS_MODES; modenum++) {
      this->modeinfo[modenum].defined = FALSE;
      this->modeinfo[modenum].rawenable = -1;    //!< undefined. Set in ACF.
      this->modeinfo[modenum].parammap.clear();
      this->modeinfo[modenum].configmap.clear();
      this->modeinfo[modenum].fits.userkeys.clear();
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
        this->util.chrrep(line, '[',  127);      // remove [ bracket (replace with DEL)
        this->util.chrrep(line, ']',  127);      // remove ] bracket (replace with DEL)
        this->util.chrrep(line, '\n', 127);      // remove newline (replace with DEL)
        if (line!=NULL) modesection=line;        // create a string object out of the rest of the line

        // loop through all possible observing modes
        // to validate this mode
        //
        for (int modenum=0; modenum<NUM_OBS_MODES; modenum++) {
          if (modesection == Observing_mode_str[modenum]) {
            obsmode = modenum;  // here is the observing mode for this section
            Logf("(%s) found section for mode %d: %s\n", function, obsmode, modesection.c_str());
            begin_parsing=TRUE;
            continue;
          }
        }
        if (obsmode == -1) {
          Logf("(%s) error: unrecognized observation mode section found in .acf file: %s\n", function, modesection.c_str());
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

      this->util.chrrep(line, '\\', '/');                 // replace backslash with forward slash
      this->util.chrrep(line, '\"', 127);                 // remove all quotes (replace with DEL)

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
        this->util.chrrep(lineptr, '\n', 127);                            // remove newline (replace with DEL)

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
        this->util.chrrep(lineptr, '\n', 127);                            // remove newline (replace with DEL)
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
          Logf("(%s) error: unrecognized internal parameter specified: %s\n", function, internal_key.c_str());
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
        this->util.chrrep(lineptr, '\n', 127);                            // remove newline (replace with DEL)

        // First, tokenize on the equal sign "=".
        // The token left of "=" is the keyword. Immediate right is the value
        this->util.Tokenize(lineptr, tokens, "=");
        if (tokens.size() != 2) {                                         // need at least two tokens at this point
          Logf("(%s) error: token mismatch: expected KEYWORD=value//comment (found too many ='s)\n", function);
          if (line) free(line); fclose(fp);                               // getline() mallocs a buffer for line
          return(ERROR);
        }
        keyword   = tokens[0].substr(0,8);                                // truncate keyword to 8 characters
        keystring = tokens[1];                                            // tokenize the rest in a moment

        // Next, tokenize on the slash "/".
        // The token left of "/" is the value. Anything to the right is a comment.
        this->util.Tokenize(keystring, tokens, "/");
        keyvalue   = tokens[0];

        if (tokens.size() == 2) {      // If there are two tokens then the second is a comment,
          keycomment = tokens[1];
        } else {                       // but the comment is optional.
          keycomment = "";
        }

        // Set the key type based on the contents of the value string.
        // The type will be used in the fits_file.add_user_key(...) call.
        if (this->camera_info.fits.get_keytype(keyvalue) == Common::FitsTools::TYPE_INTEGER) {
          keytype = "INT";
        }
        else
        if (this->camera_info.fits.get_keytype(keyvalue) == Common::FitsTools::TYPE_DOUBLE) {
          keytype = "REAL";
        }
        else {      // if not an int or float then a string by default
          keytype = "STRING";
        }

        // Save all of the user keyword information in a map for later
        //
        this->modeinfo[obsmode].fits.userkeys[keyword].keyword    = keyword;
        this->modeinfo[obsmode].fits.userkeys[keyword].keytype    = keytype;
        this->modeinfo[obsmode].fits.userkeys[keyword].keyvalue   = keyvalue;
        this->modeinfo[obsmode].fits.userkeys[keyword].keycomment = keycomment;
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
      Logf("(%s) loaded Archon config file OK\n", function);
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
    const char* function = "Archon::Interface::set_camera_mode";
    bool configchanged = false;
    bool paramchanged = false;

    /**
     * first check is that mode is within allowable range
     */
    if (mode < 0 || mode >= NUM_OBS_MODES) {
      Logf("(%s) invalid mode %s (%d)", function, this->Observing_mode_str[mode], mode);
      return(ERROR);
    }

    /**
     * there needs to have been a modes section defined in the .acf file
     * in order to switch to that mode, or else undesirable behavior may result
     */
    if (!this->modeinfo[mode].defined) {
      Logf("(%s) error undefined mode %s in ACF file %s\n", function,
            this->Observing_mode_str[mode], this->camera_info.configfilename.c_str());
      return(ERROR);
    }

    this->camera_info.current_observing_mode = mode;

    Logf("(%s) requested mode: %d: %s\n", function, mode, this->Observing_mode_str[mode]);

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
        Logf("(%s) error invalid frame type: %d\n", function, this->camera_info.frame_type);
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
        Logf("(%s) error invalid mode: %d\n", function, mode);
        return(ERROR);
        break;
    }

    /**
     * set current number of Archon buffers and resize local memory
     */
    int bigbuf=-1;
    std::istringstream( this->configmap["BIGBUF"].value  ) >> bigbuf;  // get value of BIGBUF from loaded acf file
    this->camera_info.nbufs = (bigbuf==1) ? 2 : 3;                     // set number of buffers based on BIGBUF

    /**
     * set bitpix based on SAMPLEMODE
     */
    int samplemode=-1;
    std::istringstream( this->configmap["SAMPLEMODE"].value ) >> samplemode;  // SAMPLEMODE=0 for 16bpp, =1 for 32bpp
    if (samplemode < 0) {
      Logf("(%s) bad or missing SAMPLEMODE from %s\n", function, this->camera_info.configfilename.c_str());
      return (ERROR);
    }
    this->camera_info.bitpix = (samplemode==0) ? 16 : 32;

    this->frame.bufsample.resize( this->camera_info.nbufs );
    this->frame.bufcomplete.resize( this->camera_info.nbufs );
    this->frame.bufmode.resize( this->camera_info.nbufs );
    this->frame.bufbase.resize( this->camera_info.nbufs );
    this->frame.bufframe.resize( this->camera_info.nbufs );
    this->frame.bufwidth.resize( this->camera_info.nbufs );
    this->frame.bufheight.resize( this->camera_info.nbufs );
    this->frame.bufpixels.resize( this->camera_info.nbufs );
    this->frame.buflines.resize( this->camera_info.nbufs );
    this->frame.bufrawblocks.resize( this->camera_info.nbufs );
    this->frame.bufrawlines.resize( this->camera_info.nbufs );
    this->frame.bufrawoffset.resize( this->camera_info.nbufs );
    this->frame.buftimestamp.resize( this->camera_info.nbufs );
    this->frame.bufretimestamp.resize( this->camera_info.nbufs );
    this->frame.buffetimestamp.resize( this->camera_info.nbufs );

    long error = NO_ERROR;
    if ((error == NO_ERROR) && paramchanged)  error = this->archon_cmd(LOADPARAMS);
    if ((error == NO_ERROR) && configchanged) error = this->archon_cmd(APPLYCDS);

    // Get the current frame buffer status
    if (error == NO_ERROR) error = this->get_frame_status();
    if (error != NO_ERROR) {
      Logf("(%s) error unable to get frame status\n", function);
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
        Logf("(%s) error bad bitpix %d\n", function, this->camera_info.bitpix);
        return (ERROR);
      }
    }
    if (error != NO_ERROR) {
      Logf("(%s) error setting axes\n", function);
      return (ERROR);
    }

    Logf("(%s) will use %d bits per pixel\n", function, this->camera_info.bitpix);

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
    const char* function = "Archon::Interface::load_mode_settings";

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
        Logf("(%s) paramchanged=%s\n", function, (paramchanged?"true":"false"));
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
      Logf("(%s) loaded mode: %s", function, this->Observing_mode_str[mode]);
    }
    else {
      Logf("(%s) %s\n", function, errstr.str().c_str());
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
      this->util.Tokenize(this->configmap[tap.str().c_str()].value, tokens, ",");

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
        int adnum = atoi(adchan.c_str()) - 1;
        if ( (adnum < 0) || (adnum > MAXADCHANS) ) {
          Logf("(%s) error: ADC channel %d outside range {%d:%d}\n", function, adnum, 0, MAXADCHANS);
          return(ERROR);
        }
        this->gain  [ adnum ] = atoi(tokens[1].c_str()); // gain as function of AD channel
        this->offset[ adnum ] = atoi(tokens[2].c_str()); // offset as function of AD channel
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
    char *saveptr, *token, *substr=0, *key=0;
    int   i, bc, newestframe, newestbuf;
    int   error=NO_ERROR;
    char  buf[64];
    char  reply[REPLY_LEN];
    const char* function = "Archon::Interface::get_frame_status";

    /**
     * send FRAME command to get frame buffer status
     */
    if ( (error = this->archon_cmd(FRAME, reply)) ) {
      Logf("(%s) error sending FRAME command\n", function);
      return(error);
    }

    bool first_pass = 1;
    do {                                  // while (1)
      if (first_pass) {                   // first call to strtok_r is different
        if ( (token = strtok_r(reply, " ", &saveptr)) != NULL ) {
          substr = strstr(token, "=");
          substr++;
          key   = strsep(&token, "=");
        }
      }
      else {                              // subsequent strtok_r calls...
        if ((token = strtok_r(NULL, " ", &saveptr)) != NULL) {
          if ( (substr = strstr(token, "=")) == NULL ) break;
          substr++;
          if ( (key=strsep(&token, "=")) == NULL) break;
        }
        else break;
      }
      first_pass=0;                       // clear for subsequent strtok_r calls

      /**
       * match received key against keywords and store matching value
       * in appropriate variable in the framestatus structure.
       */
      if (strcmp(key, "TIMER")==0) SNPRINTF(this->frame.timer,"%s", substr);
      if (strcmp(key, "RBUF")==0) this->frame.rbuf = atoi(substr);
      if (strcmp(key, "WBUF")==0) this->frame.wbuf = atoi(substr);

      // loop through the number of buffers
      //
      for (i=0; i<this->camera_info.nbufs; i++) {
        SNPRINTF(buf, "BUF%dSAMPLE", i+1);
        if (strcmp(key, buf)==0) this->frame.bufsample[i]     =atoi(substr);
        SNPRINTF(buf, "BUF%dCOMPLETE", i+1);
        if (strcmp(key, buf)==0) this->frame.bufcomplete[i]   =atoi(substr);
        SNPRINTF(buf, "BUF%dMODE", i+1);
        if (strcmp(key, buf)==0) this->frame.bufmode[i]       =atoi(substr);
        SNPRINTF(buf, "BUF%dBASE", i+1);
        if (strcmp(key, buf)==0) this->frame.bufbase[i]       =atol(substr);
        SNPRINTF(buf, "BUF%dFRAME", i+1);
        if (strcmp(key, buf)==0) this->frame.bufframe[i]      =atoi(substr);
        SNPRINTF(buf, "BUF%dWIDTH", i+1);
        if (strcmp(key, buf)==0) this->frame.bufwidth[i]      =atoi(substr);
        SNPRINTF(buf, "BUF%dHEIGHT", i+1);
        if (strcmp(key, buf)==0) this->frame.bufheight[i]     =atoi(substr);
        SNPRINTF(buf, "BUF%dPIXELS", i+1);
        if (strcmp(key, buf)==0) this->frame.bufpixels[i]     =atoi(substr);
        SNPRINTF(buf, "BUF%dLINES", i+1);
        if (strcmp(key, buf)==0) this->frame.buflines[i]      =atoi(substr);
        SNPRINTF(buf, "BUF%dRAWBLOCKS", i+1);
        if (strcmp(key, buf)==0) this->frame.bufrawblocks[i]  =atoi(substr);
        SNPRINTF(buf, "BUF%dRAWLINES", i+1);
        if (strcmp(key, buf)==0) this->frame.bufrawlines[i]   =atoi(substr);
        SNPRINTF(buf, "BUF%dRAWOFFSET", i+1);
        if (strcmp(key, buf)==0) this->frame.bufrawoffset[i]  =atoi(substr);
        SNPRINTF(buf, "BUF%dTIMESTAMP", i+1);
        if (strcmp(key, buf)==0) this->frame.buftimestamp[i]  =atoi(substr);
        SNPRINTF(buf, "BUF%dRETIMESTAMP", i+1);
        if (strcmp(key, buf)==0) this->frame.bufretimestamp[i]=atoi(substr);
        SNPRINTF(buf, "BUF%dFETIMESTAMP", i+1);
        if (strcmp(key, buf)==0) this->frame.buffetimestamp[i]=atoi(substr);
      }
    } while (1);

    newestbuf   = this->frame.index;

    if (this->frame.index < (int)this->frame.bufframe.size()) {
      newestframe = this->frame.bufframe[this->frame.index];
    }
    else {
      Logf("(%s) error: index %d exceeds number of buffers %d\n", function, this->frame.index, this->frame.bufframe.size());
      return(ERROR);
    }

    // loop through the number of buffers
    //
    int num_zero = 0;
    for (bc=0; bc<this->camera_info.nbufs; bc++) {

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
    if (num_zero == this->camera_info.nbufs) {
      newestframe = 0;
      newestbuf   = 0;
    }

    /**
     * save index of newest buffer. From this we can find the newest frame, etc.
     */
    this->frame.index = newestbuf;

    return(error);
  }
  /**************** Archon::Interface::get_frame_status ***********************/


  /**************** Archon::Interface::get_frame_status ***********************/
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
    int bufn;
    int error = NO_ERROR;
    char statestr[this->camera_info.nbufs][4];
    const char* function = "Archon::Interface::print_frame_status";
    std::stringstream message;

    // write as log message
    //
    message << "    buf base       rawoff     frame ready lines rawlines rblks width height state";
    Logf("(%s) %s\n", function, message.str().c_str()); message.str("");
    message << "    --- ---------- ---------- ----- ----- ----- -------- ----- ----- ------ -----";
    Logf("(%s) %s\n", function, message.str().c_str()); message.str("");
    for (bufn=0; bufn < this->camera_info.nbufs; bufn++) {
      memset(statestr[bufn], '\0', 4);
      if ( (this->frame.rbuf-1) == bufn)   strcat(statestr[bufn], "R");
      if ( (this->frame.wbuf-1) == bufn)   strcat(statestr[bufn], "W");
      if ( this->frame.bufcomplete[bufn] ) strcat(statestr[bufn], "C");
    }
    for (bufn=0; bufn < this->camera_info.nbufs; bufn++) {
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
      Logf("(%s) %s\n", function, message.str().c_str()); message.str("");
    }
    return(error);
  }
  /**************** Archon::Interface::get_frame_status ***********************/


  /**************** Archon::Interface::lock_buffer ****************************/
  /**
   * @fn     lock_buffer
   * @brief  lock Archon frame buffer
   * @param  int frame buffer to lock
   * @return 
   *
   */
  long Interface::lock_buffer(int buffer) {
    const char* function = "Archon::Interface::lock_buffer";
    std::stringstream sscmd;

    sscmd.str("");
    sscmd << "LOCK" << buffer;
    if ( this->archon_cmd(sscmd.str()) ) {
      Logf("(%s) error locking frame buffer %d\n", function, buffer);
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
    const char* function = "Archon::Interface::get_timer";
    std::stringstream message, timer_ss;
    std::vector<std::string> tokens;
    int  error;
    char reply[REPLY_LEN];

    // send TIMER command to get frame buffer status
    //
    if ( (error = this->archon_cmd(TIMER, reply)) != NO_ERROR ) {
      return(error);
    }

    this->util.Tokenize(reply, tokens, "=");        // Tokenize the reply

    // Reponse should be "TIMER=xxxx\n" so there needs
    // to be two tokens
    //
    if (tokens.size() != 2) {
      Logf("(%s) error unrecognized timer response: %s\n", function, reply);
      return(ERROR);
    }

    // Second token must be a hexidecimal string
    //
    std::string timer_str = tokens[1]; 
    timer_str.erase(timer_str.find('\n'));  // remove newline
    if (!std::all_of(timer_str.begin(), timer_str.end(), ::isxdigit)) {
      Logf("(%s) error unrecognized timer value: %s\n", function, timer_str.c_str());
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
    const char* function = "Archon::Interface::fetch";
    uint32_t maxblocks = (uint32_t)(1.5E9 / this->camera_info.nbufs / 1024 );
    uint64_t maxoffset = this->camera_info.nbufs==2 ? 0xD0000000 : 0xE0000000;
    uint64_t maxaddr = maxoffset + maxblocks;

    if (bufaddr < 0xA0000000 || bufaddr > maxaddr) {
      Logf("(%s) error: requested address 0x%0X outside range {0xA0000000:0x%0X}\n", function, bufaddr, maxaddr);
      return(ERROR);
    }
    if (bufblocks > maxblocks) {
      Logf("(%s) error: requested blocks 0x%0X outside range {0:0x%0X}\n", function, bufblocks, maxblocks);
      return(ERROR);
    }

    std::stringstream sscmd;
    sscmd << "FETCH"
          << std::setfill('0') << std::setw(8) << std::hex
          << bufaddr
          << std::setfill('0') << std::setw(8) << std::hex
          << bufblocks;
    std::string scmd = sscmd.str();
    std::transform( scmd.begin(), scmd.end(), scmd.begin(), ::toupper );    // make uppercase

    if (this->archon_cmd(scmd) == ERROR) {
      Logf("(%s) error sending FETCH command. Aborting read.\n", function);
      this->archon_cmd(UNLOCK);                                             // unlock all buffers
      return(ERROR);
    }

    Logf("(%s) reading %s data with %s\n", function, 
          (this->camera_info.frame_type==FRAME_RAW?"raw":"image"), scmd.c_str());
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
    const char* function = "Archon::Interface::read_frame";
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
      Logf("(%s) error: image buffer not ready\n", function);
      return(ERROR);
    }

    // Archon buffer number of the last frame read into memory
    //
    bufready = this->frame.index + 1;

    if (bufready < 1 || bufready > this->camera_info.nbufs) {
      Logf("(%s) error: invalid buffer %d\n", function, bufready);
      return(ERROR);
    }

    Logf("(%s) will read %s data from Archon controller buffer %d\n", 
         function, (this->camera_info.frame_type == FRAME_RAW ? "raw" : "image"), bufready);

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
        Logf("(%s) error: unknown frame type specified\n", function);
        return(ERROR);
        break;
    }

    Logf("(%s) bufaddr=0x%0X image_memory=0x%0X bytes bufblocks=0x%0X\n", 
          function, bufaddr, this->camera_info.image_memory, bufblocks);

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
      if ( (retval=Poll(this->sockfd, POLLTIMEOUT)) <= 0) {
        if (retval==0) Logf("(%s) Poll timeout\n", function);
        if (retval<0)  Logf("(%s) Poll error\n", function);
        error = ERROR;
        break;
      }

      // Wait for a block+header Bytes to be available
      while ( fion_read(this->sockfd) < (BLOCK_LEN+4) )
        ;

      // Check message header
      //
      SNPRINTF(check, "<%02X:", this->msgref);
      if ( (retval=read(this->sockfd, header, 4)) != 4 ) {
        Logf("(%s) error %d reading header\n", function, retval);
        error = ERROR;
        break;
      }
      if (header[0] == '?') {  // Archon retured an error
        Logf("(%s) error reading %s data\n", function,
              (this->camera_info.frame_type==FRAME_RAW?"raw ":"image "));
        this->fetchlog();      // check the Archon log for error messages
        error = ERROR;
        break;
      }
      else if (strncmp(header, check, 4) != 0) {
        Logf("(%s) Archon command-reply mismatch reading %s data. header=%s check=%s\n", function,
              (this->camera_info.frame_type==FRAME_RAW?"raw ":"image "), header, check);
        error = ERROR;
        break;
      }

      // Read the frame contents
      //
      bytesread = 0;
      do {
        toread = BLOCK_LEN - bytesread;
        if ( (retval=read(this->sockfd, ptr_image, (size_t)toread)) > 0 ) {
          bytesread += retval;         // this will get zeroed after each block
          totalbytesread += retval;    // this won't (used only for info purposes)
          printf("%10d\b\b\b\b\b\b\b\b\b\b", totalbytesread);
          ptr_image += retval;         // advance pointer
        }
      } while (bytesread < BLOCK_LEN);

    } // end of loop: for (block=0; block<bufblocks; block++)

    // give back the archon_busy semaphore to allow other threads to access the Archon now
    //
    const std::unique_lock<std::mutex> lock(this->archon_mutex);
    this->archon_busy = false;
    this->archon_mutex.unlock();

    printf("%10d complete\n", totalbytesread);
    printf("   bytes read %d (%d 1024-Byte blocks)\n", totalbytesread, bufblocks);

    // Unlock the frame buffer
    //
    if (error == NO_ERROR) error = this->archon_cmd(UNLOCK);

    // On success, write the value to the log and return
    //
    return 0;
    if (error == NO_ERROR) {
      Logf("(%s) successfully read 0x%0X%s blocks (0x%0X bytes) from Archon controller\n",
            function, (this->camera_info.frame_type==FRAME_RAW?" raw ":""), totalbytesread);
    }
    // Throw an error for any other errors
    //
    else {
      Logf("(%s) error reading Archon camera data to memory! \n", function);
    }
    return(0);
  }
  /**************** Archon::Interface::read_frame *****************************/


  /**************** Archon::Interface::write_frame ****************************/
  /**
   * @fn     write_frame
   * @brief  creates a FITS_file object to write the image_data buffer to disk
   * @param  none
   * @return 
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
    const char* function = "Archon::Interface::write_frame";
    uint32_t   *cbuf32;                  //!< used to cast char buf into 32 bit int
    uint16_t   *cbuf16;                  //!< used to cast char buf into 16 bit int

    Logf("(%s) writing %d-bit data from memory to disk\n", function, this->fits_info.bitpix);

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

        fits_file.write_image(fbuf, this->fits_info);
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
        fits_file.write_image(cbuf16, this->fits_info);
        break;
      }

      // shouldn't happen
      //
      default:
        Logf("(%s) error unrecognized bits per pixel: %d\n", function, this->fits_info.bitpix);
        return (ERROR);
        break;
    }

    return(NO_ERROR);
  }
  /**************** Archon::Interface::write_frame ****************************/


  /**************** Archon::Interface::write_config_key ***********************/
  /**
   * @fn     write_config_key
   * @brief  write a configuration KEY=VALUE pair to the Archon controller
   * @param  key
   * @param  newvalue
   * @return 
   *
   */
  long Interface::write_config_key( const char *key, const char *newvalue, bool &changed ) {
    const char* function = "Archon::Interface::write_config_key";
    std::stringstream message, sscmd;
    int error=NO_ERROR;

    if ( key==NULL || newvalue==NULL ) {
      error = ERROR;
      Logf("(%s) error: key|value cannot have NULL\n", function);
    }

    else

    if ( this->configmap.find(key) == this->configmap.end() ) {
      error = ERROR;
      Logf("(%s) error: key %s not found in configmap\n", function, key);
    }

    else

    /**
     * If no change in value then don't send the command
     */
    if ( this->configmap[key].value == newvalue ) {
      error = NO_ERROR;
      Logf("(%s) config key %s=%s not written: no change in value\n", function, key, newvalue);
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
      Logf("(%s) sending: archon_cmd(%s)\n", function, sscmd.str().c_str());
      error=this->archon_cmd((char *)sscmd.str().c_str());   // send the WCONFIG command here
      if (error==NO_ERROR) {
        this->configmap[key].value = newvalue;               // save newvalue in the STL map
        changed = true;
      }
      else {
        Logf("(%s) error: config key=value: %s=%s not written\n", function, key, newvalue);
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
    const char* function = "Archon::Interface::write_parameter";
    std::stringstream message, sscmd;
    int error=NO_ERROR;

    if ( paramname==NULL || newvalue==NULL ) {
      error = ERROR;
      Logf("(%s) error: paramname|value cannot have NULL\n", function);
    }

    else

    if ( this->parammap.find(paramname) == this->parammap.end() ) {
      error = ERROR;
      Logf("(%s) error parameter \"%s\" not found in parammap\n", function, paramname);
    }

    /**
     * If no change in value then don't send the command
     */
    if ( this->parammap[paramname].value == newvalue ) {
      error = NO_ERROR;
      Logf("(%s) parameter %s=%s not written: no change in value\n", function, paramname, newvalue);
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
      Logf("(%s) sending archon_cmd(%s)\n", function, sscmd.str().c_str());
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
    const char* function = "Archon::Interface::expose";
    long error;

//  error = this->prep_parameter("dCDSRawExpose", "1");
//  if (error == NO_ERROR) error = this->load_parameter("dCDSRawExpose", "1");
    error = this->prep_parameter("Expose", "1");
    if (error == NO_ERROR) error = this->load_parameter("Expose", "1");

    // get system time and Archon's timer after exposure starts
    // start_time is used by the FITS writer ?? //TODO (maybe can remove)
    //
    if (error == NO_ERROR) {
      this->camera_info.start_time = this->util.get_time_string();  // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
      error = this->get_timer(&this->start_time);                   // Archon internal timer
    }

    if (error == NO_ERROR) {
      Logf("(%s) exposure started at Archon time %ld\n", function, this->start_time);
    }
    if (error == NO_ERROR) error = this->prepare_image_buffer();

    int mode = this->camera_info.current_observing_mode;
    this->camera_info.fits.userkeys = this->modeinfo[mode].fits.userkeys;  // copy the mode's userkeys into camera_info
    this->fits_info = this->camera_info;                                   // copy the camera_info class, to be given to fits writer

    return (NO_ERROR);
  }
  /**************** Archon::Interface::expose *********************************/


}
