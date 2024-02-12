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
      bool is_datacube;
      bool is_longerror;                     //!< set to return error message on command port
      bool is_cubeamps;                      //!< should amplifiers be written as multi-extension data cubes?
      std::atomic<bool> _abortstate;;
      std::mutex abort_mutex;
      std::stringstream lasterrorstring;     //!< a place to preserve an error message

    public:
      Camera();
      ~Camera() {}

      bool          autodir_state;           //!< if true then images are saved in a date subdir below image_dir, i.e. image_dir/YYYYMMDD/
      bool          abortstate;              //!< set true to abort the current operation (exposure, readout, etc.)
      std::string   writekeys_when;          //!< when to write fits keys "before" or "after" exposure
      Common::Queue async;                   /// message queue object

      void set_abortstate(bool state);
      bool get_abortstate();

      void set_dirmode( mode_t mode_in ) { this->dirmode = mode_in; }

      std::map<int, std::string> firmware;   //!< firmware file for given controller device number, read from .cfg file
      std::map<int, int> readout_time;       //!< readout time in msec for given controller device number, read from .cfg file

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
      void increment_imnum() { if (this->fits_naming=="number") this->image_num++; };
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
      long          section_size;            //!< pixels to write for this section (could be less than full sensor size)
      long          image_memory;            //!< bytes per image sensor
      std::string   current_observing_mode;  //!< the current mode
      std::string   readout_name;            //!< name of the readout source
      int           readout_type;            //!< type of the readout source is an enum
      long          naxis;
      long          axes[2];
      int           binning[2];
      long          axis_pixels[2];
      long          region_of_interest[4];
      long          image_center[2];
      bool          abortexposure;
      bool          iscube;                  //!< the info object given to the FITS writer will need to know cube status
      int           extension;               //!< extension number for data cubes
      bool          shutterenable;           //!< set true to allow the controller to open the shutter on expose, false to disable it
      std::string   shutteractivate;         //!< shutter activation state
      int32_t       exposure_time;           //!< exposure time in exposure_unit
      std::string   exposure_unit;           //!< exposure time unit
      int           exposure_factor;         //!< multiplier for exposure_unit relative to 1 sec (=1 for sec, =1000 for msec, etc.)
      double        exposure_progress;       //!< exposure progress (fraction)
      int           num_pre_exposures;       //!< pre-exposures are exposures taken but not saved
      std::string   fits_name;               //!< contatenation of Camera's image_dir + image_name + image_num
      std::string   start_time;              //!< system time when the exposure started (YYYY-MM-DDTHH:MM:SS.sss)

      std::vector< std::vector<long> > amp_section;

      Common::FitsKeys userkeys;     /// create a FitsKeys object for FITS keys specified by the user
      Common::FitsKeys systemkeys;   /// create a FitsKeys object for FITS keys imposed by the software


  Information() {
        this->axes[0] = 1;
        this->axes[1] = 1;
        this->binning[0] = 1;
        this->binning[1] = 1;
        this->region_of_interest[0] = 1;
        this->region_of_interest[1] = 1;
        this->region_of_interest[2] = 1;
        this->region_of_interest[3] = 1;
        this->image_center[0] = 1;
        this->image_center[1] = 1;
        this->iscube = false;
        this->datatype = -1;
        this->type_set = false;              //!< set true when datatype has been defined
        this->exposure_time = -1;            //!< default exposure time is undefined
        this->exposure_unit = "";            //!< default exposure unit is undefined
        this->exposure_factor = -1;          //!< default factor is undefined
        this->shutteractivate = "";
        this->num_pre_exposures = 0;         //!< default is no pre-exposures
      }

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
              this->datatype = SHORT_IMG;
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

        this->naxis = 2;

        this->axis_pixels[0] = this->region_of_interest[1] -
                               this->region_of_interest[0] + 1;
        this->axis_pixels[1] = this->region_of_interest[3] -
                               this->region_of_interest[2] + 1;

        this->axes[0] = this->axis_pixels[0] / this->binning[0];
        this->axes[1] = this->axis_pixels[1] / this->binning[1];

        this->section_size = this->axes[0] * this->axes[1];                    // Pixels to write for this image section
        this->image_memory = this->detector_pixels[0] 
                           * this->detector_pixels[1] * bytes_per_pixel;       // Bytes per detector

#ifdef LOGLEVEL_DEBUG
        message << "[DEBUG] region_of_interest[1]=" << this->region_of_interest[1]
                << " region_of_interest[0]=" << this->region_of_interest[0]
                << " region_of_interest[3]=" << this->region_of_interest[3]
                << " region_of_interest[2]=" << this->region_of_interest[2]
                << " axes[0]=" << this->axes[0]
                << " axes[1]=" << this->axes[1];
        logwrite( function, message.str() );
#endif

        return (NO_ERROR);
      }
  };
  /**************** Camera::Information ***************************************/

}
#endif
