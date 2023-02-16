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

      const int SAMPMODE_UTR     = 1;  const std::string SAMPSTR_UTR     = "UTR";
      const int SAMPMODE_CDS     = 2;  const std::string SAMPSTR_CDS     = "CDS";
      const int SAMPMODE_MCDS    = 3;  const std::string SAMPSTR_MCDS    = "MCDS";
      const int SAMPMODE_NONCDSV = 4;  const std::string SAMPSTR_NONCDSV = "NONCDSV";
      const int SAMPMODE_CDSV    = 5;  const std::string SAMPSTR_CDSV    = "CDSV";

      const std::vector<std::string> SAMPMODE_NAME { SAMPSTR_UTR,
                                                     SAMPSTR_CDS,
                                                     SAMPSTR_MCDS,
                                                     SAMPSTR_NONCDSV,
                                                     SAMPSTR_CDSV,
                                                   };

      /***** PythonProc::PythonProc *******************************************/
      /**
       * @brief      
       *
       */
      PythonProc::PythonProc() {
        this->pName   = PyUnicode_FromString( "calcmcds" );
        this->pModule = PyImport_Import( this->pName );
        logwrite( "PythonProc::PythonProc", "constructed" );
      }
      /***** PythonProc::PythonProc *******************************************/


      /***** PythonProc::~PythonProc ******************************************/
      /**
       * @brief      
       *
       */
      PythonProc::~PythonProc() {
        logwrite( "PythonProc::~PythonProc", "de-constructed" );
      }
      /***** PythonProc::~PythonProc ******************************************/


      PythonProc pp;


      /**************** Archon::Interface::poweron ****************************/
      /**
       * @brief      turn off the power
       * @return     ERROR or NO_ERROR
       *
       * After applying the power, must wait 2 sec before setting Start=1
       *
       */
      long Interface::poweron( ) {
        std::string function = "Archon::Instrument::poweron";
        std::stringstream message;
	long error = this->native( "POWERON" );
        usleep( 2000000 );
        if ( error == NO_ERROR ) error = this->set_parameter( "Start", 1 );
        if ( error == NO_ERROR ) logwrite( function, "Archon powered on ok" );
        else                     logwrite( function, "ERROR powering on Archon" );
        return( error );
      }
      /**************** Archon::Interface::poweron ****************************/


      /**************** Archon::Interface::poweroff ***************************/
      /**
       * @brief      turn off the power
       * @return     ERROR or NO_ERROR
       *
       */
      long Interface::poweroff( ) {
        std::string function = "Archon::Instrument::poweroff";
        std::stringstream message;
	return this->native("POWEROFF");
      }
      /**************** Archon::Interface::poweroff ***************************/


      /**************** Archon::Interface::expose *****************************/
      /**
       * @brief      calls do_expose
       * @param[in]  nseq_in  string which must contain width and height
       * @return     ERROR or NO_ERROR
       *
       */
      long Interface::expose( std::string nseq_in ) {
        std::string function = "Archon::Instrument::expose";
        std::stringstream message;

        int nseq = 1;  // default number of sequences if not otherwise specified by nseq_in
        long ret;

        // must have specified sampmode first (set to -1 in constructor)
        //
        if ( this->camera_info.sampmode == -1 ) {
          this->camera.log_error( function, "sampmode has not been set" );
          return ERROR;
        }

        // must have specified sampmode first (set to -1 in constructor)
        //
        if ( this->camera_info.nexp == -1 ) {
          this->camera.log_error( function, "nexp undefined (error in sampmode?)" );
          return ERROR;
        }

        // If nseq_in is supplied then try to use that to set nseq, number of exposures (files)
        //
        if ( not nseq_in.empty() ) {
          try {
            nseq = std::stoi( nseq_in );    // test that nseq_in is an integer
          }
          catch (std::invalid_argument &) {
            message.str(""); message << "unable to convert requested number of sequences: " << nseq_in << " to integer";
            this->camera.log_error( function, message.str() );
            return(ERROR);
          }
          catch (std::out_of_range &) {
            message.str(""); message << "requested number of sequences  " << nseq_in << " outside integer range";
            this->camera.log_error( function, message.str() );
            return(ERROR);
          }
        }

        // Add NIRC2 system headers
        //
        this->make_camera_header();

        message.str(""); message << "beginning " << nseq << " sequence" << ( nseq > 1 ? "s" : "" );
        logwrite( function, message.str() );

        // Loop over nseq.
        // This is like sending the "expose" command nseq times,
        // so each of these generates a separate FITS file.
        //
        while( nseq-- > 0 ) {
          ret = this->do_expose( std::to_string( this->camera_info.nexp ) );
          if ( ret != NO_ERROR ) return( ret );
          message.str(""); message << nseq << " sequence" << ( nseq > 1 ? "s" : "" ) << " remaining";
          logwrite( function, message.str() );
        }

        return( ret );
      }
      /**************** Archon::Interface::expose *****************************/


      /***** Archon::Interface::make_camera_header ****************************/
      /**
       * @brief      adds header keywords to the systemkeys database
       *
       */
      void Interface::make_camera_header( ) {
        std::stringstream keystr;

        keystr.str("");
        keystr << "ITIME=" << this->camera_info.exposure_time/1000.0 << " // integration time per coadd in sec";
        this->systemkeys.addkey( keystr.str() );

        keystr.str("");
        keystr << "SAMPMODE=" << this->camera_info.sampmode << " //";
        for ( int i=0; i < (int)SAMPMODE_NAME.size(); i++ ) keystr << " " << i+1 << ":" << SAMPMODE_NAME[i];
        this->systemkeys.addkey( keystr.str() );

        switch ( this->camera_info.sampmode ) {

          case SAMPMODE_MCDS:
            keystr.str("");
            keystr << "MULTISAM=" << this->camera_info.sampmode_frames/2; // number of MCDS read pairs
            this->systemkeys.addkey( keystr.str() );
            break;

          case SAMPMODE_CDS:
          case SAMPMODE_UTR:
            keystr.str("");
            keystr << "MULTISAM=" << this->camera_info.sampmode_frames;   // number of UTR samples
            this->systemkeys.addkey( keystr.str() );
            break;

          default:
            this->systemkeys.delkey( "MULTISAM" );                        // remove the MULTISAM keyword
            break;
        }

        keystr.str("");
        keystr << "COADDS=" << this->camera_info.sampmode_ext << " // number of coadds";
        this->systemkeys.addkey( keystr.str() );

        return;
      }
      /***** Archon::Interface::make_camera_header ****************************/


      /***** Archon::Interface::recalc_geometry *******************************/
      /**
       * @brief      recalculate geometry
       * @returns    ERROR or NO_ERROR
       *
       */
      long Interface::recalc_geometry( ) {
        std::string function = "Archon::Instrument::recalc_geometry";
        std::stringstream message;
        long error = NO_ERROR;
        std::string mode = this->camera_info.current_observing_mode;

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

        return( error );
      }
      /***** Archon::Interface::recalc_geometry *******************************/


      /**************** Archon::Interface::region_of_interest *****************/
      /**
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
          int LC  = NRQ * 4;        /// LINECOUNT **overwritten below! depends on sampmode

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

          // LINECOUNT must be doubled for CDS Video mode
          //
          if ( this->camera_info.sampmode == SAMPMODE_CDSV ) {
            LC = NRQ * 8;
          }
          else {
            LC = NRQ * 4;
          }
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

          if (error==NO_ERROR) error = this->recalc_geometry();

/*****
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
*****/
        } // end if args not empty

        // regardless of args empty or not, check and return the current width and height
        //
        long check_npp=0;
        long check_nrq=0;

        if (error==NO_ERROR) error = get_parammap_value( "nPixelsPair", check_npp );
        if (error==NO_ERROR) error = get_parammap_value( "nRowsQuad", check_nrq );

