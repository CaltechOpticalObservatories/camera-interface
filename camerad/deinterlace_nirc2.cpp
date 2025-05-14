/**
 * @file    deinterlace_nirc2.h
 * @brief   nirc2-specific deinterlacing
 * @details This is an example of how different instruments might
 *          implement a specialized set of deinterlacing functions.
 *
 */
#ifdef INSTR_NIRC2  // define INSR_xxx at build time

#include "deinterlacing_modes.h"

namespace Camera {

  class DeInterlace_XXX : public DeInterlace {
    public:
      void deinterlace(char* in, uint16_t* out1, uint16_t* out2) override {
        // nirc2 implementation
      }
  };

  std::unique_ptr<DeInterlace> make_deinterlacer_xxx() {
    return std::make_unique<DeInterlace_XXX>();
  }

}

#endif
