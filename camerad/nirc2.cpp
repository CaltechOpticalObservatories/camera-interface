/**
 * @file    nirc2.cpp
 * @brief   instrument-specific class definitions for NIRC2
 * @details This file defines functions specific to NIRC2
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * Any functions defined here must have (generic) function definitions in archon.h.
 * This file contains the specific code for those function definitions which are
 * unique to the particular instrument.
 *
 */

#include <string>
#include <sstream>
#include <vector>

#include "archon.h"
#include "common.h"
#include "utilities.h"
#include "logentry.h"

namespace Archon {

      /**************** Archon::Interface::region_of_interest *****************/
      /**
       * @fn         region_of_interest
       * @brief      define a region of interest for NIRC2
       * @param[in]  args string which must contain width and height
       * @param[out] retstring, reference to string which contains the width and height
       * @return     ERROR or NO_ERROR
       *
       * Specify width and height only.
       * Width must be 32 <= cols <= 1024 and multiple of 32
       * Height must be 8 <= rows <= 1024 and multiple of 8
       * The NIRC2 region of interest is always centered on the detector.
       *
       */
      long Interface::region_of_interest( std::string args, std::string &retstring ) {
        std::string function = "Archon::Instrument::region_of_interest";
        std::stringstream message;
        std::vector<std::string> tokens;
        long error = NO_ERROR;
        int trywidth, tryheight;

        // Firmware must be loaded and a mode must have been selected
        //
        if ( ! this->firmwareloaded ) {
          this->camera.log_error( function, "no firmware loaded" );
          return ERROR;
        }

        if ( ! this->modeselected ) {
          this->camera.log_error( function, "no mode selected" );
          return ERROR;
        }

        // process args only if not empty
        //
        if ( not args.empty() ) {

          Tokenize( args, tokens, " " );

          if ( tokens.size() != 2 ) {
            message.str(""); message << "incorrect number of arguments " << tokens.size() << ": expected <width> <height>";
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }

          // extract and convert the width and height tokens
          //
          try {
            trywidth  = std::stoi( tokens.at(0) );
            tryheight = std::stoi( tokens.at(1) );
          }
          catch( std::invalid_argument const &e ) {
            message << "parsing <width> <height> " << args << ": " << e.what();
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }
          catch( std::out_of_range const &e ) {
            message << "parsing <width> <height> " << args << ": " << e.what();
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }

          // check that width and height are in range and valid values
          //
          if ( trywidth < 32 || trywidth > 1024 ) {
            message.str(""); message << "width " << trywidth << " out of range {32:1024}";
            this->camera.log_error( function, message.str() );
            error = ERROR;
          }

          if ( trywidth % 32 != 0 ) {
            message.str(""); message << "width " << trywidth << " not a multiple of 32";
            this->camera.log_error( function, message.str() );
            error = ERROR;
          }

          if ( tryheight < 8 || tryheight > 1024 ) {
            message.str(""); message << "height " << tryheight << " out of range {8:1024}";
            this->camera.log_error( function, message.str() );
            error = ERROR;
          }

          if ( tryheight % 8 != 0 ) {
            message.str(""); message << "height " << tryheight << " not a multiple of 8";
            this->camera.log_error( function, message.str() );
            error = ERROR;
          }

          // compute the parameters required for the ACF to realize this width and height
          //
          int NRQ = tryheight / 8;  /// nRowsQuad
          int SRQ = 128 - NRQ;      /// SkippedRowsQuad
          int LC  = NRQ * 4;        /// LINECOUNT

          int NPP = trywidth / 32;  /// nPixelsPair
          int SCQ = 32 - NPP;       /// SkippedColumnsQuad
          int PC  = NPP * 2;        /// PIXELCOUNT

          // write the parameters to Archon
          //
          std::stringstream cmd;

          cmd.str(""); cmd << "nRowsQuad " << NRQ;
          if (error==NO_ERROR) error = this->set_parameter( cmd.str() );

          cmd.str(""); cmd << "SkippedRowsQuad " << SRQ;
          if (error==NO_ERROR) error = this->set_parameter( cmd.str() );

          cmd.str(""); cmd << "nPixelsPair " << NPP;
          if (error==NO_ERROR) error = this->set_parameter( cmd.str() );

          cmd.str(""); cmd << "SkippedColumnsQuad " << SCQ;
          if (error==NO_ERROR) error = this->set_parameter( cmd.str() );

          std::string dontcare;

          cmd.str(""); cmd << "LINECOUNT " << LC;
          if (error==NO_ERROR) error = this->cds( cmd.str(), dontcare );

          cmd.str(""); cmd << "PIXELCOUNT " << PC;
          if (error==NO_ERROR) error = this->cds( cmd.str(), dontcare );

          // get out now if any errors
          //
          if ( error != NO_ERROR ) {
            this->camera.log_error( function, "writing ROI" );
            return error;
          }

          // update the modemap, in case someone asks again
          //
          std::string mode = this->camera_info.current_observing_mode;

          this->modemap[mode].geometry.linecount  = LC;
          this->modemap[mode].geometry.pixelcount = PC;

          if (error==NO_ERROR) error = get_configmap_value( "PIXELCOUNT", this->camera_info.detector_pixels[0] );
          if (error==NO_ERROR) error = get_configmap_value( "LINECOUNT", this->camera_info.detector_pixels[1] );

          this->camera_info.detector_pixels[0] *= this->modemap[mode].geometry.amps[0];
          this->camera_info.detector_pixels[1] *= this->modemap[mode].geometry.amps[1];
          this->camera_info.frame_type = Camera::FRAME_IMAGE;

          // ROI is the full detector
          this->camera_info.region_of_interest[0] = 1;
          this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
          this->camera_info.region_of_interest[2] = 1;
          this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];
          // Binning factor (no binning)
          this->camera_info.binning[0] = 1;
          this->camera_info.binning[1] = 1;

          error = this->camera_info.set_axes();                                                 // 16 bit raw is unsigned short int

          // allocate image_data in blocks because the controller outputs data in units of blocks
          //
          int num_detect = this->modemap[mode].geometry.num_detect;
          this->image_data_bytes = (uint32_t) floor( ((this->camera_info.image_memory * num_detect) + BLOCK_LEN - 1 ) / BLOCK_LEN ) * BLOCK_LEN;

          if (this->image_data_bytes == 0) {
            this->camera.log_error( function, "image data size is zero! check NUM_DETECT, HORI_AMPS, VERT_AMPS in .acf file" );
            error = ERROR;
          }

          this->camera_info.current_observing_mode = mode;       // identify the newly selected mode in the camera_info class object
          this->modeselected = true;                             // a valid mode has been selected

          message.str(""); message << "new mode: " << mode << " will use " << this->camera_info.bitpix << " bits per pixel";
          logwrite(function, message.str());

          // Calculate amplifier sections
          //
          int rows = this->modemap[mode].geometry.linecount;     // rows per section
          int cols = this->modemap[mode].geometry.pixelcount;    // cols per section

          int hamps = this->modemap[mode].geometry.amps[0];      // horizontal amplifiers
          int vamps = this->modemap[mode].geometry.amps[1];      // vertical amplifiers

          int x0=-1, x1, y0, y1;                                 // for indexing
          std::vector<long> coords;                              // vector of coordinates, convention is x0,x1,y0,y1
          int framemode = this->modemap[mode].geometry.framemode;

          this->camera_info.amp_section.clear();                 // vector of coords vectors, one set of coords per extension

          for ( int y=0; y<vamps; y++ ) {
            for ( int x=0; x<hamps; x++ ) {
              if ( framemode == 2 ) {
                x0 = x; x1=x+1;
                y0 = y; y1=y+1;
              }
              else {
                x0++;   x1=x0+1;
                y0 = 0; y1=1;
              }
              coords.clear();
              coords.push_back( (x0*cols + 1) );                 // x0 is xstart
              coords.push_back( (x1)*cols );                     // x1 is xstop, xrange = x0:x1
              coords.push_back( (y0*rows + 1) );                 // y0 is ystart
              coords.push_back( (y1)*rows );                     // y1 is ystop, yrange = y0:y1

              this->camera_info.amp_section.push_back( coords ); // (x0,x1,y0,y1) for this extension

            }
          }
          message.str(""); message << "identified " << this->camera_info.amp_section.size() << " amplifier sections";
          logwrite( function, message.str() );
        } // end if args not empty

        // regardless of args empty or not, check and return the current width and height
        //
        int check_npp=0;
        int check_nrq=0;

        if (error==NO_ERROR) error = get_configmap_value( "NPP", check_npp );
        if (error==NO_ERROR) error = get_configmap_value( "NRQ", check_nrq );

        int width  = 32 * check_npp;
        int height =  8 * check_nrq;

        message.str(""); message << width << " " << height;
        retstring = message.str();

        return( error );
      }
      /**************** Archon::Interface::region_of_interest *****************/

}
