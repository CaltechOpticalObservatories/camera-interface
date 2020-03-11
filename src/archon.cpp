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

#include <sstream>  // for std::stringstream
#include <iomanip>  // for setfil, setw, etc.
#include <iostream> // for hex, uppercase, etc.

namespace Archon {

  // Archon::Archon constructor
  //
  Interface::Interface() {
    this->connection_open = false;
    this->current_state = "uninitialized";
    this->sockfd = -1;
    this->msgref = 0;
  }

  // Archon::
  //
  Interface::~Interface() {
  }

  /**************** Archon::Interface::connect_to_controller ******************/
  /**
   * @fn     connect_to_controller
   * @brief
   * @param  none
   * @return 
   *
   */
  long Interface::connect_to_controller() {
    const char* function = "Archon::Interface::connect_to_controller";
    struct sockaddr_in  servaddr;
    struct hostent     *hostPtr=NULL;
    int    flags;
    int    error = ERROR;

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
    if ( (this->sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1 ) {
      this->current_state = "uninitialized";
      Logf("(%s) error %d connecting to socket\n", function, errno);
      return(ERROR);
    }

    if ( (hostPtr = gethostbyname(this->camera_info.hostname.c_str())) == NULL)  {
      if ( (hostPtr = gethostbyaddr(this->camera_info.hostname.c_str(),
                                    strlen(this->camera_info.hostname.c_str()), AF_INET)) == NULL ) {
        this->current_state = "uninitialized";
        Logf("(%s) error %d resolving host: %s\n", function, errno, this->camera_info.hostname.c_str());
        return(ERROR);
      }
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(this->camera_info.port);
    (void) memcpy(&servaddr.sin_addr, hostPtr->h_addr, hostPtr->h_length);

    if ( (connect(this->sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr))) == -1 ) {
      this->current_state = "uninitialized";
      Logf("(%s) error %d connecting to %s:%d\n", function, errno, this->camera_info.hostname.c_str(), this->camera_info.port);
      return(ERROR);
    }

    if ((flags = fcntl(this->sockfd, F_GETFL, 0)) < 0) {
      this->current_state = "uninitialized";
      Logf("(s) error: F_GETFL: %d\n", function, errno);
      return(ERROR);
    }

    flags |= O_NONBLOCK;

    if (fcntl(this->sockfd, F_SETFL, flags) < 0) {
      this->current_state = "uninitialized";
      Logf("(%s) error: F_SETFL: %d\n", function, errno);
      return(ERROR);
    }

    // this->sockfd contains the socket file descriptor to the new conncection
    //
    error = 0;
    Logf("(%s) socket connection to %s:%d established on fd %d\n", function, 
         this->camera_info.hostname.c_str(), this->camera_info.port, this->sockfd);

    // On success, write the value to the log
    //
    if (error == NO_ERROR) {
        this->connection_open = true;
      Logf("(%s) Archon connection established\n", function);
    }
    // Throw an error for any other errors
    //
    else {
      this->current_state = "uninitialized";
      Logf("(%s) Archon camera connection error\n", function);
    }

    // empty the Archon log
    //
    if (error == NO_ERROR) {
      error = this->fetchlog();
    }
    this->current_state = "initialized";
    return(error);
  }
  /**************** Archon::Interface::connect_to_controller ******************/


  /**************** Archon::Interface::close_connection ***********************/
  /**
   * @fn     close_connection
   * @brief
   * @param  none
   * @return 
   *
   */
  long Interface::close_connection() {
    const char* function = "Archon::Interface::close_connection";
    Logf("(%s)\n", function);
    return 0;
  }
  /**************** Archon::Interface::close_connection ***********************/


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

    std::stringstream  sscmd;

    // build command: ">xxCOMMAND\n" where xx=hex msgref and COMMAND=command
    //
    sscmd << ">"
          << std::uppercase
          << std::setfill('0')
          << std::setw(2)
          << std::hex
          << this->msgref
          << cmd
          << "\n";
    // make string of command without newline (for logging purposes)
    //
    std::string fcmd = sscmd.str(); fcmd.erase(fcmd.find('\n'));

    // build the command checksum: msgref used to check that reply matches command
    //
    SNPRINTF(check, "<%02X", this->msgref);

    // log the command as long as it's not a STATUS, TIMER, WCONFIG or FRAME command
    //
    if ( (cmd != "WCONFIG") &&
         (cmd != "TIMER")   &&
         (cmd != "STATUS")  &&
         (cmd != "FRAME") ) {
      Logf("(%s) sending command: %s\n", function, fcmd.c_str());
    }

    // send the command
    //
    if ( (sock_write(this->sockfd, (char *)sscmd.str().c_str())) == -1) {
      Logf("(%s) error writing to camera socket\n", function);
    }
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
    Logf("(%s)\n", function);
    return 0;
  }
  /**************** Archon::Interface::load_config ****************************/
}