//      this->camera_info.axes[0]  = 32 * check_npp;
//      this->camera_info.axes[1] =  8 * check_nrq;
//      message.str(""); message << this->camera_info.axes[0] << " " << this->camera_info.axes[1];

        this->camera_info.imwidth  = 32 * check_npp;
        this->camera_info.imheight =  8 * check_nrq;
        message.str(""); message << this->camera_info.imwidth << " " << this->camera_info.imheight;

        retstring = message.str();

        return( error );
      }
      /**************** Archon::Interface::region_of_interest *****************/


      /**************** Archon::Interface::sample_mode ************************/
      /**
       * @brief      set the sample mode
       * @param[in]  args
       * @param[out] retstring
       * @return     ERROR or NO_ERROR
       *
       * Input args string contains sample mode and a count for that mode.
       * valid strings for each mode are as follows:
       *  UTR:     1 <samples> <ramps>
       *  CDS:     2
       *  MCDS:    3 <pairs> <ext>
       *  nonCDSV: 4 <frames>
       *  CDSV:    5 <frames>
       *
       * Modes 1 (UTR) and 3 (MCDS) require a count value
       *
       */
      long Interface::sample_mode( std::string args, std::string &retstring ) {
        std::string function = "Archon::Interface::sample_mode";
        std::stringstream message;
        std::vector<std::string> tokens;
        long error = NO_ERROR;

        // Firmware must be loaded before selecting a mode because this will write to the controller
        //
        if ( ! this->firmwareloaded ) {
          this->camera.log_error( function, "no firmware loaded" );
          return ERROR;
        }

        int tryframes=-1;
        int tryext=-1;
        int trynexp=-1;
        int mode_in=-1;
        int argin_i=-1;
        int argin_j=-1;

        // process args only if not empty
        //
        if ( not args.empty() ) {

          Tokenize( args, tokens, " " );

          // extract and convert the mode and value tokens from the input arg string
          //
          try {
            if ( tokens.size() > 0 ) mode_in = std::stoi( tokens.at(0) );
            if ( tokens.size() > 1 ) argin_i = std::stoi( tokens.at(1) );
            if ( tokens.size() > 2 ) argin_j = std::stoi( tokens.at(2) );
            if ( tokens.size() > 3 ) {
              message << "received " << tokens.size() << " arguments";
              this->camera.log_error( function, message.str() );
              return( ERROR );
            }
          }
          catch( std::invalid_argument const &e ) {
            message << "invalid argument parsing args " << args << ": " << e.what();
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }
          catch( std::out_of_range const &e ) {
            message << "out of range parsing args " << args << ": " << e.what();
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }

          // Check the values of the additional args for the requested mode.
          // Each mode will have its own requirements on the accompanying values.
          //
          switch( mode_in ) {

            // For UTR, the first argument i is the number of frames (cubedepth).
            //
            case SAMPMODE_UTR:
              tryframes = argin_i;
              tryext    = argin_j;
              trynexp   = argin_j;
              if ( tryframes < 0 ) {
                this->camera.log_error( function, "no UTR samples were specified" );
                return( ERROR );
              }
              if ( tryframes < 1 ) {
                message.str(""); message << "requested UTR samples " << tryframes << " must be > 0";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              if ( tryext < 1 ) {
                message.str(""); message << "requested UTR ramps " << tryext << " must be > 0";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              // write the parameters to Archon to set UTR and clear the other modes
              //
              if (error==NO_ERROR) error = this->set_parameter( this->mcdspairs_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->mcdsmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrmode_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrsamples_param, tryframes );
              this->camera_info.cubedepth = tryframes;
              this->camera_info.fitscubed = tryframes;
              this->camera_info.iscds = false;
              break;

            case SAMPMODE_CDS:
              tryframes = 2;
              tryext    = argin_i;
              trynexp   = argin_i;
              if ( tryext < 1 ) {
                message.str(""); message << "must specify a non-zero number of extensions";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              if ( argin_j != -1 ) {
                this->camera.log_error( function, "too many arguments for this mode" );
                return( ERROR );
              }
              // write the parameters to Archon to set CDS (a special form of MCDS) and clear the other modes
              //
              if (error==NO_ERROR) error = this->set_parameter( this->mcdspairs_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->mcdsmode_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrsamples_param, 0 );
              this->camera_info.cubedepth = 2;
              this->camera_info.fitscubed = 2;
              this->camera_info.iscds = true;
              break;

            // For MCDS, tryframes will be the total number of frames per extension (=cubedepth)
            // but only tryframes/2 MCDS pairs, so tryframes must be a multiple of 2.
            //
            case SAMPMODE_MCDS:
              tryframes = argin_i;
              tryext    = argin_j;
              trynexp   = argin_j;
              if ( tryframes < 1 ) {
                message.str(""); message << "must specify a non-zero number of frames";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              if ( tryext == -1 ) {
                this->camera.log_error( function, "must specify number of extensions" );
                return( ERROR );
              }
              if ( tryext < 1 ) {
                this->camera.log_error( function, "must specify non-zero number of extensions" );
                return( ERROR );
              }
              if ( tryframes % 2  != 0 ) {
                message.str(""); message << "requested MCDS frames " << tryframes << " must be a multiple of 2";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              // write the parameters to Archon to set MCDS and clear the other modes
              //
              if (error==NO_ERROR) error = this->set_parameter( this->mcdspairs_param, tryframes/2 );  // number of pairs is tryframes/2
              if (error==NO_ERROR) error = this->set_parameter( this->mcdsmode_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrsamples_param, 0 );
              this->camera_info.cubedepth = tryframes;
              this->camera_info.fitscubed = tryframes;
              this->camera_info.iscds = false;
              break;

            case SAMPMODE_NONCDSV:
              //
              // For non-CDS video (Rx mode) the single argument specifies
              // the number of extensions. There will always be 1 frame per extension
              // with N extensions, so this N will be passed to do_expose( trynexp ).
              //
              tryframes = 1;
              tryext    = argin_i;
              trynexp   = argin_i;
              if ( tryext < 1 ) {
                message.str(""); message << "must specify a non-zero number of frames";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              // write the parameters to Archon to set RX and clear the other modes
              //
              if (error==NO_ERROR) error = this->set_parameter( this->mcdspairs_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->mcdsmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxmode_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrsamples_param, 0 );
              this->camera_info.cubedepth = 1;
              this->camera_info.fitscubed = 1;
              this->camera_info.iscds = false;
              break;

            case SAMPMODE_CDSV:
              //
              // For CDS video (RxR mode) the single argument specifies
              // the number of extensions. There will always be 1 frame per extension
              // with N extensions, so this N will be passed to do_expose( trynexp ).
              //
              // This differs from Rx mode in that each frame is 2x the size.
              //
              tryframes = 1;
              tryext    = argin_i;
              trynexp   = argin_i;
              if ( tryext < 1 ) {
                message.str(""); message << "must specify a non-zero number of frames";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              // write the parameters to Archon to set RX and clear the other modes
              //
              if (error==NO_ERROR) error = this->set_parameter( this->mcdspairs_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->mcdsmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxrmode_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrsamples_param, 0 );
              this->camera_info.cubedepth = 1;
              this->camera_info.fitscubed = 2;
              this->camera_info.iscds = false;
              break;

            default:
              message.str(""); message << "unrecognized sample mode: " << mode_in;
              this->camera.log_error( function, message.str() );
              return( ERROR );
          }

          // Enable multi-extensions always, for consistency
          //
          this->camera.mex( true );

          // One last error check.
          // Do not allow camera_info to set a value less than 1 for either frames or extensions.
          //
          if ( tryframes < 1 || tryext < 1 ) error = ERROR;

          if ( error == NO_ERROR ) {
            this->camera_info.sampmode        = mode_in;
            this->camera_info.sampmode_ext    = tryext;
            this->camera_info.sampmode_frames = tryframes;
            this->camera_info.nexp            = trynexp;
            this->camera_info.set_axes();
          }
          else {
            message.str(""); message << "frames, extensions can't be <1: tryframes=" << tryframes << " tryext=" << tryext;
            this->camera.log_error( function, message.str() );
          }

          // Now LINECOUNT must be set because CDSV is x2 the size.
          // It will always be a multiple of nRowsQuad.
          // Also set the readout mode here, either NIRC2 or NIRC2VIDEO,
          // which is required for descrambling.
          //
          long nRowsQuad=0;
          long LINECOUNT=0;
          std::string dontcare;
          std::stringstream cmd;

          if (error==NO_ERROR) error = get_parammap_value( "nRowsQuad", nRowsQuad );

          switch( mode_in ) {
            case SAMPMODE_CDSV:
              this->readout( "NIRC2VIDEO", dontcare );
              LINECOUNT = nRowsQuad * 8;
              break;
            case SAMPMODE_UTR:
            case SAMPMODE_CDS:
            case SAMPMODE_MCDS:
            case SAMPMODE_NONCDSV:
              this->readout( "NIRC2", dontcare );
              LINECOUNT = nRowsQuad * 4;
              break;
            default:
              message.str(""); message << "unrecognized sample mode: " << mode_in;
              this->camera.log_error( function, message.str() );
              error = ERROR;
              break;
          }

          cmd << "LINECOUNT " << LINECOUNT;
          if (error==NO_ERROR) error = this->cds( cmd.str(), dontcare );

          // update the modemap, in case someone asks again
          //
          this->modemap[ this->camera_info.current_observing_mode ].geometry.linecount  = LINECOUNT;

          if (error==NO_ERROR) error = get_configmap_value( "PIXELCOUNT", this->camera_info.detector_pixels[0] );
          if (error==NO_ERROR) error = get_configmap_value( "LINECOUNT", this->camera_info.detector_pixels[1] );

          if (error==NO_ERROR) error = this->recalc_geometry();
        }

#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "[DEBUG] sampmode=" << this->camera_info.sampmode
                                 << " sampmode_ext=" << this->camera_info.sampmode_ext
                                 << " sampmode_frames=" << this->camera_info.sampmode_frames
                                 << " nexp=" << this->camera_info.nexp
                                 << " mex=" << ( this->camera.mex() ? true : false );
        logwrite( function, message.str() );
#endif

        message.str(""); message << this->camera_info.sampmode
                                 << " " << this->camera_info.sampmode_frames
                                 << " " << this->camera_info.sampmode_ext;
        retstring = message.str();

        message.str(""); message << "sample mode = " << retstring;
        logwrite( function, message.str() );

        return( error );
      }
      /**************** Archon::Interface::sample_mode ************************/

}
