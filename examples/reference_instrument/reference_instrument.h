/**
 * @file    reference_instrument.h
 * @brief   the smallest instrument that builds against camerad
 */
#pragma once

#include "archon_interface.h"

namespace Camera {

  /// Instrument adding nothing to the Archon interface, as a starting point
  class ReferenceInstrument : public ArchonInterface {
    public:
      void configure_instrument() override;
  };

}
