/**
 * @file    archon.h
 * @brief   
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#ifndef ARCHON_H
#define ARCHON_H

// Archon block size
# define BLOCK_LEN   1024

// Reply buffer size (over-estimate)
# define REPLY_LEN   100 * BLOCK_LEN

// Archon commands
# define  SYSTEM        (char *)"SYSTEM"
# define  STATUS        (char *)"STATUS"
# define  FRAME         (char *)"FRAME"
# define  CLEARCONFIG   (char *)"CLEARCONFIG"
# define  POLLOFF       (char *)"POLLOFF"
# define  POLLON        (char *)"POLLON"
# define  APPLYALL      (char *)"APPLYALL"
# define  POWERON       (char *)"POWERON"
# define  POWEROFF      (char *)"POWEROFF"
# define  APPLYCDS      (char *)"APPLYCDS"
# define  RESETTIMING   (char *)"RESETTIMING"
# define  HOLDTIMING    (char *)"HOLDTIMING"
# define  RELEASETIMING (char *)"RELEASETIMING"
# define  LOADPARAMS    (char *)"LOADPARAMS"
# define  UNLOCK        (char *)"LOCK0"

namespace Archon {

  class Information {
    private:
    public:
      std::string hostname;                  //<! Archon controller hostname
      int         port;                      //!< Archon controller TPC/IP port number
  };

  class Interface {
    private:
    public:
      Interface();
      ~Interface();

      bool connection_open;                  //!< is there a connection open to the controller?
      std::string current_state;             //!< current state of the controller
      int  sockfd;                           //!< socket file descriptor to Archon controller
      int  msgref;                           //!< Archon message reference identifier, matches reply to command

      long connect_to_controller();
      long close_connection();
      long load_config(std::string cfgfile);
      long archon_cmd(std::string cmd);
      long archon_cmd(std::string cmd, char *reply);
      long fetchlog();

      Information camera_info;
  };

}

#endif
