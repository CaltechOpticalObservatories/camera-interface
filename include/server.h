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

#include <csignal>
#include <map>
#include <mutex>
#include <thread>

#include "archon.h"
#include "common.h"

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
        std::cerr << "(Archon::Server) closing sockets\n";
      }
      /** Archon::~Server **********************************************************/

      int nonblocking_socket;
      int blocking_socket;

      typedef enum { BLOCK, NONBLOCK } port_type_t;

      typedef struct {
        port_type_t  port_type;
        char         cliaddr_str[20];
        int          cliport;
        int          connfd;
        char        *command;
      } conndata_struct;

      typedef std::map<int, conndata_struct> conndata_t;

      conndata_t conndata;

      std::mutex conn_mutex;

      /** Archon::Server::exit_cleanly *********************************************/
      /**
       * @fn     signal_handler
       * @brief  handles ctrl-C and exits
       * @param  int signo
       * @return nothing
       *
       */
      void exit_cleanly(void) {
        Logf("(Archon::Server::exit_cleanly) server exiting\n");
        exit(0);
      }
      /** Archon::Server::exit_cleanly *********************************************/
  };
}
#endif
