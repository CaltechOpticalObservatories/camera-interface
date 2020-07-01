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

#ifdef ARC66_PCIE
#include "arc66.h"
#elif ARC64_PCI
#include "arc64.h"
#elif STA_ARCHON
#include "archon.h"
#endif

#include "logentry.h"
#include "config.h"
#include "network.h"

#define  NBPORT       3030  //!< non-blocking port
#define  BLKPORT      3031  //!< blocking port
#define  N_THREADS    10    //!< total number of threads spawned by server, one for blocking and the remainder for non-blocking
#define  BUFSIZE      1024  //!< size of the input command buffer
#define  CONN_TIMEOUT 3000  //<! incoming (non-blocking) connection timeout in milliseconds

namespace Camera {

#ifdef ARC66_PCIE
  class Server : public Arc66::Interface {
#elif ARC64_PCI
  class Server : public Arc64::Interface {
#elif STA_ARCHON
  class Server : public Archon::Interface {
#endif
    private:
    public:
      Server() { }

      /** Camera::~Server **********************************************************/
      /**
       * @fn     ~Server
       * @brief  class deconstructor cleans up on exit
       */
      ~Server() {
        close(this->nonblocking_socket);
        close(this->blocking_socket);
        closelog();  // close the logfile, if open
      }
      /** Camera::~Server **********************************************************/

      int nonblocking_socket;
      int blocking_socket;

      Network::TcpSocket nonblocking;

      std::mutex conn_mutex;             //!< mutex to protect against simultaneous access to Accept()

      /** Camera::Server::exit_cleanly *********************************************/
      /**
       * @fn     signal_handler
       * @brief  handles ctrl-C and exits
       * @param  int signo
       * @return nothing
       *
       */
      void exit_cleanly(void) {
        std::string function = "Camera::Server::exit_cleanly";
        logwrite(function, "server exiting");
        exit(EXIT_SUCCESS);
      }
      /** Camera::Server::exit_cleanly *********************************************/
  };
}
#endif
