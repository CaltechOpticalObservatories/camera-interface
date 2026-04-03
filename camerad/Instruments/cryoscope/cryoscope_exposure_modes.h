/**
 * @file     Instruments/cryoscope/cryoscope_exposure_modes.h
 * @brief    delcares CryoScope-specific exposure mode classes
 * @details  Declares classes that implement exposure modes supported by
 *           CryoScope. These classes override virtual functions in the
 *           ExposureMode base class to provide mode-specific behavior.
 *
 */

#pragma once

#include "exposure_modes.h"  // ExposureMode base class

namespace Camera {

  /**
   * @namespace  all recognized exposure modes for CryoScope
   */
  namespace CryoScopeExposureMode {
    constexpr const char* XXX = "XXX";
    constexpr const char* ALLMODES[] = {XXX};
  }

  class CryoScope;

  /**
   * @brief      class constructor example for cryoscope-specific non-standard mode XXX
   * @param[in]  _interface  Pointer to Camera InterfaceType
   * @param[in]  _modeargs   mode arguments
   */
  class ExposureModeXXX : public ExposureModeTemplate<Camera::ArchonInterface,
                                                      Camera::ArchonImageBuffer> {
    public:
      ExposureModeXXX(Camera::ArchonInterface* iface, std::vector<std::string> modeargs)
        : ExposureModeTemplate<Camera::ArchonInterface,
                               Camera::ArchonImageBuffer>(iface) {
          this->modetype = CryoScopeExposureMode::XXX;
          this->modeargs = std::move(modeargs);
        }

    long expose() override;
    void image_acquisition_thread() override;
  };

}
