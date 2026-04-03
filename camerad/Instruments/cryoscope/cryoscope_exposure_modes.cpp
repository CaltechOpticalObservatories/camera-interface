/**
 * @file     Instruments/cryoscope/cryoscope_exposure_modes.h
 * @brief    implements CryoScope-specific exposure modes
 *
 */

#include "archon_exposure_modes.h"
#include "cryoscope_exposure_modes.h"
#include "archon_interface.h"

namespace Camera {

  /***** Camera::ExposureModeXXX::expose *************************************/
  /**
   * @brief  implementation of cryoscope-specific expose for a non-standard mode
   *
   */
  long ExposureModeXXX::expose() {
    const std::string function("Camera::ExposureModeXXX::expose");
    logwrite(function, "meep meep");
    return NO_ERROR;
  }
  /***** Camera::ExposureModeXXX::expose *************************************/


  /***** Camera::ExposureModeXXX::image_acquisition_thread ********************/
  /**
   * @brief      producer thread for non-standard ExposureMode XXX
   * @details    Spawned by Camera::ArchonInterface::do_expose()
   *
   */
  void ExposureModeXXX::image_acquisition_thread() {
    const std::string function("Camera::ExposureModeXXX::image_acquisition_thread");
    char message[256];

    logwrite(function, "mode args:");

    for (const auto &arg : this->modeargs) {
      logwrite(function, arg);
    }
  }
  /***** Camera::ExposureModeXXX::image_acquisition_thread ********************/

}
