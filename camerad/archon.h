/**
 * @file    archon.h
 * @brief   
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#pragma once

#include <CCfits/CCfits>           //!< needed here for types in set_axes()
#include <atomic>
#include <chrono>
#include <numeric>
#include <fenv.h>
#include <string_view>
#include <zmqpp/zmqpp.hpp>
#include <nlohmann/json.hpp>

// #include "opencv2/opencv.hpp"      /// OpenCV used for image manipulation
#include "utilities.h"
#include "common.h"
#include "camera.h"
#include "config.h"
#include "logentry.h"
#include "network.h"
#include "fits.h"       /// old version renames FITS_file to __FITS_file, will go away soon
#include "fits_file.h"  /// new version implements FITS_file
#include "deinterlace_modes.h"
#include "camerad_commands.h"

constexpr int MAXADCCHANS =   16;              //!< max number of ADC channels per controller (4 mod * 4 ch/mod)
constexpr int MAXADMCHANS =   72;              //!< max number of ADM channels per controller (4 mod * 18 ch/mod)
constexpr int BLOCK_LEN   = 1024;              //!< Archon block size
constexpr int REPLY_LEN   =  100 * BLOCK_LEN;  //!< Reply buffer size (over-estimate)

// Archon commands
//
const std::string  SYSTEM        = "SYSTEM";
const std::string  STATUS        = "STATUS";
const std::string  FRAME         = "FRAME";
const std::string  CLEARCONFIG   = "CLEARCONFIG";
const std::string  POLLOFF       = "POLLOFF";
const std::string  POLLON        = "POLLON";
const std::string  APPLYALL      = "APPLYALL";
const std::string  POWERON       = "POWERON";
const std::string  POWEROFF      = "POWEROFF";
const std::string  APPLYCDS      = "APPLYCDS";
const std::string  APPLYSYSTEM   = "APPLYSYSTEM";
const std::string  RESETTIMING   = "RESETTIMING";
const std::string  LOADTIMING    = "LOADTIMING";
const std::string  HOLDTIMING    = "HOLDTIMING";
const std::string  RELEASETIMING = "RELEASETIMING";
const std::string  LOADPARAMS    = "LOADPARAMS";
const std::string  TIMER         = "TIMER";
const std::string  FETCHLOG      = "FETCHLOG";
const std::string  UNLOCK        = "LOCK0";

// Minimum required backplane revisions for certain features
//
const std::string REV_RAMP           = "1.0.548";
const std::string REV_SENSORCURRENT  = "1.0.758";
const std::string REV_HEATERTARGET   = "1.0.1087";
const std::string REV_FRACTIONALPID  = "1.0.1054";
const std::string REV_VCPU           = "1.0.784";

namespace Archon {

    constexpr std::string_view QUIET = "quiet";  // allows sending commands without logging
    constexpr uint32_t MSEC_TO_TICK = 100000;    // Archon clock ticks per millisecond
    constexpr uint32_t MAX_EXPTIME  = 0xFFFFF;   // Archon parameters are limited to 20 bits

    // Archon hardware-based constants.
    // These shouldn't change unless there is a significant hardware change.
    //
    const int nbufs = 3; //!< total number of frame buffers  //TODO rename to maxnbufs?
    const int nmods = 12; //!< number of modules per controller
    const int nadchan = 4; //!< number of channels per ADC module

    // Parameter defaults, unless overridden by the configuration file
    //
    const int DEF_TRIGIN_EXPOSE_ENABLE = 1;
    const int DEF_TRIGIN_EXPOSE_DISABLE = 0;
    const int DEF_TRIGIN_UNTIMED_ENABLE = 1;
    const int DEF_TRIGIN_UNTIMED_DISABLE = 0;
    const int DEF_TRIGIN_READOUT_ENABLE = 1;
    const int DEF_TRIGIN_READOUT_DISABLE = 0;
    const int DEF_SHUTENABLE_ENABLE = 1;
    const int DEF_SHUTENABLE_DISABLE = 0;

    /**
     * @brief      exposure modes
     */
    enum class ExposureMode {
      EXPOSUREMODE_RAW,
      EXPOSUREMODE_CCD,
      EXPOSUREMODE_FOWLER,
      EXPOSUREMODE_RXRVIDEO,
      EXPOSUREMODE_UTR
    };

    /**
     * @brief      deinterlace modes
     */
    enum class DeInterlaceMode {
      DEINTERLACE_NONE,
      DEINTERLACE_RXRVIDEO
    };

    class Interface;     //!< forward declaration
    class ExposureBase;  //!< forward declaration


    /***** Archon::convert_archon_buffer **************************************/
    /**
     * @brief      convert Archon char* buffer to the correct type
     * @details    The Archon transmits 8-bit Bytes which must be organized as
     *             either 16-bit words or 32-bit words, depending on whether
     *             the Archon is using HDR mode. This function casts the Archon
     *             buffer to the appropriate type as well as performs optional
     *             right-shifting.
     * @param[in]  bufin     Archon buffer
     * @param[in]  imgsz     image size, i.e. number of pixels in buffer
     * @param[in]  hdrshift  number of right-shift bits in HDR mode
     * @return     newly allocated and converted buffer of type U
     *
     */
    template <typename T, typename U=T>
    std::vector<U> convert_archon_buffer( const char* bufin, size_t imgsz, int hdrshift=0 ) {
      // cast the char* buffer from Archon to the requested type <T>
      const T* typecast_bufin = reinterpret_cast<const T*>(bufin);

      // return buffer can be of a different type <U>
      std::vector<U> bufout(imgsz);

      for ( size_t pix=0; pix<imgsz; ++pix ) {
        // for 32-bit scale by hdrshift
        if constexpr ( std::is_same<T, uint32_t>::value ) {
          bufout[pix] = static_cast<U>(typecast_bufin[pix] >> hdrshift);
        }
        else
        // for signed 16-bit subtract 2^15 from every pixel
        if constexpr ( std::is_same<T, int16_t>::value ) {
          bufout[pix] = typecast_bufin[pix] - 32768;
        }
        // for all others it's a straight copy
        else {
          bufout[pix] = typecast_bufin[pix];
        }
      }
      return bufout;
    }
    /***** Archon::convert_archon_buffer **************************************/


    /***** Archon::createDeInterlacer *******************************************/
    /**
     * @brief      factory function for creating a deinterlacer object
     * @details    This is the main entry point for the expose command. Things
     *             common to all exposure modes are done here, and things unique
     *             to a specialization are handled by calling expose_for_mode().
     * @param[in]  nseq_in
     * @return     ERROR | NO_ERROR
     *
     */
    template <typename T>
    std::unique_ptr<DeInterlaceBase> createDeInterlacer( const std::vector<T> &buffer,
                                                         const std::string &mode ) {
      if ( mode == "none" ) {
        return std::make_unique<DeInterlace_None<T>>(buffer);
      }
      else
      if ( mode == "rxrv" ) {
        return std::make_unique<DeInterlace_RXRVideo<T>>(buffer);
      }
      else
      return nullptr;
    }
    /***** Archon::createDeInterlacer *******************************************/


    /***** Archon::Interface **************************************************/
    /**
     * @class      Archon::Interface
     * @brief      describes the interface to an Archon
     *
     */
    class Interface {
      private:
        unsigned long int start_timer, finish_timer; //!< Archon internal timer, start and end of exposure
        int n_hdrshift; //!< number of right-shift bits for Archon buffer in HDR mode

        std::unique_ptr<ExposureBase> pExposureMode;  /// pointer to ExposureBase class
        std::string exposure_mode_str;                /// human readable representation of exposure mode
        zmqpp::context context;
        std::vector<int> configured_devnums;  //!< vector of configured Arc devices (from camerad.cfg file)
        std::vector<int> devnums;    //!< vector of all opened and connected devices
        int _expbuf;                 //!< points to next avail in exposure vector
        std::mutex _expbuf_mutex;    //!< mutex to protect expbuf operations
        std::mutex epend_mutex;
        std::vector<int> exposures_pending;  //!< vector of devnums that have a pending exposure (which needs to be stored)
        void retval_to_string( std::uint32_t check_retval, std::string& retstring );

      public:
        Interface();
        ~Interface();

        void select_exposure_mode( ExposureMode mode );  /// select exposure mode
        const std::string get_exposure_mode() { return this->exposure_mode_str; }

        // Class Objects
        //
        Network::TcpSocket archon;
        Camera::Information camera_info; /// this is the main camera_info object
        Camera::Information fits_info; /// used to copy the camera_info object to preserve info for FITS writing

        std::mutex publish_mutex;
        std::mutex collect_mutex;
        std::condition_variable publish_condition;
        std::condition_variable collect_condition;

        std::atomic<bool> publish_enable;
        std::atomic<bool> collect_enable;

        std::unique_ptr<Common::PubSub> publisher;       ///< publisher object
        std::string publisher_address;                   ///< publish socket endpoint
        std::string publisher_topic;                     ///< my default topic for publishing
        std::unique_ptr<Common::PubSub> subscriber;      ///< subscriber object
        std::string subscriber_address;                  ///< subscribe socket endpoint
        std::vector<std::string> subscriber_topics;      ///< list of topics I subscribe to
        std::atomic<bool> is_subscriber_thread_running;  ///< is my subscriber thread running?
        std::atomic<bool> should_subscriber_thread_run;  ///< should my subscriber thread run?
        std::unordered_map<std::string,
                          std::function<void(const nlohmann::json&)>> topic_handlers;
                                                        ///< maps a handler function to each topic

        // publish/subscribe functions
        //
        long init_pubsub(const std::initializer_list<std::string> &topics={}) {
          return Common::PubSubHandler<Interface>::init_pubsub(context, *this, topics);
        }
        void start_subscriber_thread() { Common::PubSubHandler<Interface>::start_subscriber_thread(*this); }
        void stop_subscriber_thread()  { Common::PubSubHandler<Interface>::stop_subscriber_thread(*this); }

        void handletopic_snapshot( const nlohmann::json &jmessage ) {
          //TODO:
        }

        void publish_snapshot();
        void publish_snapshot(std::string &retstring);

        int numdev;


        Camera::Camera camera; /// instantiate a Camera object
        Common::FitsKeys userkeys; //!< instantiate a Common object
        Common::FitsKeys systemkeys; //!< instantiate a Common object

        Config config;

        __FITS_file fits_file; //!< instantiate a FITS container object "__" designates old version

        int activebufs;                     //!< Archon controller number of active frame buffers
        int msgref; //!< Archon message reference identifier, matches reply to command
        bool abort;
        int taplines;
        int configlines;  //!< number of configuration lines
        bool logwconfig;  //!< optionally log WCONFIG commands
        std::vector<int> gain; //!< digital CDS gain (from TAPLINE definition)
        std::vector<int> offset; //!< digital CDS offset (from TAPLINE definition)
        bool is_camera_mode; //!< true if a valid camera mode has been selected, false otherwise
        bool firmwareloaded; //!< true if firmware is loaded, false otherwise
        bool is_longexposure_set; //!< true for long exposure mode (exptime in sec), false for exptime in msec
        bool is_window; //!< true if in window mode for h2rg, false if not
        bool is_autofetch;
        bool is_unp;                        //!< should I write unprocessed files?
        int win_hstart;
        int win_hstop;
        int win_vstart;
        int win_vstop;
        int taplines_store; //!< int number of original taplines
        std::string tapline0_store; //!< store tapline0 for window mode so can restore later
        std::string power_status;             //!< archon power status

        bool lastcubeamps;

        int ring_index;                     //!< index into ring buffer, counts 0,1,0,1,...
        std::vector<char*> signal_buf;      //!< signal frame data ring buffer
        std::vector<char*> reset_buf;       //!< reset frame data ring buffer
        char *archon_buf;                   //!< image data buffer as FETCH-ed from Archon
        uint32_t archon_buf_bytes;          //!< requested number of bytes allocated for archon_buf rounded up to block size
        uint32_t archon_buf_allocated;      //!< allocated number of bytes for archon_buf

        std::string trigin_state; //!< for external triggering of exposures

        int trigin_expose; //!< current value of trigin expose
        int trigin_expose_enable; //!< value which enables trigin expose
        int trigin_expose_disable; //!< value which disables trigin expose

        int trigin_untimed; //!< current value of trigin_untimed
        int trigin_untimed_enable; //!< value which enables trigin untimed
        int trigin_untimed_disable; //!< value which disables trigin untimed

        int trigin_readout; //!< current value of trigin_readout
        int trigin_readout_enable; //!< value which enables trigin readout
        int trigin_readout_disable; //!< value which disables trigin readout

        std::string trigin_exposeparam; //!< parameter name to write for trigin_expose
        std::string trigin_untimedparam; //!< parameter name to write for trigin_untimed
        std::string trigin_readoutparam; //!< parameter name to write for trigin_readout

        float heater_target_min; //!< minimum heater target temperature
        float heater_target_max; //!< maximum heater target temperature

        std::atomic<bool> archon_busy; //!< indicates a thread is accessing Archon
        std::mutex archon_mutex;
        //!< protects Archon from being accessed by multiple threads,
                                                    //!< use in conjunction with archon_busy flag
        std::string longexposeparam; //!< param name to control longexposure in ACF (empty=not supported)
        std::string exposeparam; //!< param name to trigger exposure when set =1

        std::string shutenableparam; //!< param name to enable shutter open on expose
        int shutenable_enable; //!< the value which enables shutter enable
        int shutenable_disable; //!< the value which disables shutter enable

        // Functions
        //
        void ring_index_inc() { if (++this->ring_index==2) this->ring_index=0; }
        int  prev_ring_index() { int i=this->ring_index-1; return( i<0 ? 1 : i ); }
        long save_unp(std::string args, std::string &retstring);
        long fits_compression(std::string args, std::string &retstring);

        static long interface(std::string &iface); //!< get interface type
        long configure_controller(); //!< get configuration parameters
        long prepare_archon_buffer(); //!< prepare archon_buf, allocating memory as needed
        long connect_controller( const std::string devices_in, std::string &retstring ); //!< open connection to archon controller
        long disconnect_controller(); //!< disconnect from archon controller
        long load_timing(std::string acffile); //!< load specified ACF then LOADTIMING
        long load_timing(std::string acffile, std::string &retstring);

        long load_firmware(std::string acffile); //!< load specified acf then APPLYALL
        long load_firmware(std::string acffile, std::string &retstring);

        long load_acf(std::string acffile); //!< only load (WCONFIG) the specified ACF file
        long set_camera_mode(std::string mode_in);

        long load_mode_settings(std::string mode);

        long native(const std::string &cmd);

        long archon_cmd(std::string cmd);

        long archon_cmd(std::string cmd, std::string &reply);

        long read_parameter(const std::string &paramname, std::string &valstring);

        long prep_parameter(const std::string &paramname, std::string value);

        long load_parameter(std::string paramname, std::string value);

        long fetchlog();

        long get_frame_status();

        void print_frame_status();

        long lock_buffer(int buffer);

        long get_timer(unsigned long int *timer);

        long fetch(uint64_t bufaddr, uint32_t bufblocks);

        long read_frame(); //!< read Archon frame buffer into host memory
        long hread_frame();

        long read_frame(Camera::frame_type_t frame_type); /// read Archon frame buffer into host memory
        long write_frame(); //!< write (a previously read) Archon frame buffer to disk
        long write_raw(); //!< write raw 16 bit data to a FITS file
        long write_config_key(const char *key, const char *newvalue, bool &changed);

        long write_config_key(const char *key, int newvalue, bool &changed);

        long write_parameter(const char *paramname, const char *newvalue, bool &changed);

        long write_parameter(const char *paramname, int newvalue, bool &changed);

        long write_parameter(const char *paramname, const char *newvalue);

        long write_parameter(const char *paramname, int newvalue);

        template<class T>
        long get_configmap_value(std::string key_in, T &value_out);

        void add_filename_key();

        long get_status_key( std::string key, std::string &value );  /// get value for indicated key from STATUS string

        long power( std::string args, std::string &retstring );      /// wrapper for do_power
        long do_power( std::string args, std::string &retstring );   /// set/get Archon power state

        long expose(std::string nseq_in);    /// new version
        long __expose(std::string nseq_in);  /// old version

        long hexpose(std::string nseq_in);

        long hsetup();

        long hroi(std::string geom_in, std::string &retstring);

        long hwindow(std::string state_in, std::string &state_out);

        long autofetch(std::string state_in, std::string &state_out);

        long video();

        long wait_for_exposure();

        long wait_for_readout();

        long hwait_for_readout();

        long get_parameter(std::string parameter, std::string &retstring);

        long set_parameter(std::string parameter, long value);

        long set_parameter(std::string parameter);

        long exptime(std::string exptime_in, std::string &retstring);

        void copy_keydb(); /// copy user keyword database into camera_info
        long longexposure(std::string state_in, std::string &state_out);

        long shutter(std::string shutter_in, std::string &shutter_out);

        long hdrshift(std::string bits_in, std::string &bits_out);

        long trigin(std::string state_in);

        long heater(std::string args, std::string &retstring);

        long sensor(std::string args, std::string &retstring);

        long bias(std::string args, std::string &retstring);

        long cds(std::string args, std::string &retstring);

        long inreg(std::string args);

        long region_of_interest(std::string args, std::string &retstring);

        long test(std::string args, std::string &retstring);

        /**
         * @var     struct geometry_t geometry[]
         * @details structure of geometry which is unique to each observing mode
         */
        struct geometry_t {
            int amps[2]; // number of amplifiers per detector for each axis, set in set_camera_mode
            int num_detect; // number of detectors, set in set_camera_mode
            int linecount; // number of lines per tap
            int pixelcount; // number of pixels per tap
            int framemode; // Archon deinterlacing mode, 0=topfirst, 1=bottomfirst, 2=split
        };

        /**
         * @var     struct tapinfo_t tapinfo[]
         * @details structure of tapinfo which is unique to each observing mode
         */
        struct tapinfo_t {
            int num_taps;
            int tap[16];
            float gain[16];
            float offset[16];
            std::string readoutdir[16];
        };

        /**
         * @var     struct frame_data_t frame
         * @details structure to contain Archon results from "FRAME" command
         */
        struct frame_data_t {
            int index; // index of newest buffer data
            int frame; // frame of newest buffer data
            int next_index; // index of next buffer
            std::string timer; // current hex 64 bit internal timer
            int rbuf; // current buffer locked for reading
            int wbuf; // current buffer locked for writing
            std::vector<int> bufsample; // sample mode 0=16 bit, 1=32 bit
            std::vector<int> bufcomplete; // buffer complete, 1=ready to read
            std::vector<int> bufmode; // buffer mode: 0=top 1=bottom 2=split
            std::vector<uint64_t> bufbase; // buffer base address for fetching
            std::vector<int> bufframen; // buffer frame number
            std::vector<int> bufwidth; // buffer width
            std::vector<int> bufheight; // buffer height
            std::vector<int> bufpixels; // buffer pixel progress
            std::vector<int> buflines; // buffer line progress
            std::vector<int> bufrawblocks; // buffer raw blocks per line
            std::vector<int> bufrawlines; // buffer raw lines
            std::vector<int> bufrawoffset; // buffer raw offset
            std::vector<uint64_t> buftimestamp; // buffer hex 64 bit timestamp
            std::vector<uint64_t> bufretimestamp; // buf trigger rising edge time stamp
            std::vector<uint64_t> buffetimestamp; // buf trigger falling edge time stamp
        } frame;

        /** @var      vector modtype
         *  @details  stores the type of each module from the SYSTEM command
         */
        std::vector<int> modtype;

        /** @var      vector modversion
         *  @details  stores the version of each module from the SYSTEM command
         */
        std::vector<std::string> modversion;

        std::string backplaneversion;

        /** @var      int lastframe
         *  @details  the last (I.E. previous) frame number acquired
         */
        int lastframe;

        /**
         * rawinfo_t is a struct which contains variables specific to raw data functions
         */
        struct rawinfo_t {
            int adchan; // selected A/D channels
            int rawsamples; // number of raw samples per line
            int rawlines; // number of raw lines
            int iteration; // iteration number
            int iterations; // number of iterations
        } rawinfo;

        /**
         * config_line_t is a struct for the configfile key=value map, used to
         * store the configuration line and its associated line number.
         */
        typedef struct {
            int line; // the line number, used for updating Archon
            std::string value; // used for configmap
        } config_line_t;

        /**
         * param_line_t is a struct for the PARAMETER name key=value map, used to
         * store parameters where the format is PARAMETERn=parametername=value
         */
        typedef struct {
            std::string key; // the PARAMETERn part
            std::string name; // the parametername part
            std::string value; // the value part
            int line; // the line number, used for updating Archon
        } param_line_t;

        typedef std::map<std::string, config_line_t> cfg_map_t;
        typedef std::map<std::string, param_line_t> param_map_t;

        cfg_map_t configmap;
        param_map_t parammap;

        /**
         * \var     modeinfo_t modeinfo
         * \details structure contains a configmap and parammap unique to each mode,
         *          specified in the [MODE_*] sections at the end of the .acf file.
         */
        typedef struct {
            int rawenable; //!< initialized to -1, then set according to RAWENABLE in .acf file
            cfg_map_t configmap; //!< key=value map for configuration lines set in mode sections
            param_map_t parammap; //!< PARAMETERn=parametername=value map for mode sections
            Common::FitsKeys acfkeys; //!< create a FitsKeys object to hold user keys read from ACF file for each mode
            geometry_t geometry;
            tapinfo_t tapinfo;
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
    /***** Archon::Interface **************************************************/
}
