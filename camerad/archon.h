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
#include <variant>

#include "utilities.h"
#include "common.h"
#include "camera.h"
#include "config.h"
#include "logentry.h"
#include "network.h"
#include "fits.h"       // old
#include "fits_file.h"  // new

#define REPLY_LEN 100 * BLOCK_LEN  //!< Reply buffer size (over-estimate)

namespace Archon {

    constexpr int MAXADCCHANS =   16;            //!< max number of ADC channels per controller (4 mod * 4 ch/mod)
    constexpr int MAXADMCHANS =   72;            //!< max number of ADM channels per controller (4 mod * 18 ch/mod)
    constexpr int BLOCK_LEN   = 1024;            //!< Archon block size

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
    const std::string REV_RAMP          = "1.0.548";
    const std::string REV_SENSORCURRENT = "1.0.758";
    const std::string REV_HEATERTARGET  = "1.0.1087";
    const std::string REV_FRACTIONALPID = "1.0.1054";
    const std::string REV_VCPU          = "1.0.784";

    const std::string_view QUIET = "quiet";      // allows sending commands without logging
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
     * @brief   deinterlacing modes
     */
    enum class DeInterlaceMode {
      NONE,
      RXRVIDEO
    };

    /***** PostProcessBase ***************************************************/
    /**
     * @brief   base class for PostProcess template class
     * @details This is the polymorphic base class for the PostProcess
     *          template class.
     *
     */
    class PostProcessBase {
      protected:
        DeInterlaceMode _mode;
        uint16_t _rows;
        uint16_t _cols;
        long _error;

      public:
        PostProcessBase( DeInterlaceMode mode ) : _mode(mode), _rows(0), _cols(0), _error(NO_ERROR) { }

        virtual ~PostProcessBase() = default;

        long get_error() const { return _error; }

        void set_mode( DeInterlaceMode mode ) { _mode = mode; }

        DeInterlaceMode get_mode() const { return _mode; }

        void set_dimensions( uint16_t rows, uint16_t cols ) {
          _rows = rows;
          _cols = cols;
        }
    };
    /***** PostProcessBase ***************************************************/

    /***** PostProcess *******************************************************/
    /**
     * @brief   template class for deinterlacing
     * @details This class is templated to handle the data types in the
     *          variant. It stores pointers to the buffers needed for
     *          deinterlacing, raw input, and the signal and reset output
     *          buffers. The deinterlace() function performs the deinterlacing.
     *
     *          If the deinterlacing mode is not specified on class contruction
     *          then DeInterlaceMode::NONE is used. The deinterlacing mode can
     *          always be changed with the base class set_mode() function.
     *
     */
    template <typename T>
    class PostProcess : public PostProcessBase {
      private:
        T* rawbuf;
        T* sigbuf;
        T* resbuf;

        // TODO move to a separate file
        void deinterlace_RXRVIDEO() {
          std::string function="Archon::PostProcess::deinterlace_RXRVIDEO";
          std::stringstream message;

          if ( _rows==0 || _cols==0 ) {
            message.str(""); message << "ERROR invalid image dimensions rows=" << _rows << " cols=" << _cols
                                     << ": cannot be zero";
            logwrite( function, message.str() );
            _error = ERROR;
            return;
          }

          if ( rawbuf==nullptr || sigbuf==nullptr || resbuf==nullptr ) {
            logwrite( function, "ERROR image buffers not initialized" );
            _error = ERROR;
            return;
          }

          logwrite( "PostProcess::deinterlace_RXRVIDEO", "actual deinterlacing here" );

          for ( int row=0; row < _rows; ++row ) {
            for ( int col=0; col < _cols; col+=64 ) {
              std::memcpy( &sigbuf[row*_cols + col], &rawbuf[row*_cols*2 + col*2],      64*sizeof(T) );
              std::memcpy( &resbuf[row*_cols + col], &rawbuf[row*_cols*2 + col*2 + 64], 64*sizeof(T) );
            }
          }
          return;
        }

