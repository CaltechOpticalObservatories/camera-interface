/**
 * @file    camera.h
 * @brief   camera interface functions header file common to all camera interfaces (astrocam, archon)
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#ifndef CAMERA_H
#define CAMERA_H

#include <CCfits/CCfits>           /// needed here for types in set_axes()
#include <dirent.h>                /// for imdir()
#include <map>
#include <vector>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>

#include "common.h"
#include "logentry.h"
#include "utilities.h"

// handy snprintf shortcut
#define SNPRINTF(VAR, ...) { snprintf(VAR, sizeof(VAR), __VA_ARGS__); }

namespace Camera {

  /**************** Camera::Camera ********************************************/
  //
  class Camera {
    private:
      std::string image_dir;
      std::string base_name;
      std::string fits_naming;
      std::string fitstime;                  //!< "YYYYMMDDHHMMSS" uesd for filename, set by get_fitsname()
      mode_t dirmode;                        //!< user specified mode to OR with 0700 for imdir creation
      int image_num;
      bool is_coadd;                         /// set true to co-add any processed cds frames
      bool is_mex;                           /// set true for multi-extensions
      bool is_longerror;                     //!< set to return error message on command port
      bool is_mexamps;                       //!< should amplifiers be written as multi-extension?
      std::atomic<bool> abortstate;          /// set true to abort the current operation (exposure, readout, etc.)
      std::atomic<bool> exposing;            /// set when exposure in progress
      std::stringstream lasterrorstring;     //!< a place to preserve an error message

    public:
      Camera();
      ~Camera();

      bool          autodir_state;           //!< if true then images are saved in a date subdir below image_dir, i.e. image_dir/YYYYMMDD/
      std::string   writekeys_when;          //!< when to write fits keys "before" or "after" exposure
      Common::Queue async;                   /// message queue object

      inline void set_abort()   { this->abortstate.store( true, std::memory_order_seq_cst );  };
      inline void clear_abort() { this->abortstate.store( false, std::memory_order_seq_cst ); };
      inline bool is_aborted()  { return this->abortstate.load( std::memory_order_seq_cst );  };

      inline void set_exposing()   { this->exposing.store( true, std::memory_order_seq_cst );  };
      inline void clear_exposing() { this->exposing.store( false, std::memory_order_seq_cst ); };
      inline bool is_exposing()    { return this->exposing.load( std::memory_order_seq_cst );  };

      inline bool coadd()       { return this->is_coadd;    };
      inline void coadd(bool in){ this->is_coadd = in;      };

      void set_dirmode( mode_t mode_in ) { this->dirmode = mode_in; }

      std::map<int, std::string> firmware;   //!< firmware file for given controller device number, read from .cfg file
      std::map<int, int> readout_time;       //!< readout time in msec for given controller device number, read from .cfg file

      std::string default_sampmode;          //!< optional default samplemode can be set in .cfg file
      std::string default_exptime;           //!< optional default exptime can be set in .cfg file
      std::string default_roi;               //!< optional default roi can be set in .cfg file

      std::string power_status;              //!< archon power status

      void log_error( std::string function, std::string message );

      std::string get_longerror();

      long imdir(std::string dir_in);
      long imdir(std::string dir_in, std::string& dir_out);
      long autodir(std::string state_in, std::string& state_out);
      long basename(std::string name_in);
      long basename(std::string name_in, std::string& name_out);
      long imnum(std::string num_in, std::string& num_out);
      long writekeys(std::string writekeys_in, std::string &writekeys_out);
      long fitsnaming(std::string naming_in, std::string& naming_out);
      void increment_imnum() { if (this->fits_naming.compare("number")==0) this->image_num++; };
      void set_fitstime(std::string time_in);
      long get_fitsname(std::string &name_out);
      long get_fitsname(std::string controllerid, std::string &name_out);
      void mex(bool state_in);
      bool mex();
      long mex(std::string state_in, std::string &state_out);
      long coadd(std::string state_in, std::string &state_out);
      void longerror(bool state_in);
      bool longerror();
      long longerror(std::string state_in, std::string &state_out);
      void mexamps(bool state_in);
      bool mexamps();
      long mexamps(std::string state_in, std::string &state_out);
  };
  /**************** Camera::Camera ********************************************/

  typedef enum {
    FRAME_IMAGE,
    FRAME_RAW,
    NUM_FRAME_TYPES
  } frame_type_t;

  const char * const frame_type_str[NUM_FRAME_TYPES] = {
    "IMAGE",
    "RAW"
  };

  /**************** Camera::Information ***************************************/
  //
  class Information {
    private:
    public:
      std::string   hostname;                //!< Archon controller hostname
      int           port;                    //!< Archon controller TPC/IP port number
      int           activebufs;              //!< Archon controller number of active frame buffers
      int           bitpix;                  //!< Archon bits per pixel based on SAMPLEMODE
      int           datatype;                //!< FITS data type (corresponding to bitpix) used in set_axes()
      bool          type_set;                //!< set when FITS data type has been defined
      frame_type_t  frame_type;              //!< frame_type is IMAGE or RAW
      long          detector_pixels[2];      //!< element 0=cols (pixels), 1=rows (lines)
      long          section_size;            //!< pixels to write for this section (could be less than full sensor size but accounts for cubedepth)
      long          image_memory;            //!< bytes per image sensor
      std::string   current_observing_mode;  //!< the current mode
      std::string   readout_name;            //!< name of the readout source
      int           readout_type;            //!< type of the readout source is an enum
      long          axes[3];                 //!< element 0=cols, 1=cols, 2=cubedepth

      // these break the STL map paradigm but it's simpler, and all that NIRC2 needs
      //
      double pixel_time;                     //!< pixel time in usec
      double pixel_skip_time;                //!< pixel skip time in usec
      double row_overhead_time;              //!< row overhead time in usec
      double row_skip_time;                  //!< row skip time in usec
      double frame_start_time;               //!< frame start time in usec
      double fs_pulse_time;                  //!< fs pulse time in usec

      /**
       * @var     cubedepth
       * @brief   depth, or number of slices for 3D data cubes
       * @details cubedepth is used to calculate memory allocation and is not necessarily the
       *          same as fitscubed, which is used to force the fits writer to create a cube.
       *
       */
      long          cubedepth;

      /**
       * @var     fitscubed
       * @brief   depth, or number of slices for 3D data cubes
       * @details fitscubed is used to tell the fitswriter to create a file with a 3rd axis or not
       *
       */
      long          fitscubed;

      int           ncoadd;                  /// current count of completed number of coadds
      int           nslice;                  /// current count of completed number of slices
      int           binning[2];
      long          axis_pixels[2];
      long          region_of_interest[4];
      long          image_center[2];
      long          imwidth;                 /// image width displayed (written to FITS)
      long          imheight;                /// image height displayed (written to FITS)
      long          imwidth_read;            /// image width read from controller
      long          imheight_read;           /// image height read from controller
      bool          exposure_aborted;        /// was this exposure aborted?
      bool          iscds;                   //!< is CDS subtraction requested?
      int           nmcds;                   ///  number of MCDS samples
      bool          ismex;                   //!< the info object given to the FITS writer will need to know multi-extension status
      std::atomic<int>  extension;           //!< extension number for multi-extension files
      bool          shutterenable;           //!< set true to allow the controller to open the shutter on expose, false to disable it
      std::string   shutteractivate;         //!< shutter activation state
      int32_t       exposure_time;           //!< requested exposure time in exposure_unit
      int32_t       exposure_delay;          //!< exposure delay given to controller in exposure_unit
      int32_t       requested_exptime;       //!< user-requested exposure time
      int32_t       readouttime;             //!< readout time, or minimum exposure time
      std::string   exposure_unit;           //!< exposure time unit
      int           exposure_factor;         //!< multiplier for exposure_unit relative to 1 sec (=1 for sec, =1000 for msec, etc.)
      double        exposure_progress;       //!< exposure progress (fraction)
      int           num_pre_exposures;       //!< pre-exposures are exposures taken but not saved
      bool          is_cds;                  //!< is this a CDS exposure? (subtraction will be done)
      int           nseq;                    //!< number of exposure sequences
      int           nexp;                    //!< the number to be passed to do_expose (not all instruments use this)
      int           num_coadds;              //!< number of coadds
      int           sampmode;                //!< sample mode (NIRC2)
      int           sampmode_ext;            //!< sample mode number of extensions (NIRC2)
      int           sampmode_frames;         //!< sample mode number of frames (NIRC2)
      std::string   fits_name;               //!< contatenation of Camera's image_dir + image_name + image_num
      std::string   cmd_start_time;          //!< system time when the expose command arrived (YYYY-MM-DDTHH:MM:SS.sss)
      std::string   start_time;              //!< system time when the exposure started (YYYY-MM-DDTHH:MM:SS.sss)
      std::string   stop_time;               //!< system time when the exposure stopped (YYYY-MM-DDTHH:MM:SS.sss)

      std::vector< std::vector<long> > amp_section;

      Common::FitsKeys userkeys;     /// create a FitsKeys object for FITS keys specified by the user
      Common::FitsKeys systemkeys;   /// create a FitsKeys object for FITS keys imposed by the software
      Common::FitsKeys extkeys;      /// create a FitsKeys object for FITS keys imposed by the software

      // Default Information class constructor and initializer list
      //
      Information()
        : datatype(0),
          type_set(false),                   //!< set true when datatype has been defined
          axes{1, 1, 1},
          pixel_time(0),
          pixel_skip_time(0),
          row_overhead_time(0),
          row_skip_time(0),
          frame_start_time(0),
          fs_pulse_time(0),
          cubedepth(1),
          fitscubed(1),
          ncoadd(0),
          nslice(0),
          binning{1, 1},
          region_of_interest{1, 1, 1, 1},
          image_center{1, 1},
          exposure_aborted(false),
          iscds(false),
          nmcds(0),
          ismex(false),
          shutteractivate(""),
          exposure_time(-1),                 //!< default is undefined
          requested_exptime(0),              //!< default is undefined
          readouttime(-1),                   //!< default is undefined
          exposure_unit(""),                 //!< default unit undefined
          exposure_factor(-1),               //!< default factor is undefined
          num_pre_exposures(0),              //!< default is no pre-exposures
          is_cds(false),
          nseq(1),
          nexp(-1),
          num_coadds(1),                     //!< default num of coadds
          sampmode(-1),
          sampmode_ext(-1),
          sampmode_frames(-1)
      {
      }

    // Copy constructor needed because the default copy constructor is deleted,
    // due to the atomic variable (which doesn't have a copy or copy assignment
    // constructor). So this manually loads and stores the atomic. All of the
    // other variables have to come along, too.
    //
    Information( const Information &other )
        : hostname( other.hostname ),
          port( other.port ),
          activebufs( other.activebufs ),
          bitpix( other.bitpix ),
          datatype( other.datatype ),
          type_set( other.type_set ),
          frame_type( other.frame_type ),
          detector_pixels{other.detector_pixels[0], other.detector_pixels[1]},
          section_size( other.section_size ),
          image_memory( other.image_memory ),
          current_observing_mode( other.current_observing_mode ),
          readout_name( other.readout_name ),
          readout_type( other.readout_type ),
          axes{other.axes[0], other.axes[1], other.axes[2]},
          pixel_time( other.pixel_time ),
          pixel_skip_time( other.pixel_skip_time ),
          row_overhead_time( other.row_overhead_time ),
          row_skip_time( other.row_skip_time ),
          frame_start_time( other.frame_start_time ),
          fs_pulse_time( other.fs_pulse_time ),
          cubedepth( other.cubedepth ),
          fitscubed( other.fitscubed ),
          ncoadd( other.ncoadd ),
          nslice( other.nslice ),
          binning{other.binning[0], other.binning[1]},
          axis_pixels{other.axis_pixels[0], other.axis_pixels[1]},
          region_of_interest{other.region_of_interest[0], other.region_of_interest[1], other.region_of_interest[2], other.region_of_interest[3]},
          image_center{other.image_center[0], other.image_center[1]},
          imwidth( other.imwidth ),
          imheight( other.imheight ),
          imwidth_read( other.imwidth_read ),
          imheight_read( other.imheight_read ),
          exposure_aborted( other.exposure_aborted ),
          iscds( other.iscds ),
          nmcds( other.nmcds ),
          ismex( other.ismex ),
          extension( other.extension.load() ),
          shutterenable( other.shutterenable ),
          shutteractivate( other.shutteractivate ),
          exposure_time( other.exposure_time ),
          exposure_delay( other.exposure_delay ),
          requested_exptime( other.requested_exptime ),
          readouttime( other.readouttime ),
          exposure_unit( other.exposure_unit ),
          exposure_factor( other.exposure_factor ),
          exposure_progress( other.exposure_progress ),
          num_pre_exposures( other.num_pre_exposures ),
          is_cds( other.is_cds ),
          nseq( other.nseq ),
          nexp( other.nexp ),
          num_coadds( other.num_coadds ),
          sampmode( other.sampmode ),
          sampmode_ext( other.sampmode_ext ),
          sampmode_frames( other.sampmode_frames ),
          fits_name( other.fits_name ),
          cmd_start_time( other.cmd_start_time ),
          start_time( other.start_time ),
          stop_time( other.stop_time ),
          amp_section( other.amp_section ),
          userkeys( other.userkeys ),
          systemkeys( other.systemkeys ),
          extkeys( other.extkeys )
    {
    }

    // Copy assignment operator
    //
    Information& operator=( const Information& other ) {
        if ( this != &other ) {
            hostname = other.hostname;
            port = other.port;
            activebufs = other.activebufs;
            bitpix = other.bitpix;
            datatype = other.datatype;
            type_set = other.type_set;
            frame_type = other.frame_type;
            detector_pixels[0] = other.detector_pixels[0];
            detector_pixels[1] = other.detector_pixels[1];
            section_size = other.section_size;
            image_memory = other.image_memory;
            current_observing_mode = other.current_observing_mode;
            readout_name = other.readout_name;
            readout_type = other.readout_type;
            axes[0] = other.axes[0];
            axes[1] = other.axes[1];
            axes[2] = other.axes[2];
            pixel_time = other.pixel_time;
            pixel_skip_time = other.pixel_skip_time;
            row_overhead_time = other.row_overhead_time;
            row_skip_time = other.row_skip_time;
            frame_start_time = other.frame_start_time;
            fs_pulse_time = other.fs_pulse_time;
            cubedepth = other.cubedepth;
            fitscubed = other.fitscubed;
            ncoadd = other.ncoadd;
            nslice = other.nslice;
            binning[0] = other.binning[0];
            binning[1] = other.binning[1];
            axis_pixels[0] = other.axis_pixels[0];
            axis_pixels[1] = other.axis_pixels[1];
            region_of_interest[0] = other.region_of_interest[0];
            region_of_interest[1] = other.region_of_interest[1];
            region_of_interest[2] = other.region_of_interest[2];
            region_of_interest[3] = other.region_of_interest[3];
            image_center[0] = other.image_center[0];
            image_center[1] = other.image_center[1];
            imwidth = other.imwidth;
            imheight = other.imheight;
            imwidth_read = other.imwidth_read;
            imheight_read = other.imheight_read;
            exposure_aborted = other.exposure_aborted;
            iscds = other.iscds;
            nmcds = other.nmcds;
            ismex = other.ismex;
            extension.store( other.extension.load() );
            shutterenable = other.shutterenable;
            shutteractivate = other.shutteractivate;
            exposure_time = other.exposure_time;
            exposure_delay = other.exposure_delay;
            requested_exptime = other.requested_exptime;
            readouttime = other.readouttime;
            exposure_unit = other.exposure_unit;
            exposure_factor = other.exposure_factor;
            exposure_progress = other.exposure_progress;
            num_pre_exposures = other.num_pre_exposures;
            is_cds = other.is_cds;
            nseq = other.nseq;
            nexp = other.nexp;
            num_coadds = other.num_coadds;
            sampmode = other.sampmode;
            sampmode_ext = other.sampmode_ext;
            sampmode_frames = other.sampmode_frames;
            fits_name = other.fits_name;
            cmd_start_time = other.cmd_start_time;
            start_time = other.start_time;
            stop_time = other.stop_time;
            amp_section = other.amp_section;
            userkeys = other.userkeys;
            systemkeys = other.systemkeys;
            extkeys = other.extkeys;
        }
        return *this;
    }

    // Destructor
    //
    ~Information() = default;

      long pre_exposures( std::string num_in, std::string &num_out );

      long set_axes() {
        std::string function = "Camera::Information::set_axes";
        std::stringstream message;
        long bytes_per_pixel;

        if ( this->frame_type == FRAME_RAW ) {
          bytes_per_pixel = 2;
          this->datatype = USHORT_IMG;
        }
        else {
          switch ( this->bitpix ) {
            case 16:
              bytes_per_pixel = 2;
              this->datatype = USHORT_IMG;
              break;
            case 32:
              bytes_per_pixel = 4;
              this->datatype = FLOAT_IMG;
              break;
          default:
            message << "ERROR: unknown bitpix " << this->bitpix << ": expected {16,32}";
            logwrite( function, message.str() );
            return (ERROR);
          }
        }
        this->type_set = true;         // datatype has been set

        this->axis_pixels[0] = this->region_of_interest[1] -
                               this->region_of_interest[0] + 1;
        this->axis_pixels[1] = this->region_of_interest[3] -
                               this->region_of_interest[2] + 1;

        if ( this->cubedepth > 1 ) {
          this->axes[0] = this->axis_pixels[0] / this->binning[0];  // cols
          this->axes[1] = this->axis_pixels[1] / this->binning[1];  // rows
          this->axes[2] = this->cubedepth;                          // cubedepth
        }
        else {
          this->axes[0] = this->axis_pixels[0] / this->binning[0];  // cols
          this->axes[1] = this->axis_pixels[1] / this->binning[1];  // rows
          this->axes[2] = 1;                                        // (no cube)
        }

        this->section_size = this->axes[0] * this->axes[1] * this->axes[2];    // Pixels to write for this image section, includes depth for 3D data cubes

        this->image_memory = this->detector_pixels[0] 
                           * this->detector_pixels[1] * bytes_per_pixel;       // Bytes per detector, single frame read

#ifdef LOGLEVEL_DEBUG
        message << "[DEBUG] region_of_interest[1]=" << this->region_of_interest[1]
                << " region_of_interest[0]=" << this->region_of_interest[0]
                << " region_of_interest[3]=" << this->region_of_interest[3]
                << " region_of_interest[2]=" << this->region_of_interest[2]
                << " axes[0]=" << this->axes[0]
                << " axes[1]=" << this->axes[1]
                << " axes[2]=" << this->axes[2];
        logwrite( function, message.str() );
#endif

        return (NO_ERROR);
      }
  };
  /**************** Camera::Information ***************************************/

}
#endif
