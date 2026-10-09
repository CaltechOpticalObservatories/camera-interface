/**
 * @file     reference_exposure_modes.h
 * @brief    declares the exposure mode the reference instrument adds
 *
 */

#pragma once

#include "archon_exposure_modes.h"

namespace Camera {

  /**
   * @namespace  exposure modes added to those of ArchonExposureMode
   */
  namespace ReferenceExposureMode {
    constexpr const char* REFERENCE = "REFERENCE";
  };

  // Reads out as SINGLE and differs only in the type it reports
  class ExposureModeReference : public ExposureModeSingle {
    public:
      ExposureModeReference(Camera::ArchonInterface* iface)
        : ExposureModeSingle(iface) {
          type=ReferenceExposureMode::REFERENCE;
        }
  };
}
