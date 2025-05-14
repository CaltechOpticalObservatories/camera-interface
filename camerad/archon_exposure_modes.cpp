/**
 * @file     archon_exposure_modes.h
 * @brief    implements Archon-specific exposure modes
 *
 */

#include "archon_exposure_modes.h"
#include "archon_interface.h"

namespace Camera {

  /***** Camera::Expose_CCD **************************************************/
  /**
   * @brief  implementation of Archon-specific expose for CCD
   *
   */
  long Expose_CCD::expose() {
    const std::string function("Camera::Expose_CCD::expose");
    logwrite(function, "hi");
    return NO_ERROR;
  }
  /***** Camera::Expose_CCD **************************************************/


  /***** Camera::Expose_RXRV *************************************************/
  /**
   * @brief  implementation of Archon-specific expose for RXR-Video
   *
   */
  long Expose_RXRV::expose() {
    const std::string function("Camera::Expose_RXRV::expose");

    size_t sz=100;

    // Two each of signal and reset buffers, current and previous, since
    // we need to pair the reset from the previous frame with signal from
    // the current frame. These will hold deinterlaced frames.
    //
    std::vector<std::vector<uint16_t>> sigbuf(2, std::vector<uint16_t>(sz));
    std::vector<std::vector<uint16_t>> resbuf(2, std::vector<uint16_t>(sz));

    // allocate memory for frame buffer read from Archon
    interface->allocate_framebuf(sz);

    // create an appropriate deinterlacer object
    try { deinterlacer = make_deinterlacer("rxrv");
    }
    catch(const std::exception &e) {
      logwrite(function, "ERROR: "+std::string(e.what()));
      return ERROR;
    }
    auto* pd = dynamic_cast<DeInterlaceMode<char, uint16_t, ModeRXRV>*>(deinterlacer.get());

    // read first frame pair into my frame buffer
    interface->read_frame();

    // process (deinterlace) first frame pair
    pd->deinterlace(interface->get_framebuf(), sigbuf[0].data(), resbuf[0].data());

    // show contents
    std::cerr << "(" << function << ") sig:";
    for (int i=0; i<10; i++) std::cerr << " " << sigbuf[0][i];
    std::cerr << std::endl;
    std::cerr << "(" << function << ") res:";
    for (int i=0; i<10; i++) std::cerr << " " << resbuf[0][i];
    std::cerr << std::endl;

    pd->test();
    deinterlacer->test();

    // loop:
    // subsequent frame pairs, read, deinterlace, write

    return NO_ERROR;
  }
  /***** Camera::Expose_RXRV *************************************************/

}
