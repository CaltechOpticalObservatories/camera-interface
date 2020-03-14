/**
 * @file    archon.h
 * @brief   
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#ifndef ARCHON_H
#define ARCHON_H

// number of observing modes
//#define NUM_OBS_MODES 1

// poll timeout in msec
#define POLLTIMEOUT 5000

// Archon block size
#define BLOCK_LEN   1024

// Reply buffer size (over-estimate)
#define REPLY_LEN   100 * BLOCK_LEN

// Archon commands
#define  SYSTEM        (char *)"SYSTEM"
#define  STATUS        (char *)"STATUS"
#define  FRAME         (char *)"FRAME"
#define  CLEARCONFIG   (char *)"CLEARCONFIG"
#define  POLLOFF       (char *)"POLLOFF"
#define  POLLON        (char *)"POLLON"
#define  APPLYALL      (char *)"APPLYALL"
#define  POWERON       (char *)"POWERON"
#define  POWEROFF      (char *)"POWEROFF"
#define  APPLYCDS      (char *)"APPLYCDS"
#define  RESETTIMING   (char *)"RESETTIMING"
#define  HOLDTIMING    (char *)"HOLDTIMING"
#define  RELEASETIMING (char *)"RELEASETIMING"
#define  LOADPARAMS    (char *)"LOADPARAMS"
#define  UNLOCK        (char *)"LOCK0"

namespace Archon {

  class Information {
    private:
    public:
      std::string hostname;                  //<! Archon controller hostname
      int         port;                      //!< Archon controller TPC/IP port number
      int         nbufs;                     //!< Archon controller number of frame buffers
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

      bool archon_busy;                      //!< indicates a thread is accessing Archon
      std::mutex archon_mutex;               //!< protects Archon from being accessed by multiple threads,
                                             //!< use in conjunction with archon_busy flag

      long connect_controller();             //!< open connection to archon controller
      long disconnect_controller();          //!< disconnect from archon controller
      long load_config(std::string cfgfile);
      long archon_prim(std::string cmd);
      long archon_cmd(std::string cmd);
      long archon_cmd(std::string cmd, char *reply);
      long read_parameter(std::string paramname, std::string &valstring);
      long prep_parameter(std::string paramname, std::string value);
      long load_parameter(std::string paramname, std::string value);
      long fetchlog();
      long read_frame();

      Information camera_info;

      typedef enum {
        MODE_DEFAULT = 0,
        NUM_OBS_MODES
      } Observing_modes;

      const char * const Observing_mode_str[NUM_OBS_MODES] = {
        "MODE_DEFAULT"
      };

      /**
       * config_line_t is a struct for the configfile key=value map, used to
       * store the configuration line and its associated line number.
       */
      typedef struct {
        int    line;           // the line number, used for updating Archon
        std::string value;     // used for configmap
      } config_line_t;

      /**
       * param_line_t is a struct for the PARAMETER name key=value map, used to
       * store parameters where the format is PARAMETERn=parametername=value
       */
      typedef struct {
        std::string key;       // the PARAMETERn part
        std::string name;      // the parametername part
        std::string value;     // the value part
        int    line;           // the line number, used for updating Archon
      } param_line_t;

      typedef struct {
        std::string keyword;
        std::string keytype;
        std::string keyvalue;
        std::string keycomment;
      } user_key_t;

      typedef std::map<std::string, config_line_t>  cfg_map_t;
      typedef std::map<std::string, param_line_t>   param_map_t;
      typedef std::map<std::string, user_key_t>     fits_key_t;

      cfg_map_t   configmap;
      param_map_t parammap;

      /** 
       * \var     modeinfo_t modeinfo
       * \details structure contains a configmap and parammap unique to each mode,
       *          specified in the [MODE_*] sections at the end of the .acf file.
       */
      struct modeinfo_t {
        bool        defined;     //!< clear by default, set if mode section encountered in .acf file
        int         rawenable;   //!< initialized to -1, then set according to RAWENABLE in .acf file
        cfg_map_t   configmap;   //!< key=value map for configuration lines set in mode sections
        param_map_t parammap;    //!< PARAMETERn=parametername=value map for mode sections
        fits_key_t  userkeys;    //!< user supplied FITS header keys
      } modeinfo[NUM_OBS_MODES];

      /**
       * generic key=value STL map for Archon commands
       */
      typedef std::map<std::string, std::string> map_t;

      /**
       * \var     map_t systemmap
       * \details key=value map for Archon SYSTEM command
       */
      map_t systemmap;

      /**
       * \var     map_t statusmap
       * \details key=value map for Archon STATUS command
       */
      map_t statusmap;

  };

}

#endif
