/** ---------------------------------------------------------------------------
 * @file     server.h
 * @brief    
 * @author   David Hale <dhale@astro.caltech.edu>
 * @date     
 * @modified 
 *
 */

#ifndef SERVER_H
#define SERVER_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <csignal>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "archon.h"
#include "logentry.h"
#include "config.h"
#include "network.h"

#define  NBPORT       3030  //!< non-blocking port
#define  BLKPORT      3031  //!< blocking port
#define  N_THREADS    10    //!< total number of threads spawned by server, one for blocking and the remainder for non-blocking
#define  BUFSIZE      1024  //!< size of the input command buffer
#define  CONN_TIMEOUT 3000  //<! incoming (non-blocking) connection timeout in milliseconds

#define MATCH(buf1, buf2) (!strncasecmp(buf1, buf2, strlen(buf2)))

#define STRIPCOMMAND(destbuf, sourcebuf) do { \
  *destbuf=0; \
  if (strstr(sourcebuf, " ") != NULL) { \
    strncat(destbuf, sourcebuf, (strstr(sourcebuf, " ")-(sourcebuf))); \
    sourcebuf += (strstr(sourcebuf, " ")-(sourcebuf))+strlen(" "); \
    } \
  else { \
    strncat(destbuf, sourcebuf, (strstr(sourcebuf, "\r")-(sourcebuf))); \
    sourcebuf=NULL; \
    } \
  if (sourcebuf) { if ((c_ptr=strrchr(sourcebuf,'\n'))) *c_ptr='\0'; } \
  if (sourcebuf) { if ((c_ptr=strrchr(sourcebuf,'\r'))) *c_ptr='\0'; } \
  } while(0)

namespace Archon {

  class Server : public Interface {
    private:
    public:
      Server() { }

      /** Archon::~Server **********************************************************/
      /**
       * @fn     ~Server
       * @brief  class deconstructor cleans up on exit
       */
      ~Server() {
        close(this->nonblocking_socket);
        close(this->blocking_socket);
        closelog();  // close the logfile, if open
      }
      /** Archon::~Server **********************************************************/

      int nonblocking_socket;
      int blocking_socket;

      Network::TcpSocket nonblocking;

      std::mutex conn_mutex;             //!< mutex to protect against simultaneous access to Accept()

      /** Archon::Server::exit_cleanly *********************************************/
      /**
       * @fn     signal_handler
       * @brief  handles ctrl-C and exits
       * @param  int signo
       * @return nothing
       *
       */
      void exit_cleanly(void) {
        std::string function = "Archon::Server::exit_cleanly";
        logwrite(function, "server exiting");
        exit(EXIT_SUCCESS);
      }
      /** Archon::Server::exit_cleanly *********************************************/
  };
}
#endif
