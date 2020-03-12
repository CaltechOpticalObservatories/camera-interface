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

#include <sstream>   // for std::stringstream
#include <iomanip>   // for setfil, setw, etc.
#include <iostream>  // for hex, uppercase, etc.
#include <algorithm> 
#include <string>

namespace Archon {

  // Archon::Archon constructor
  //
  Interface::Interface() {
    this->archon_busy = false;
    this->connection_open = false;
    this->current_state = "uninitialized";
    this->sockfd = -1;
    this->msgref = 0;
    this->camera_info.hostname = "192.168.1.2";
    this->camera_info.port=4242;
  }

  // Archon::
  //
  Interface::~Interface() {
  }

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

    return(error);
  }
  /**************** Archon::Interface::disconnect_controller ******************/


  long Interface::archon_prim(std::string cmd) { // use this form when the calling
    char reply[REPLY_LEN];                       // function doesn't need to look at the reply
    long ret = archon_cmd(cmd, reply);
    Logf("%s\n", reply);
    return( ret );
  }
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
    char    buffer[BLOCK_LEN];            //!< temporary buffer for holding Archon replies
    int     error = NO_ERROR;

    unsigned long bufcount;

    // nothing to do if no connection open to controller
    //
    if (!this->connection_open) {
      Logf("(%s) error: connection not open to controller\n", function);;
      return(ERROR);
    }

    /**
     * Hold a scoped lock for the duration of this function, not just
     * for setting this semaphore.
     *
     * The archon_busy semaphore is used to prevent the status thread
     * from accessing the Archon, but the scoped mutex lock is used to
     * prevent all other threads from accessing the Archon while the
     * status thread has it.
     */
    if (this->archon_busy) return(BUSY);
    const std::lock_guard<std::mutex> lock(this->archon_mutex);
    this->archon_busy = true;

    // build command: ">xxCOMMAND\n" where xx=hex msgref and COMMAND=command
    //
    std::stringstream  sscmd;         // sscmd = stringstream, building command
    sscmd << ">"
          << std::setfill('0')
          << std::setw(2)
          << std::hex
          << this->msgref
          << cmd
          << "\n";
    std::string scmd = sscmd.str();   // scmd = string, command to send
    std::transform( scmd.begin(), scmd.end(), scmd.begin(), ::toupper );    // make uppercase

    // build the command checksum: msgref used to check that reply matches command
    //
    SNPRINTF(check, "<%02X", this->msgref);

    // log the command as long as it's not a STATUS, TIMER, WCONFIG or FRAME command
    //
    if ( (cmd != "WCONFIG") &&
         (cmd != "TIMER")   &&
         (cmd != "STATUS")  &&
         (cmd != "FRAME") ) {
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
    if (cmd == "FETCH") return (error);

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
      Logf("(%s) Archon controller returned error processing command: %s", function, cmd.c_str());
      this->fetchlog();      // check the Archon log for error messages
    }
    else
    if (strncmp(reply, check, 3) != 0) {
      // Command-Reply mismatch
      //
      error = ERROR;
      std::string hdr = reply;
      Logf("(%s) error: command-reply mismatch for command %s: received header:%s\n", 
           function, cmd.c_str(), hdr.substr(0,3).c_str());
    }
    else {
      // command and reply are a matched pair
      error = NO_ERROR;

      // log the command as long as it's not a STATUS, TIMER, WCONFIG or FRAME command
      if ( (error==NO_ERROR)  &&
           (cmd != "WCONFIG") &&
           (cmd != "TIMER")   &&
           (cmd != "STATUS")  &&
           (cmd != "FRAME") ) {
        Logf("(%s) command %02X sent OK\n", function, this->msgref);
      }

      memmove( reply, reply+3, strlen(reply) );       // strip off the msgref from the reply
      this->msgref = (this->msgref + 1) % 256;        // increment msgref
    }

    // clear the semaphore (still have the mutex this entire function)
    //
    this->archon_busy = false;

    return(error);
  }
  /**************** Archon::Interface::archon_cmd *****************************/


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
      if ( (retval=this->archon_cmd((char*)"FETCHLOG", reply))!=NO_ERROR ) { // send command here
        Logf("(%s) error calling FETCHLOG\n", function);
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

    Logf("(%s) %s\n", function, cfgfile.c_str());

    Common::Utilities util;

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
      this->modeinfo[modenum].userkeys.clear();
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
        util.chrrep(line, '[',  127);            // remove [ bracket (replace with DEL)
        util.chrrep(line, ']',  127);            // remove ] bracket (replace with DEL)
        util.chrrep(line, '\n', 127);            // remove newline (replace with DEL)
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

      util.chrrep(line, '\\', '/');                       // replace backslash with forward slash
      util.chrrep(line, '\"', 127);                       // remove all quotes (replace with DEL)

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
        util.chrrep(lineptr, '\n', 127);                                  // remove newline (replace with DEL)

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
        util.chrrep(lineptr, '\n', 127);                                  // remove newline (replace with DEL)
        if ( (keyptr = strsep(&lineptr, "=")) == NULL ) continue;         // keyptr: KEY, lineptr: VALUE

        std::string internal_key = keyptr;

        if (internal_key == "") {
          ;
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
        util.chrrep(lineptr, '\n', 127);                                  // remove newline (replace with DEL)

        // First, tokenize on the equal sign "=".
        // The token left of "=" is the keyword. Immediate right is the value
        util.Tokenize(lineptr, tokens, "=");
        if (tokens.size() != 2) {                                         // need at least two tokens at this point
          Logf("(%s) error: token mismatch: expected KEYWORD=value//comment (found too many ='s)\n", function);
          if (line) free(line); fclose(fp);                               // getline() mallocs a buffer for line
          return(ERROR);
        }
        keyword   = tokens[0].substr(0,8);                                // truncate keyword to 8 characters
        keystring = tokens[1];                                            // tokenize the rest in a moment

        // Next, tokenize on the slash "/".
        // The token left of "/" is the value. Anything to the right is a comment.
        util.Tokenize(keystring, tokens, "/");
        keyvalue   = tokens[0];

        if (tokens.size() == 2) {      // If there are two tokens then the second is a comment,
          keycomment = tokens[1];
        } else {                       // but the comment is optional.
          keycomment = "";
        }

        // Set the key type based on the contents of the value string.
        // The type will be used in the fits_file.add_user_key(...) call.
        if (util.get_keytype(keyvalue) == Common::Utilities::TYPE_INTEGER) {
          keytype = "INT";
        }
        else
        if (util.get_keytype(keyvalue) == Common::Utilities::TYPE_DOUBLE) {
          keytype = "REAL";
        }
        else {      // if not an int or float then a string by default
          keytype = "STRING";
        }

        // Save all of the user keyword information in a map for later
        //
        this->modeinfo[obsmode].userkeys[keyword].keyword    = keyword;
        this->modeinfo[obsmode].userkeys[keyword].keytype    = keytype;
        this->modeinfo[obsmode].userkeys[keyword].keyvalue   = keyvalue;
        this->modeinfo[obsmode].userkeys[keyword].keycomment = keycomment;
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
        if (error == NO_ERROR) error = this->archon_cmd((char *)sscmd.str().c_str());
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
    return(error);
  }
  /**************** Archon::Interface::load_config ****************************/
}
