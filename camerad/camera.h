#pragma once

#include <CCfits/CCfits>

#include "common.h"
#include "shutter.h"

namespace Camera {

  constexpr int COL = 0;
  constexpr int ROW = 1;

  /**
   * @typedef frame_type_t
   * @brief   ENUM list of frame types
   */
  typedef enum {
    FRAME_IMAGE,
    FRAME_RAW,
    NUM_FRAME_TYPES
  } frame_type_t;

  const char * const frame_type_str[NUM_FRAME_TYPES] = {
    "IMAGE",
    "RAW"
  };


  class Camera {
    private:
      std::stringstream lasterrorstring;     //!< a place to preserve an error message

    public:
      bool          bonn_shutter;            //!< set false if Bonn shutter is not connected (defaults true)

      Shutter shutter;

      void test();
      void log_error( std::string function, std::string message );
  };


  class Information {
    public:
      long          axes[3];                 //!< element 0=cols, 1=cols, 2=cubedepth
      long          axis_pixels[2];
      int           binning[2];
      int           bitpix;                  //!< Archon bits per pixel based on SAMPLEMODE

      /**
       * @var     cubedepth
       * @brief   depth, or number of slices for 3D data cubes
       * @details cubedepth is used to calculate memory allocation and is not necessarily the
       *          same as fitscubed, which is used to force the fits writer to create a cube.
       *
       */
      long          cubedepth;

      int           datatype;                //!< FITS data type (corresponding to bitpix) used in set_axes()
      long          detector_pixels[2];      //!< element 0=cols (pixels), 1=rows (lines)
      frame_type_t  frame_type;              //!< frame_type is IMAGE or RAW
      long          image_memory;            //!< bytes per image sensor
      bool          ismex;                   //!< the info object given to the FITS writer will need to know multi-extension status
      long          region_of_interest[4];
      long          section_size;            //!< pixels to write for this section (could be less than full sensor size)
      bool          type_set;                //!< set when FITS data type has been defined

      /***** Camera::Information:set_axes *************************************/
      /**
       * @brief   
       * @return  ERROR or NO_ERROR
       *
       */
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
                           * this->detector_pixels[1] * bytes_per_pixel;       // Bytes per detector

#ifdef LOGLEVEL_DEBUG
        message << "[DEBUG] region_of_interest[1]=" << this->region_of_interest[1]
                << " region_of_interest[0]=" << this->region_of_interest[0]
                << " region_of_interest[3]=" << this->region_of_interest[3]
                << " region_of_interest[2]=" << this->region_of_interest[2]
                << " axes[0]=" << this->axes[0]
                << " axes[1]=" << this->axes[1]
                << " binning[0]=" << this->binning[0]
                << " binning[1]=" << this->binning[1];
        logwrite( function, message.str() );
#endif

        return (NO_ERROR);
      }
  };
}
