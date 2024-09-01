/**
 * @file    camera.h
 * @brief   camera interface functions header file common to all camera interfaces (astrocam, archon)
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#pragma once

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

    /***** Camera::Camera *****************************************************/
    //
    class Camera {
      private:
        std::string image_dir;
        std::string base_name;
        std::string fits_naming;

        std::string fitstime; //!< "YYYYMMDDHHMMSS" uesd for filename, set by get_fitsname()

        mode_t dirmode; //!< user specified mode to OR with 0700 for imdir creation
        int image_num;
        bool is_datacube;
        bool is_longerror; //!< set to return error message on command port
        bool is_cubeamps; //!< should amplifiers be written as multi-extension data cubes?
        std::atomic<bool> _abortstate;;

        std::mutex abort_mutex;
        std::stringstream lasterrorstring; //!< a place to preserve an error message

      public:
        Camera() : image_dir("/tmp"), base_name("image"), fits_naming("time"),
                   dirmode(0), image_num(0), is_datacube(false), is_longerror(false), is_cubeamps(false),
                   _abortstate(false),
                   autodir_state(true), abortstate(false), writekeys_when("before") {
        }


        bool autodir_state;
        //!< if true then images are saved in a date subdir below image_dir, i.e. image_dir/YYYYMMDD/
        bool abortstate; //!< set true to abort the current operation (exposure, readout, etc.)

        std::string writekeys_when; //!< when to write fits keys "before" or "after" exposure
        Common::Queue async; /// message queue object

        void set_abortstate(bool state);

        bool get_abortstate();

        void set_dirmode(mode_t mode_in) { this->dirmode = mode_in; }

        std::map<int, std::string> firmware; //!< firmware file for given controller device number, read from .cfg file
        std::map<int, int> readout_time;
        //!< readout time in msec for given controller device number, read from .cfg file

        std::string power_status;              //!< archon power status

        void log_error(std::string function, std::string message);

        std::string get_longerror();

        long imdir(std::string dir_in);
        long imdir(std::string dir_in, std::string &dir_out);
        long autodir(std::string state_in, std::string &state_out);
        long basename(std::string name_in);
        long basename(std::string name_in, std::string &name_out);
        long imnum(std::string num_in, std::string &num_out);
        long writekeys(std::string writekeys_in, std::string &writekeys_out);
        long fitsnaming(std::string naming_in, std::string &naming_out);
        void increment_imnum() { if (this->fits_naming == "number") this->image_num++; };
        void set_fitstime(std::string time_in);
        long get_fitsname(std::string &name_out);
        long get_fitsname(std::string controllerid, std::string &name_out);

        void abort();

        void datacube(bool state_in);

        bool datacube();

        long datacube(std::string state_in, std::string &state_out);

        void longerror(bool state_in);

        bool longerror();

        long longerror(std::string state_in, std::string &state_out);

        void cubeamps(bool state_in);

        bool cubeamps();

        long cubeamps(std::string state_in, std::string &state_out);
    };
    /***** Camera::Camera *****************************************************/

    typedef enum {
        FRAME_IMAGE,
        FRAME_RAW,
        NUM_FRAME_TYPES
    } frame_type_t;

    const char *const frame_type_str[NUM_FRAME_TYPES] = {
        "IMAGE",
        "RAW"
    };


    /***** Camera::ExposureTime ***********************************************/
    /**
     * @class  ExposureTime
     * @brief  creates object that encapsulates exposure time with its unit
     *
     * This class allows setting and getting an exposure time together with
     * its unit. The default unit and value is msec and 0. Internally, the
     * class stores the exposure time as an unsigned 32 bit value in the
     * units of _unit but a specific unit can be requested with .s() and .ms()
     * functions. Standard operators + - * / > < >= <= == are overloaded here
     * to operate on scalars only.
     *
     */
    class ExposureTime {
      private:
        uint32_t    _value;            // exposure time in the current units
        bool        _is_set;           // has it been set using the class value() function?
        std::string _unit;             // "s" for seconds, "ms" for milliseconds
        bool        _is_longexposure;  // true for s, false for ms

      public:
        /**
         * @brief      class constructor optionally sets exposure time and unit
         * @param[in]  time  exposure time, defaults to 0 if not provided
         * @param[in]  u     unit, "s" or "ms", defaults to ms if not provided
         *
         * Since this is a generic class, there is no range check on the
         * exposure time value; that must be done by whatever instantiates
         * an ExposureTime object.
         *
         */
        explicit ExposureTime( uint32_t time=0, const std::string &u="ms" )
                   : _value(0), _is_set(false), _unit(u), _is_longexposure(u=="s") {
          if ( u != "ms" && u != "s" ) throw std::invalid_argument("invalid unit, expected \"s\" or \"ms\"");
          this->value(time);
        }

        /**
         * @brief      set the unit
         * @details    this will modify the value as necessary
         * @param[in]  u  "ms" or "s" for milliseconds or seconds
         */
        void unit( const std::string &newunit ) {
          if ( newunit == "s" || newunit == "ms" ) {
            if ( _unit != newunit ) {
              _value = ( newunit == "ms" ? _value*1000 : _value/1000 );
              _unit = newunit;
              _is_longexposure = ( newunit == "s" );
            }
          }
          else throw std::invalid_argument("invalid unit, expected \"s\" or \"ms\"");
        }

        /**
         * @brief      return the current unit
         * @return     "s" | "ms"
         */
        std::string unit() const { return _unit; }

        /**
         * @brief      set the longexposure state
         * @details    uses the class unit() function to both set the unit
         *             and modify the value as necessary.
         * @param[in]  true | false
         */
        void longexposure( bool state ) { this->unit( state ? "s" : "ms" ); }

        /**
         * @brief      return the longexposure state
         * @return     true | false
         */
	bool is_longexposure() const { return _is_longexposure; }

        /**
         * @brief      return the exposure time in units of milliseconds
         * @return     exposure time
         */
        uint32_t ms() const {
          return ( _unit == "ms" ? _value : _value * 1000 );
        }

        /**
         * @brief      return the exposure time in units of seconds
         * @return     exposure time
         */
        uint32_t s() const {
          return ( _unit == "s" ? _value : _value / 1000 );
        }

        /**
         * @brief      return the exposure time in the current units
         * @return     exposure time
         */
        uint32_t value() const {
          return _value;
        }

        /**
         * @brief      set the exposure time
         * @details    There is no range check on the value; that must be done
         *             by whatever instantiates an ExposureTime object.
         * @param[in]  time  exposure time in the current unit
         */
        void value( uint32_t time ) {
          _value = time;
          _is_set = true;
        }

        /**
         * @brief      return _is_set flag
         * @details    allows checking if a user has explicitly set the exposure time.
         * @return     true | false
         */
	bool is_set() const { return _is_set; }

        /**
         * @brief      overload operators to perform in the correct units
         * @details    Use .s() or .ms() to operate on seconds or milliseconds.
         *             These only work on scalars (not other ExposureTime objects).
         *
         * E.G. .ms() +/- 1000 will add/subtract 1000 msec to/from the current value
         *       .s() +/- 1    will add/subtract 1 sec to/from the current value
         *       .s() == xxx   will compare value in seconds to xxx
         */
        ExposureTime operator+( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() );
          return ExposureTime( val + scalar, _unit );
        }
        ExposureTime operator-( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() );
          return ExposureTime( val - scalar, _unit );
        }
        ExposureTime operator*( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return ExposureTime( val * scalar );
        }
        ExposureTime operator/( uint32_t scalar ) const {
          if ( scalar == 0 ) throw std::invalid_argument("division by zero");
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return ExposureTime( val / scalar );
        }
        bool operator<( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return val < scalar;
        }
        bool operator>( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return val > scalar;
        }
        bool operator==( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return val == scalar;
        }
        bool operator<=( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return val <= scalar;
        }
        bool operator>=( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return val >= scalar;
        }
    };
    /***** Camera::ExposureTime ***********************************************/


    /***** Camera::Information ************************************************/
    /**
     * @class  Information
     * @brief  
     *
     */
    class Information {
      private:
      public:
    int ccd_id;            //!< ID value of CCD
    int amp_id;            //!< ID value of CCD amplifier
    int framenum;          //!< Archon buffer frame number

    int serial_prescan;                 //!< Serial prescan number
    int serial_overscan;                //!< Serial overscan number 
    int parallel_overscan;      //!< Parallel overscan number
    int image_cols;                                     //!< Number of columns in the image
    int image_rows;                                     //!< Number of rows in the image
    std::string ccd_name;  //!< name of CCD
    std::string amp_name;  //!< name of CCD amplifier

    std::string detector;                                               //!< Detector name string
    std::string detector_software;      //!< Detector software version string
    std::string detector_firmware;      //!< Detector hardware version string

    float pixel_scale;  //!< Image pixel scale, like arcsec per pixel
    float ccd_gain;                     //!< Gain value for CCD electronics
    float read_noise;           //!< Read noise value
    float dark_current; //!< Dark current value

    size_t image_size;     //!< pixels per CCD
//  unsigned long image_memory;   //!< bytes per CCD

    // the following 4-element arrays are the components of FITS header
    // keywords, [0:1,2:3], i.e CCDSEC[0:1,2:3] where 0,1,2,3 are the 4
    // elements of detsize[]
    int ccdsec[4];   //!< Detection section
    int ampsec[4];   //!< amplifier section
    int trimsec[4];  //!< trim section
    int datasec[4];  //!< data section
    int biassec[4];  //!< bias section (over-scan)
    int detsec[4];   //!< detector section
    int detsize[4];  //!< physical size of the CCD

    std::vector<std::string> ccdid;  //!< CCDID = unique CCD identifier
    int bytes_per_pixel;                                //!< Bytes per pixle
    int gain;                                                                           //!< Image gain value

    int fits_compression_type;  //!< compression algorithm used for the FITS object
    int fits_noisebits;                                 //!< noisebits parameter used when compressing floating-point images
//  int current_observing_mode; //!< Camera imaging mode
//  float exposure_time;                                //!< Camera total exposure time
    float frame_exposure_time;  //!< Exposure time for individual frames

                std::string directory;                          //!< Data directory
    std::string image_name;                             //!< Name of the image, includes all info
    std::string basename;                               //!< Basic image name

    /** @var     bitpix
     *  @brief   FITS datatype (not literally bits per pixel)
     *  @details This is the datatype of the primary image. bitpix may be one
     *           of the following CFITSIO constants: BYTE_IMG, SHORT_IMG,
     *           LONG_IMG, FLOAT_IMG, DOUBLE_IMG, USHORT_IMG, ULONG_IMG,
     *           LONGLONG_IMG. Note that if you send in a bitpix of USHORT_IMG
     *           or ULONG_IMG, CCfits will set HDU::bitpix() to its signed
     *           equivalent (SHORT_IMG or LONG_IMG), and then set BZERO to 2^15
     *           or 2^31.
     */
    int         bitpix;

    int         datatype;                //!< FITS data type (corresponding to bitpix) used in set_axes()
    bool        type_set;                //!< set when FITS data type has been defined
    std::vector<long> naxes;             //!< array of axis lengths where element 0=cols, 1=rows, 2=cubedepth
    frame_type_t frame_type;             //!< frame_type is IMAGE or RAW
    long        detector_pixels[2];      //!< number of physical pixels. element 0=cols (pixels), 1=rows (lines)
    long        section_size;            //!< pixels to write for this section (could be less than full sensor size)
    uint32_t    image_memory;            //!< bytes per image sensor
    std::string current_observing_mode;  //!< the current mode
    std::string readout_name;            //!< name of the readout source
    int         readout_type;            //!< type of the readout source is an enum
    long        naxis;                   //!< number of axes in the image (3 for data cube)
    long        axes[3];                 //!< array of axis lengths where element 0=cols, 1=rows, 2=cubedepth <-- here for old fits.h
    int         binning[2];              //!< pixel binning, each axis
    long        axis_pixels[2];          //!< number of physical pixels used, before binning
    long        region_of_interest[4];   //!< region of interest
    bool        abortexposure;
    bool        iscube;                  //!< the info object given to the FITS writer will need to know cube status
    int         extension;               //!< extension number for data cubes
    bool        shutterenable;           //!< set true to allow the controller to open the shutter on expose, false to disable it
    std::string shutteractivate;         //!< shutter activation state
    double      exposure_progress;       //!< exposure progress (fraction)
    int         num_pre_exposures;       //!< pre-exposures are exposures taken but not saved
    std::string fits_name;               //!< contatenation of Camera's image_dir + image_name + image_num
    std::string start_time;              //!< system time when the exposure started (YYYY-MM-DDTHH:MM:SS.sss)

    std::vector<std::vector<long> > amp_section;

    ExposureTime exposure_time;          //!< exposure time object, carries value and unit

    Common::FitsKeys userkeys;           /// create a FitsKeys object for FITS keys specified by the user
    Common::FitsKeys systemkeys;         /// create a FitsKeys object for FITS keys imposed by the software


      // Default Information class constructor and initializer list
      //
      Information()
        : datatype(0),
          type_set(false),                   //!< set true when datatype has been defined
          naxes(2),
//        axes{1, 1, 1},                     // used by old fits.h system
//        cubedepth(1),
//        fitscubed(1),
//        ncoadd(0),
//        nslice(0),
          binning{1, 1},
          region_of_interest{1, 1, 1, 1},
//        exposure_aborted(false),
//        iscds(false),
//        nmcds(0),
//        ismex(false),
          shutteractivate(""),
//        requested_exptime(0),              //!< default is undefined
//        readouttime(-1),                   //!< default is undefined
          num_pre_exposures(0)               //!< default is no pre-exposures
//        is_cds(false),
//        nseq(1),
//        nexp(-1),
//        num_coadds(1),                     //!< default num of coadds
//        sampmode(-1),
//        sampmode_ext(-1),
//        sampmode_frames(-1)
      {
      }

        long pre_exposures(std::string num_in, std::string &num_out);

	/***** Camera::Information::set_axes **********************************/
        /**
         * @brief      set image memory space
         * @details    Sets up the image memory space, including the byte size
         *             of the images and the number of pixels in each axis. This
         *             information has to be correct to match the output memory
         *             size of camera images in the current setup and write
         *             FITS files properly.
         * @param[in]  bitpix_in  CCFits datatype, not the literal bits per pixel
         * @return     ERROR | NO_ERROR
         *
         */
        long set_axes( int bitpix_in ) {
          std::string function = "Camera::Information::set_axes";
          std::stringstream message;

          this->bitpix = bitpix_in;

          if (this->frame_type == FRAME_RAW) {
            this->bytes_per_pixel = 2;
            this->datatype = USHORT_IMG;
          }
          else {
            switch (this->bitpix) {
              case SHORT_IMG:
              case USHORT_IMG:
                this->bytes_per_pixel = 2;
                this->datatype = SHORT_IMG;
                message.str(""); message << "datatype=SHORT_IMG, 2 Bytes/pixel, ";
                break;
              case LONG_IMG:
              case ULONG_IMG:
              case FLOAT_IMG:
                this->bytes_per_pixel = 4;
                this->datatype = FLOAT_IMG;
                message.str(""); message << "datatype=FLOAT_IMG, 4 Bytes/pixel, ";
                break;
              default:
                message << "ERROR: unknown bitpix " << this->bitpix;
                logwrite(function, message.str());
                return ERROR;
            }
          }
          this->type_set = true; // datatype has been set

          this->naxis = 2;

          this->axis_pixels[0] = this->region_of_interest[1] -
                                 this->region_of_interest[0] + 1;
          this->axis_pixels[1] = this->region_of_interest[3] -
                                 this->region_of_interest[2] + 1;

          // set the array lengths of each axis in pixels
          //
          this->naxes[0] = this->axis_pixels[0] / this->binning[0];
          this->naxes[1] = this->axis_pixels[1] / this->binning[1];

          this->section_size = this->naxes[0] * this->naxes[1]; // Pixels to write for this image section
          this->image_size   = this->naxes[0] * this->naxes[1]; // Pixels to write for this image section

          this->image_memory = this->detector_pixels[0]
                               * this->detector_pixels[1] * this->bytes_per_pixel; // Bytes per detector

          message << this->naxes[0] << " x " << this->naxes[1] << " pixels";
          logwrite( function, message.str() );

#ifdef LOGLEVEL_DEBUG
          message.str("");
          message << "[DEBUG]"
                  << " region_of_interest[1]=" << this->region_of_interest[1]
                  << " region_of_interest[0]=" << this->region_of_interest[0]
                  << " region_of_interest[3]=" << this->region_of_interest[3]
                  << " region_of_interest[2]=" << this->region_of_interest[2]
                  << " naxes[0]=" << this->naxes[0]
                  << " naxes[1]=" << this->naxes[1]
                  << " detector_pixels[0]=" << this->detector_pixels[0]
                  << " detector_pixels[1]=" << this->detector_pixels[1]
                  << " section_size=" << this->section_size << "pix"
                  << " image_size=" << this->image_size << "pix"
                  << " image_memory=" << this->image_memory << "Bytes";
          logwrite( function, message.str() );
#endif

          return NO_ERROR;
        }
	/***** Camera::Information::set_axes **********************************/
    };
    /***** Camera::Information ************************************************/
}
