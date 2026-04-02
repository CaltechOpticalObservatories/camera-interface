/**
 * @file    Instruments/cryoscope/cryoscope_instrument.h
 * @brief   contains properties unique to the CryoScope instrument
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#pragma once

#include "archon_interface.h"  /// CryoScope uses ArchonInteface

namespace Camera {

  /***** Camera::CryoScope ****************************************************/
  /**
   * @class    CryoScope
   * @brief    derived class inherits from ArchonInterface
   * @details  This class describes CryoScope-specific functionality.
   *
   */
  class CryoScope : public ArchonInterface {
    public:
      // instrument command dispatcher
      long instrument_cmd(const std::string &cmd,
                          const std::string &args,
                          std::string &retstring) override;

      void configure_instrument() override;
      long power(std::string args, std::string &retstring) override;

      std::vector<std::string> get_exposure_modes() override;
      long set_exposure_mode(const std::string &modein, const std::vector<std::string> &args) override;

    private:
      // these are CryoScope-specific functions
      std::string start_param;
      long setup_detector();
  };
  /***** Camera::CryoScope ****************************************************/
}
