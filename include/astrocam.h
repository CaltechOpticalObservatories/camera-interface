/**
 * @file    astrocam.h
 * @brief   
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#ifndef ASTROCAM_H
#define ASTROCAM_H

#include <CCfits/CCfits>           //!< needed here for types in set_axes()
#include <atomic>
#include <chrono>
#include <numeric>

#include "utilities.h"
#include "common.h"
#include "config.h"
#include "logentry.h"

namespace AstroCam {

  /**
   * This is where you put things that are common to both ARC interfaces,
   * ARC-64 PCI and ARC-66 PCIe.
   *
   */
  class CommonInterface {
    private:
      int rows;
      int cols;
      int bufsize;
      int framecount;
      bool isabort_exposure;
      int FITS_STRING_KEY;
      int FITS_DOUBLE_KEY;
      int FITS_INTEGER_KEY;
      int FITS_BPP16;
      int FITS_BPP32;

    public:
      CommonInterface();
      ~CommonInterface();

      Config config;

      Common::Common common;

      long configure_controller();

      int keytype_string()  { return this->FITS_STRING_KEY;  };
      int keytype_double()  { return this->FITS_DOUBLE_KEY;  };
      int keytype_integer() { return this->FITS_INTEGER_KEY; };
      int fits_bpp16()      { return this->FITS_BPP16;       };
      int fits_bpp32()      { return this->FITS_BPP32;       };
      int get_rows() { return this->rows; };
      int get_cols() { return this->cols; };
      int get_bufsize() { return this->bufsize; };
      int get_driversize();
      bool get_abortexposure() { return this->isabort_exposure; };
      void abort_exposure() { this->isabort_exposure = true; };

      int set_rows(int r) { if (r>0) this->rows = r; return r; };
      int set_cols(int c) { if (c>0) this->cols = c; return c; };

      int get_image_rows() { return this->rows; };
      int get_image_cols() { return this->cols; };

      void set_framecount( int num ) { this->framecount = num; };
      void increment_framecount() { this->framecount++; };
      int get_framecount() { return this->framecount; };

      void set_imagesize(int rowsin, int colsin, int* status);

  };

  class Information {
    private:
    public:
      int           datatype;                //!< FITS data type (corresponding to bitpix)
      std::string   configfilename;          //!< Archon controller configuration file
      long          detector_pixels[2];
      long          image_size;              //!< pixels per image sensor
      long          image_memory;            //!< bytes per image sensor
      int           current_observing_mode;
      long          naxis;
      long          axes[2];
      int           binning[2];
      long          axis_pixels[2];
      long          region_of_interest[4];
      long          image_center[2];
      bool          data_cube;
      std::string   fits_name;               //!< contatenation of Common's image_dir + image_name + image_num
      std::string   start_time;              //!< system time when the exposure started (YYYY-MM-DDTHH:MM:SS.sss)

      Common::FitsKeys userkeys;             //!< create a FitsKeys object for FITS keys specified by the user

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
        this->data_cube = false;
      }

      long set_axes(int datatype_in) {
        this->datatype = datatype_in;
        long bytes_per_pixel;

        switch (this->datatype) {
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
