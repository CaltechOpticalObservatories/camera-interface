/**
 * @file    generic.cpp
 * @brief   instrument-specific class definitions for generic instruments
 * @details This file defines functions for any generic instrument
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * Any functions defined here must have (generic) function definitions in archon.h.
 * This file contains the specific code for those function definitions which are
 * unique to the particular instrument.
 *
 * More than likely, if a function is here then it is not supported, otherwise it
 * would be defined for a particular instrument. This file is here so that the main
 * daemon will compile.
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

      /***** Archon::Interface::make_camera_header ****************************/
      /**
       * @brief      adds header keywords to the systemkeys database
       * @param[in]  
       * @param[out] 
       * @return     
       *
       */
      void Interface::make_camera_header( ) {
        std::stringstream keystr;

        keystr.str("");
        keystr << "EXPTIME=" << this->camera_info.exposure_time << " // exposure time in " << ( this->is_longexposure ? "sec" : "msec" );
        this->systemkeys.addkey( keystr.str() );

        keystr.str("");
        keystr << "NSEQ=" << this->camera_info.nseq << " // number of exposure sequences";
        this->systemkeys.addkey( keystr.str() );

        return;
      }
      /***** Archon::Interface::make_camera_header ****************************/


      /***** Archon::Interface::calc_readouttime ******************************/
      /**
       * @brief      
       * @return     ERROR
       *
       * not currently supported for generic instruments.
       *
       */
      long Interface::calc_readouttime( ) {
        std::string function = "Archon::Interface::calc_readouttime";
        this->camera.log_error( function, "not supported" );
        return( ERROR );
      }
      /***** Archon::Interface::calc_readouttime ******************************/


      /***** Archon::Interface::region_of_interest ****************************/
      /**
       * @brief      define a region of interest
       * @param[in]  args
       * @param[out] retstring
       * @return     ERROR
       *
       * ROI is not currently supported for generic instruments.
       *
       * Includes an overloaded version which has no return value
       *
       */
      long Interface::region_of_interest( std::string args ) {
        std::string dontcare;
        return this->region_of_interest( args, dontcare );
      }
      long Interface::region_of_interest( std::string args, std::string &retstring ) {
        std::string function = "Archon::Interface::region_of_interest";
        std::stringstream message;
        this->camera.log_error( function, "ROI not supported" );
        return( ERROR );
      }
      /***** Archon::Interface::region_of_interest ****************************/


      /**************** Archon::Interface::multi_cds **************************/
      /**
       * @brief      
       * @param[in]  args
       * @param[out] retstring
       * @return     
       *
       * MCDS is not supported for generic instruments.
       *
       */
      long Interface::multi_cds( std::string args, std::string &retstring ) {
        std::string function = "Archon::Interface::multi_cds";
        std::stringstream message;
        this->camera.log_error( function, "MCDS not supported" );
        return( ERROR );
      }
      /**************** Archon::Interface::multi_cds **************************/


      /***** Archon::Interface::sample_mode ***********************************/
      /**
       * @brief      
       * @param[in]  args
       * @param[out] retstring
       * @return     
       *
       * sample_mode is not supported for generic instruments.
       *
       * Includes an overloaded version which has no return value
       *
       */
      long Interface::sample_mode( std::string args ) {
        std::string dontcare;
        return this->sample_mode( args, dontcare );
      }
      long Interface::sample_mode( std::string args, std::string &retstring ) {
        std::string function = "Archon::Interface::sample_mode";
        std::stringstream message;
        this->camera.log_error( function, "sample_mode command not supported" );
        return( ERROR );
      }
      /***** Archon::Interface::sample_mode ***********************************/


      /***** Archon::Interface::deinterlace ***********************************/
      /**
       * @brief      deinterlace
       * @param[in]  
       * @param[out] 
       * @return     
       *
       * Deinterlacing is not supported for generic instruments.
       *
       */
      template <class T>
      T* Interface::deinterlace(T* imbuf) {
        std::string function = "Archon::Instrument::deinterlace";
#ifdef LOGLEVEL_DEBUG
        logwrite( function, "deinterlacing not supported" );
#endif
        return( (T*)NULL );
      }
      /***** Archon::Interface::deinterlace ***********************************/


      /***** Archon::Interface::longexposure **********************************/
      /**
       * @brief      set/get longexposure mode
       * @param[in]  state_in   string to set long exposure state, can be {0,1,true,false}
       * @param[out] state_out  reference to string containing the long exposure state
       * @return     ERROR or NO_ERROR
       *
       */
      long Interface::longexposure(std::string state_in, std::string &state_out) {
        std::string function = "Archon::Interface::longexposure";
        std::stringstream message;
        int error = NO_ERROR;

        // If something is passed then try to use it to set the longexposure state
        //
        if ( !state_in.empty() ) {
          try {
            std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::toupper );  // make uppercase
            if ( state_in == "FALSE" || state_in == "0" ) this->is_longexposure = false;
            else
            if ( state_in == "TRUE" || state_in == "1" ) this->is_longexposure = true;
            else {
              message.str(""); message << "longexposure state " << state_in << " is invalid. Expecting {true,false,0,1}";
              this->camera.log_error( function, message.str() );
              return( ERROR );
            }
          }
          catch (...) {
            message.str(""); message << "unknown exception converting longexposure state " << state_in << " to uppercase";
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }
        }

        // error or not, the state reported now will be whatever was last successfully set
        //
        this->camera_info.exposure_unit   = ( this->is_longexposure ? "sec" : "msec" );
        this->camera_info.exposure_factor = ( this->is_longexposure ? 1 : 1000 );
        state_out = ( this->is_longexposure ? "true" : "false" );

        // if no error then set the parameter on the Archon
        //
        if ( error==NO_ERROR ) {
          std::stringstream cmd;
          cmd << "longexposure " << ( this->is_longexposure ? 1 : 0 );
          if ( error==NO_ERROR ) error = this->set_parameter( cmd.str() );
        }

        return( error );
      }
      /***** Archon::Interface::longexposure **********************************/

}
