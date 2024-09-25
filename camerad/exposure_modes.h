/**
 * @file     exposure_modes.h
 * @brief    describes the supported exposure modes
 * @details  This contains derived classes for each exposure mode,
 *           selected by Archon::Interface::select_expose_mode().
 *           These inherit from the ExposureBase class and override
 *           the expose_for_mode() function.
 *
 */
#pragma once

#include "archon.h"

namespace Archon {


  /***** Archon::Expose_CCD ***************************************************/
  class Expose_CCD : public ExposureBase {
    public:
      Expose_CCD(Interface* interface) : ExposureBase(interface) { }

      long expose_for_mode() override {
        const std::string function="Archon::Expose_CCD::expose_for_mode";
        logwrite( "Archon::Expose_CCD::expose_for_mode", "CCD" );
        return NO_ERROR;
      }
  };
  /***** Archon::Expose_CCD ***************************************************/


  /***** Archon::Expose_UTR ***************************************************/
  class Expose_UTR : public ExposureBase {
    public:
      Expose_UTR(Interface* interface) : ExposureBase(interface) { }

      long expose_for_mode() override {
        const std::string function="Archon::Expose_UTR::expose_for_mode";
        logwrite( "Archon::Expose_UTR::expose_for_mode", "UTR" );
        return NO_ERROR;
      }
  };
  /***** Archon::Expose_UTR ***************************************************/


  /***** Archon::Expose_Fowler ************************************************/
  class Expose_Fowler : public ExposureBase {
    public:
      Expose_Fowler(Interface* interface) : ExposureBase(interface) { }

      long expose_for_mode() override {
        const std::string function="Archon::Expose_Fowler::expose_for_mode";
        logwrite( "Archon::Expose_Fowler::expose_for_mode", "Fowler" );
        return NO_ERROR;
      }
  };
  /***** Archon::Expose_Fowler ************************************************/


  /***** Archon::Expose_RXRV **************************************************/
  /**
   * @class      Archon::Expose_RXRV
   * @brief      derived exposure mode class for RXR video
   * @details    This is constructed with a pointer to the Archon::Interface
   *             class so that member functions have access to Interface,
   *             using the "this->interface->" pointer dereference.
   *
   */
  class Expose_RXRV : public ExposureBase {
    public:
      Expose_RXRV(Interface* interface) : ExposureBase(interface) { }

      /***** Archon::Expose_RXRV::expose_for_mode *****************************/
      /**
       * @brief      this is the exposure sequence for RXR Video
       *
       */
      long expose_for_mode() override {
        const std::string function="Archon::Expose_RXRV::expose_for_mode";
        std::stringstream message;

        logwrite( function, "RXRV" );
        logwrite( "Archon::Expose_RXRV::isconnected", std::to_string(this->interface->archon.isconnected()) );

        // **********************************
        // ******** first frame here ********
        // **********************************

        // Read the first frame buffer from Archon to host and decrement my local
        // frame counter.  This call to read_frame() reads into this->archon_buf.
        //

        // Always deinterlace first frame.
        // Write here only if is_unp set to write unprocessed images.
        //

          //
          // -- MAIN SEQUENCE LOOP --
          //

            // increment ring index
            //

            // Wait for detector readout into Archon internal frame buffer
            //

            // **********************************
            // ******** next frames here ********
            // **********************************

            // Read the next (and subsequent) frame buffers from Archon to host and
            // decrement my local frame counter.  This reads into this->archon_buf.
            //

            // De-interlace each subsequent frame
            //

        // for multi-exposure (non-cubeamp) cubes, close the FITS file now that they've all been written
        //

        // Complete the FITS file after processing all frames.
        // This closes the file(s) for any defined FITS_file object(s), and
        // shuts down the FITS engine, waiting for the queue to empty if needed.
        //

        return NO_ERROR;
      }
      /***** Archon::Expose_RXRV::expose_for_mode *****************************/
  };
  /***** Archon::Expose_RXRV **************************************************/

}