      public:
        PostProcess(): PostProcessBase(DeInterlaceMode::NONE), rawbuf(nullptr), sigbuf(nullptr), resbuf(nullptr) { }
        PostProcess( DeInterlaceMode mode ): PostProcessBase(mode), rawbuf(nullptr), sigbuf(nullptr), resbuf(nullptr) { }

        /***** PostProcess::subtract_frames ***********************************/
        /**
         * @brief      performs the CDS subtraction
         * @param[in]  image   T pointer to buffer with raw image
         * @param[in]  signal  T pointer to buffer to hold signal frame
         * @param[in]  reset   T pointer to buffer to hold reset frame
         *
         */
        void subtract_frames( T* image, T* signal, T* reset ) {
          std::string function="Archon::PostProcess::subtract_frames";
          std::stringstream message;
          message << "subtracting frames for datatype " << demangle(typeid(T).name());
          logwrite( function, message.str() );
          return;
        }
        /***** PostProcess::subtract_frames ***********************************/

        /***** PostProcess::deinterlace ***************************************/
        /**
         * @brief      calls the appropriate deinterlacing mode function
         * @param[in]  image   T pointer to buffer with raw image
         * @param[in]  signal  T pointer to buffer to hold signal frame
         * @param[in]  reset   T pointer to buffer to hold reset frame
         *
         */
        void deinterlace( T* image, T* signal, T* reset ) {
          std::string function="Archon::PostProcess::deinterlace";
          std::stringstream message;

          message << "deinterlacing for datatype " << demangle(typeid(T).name());
          logwrite( function, message.str() );

          this->rawbuf = image;
          this->sigbuf = signal;
          this->resbuf = reset;

          switch ( _mode ) {
            case DeInterlaceMode::RXRVIDEO:
              logwrite( function, "selected mode RXRVIDEO" );
              deinterlace_RXRVIDEO();
              break;

            default:
              message.str(""); message << "ERROR unknown deinterlacing mode ";
              logwrite( function, message.str() );
          }

          return;
        }
        /***** PostProcess::deinterlace ***************************************/
    };
    /***** PostProcess *******************************************************/

    /***** DeInterlace *******************************************************/
    /**
     * @brief      visitor class for deinterlacing
     * @details    This class uses a template operator() to handle each specific
     *             PostProcess type specified by the std::variant. When
     *             std::visit is called with an instance of this visitor class
     *             and a PostProcess variant, it will invoke the appropriate
     *             operator() based on the type of the PostProcess object. The
     *             visitor passes void pointers to the buffers, casting them to
     *             the correct type.
     *
     */
    class DeInterlace {
      private:
        void* _image;
        void* _signal;
        void* _reset;

      public:
        DeInterlace( void* image, void* signal, void* reset )
          : _image(image), _signal(signal), _reset(reset) { }

        template <typename T>
        void operator()(PostProcess<T> &obj) const {
          obj.deinterlace( static_cast<T*>(_image), static_cast<T*>(_signal), static_cast<T*>(_reset) );
        }
    };
    /***** DeInterlace *******************************************************/

    /***** CDS_Subtact *******************************************************/
    /**
     * @brief      visitor class for CDS subtraction
     * @details    This class uses a template operator() to handle each specific
     *             PostProcess type specified by the std::variant. When
     *             std::visit is called with an instance of this visitor class
     *             and a PostProcess variant, it will invoke the appropriate
     *             operator() based on the type of the PostProcess object. The
     *             visitor passes void pointers to the buffers, casting them to
     *             the correct type.
     *
     */
    class CDS_Subtract {
      private:
        void* _image;
        void* _signal;
        void* _reset;

      public:
        CDS_Subtract( void* image, void* signal, void* reset )
          : _image(image), _signal(signal), _reset(reset) { }

        template <typename T>
        void operator()(PostProcess<T> &obj) const {
          obj.subtract_frames( static_cast<T*>(_image), static_cast<T*>(_signal), static_cast<T*>(_reset) );
        }
    };
    /***** CDS_Subtract ******************************************************/


    class Interface {
      private:
        unsigned long int start_timer, finish_timer; //!< Archon internal timer, start and end of exposure
        int n_hdrshift; //!< number of right-shift bits for Archon buffer in HDR mode

