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

  class Server : public Archon::Interface {
    private:
    public:
      Server() { }
      ~Server();
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

      void exit_cleanly(void);

  };
}
#endif
