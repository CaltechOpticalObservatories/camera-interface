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
       * @brief      set/get a region of interest
       * @details    not currently supported for generic instruments
       * @param[in]  args       n/a
       * @param[out] retstring  n/a
       * @return     ERROR
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


      /***** Archon::Interface::sample_mode ***********************************/
      /**
       * @brief      set/get a sample mode
       * @details    not currently supported for generic instruments
       * @param[in]  args       n/a
       * @param[out] retstring  n/a
       * @return     
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


      /***** Archon::Interface::power *****************************************/
      /**
       * @brief      wrapper for Archon::Interface::do_power()
       * @param[in]  state_in  requested power state
       * @param[out] restring  return string holds power state
       * @return     ERROR or NO_ERROR
       *
       */
      long Interface::power( std::string state_in, std::string &retstring ) {
        std::string function = "Archon::Instrument::power";
        std::stringstream message;

        // use Archon::Interface::do_power() to set/get the power
        //
        return( this->do_power( state_in, retstring ) );
      }
      /***** Archon::Interface::power *****************************************/


      /***** Archon::Interface::expose ****************************************/
      /**
       * @brief      wrapper for Archon::Interface::do_expose()
       * @param[in]  nseq_in  string which can (optionally) contain number of sequences
       * @return     ERROR or NO_ERROR
       *
       */
      long Interface::expose( std::string nseq_in ) {
        std::string function = "Archon::Instrument::expose";
        std::stringstream message;

        this->camera.clear_abort();                                   // clear the abort state
        this->camera_info.exposure_aborted = false;

        int nseq = 1;  // default number of sequences if not otherwise specified by nseq_in
        long ret;

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

        message.str(""); message << "beginning " << nseq << " sequence" << ( nseq > 1 ? "s" : "" );
        logwrite( function, message.str() );

        // Loop over nseq.
        // This is like sending the "expose" command nseq times,
        // so each of these generates a separate FITS file.
        //
        int totseq = nseq;
        while( not this->camera.is_aborted() && ( nseq-- > 0 ) ) {
          ret = this->do_expose( std::to_string( this->camera_info.nexp ) );
          message.str(""); message << "NSEQ:" << (totseq-nseq);
          this->camera.async.enqueue( message.str() );
          if ( ret != NO_ERROR ) return( ret );
          message.str(""); message << nseq << " sequence" << ( nseq > 1 ? "s" : "" ) << " remaining";
          logwrite( function, message.str() );
        }

        return( ret );
      }
      /***** Archon::Interface::expose ****************************************/

}
