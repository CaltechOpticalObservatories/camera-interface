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

      /**************** Archon::Interface::region_of_interest *****************/
      /**
       * @brief      define a region of interest
       * @param[in]  args
       * @param[out] retstring
       * @return     
       *
       */
      long Interface::region_of_interest( std::string args, std::string &retstring ) {
        std::string function = "Archon::Interface::region_of_interest";
        std::stringstream message;
        this->common.log_error( function, "ROI not supported" );
        return( ERROR );
      }
      /**************** Archon::Interface::region_of_interest *****************/


      /**************** Archon::Interface::multi_cds **************************/
      /**
       * @brief      
       * @param[in]  args
       * @param[out] retstring
       * @return     
       *
       */
      long Interface::multi_cds( std::string args, std::string &retstring ) {
        std::string function = "Archon::Interface::multi_cds";
        std::stringstream message;
        this->common.log_error( function, "ROI not supported" );
        return( ERROR );
      }
      /**************** Archon::Interface::multi_cds **************************/


      /**************** Archon::Interface::deinterlace ************************/
      /**
       * @brief      deinterlace
       * @param[in]  
       * @param[out] 
       * @return     
       *
       */
      template <class T>
      T* Interface::deinterlace(T* imbuf) {
        std::string function = "Archon::Instrument::deinterlace";
        this->common.log_error( function, "deinterlacing not supported" );
        return( (T*)NULL );
      }
      /**************** Archon::Interface::deinterlace ************************/


}
