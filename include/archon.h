/**
 * @file    archon.h
 * @brief   
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#ifndef ARCHON_H
#define ARCHON_H

#include <CCfits/CCfits>           //!< needed here for types in set_axes()
#include <atomic>
#include <chrono>
#include <numeric>

#include "utilities.h"
#include "common.h"
#include "config.h"
#include "logentry.h"
#include "network.h"
#include "fits.h"

#define POLLTIMEOUT 5000           //!< poll timeout in msec
#define MAXADCHANS 16              //!< max number of AD channels per controller
#define MAXCCDS 4                  //!< max number of CCDs handled by one controller
#define BLOCK_LEN 1024             //!< Archon block size
#define REPLY_LEN 100 * BLOCK_LEN  //!< Reply buffer size (over-estimate)

// Archon commands
#define  SYSTEM        std::string("SYSTEM")
#define  STATUS        std::string("STATUS")
#define  FRAME         std::string("FRAME")
#define  CLEARCONFIG   std::string("CLEARCONFIG")
#define  POLLOFF       std::string("POLLOFF")
#define  POLLON        std::string("POLLON")
#define  APPLYALL      std::string("APPLYALL")
#define  POWERON       std::string("POWERON")
#define  POWEROFF      std::string("POWEROFF")
#define  APPLYCDS      std::string("APPLYCDS")
#define  RESETTIMING   std::string("RESETTIMING")
#define  HOLDTIMING    std::string("HOLDTIMING")
#define  RELEASETIMING std::string("RELEASETIMING")
#define  LOADPARAMS    std::string("LOADPARAMS")
#define  TIMER         std::string("TIMER")
#define  FETCHLOG      std::string("FETCHLOG")
#define  UNLOCK        std::string("LOCK0")

namespace Archon {

  // Archon hardware-based constants.
  // These shouldn't change unless there is a significant hardware change.
  //
  const int nbufs = 3;             //!< total number of frame buffers  //TODO rename to maxnbufs?
  const int nmods = 12;            //!< number of modules per controller
  const int nadchan = 4;           //!< number of channels per ADC module

  class Interface {
    private:
      unsigned long int start_timer, finish_timer;  //!< Archon internal timer, start and end of exposure

    public:
      Interface();
      ~Interface();

      // Class Objects
      //
      Network::TcpSocket archon;
      Common::Information camera_info;       //!< this is the main camera_info object
      Common::Information fits_info;         //!< used to copy the camera_info object to preserve info for FITS writing
      Common::Common common;                 //!< instantiate a Common object
      Common::FitsKeys userkeys;             //!< instantiate a Common object
      Config config;

      FITS_file fits_file;                   //!< instantiate a FITS container object

      int  msgref;                           //!< Archon message reference identifier, matches reply to command
      bool abort;
      int  taplines;
      int  gain[MAXADCHANS];                 //!< digital CDS gain (from TAPLINE definition)   //TODO convert to vector
      int  offset[MAXADCHANS];               //!< digital CDS offset (from TAPLINE definition) //TODO convert to vector
      bool modeselected;                     //!< true if a valid mode has been selected, false otherwise
      bool firmwareloaded;                   //!< true if firmware is loaded, false otherwise

      char *image_data;                      //!< image data buffer
      uint32_t image_data_bytes;             //!< requested number of bytes allocated for image_data rounded up to block size
      uint32_t image_data_allocated;         //!< allocated number of bytes for image_data

      std::atomic<bool> archon_busy;         //!< indicates a thread is accessing Archon
      std::mutex archon_mutex;               //!< protects Archon from being accessed by multiple threads,
                                             //!< use in conjunction with archon_busy flag
      std::string exposeparam;               //!< param name to trigger exposure when set =1

      // Functions
      //
      long interface(std::string &iface);    //!< get interface type
      long configure_controller();           //!< get configuration parameters
      long prepare_image_buffer();           //!< prepare image_data, allocating memory as needed
      long connect_controller(std::string devices_in);  //!< open connection to archon controller
      long disconnect_controller();          //!< disconnect from archon controller
      long load_firmware();                  //!< load default firmware (ACF)
      long load_firmware(std::string acffile); //!< load specified firmware (ACF)
      long load_firmware(std::string acffile, std::string retstring);
      long set_camera_mode(std::string mode_in);
      long load_mode_settings(std::string mode);
      long native(std::string cmd);
      long archon_cmd(std::string cmd);
      long archon_cmd(std::string cmd, std::string &reply);
      long read_parameter(std::string paramname, std::string &valstring);
      long prep_parameter(std::string paramname, std::string value);
      long load_parameter(std::string paramname, std::string value);
      long fetchlog();
      long get_frame_status();
      long print_frame_status();
      long lock_buffer(int buffer);
      long get_timer(unsigned long int *timer);
      long fetch(uint64_t bufaddr, uint32_t bufblocks);
      long read_frame();                     //!< read Archon frame buffer into host memory
      long read_frame(Common::frame_type_t frame_type); //!< read Archon frame buffer into host memory
      long write_frame();                    //!< write (a previously read) Archon frame buffer to disk
      long write_raw();                      //!< write raw 16 bit data to a FITS file
      long write_config_key( const char *key, const char *newvalue, bool &changed );
      long write_config_key( const char *key, int newvalue, bool &changed );
      long write_parameter( const char *paramname, const char *newvalue, bool &changed );
      long write_parameter( const char *paramname, int newvalue, bool &changed );
      long write_parameter( const char *paramname, const char *newvalue );
      long write_parameter( const char *paramname, int newvalue );
      template <class T> long get_configmap_value(std::string key_in, T& value_out);
      long expose(std::string nseq_in);
      long wait_for_exposure();
      long wait_for_readout();
      long get_parameter(std::string parameter, std::string &retstring);
      long set_parameter(std::string parameter);
      long exptime(std::string exptime_in, std::string &retstring);
      long bias(std::string args, std::string &retstring);

      /**
       * @var     struct geometry_t geometry[]
       * @details structure of geometry which is unique to each observing mode
       */
      struct geometry_t {
        int  amps_per_ccd[2];      // number of amplifiers per CCD for each axis, set in set_camera_mode
        int  num_ccds;             // number of CCDs, set in set_camera_mode
        int  linecount;            // number of lines per tap
        int  pixelcount;           // number of pixels per tap
      };

      /**
       * @var     struct tapinfo_t tapinfo[]
       * @details structure of tapinfo which is unique to each observing mode
       */
      struct tapinfo_t {
        int   num_taps;
        int   tap[16];
        float gain[16];
        float offset[16];
        std::string readoutdir[16];
      };

      /**
       * @var     struct frame_data_t frame
       * @details structure to contain Archon results from "FRAME" command
       */
      struct frame_data_t {
        int      index;                       // index of newest buffer data
        int      frame;                       // index of newest buffer data
        std::string timer;                    // current hex 64 bit internal timer
        int      rbuf;                        // current buffer locked for reading
        int      wbuf;                        // current buffer locked for writing
        std::vector<int>      bufsample;      // sample mode 0=16 bit, 1=32 bit
        std::vector<int>      bufcomplete;    // buffer complete, 1=ready to read
        std::vector<int>      bufmode;        // buffer mode: 0=top 1=bottom 2=split
        std::vector<uint64_t> bufbase;        // buffer base address for fetching
        std::vector<int>      bufframen;      // buffer frame number
        std::vector<int>      bufwidth;       // buffer width
        std::vector<int>      bufheight;      // buffer height
        std::vector<int>      bufpixels;      // buffer pixel progress
        std::vector<int>      buflines;       // buffer line progress
        std::vector<int>      bufrawblocks;   // buffer raw blocks per line
        std::vector<int>      bufrawlines;    // buffer raw lines
        std::vector<int>      bufrawoffset;   // buffer raw offset
        std::vector<uint64_t> buftimestamp;   // buffer hex 64 bit timestamp
        std::vector<uint64_t> bufretimestamp; // buf trigger rising edge time stamp
        std::vector<uint64_t> buffetimestamp; // buf trigger falling edge time stamp
      } frame;

      /** @var      vector modtype
       *  @details  stores the type of each module listed under [SYSTEM] in the acf file
       */
      std::vector<int> modtype;

      /** @var      int lastframe
       *  @details  the last (I.E. previous) frame number acquired
       */
      int lastframe;

      /**
       * rawinfo_t is a struct which contains variables specific to raw data functions
       */
      struct rawinfo_t {
        int adchan;          // selected A/D channels
        int rawsamples;      // number of raw samples per line
        int rawlines;        // number of raw lines
        int iteration;       // iteration number
        int iterations;      // number of iterations
      } rawinfo;

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

      typedef std::map<std::string, config_line_t>  cfg_map_t;
      typedef std::map<std::string, param_line_t>   param_map_t;

      cfg_map_t   configmap;
      param_map_t parammap;

      /** 
       * \var     modeinfo_t modeinfo
       * \details structure contains a configmap and parammap unique to each mode,
       *          specified in the [MODE_*] sections at the end of the .acf file.
       */
      typedef struct {
        int         rawenable;     //!< initialized to -1, then set according to RAWENABLE in .acf file
        cfg_map_t   configmap;     //!< key=value map for configuration lines set in mode sections
        param_map_t parammap;      //!< PARAMETERn=parametername=value map for mode sections
        Common::FitsKeys acfkeys;  //!< create a FitsKeys object to hold user keys read from ACF file for each mode
        geometry_t  geometry;
        tapinfo_t   tapinfo;
      } modeinfo_t;

      std::map<std::string, modeinfo_t> modemap;

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
