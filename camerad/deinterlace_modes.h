/**
 * @file   deinterlace_modes.h
 * @brief  template-based implementation for deinterlacing
 *
 */

#pragma once

#include "common.h"

namespace Camera {

  /**
   * @brief    tags for constructing appropriate deinterlacer object
   * @details  DeInterlaceMode tags are used to distinguish the type
   *           of deinterlacer the factory function will make. Empty
   *           structs take zero space.
   */
  struct ModeNone {};
  struct ModeRXRV {};
  struct ModeFowler {};
  struct ModeCCD {};
  struct ModeUTR {};

  /**
   * @brief    DeInterlace abstract base class
   * @details  Actual deinterlacing implementations are defined in
   *           template specifications.
   */
  class DeInterlaceBase {
    public:
      virtual ~DeInterlaceBase() = default;
      virtual void test() = 0;
  };

  /**
   * @brief    template class for mode-specific deinterlacing
   * @details  Specialize this class for each ModeTag to implement the
   *           corresponding deinterlacer.
   */
  template <typename TIN, typename TOUT, typename ModeTag>
  class DeInterlaceMode : public DeInterlaceBase {
    public:
      void test() override;
      void deinterlace(TIN* bufin, TOUT* bufout) {}
      void deinterlace(TIN* bufin, TOUT* butout1, TOUT* butout2) {}
  };

  /**
   * @brief    default specialization function, unless defined
   */
  template <typename TIN, typename TOUT, typename ModeTag>
  void DeInterlaceMode<TIN, TOUT, ModeTag>::test() {
    logwrite("Camera::DeInterlaceMode::test", "not implemented for this mode");
  }

  /**
   * @brief    factory function creates appropriate deinterlacer object
   */
  std::unique_ptr<DeInterlaceBase> make_deinterlacer(const std::string &mode);

}