      public:
        Interface();

        ~Interface();

        // Class Objects
        //
        Network::TcpSocket archon;
        Camera::Information camera_info; /// this is the main camera_info object
        Camera::Information fits_info;   /// used to copy the camera_info object to preserve info for FITS writing
        Camera::Camera camera;           /// instantiate a Camera object
        Common::FitsKeys userkeys;       //!< instantiate a Common object
        Common::FitsKeys systemkeys;     //!< instantiate a Common object

        template<typename T>
        void copy_image_buf( const char* rawbuf, std::vector<T> &destbuf, size_t npix, size_t bpp ) {
          destbuf.resize(npix);
          for ( size_t i=0; i<npix; ++i ) {
            std::memcpy( &destbuf[i], &rawbuf[i*bpp], sizeof(T) );
          }
          std::stringstream message;
          message << "typed_buf is of type " << demangle(typeid(T).name());
          logwrite( "Archon::Interface::copy_image_buf", message.str() );
          return;
        }

        std::variant<std::vector<int16_t>,
                     std::vector<uint16_t>,
                     std::vector<uint32_t>> typed_buf;

        struct typed_buf_ptr {
          template<typename T>
          void* operator()(const std::vector<T> &vec) const {
            return ( vec.empty() ? nullptr : static_cast<void*>(const_cast<T*>(vec.data())) );
          }
        };

        // Create a deinterlacer using a variant of acceptable data types
        //
        std::variant<PostProcess<int16_t>,
                     PostProcess<uint16_t>,
                     PostProcess<uint32_t>> postprocessor;

        void set_frame_dimensions( uint16_t r, uint16_t c ) {
          auto SetDimensions = [rows=r, cols=c](auto &postprocessor) { postprocessor.set_dimensions(rows,cols); };
          std::visit( SetDimensions, postprocessor );
          return;
        }

        Config config;

        bITS_file fits_file; //!< instantiate a FITS container object

        int activebufs; //!< Archon controller number of active frame buffers
        int msgref; //!< Archon message reference identifier, matches reply to command
        bool abort;
        int taplines;
        int configlines;  //!< number of configuration lines
        bool logwconfig;  //!< optionally log WCONFIG commands
        std::vector<int> gain; //!< digital CDS gain (from TAPLINE definition)
        std::vector<int> offset; //!< digital CDS offset (from TAPLINE definition)
        bool modeselected; //!< true if a valid mode has been selected, false otherwise
        bool firmwareloaded; //!< true if firmware is loaded, false otherwise
        bool is_longexposure_set; //!< true for long exposure mode (exptime in sec), false for exptime in msec
        bool is_window; //!< true if in window mode for h2rg, false if not
        bool is_autofetch;
        int win_hstart;
        int win_hstop;
        int win_vstart;
        int win_vstop;
        int taplines_store; //!< int number of original taplines
        std::string tapline0_store; //!< store tapline0 for window mode so can restore later

        bool lastcubeamps;

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

        int ring_index;                     //!< index into ring buffer, counts 0,1,0,1,...
        std::vector<char*> signal_buf;      //!< signal frame data ring buffer
        std::vector<char*> reset_buf;       //!< reset frame data ring buffer
        char *image_buf;                    //!< image data buffer
        uint32_t image_buf_bytes;           //!< requested number of bytes allocated for image_buf rounded up to block size
        uint32_t image_buf_allocated;       //!< allocated number of bytes for image_buf

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
        static long interface(std::string &iface); //!< get interface type
        long configure_controller(); //!< get configuration parameters
        long prepare_image_buffer(); //!< prepare image_buf, allocating memory as needed
        long connect_controller(const std::string &devices_in); //!< open connection to archon controller
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

        long get_status_key( std::string key, std::string &value );     /// get value for indicated key from STATUS string

        long power( std::string state_in, std::string &retstring );     /// wrapper for do_power
        long do_power( std::string state_in, std::string &retstring );  /// set/get Archon power state

        long expose(std::string nseq_in);

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
}
