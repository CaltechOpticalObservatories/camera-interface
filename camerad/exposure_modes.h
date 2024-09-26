/**
 * @file     exposure_modes.h
 *
 * @brief    describes the supported exposure modes
 *
 * @details  This contains derived classes for each exposure mode, selected
 *           by Archon::Interface::select_expose_mode(). These inherit from
 *           the ExposureBase class and override the expose_for_mode function.
 *
 *           Each class is constructed with a pointer to the Archon::Interface
 *           class so that member functions have access to Interface, using the
 *           "this->interface->" pointer dereference.
 *
 */
#pragma once

#include "archon.h"

namespace Archon {


  /***** Archon::Expose_Raw ***************************************************/
  class Expose_Raw : public ExposureBase {
    public:
      Expose_Raw(Interface* interface) : ExposureBase(interface) { }

      long expose_for_mode() override {
        const std::string function="Archon::Expose_Raw::expose_for_mode";
        logwrite( "Archon::Expose_Raw::expose_for_mode", "Raw" );
        return NO_ERROR;
      }
  };
  /***** Archon::Expose_CCD ***************************************************/


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
   * @details    Each Archon frame buffer contains a pair of frames, a read and
   *             a reset frame. The reset frame belongs to the next read, so
   *             the read of the first pair and the reset of the last pair are
   *             not used.
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
        long error = ERROR;

        // Create FITS_file pointers for the files used in this function,
        // for CDS and unprocessed images.
        //
        auto file_cds = std::make_unique<FITS_file<int32_t>>(interface->camera.datacube() ? true : false);
        auto file_unp = std::make_unique<FITS_file<uint16_t>>(interface->camera.datacube() ? true : false);

        logwrite( function, "RXRV" );
        logwrite( "Archon::Expose_RXRV::isconnected", std::to_string(interface->archon.isconnected()) );

        // ***************************************
        // ******** first frame pair here ********
        // ***************************************

        // Read the first frame buffer from Archon to host and decrement my local
        // frame counter.  This call to read_frame() reads into interface->archon_buf.
        //
        error = interface->read_frame();

        if ( error != NO_ERROR ) {
          logwrite( function, "ERROR reading frame buffer" );
          return ERROR;
        }

        // Always deinterlace first frame.
        // Write here only if is_unp set to write unprocessed images.
        //

        //
        // -- MAIN SEQUENCE LOOP --
        //
        while ( nseq > 0 ) {

          // increment ring index
          //
          interface->ring_index_inc();

          if ( interface->camera_info.exposure_time.value() != 0 ) {           // wait for the exposure delay to complete (if there is one)
            error = interface->wait_for_exposure();
            if ( error != NO_ERROR ) {
              logwrite( function, "ERROR: waiting for exposure" );
              break;
            }
          }

          // Wait for detector readout into Archon internal frame buffer
          //
          error = interface->wait_for_readout();
          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR waiting for readout" );
            break;
          }

          // *********************************************
          // ******** subsequent frame pairs here ********
          // *********************************************

          // Read the next (and subsequent) frame buffers from Archon to host and
          // decrement my local frame counter.  This reads into interface->archon_buf.
          //
          error = interface->read_frame();
          nseq--;

          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR reading frame buffer" );
            break;
          }

          // De-interlace each subsequent frame
          //

          // Write CDS frame
          //

          if ( error != NO_ERROR ) break;
        }

        // Complete the FITS file after processing all frames.
        // This closes the file(s) for any defined FITS_file object(s), and
        // shuts down the FITS engine, waiting for the queue to empty if needed.
        //
        if ( file_cds ) file_cds->complete();
        if ( file_unp ) file_unp->complete();

        return error;
      }
      /***** Archon::Expose_RXRV::expose_for_mode *****************************/
  };
  /***** Archon::Expose_RXRV **************************************************/

}
