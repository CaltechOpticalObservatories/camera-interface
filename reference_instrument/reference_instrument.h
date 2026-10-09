/**
 * @file    reference_instrument.h
 * @brief   declares the reference instrument
 * @details Belongs to no project. Overrides each instrument specialization
 *          point so that the core's CI exercises them.
 *
 */

#pragma once

#include "archon_interface.h"

const std::string REFERENCE_REFVALUE("refvalue");

namespace Camera {

  class ReferenceInstrument : public ArchonInterface {
    public:
      void configure_instrument() override;

      bool is_instrument_command(const std::string &cmd) override;
      std::vector<std::string> instrument_commands() const override;
      long instrument_cmd(const std::string &cmd,
                          const std::string &args,
                          std::string &retstring) override;

      std::vector<std::string> get_exposure_modes() override;
      long set_exposure_mode(const std::string &modein, const std::vector<std::string> &modeargs) override;

    private:
      std::string reference_value;  ///< REFERENCE_VALUE from the config file

      long refvalue(const std::string &args, std::string &retstring);
  };

}
