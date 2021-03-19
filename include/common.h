/**
 * @file    common.h
 * @brief   common interface functions header file
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#ifndef COMMON_H
#define COMMON_H

#include <CCfits/CCfits>           //!< needed here for types in set_axes()
#include <map>
#include <vector>
#include <mutex>
#include <atomic>

#include "logentry.h"
#include "utilities.h"

const long NOTHING = -1;
const long NO_ERROR = 0;
const long ERROR = 1;
const long BUSY = 2;
const long TIMEOUT = 3;

// handy snprintf shortcut
#define SNPRINTF(VAR, ...) { snprintf(VAR, sizeof(VAR), __VA_ARGS__); }

namespace Common {

  class FitsKeys {
    private:
    public:
      FitsKeys() {}
      ~FitsKeys() {}

      std::string get_keytype(std::string keyvalue);         //!< return type of keyword based on value
      long listkeys();                                       //!< list FITS keys in the internal database
      long addkey(std::string arg);                          //!< add FITS key to the internal database

      typedef struct {                                       //!< structure of FITS keyword internal database
        std::string keyword;
        std::string keytype;
        std::string keyvalue;
        std::string keycomment;
      } user_key_t;

      typedef std::map<std::string, user_key_t> fits_key_t;  //!< STL map for the actual keyword database

      fits_key_t keydb;                                      //!< keyword database
  };

  class Common {
    private:
      std::string image_dir;
      std::string base_name;
      std::string fits_naming;
      std::string fitstime;                                  //!< "YYYYMMDDHHMMSS" uesd for filename, set by get_fitsname()
      int image_num;
      std::atomic<bool> _abortstate;;
      std::mutex abort_mutex;

    public:
      Common();
      ~Common() {}

      std::string   shutterstate;            //!< one of 4 allowed states: enable, disable, open, close
      bool          shutterenable;           //!< set true to allow the controller to open the shutter on expose, false to disable it
      bool          abortstate;              //!< set true to abort the current operation (exposure, readout, etc.)

      void set_abortstate(bool state);
      bool get_abortstate();

      std::map<int, std::string> firmware;   //!< firmware file for given controller device number, read from .cfg file
      std::map<int, int> readout_time;       //!< readout time in msec for given controller device number, read from .cfg file

      long imdir(std::string dir_in);
      long imdir(std::string dir_in, std::string& dir_out);
      long basename(std::string name_in);
      long basename(std::string name_in, std::string& name_out);
      long imnum(std::string num_in, std::string& num_out);
      long fitsnaming(std::string naming_in, std::string& naming_out);
      void increment_imnum() { if (this->fits_naming.compare("number")==0) this->image_num++; };
      void set_fitstime(std::string time_in);
      std::string get_fitsname();
      std::string get_fitsname(std::string controllerid);
      void abort();
  };

  typedef enum {
    FRAME_IMAGE,
    FRAME_RAW,
    NUM_FRAME_TYPES
  } frame_type_t;

  const char * const frame_type_str[NUM_FRAME_TYPES] = {
    "IMAGE",
    "RAW"
  };

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
      long          detector_pixels[2];
      long          image_size;              //!< pixels per image sensor
      long          image_memory;            //!< bytes per image sensor
      std::string   current_observing_mode;  //!< the current mode
      long          naxis;
      long          axes[2];
      int           binning[2];
      long          axis_pixels[2];
      long          region_of_interest[4];
      long          image_center[2];
      std::string   shutterstate;
      bool          openshutter;
      bool          abortexposure;
      bool          datacube;
      int           extension;               //!< extension number for data cubes
      int           exposure_time;           //!< exposure time in msec
      double        exposure_progress;       //!< exposure progress (fraction)
      std::string   fits_name;               //!< contatenation of Common's image_dir + image_name + image_num
      std::string   start_time;              //!< system time when the exposure started (YYYY-MM-DDTHH:MM:SS.sss)

      FitsKeys userkeys;             //!< create a FitsKeys object for FITS keys specified by the user
      FitsKeys systemkeys;           //!< create a FitsKeys object for FITS keys imposed by the software


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
        this->datacube = false;
        this->datatype = -1;
        this->type_set = false;  //!< set true when datatype has been defined
      }

      long set_axes(int datatype_in) {
        long bytes_per_pixel;

        switch (datatype_in) {
          case SHORT_IMG:
          case USHORT_IMG:
            bytes_per_pixel = 2;
            break;
          case LONG_IMG:
          case ULONG_IMG:
          case FLOAT_IMG:
            bytes_per_pixel = 4;
            break;
          default:
            return (ERROR);
        }
        this->datatype = datatype_in;
        this->type_set = true;         // datatype has been set

        this->naxis = 2;

        this->axis_pixels[0] = this->region_of_interest[1] -
                               this->region_of_interest[0] + 1;
        this->axis_pixels[1] = this->region_of_interest[3] -
                               this->region_of_interest[2] + 1;

        this->axes[0] = this->axis_pixels[0] / this->binning[0];
        this->axes[1] = this->axis_pixels[1] / this->binning[1];

        this->image_size   = this->axes[0] * this->axes[1];                    // Pixels per CCD
        this->image_memory = this->axes[0] * this->axes[1] * bytes_per_pixel;  // Bytes per CCD

        return (NO_ERROR);
      }
  };
}
#endif
