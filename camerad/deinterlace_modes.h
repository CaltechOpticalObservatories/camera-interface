/**
 * @file   deinterlace_modes.h
 * @brief  template-based implementation for deinterlacing
 *
 */

#pragma once

#include "common.h"

namespace Camera {

  /**
   * @brief    DeInterlace abstract base class
   */
  class DeInterlace {
    public:
      virtual ~DeInterlace() = default;
      virtual void deinterlace(char* in, char* out) {
        throw std::runtime_error("deinterlace(char*,char*) not supported");
      }
      virtual void deinterlace(char* in, uint16_t* out) {
        throw std::runtime_error("deinterlace(char*,uint16_t*) not supported");
      }
      virtual void deinterlace(char* in, uint16_t* out1, uint16_t* out2) {
        throw std::runtime_error("deinterlace(char*,uint16_t*,uint16_t*) not supported");
      }
  };

  class DeInterlace_None : public DeInterlace {
    public:
      void deinterlace(char* in, uint16_t* buf) override;
  };

  class DeInterlace_RXRV : public DeInterlace {
    public:
      void deinterlace(char* in, uint16_t* buf1, uint16_t* buf2) override;
  };

  /**
   * @brief    factory function creates appropriate deinterlacer object
   */
  std::unique_ptr<DeInterlace> make_deinterlacer(const std::string &mode);

}
