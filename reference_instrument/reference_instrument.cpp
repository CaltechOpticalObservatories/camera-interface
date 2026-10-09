/**
 * @file    reference_instrument.cpp
 * @brief   implementation of the reference instrument
 *
 */

#include "reference_instrument.h"
#include "reference_exposure_modes.h"
#include "utilities.h"

#include <algorithm>

namespace Camera {

  /***** Camera::ReferenceInstrument::configure_instrument ********************/
  /**
   * @brief      extract instrument parameters from the config file
   * @details    The config file has already been read into the Config class.
   *
   */
  void ReferenceInstrument::configure_instrument() {
    const std::string function("Camera::ReferenceInstrument::configure_instrument");

    for (int row=0; row < this->configfile.n_rows; row++) {
      // REFERENCE_VALUE
      if (this->configfile.param[row]=="REFERENCE_VALUE") {
        this->reference_value = this->configfile.arg[row];
        logwrite(function, "config:"+this->configfile.param[row]+"="+this->configfile.arg[row]);
      }
    }
  }
  /***** Camera::ReferenceInstrument::configure_instrument ********************/


  /***** Camera::ReferenceInstrument::is_instrument_command *******************/
  /**
   * @brief      returns true if cmd is one of instrument_commands()
   * @details    The server consults this before every core command, so
   *             refvalue is reached only through it. That gate is not the
   *             design: a name the core does not recognize is meant to pass
   *             to the controller and then to the instrument with no gate.
   *             This override goes when the server does that.
   *
   */
  bool ReferenceInstrument::is_instrument_command(const std::string &cmd) {
    const auto commands = this->instrument_commands();
    return std::find(commands.begin(), commands.end(), cmd) != commands.end();
  }
  /***** Camera::ReferenceInstrument::is_instrument_command *******************/


  /***** Camera::ReferenceInstrument::instrument_commands *********************/
  /**
   * @brief      names of the commands this instrument handles
   *
   */
  std::vector<std::string> ReferenceInstrument::instrument_commands() const {
    return { REFERENCE_REFVALUE };
  }
  /***** Camera::ReferenceInstrument::instrument_commands *********************/


  /***** Camera::ReferenceInstrument::instrument_cmd **************************/
  /**
   * @brief      dispatcher for reference instrument commands
   * @param[in]  cmd        command
   * @param[in]  args       argument list
   * @param[out] retstring  return string
   * @return     ERROR|NO_ERROR|HELP
   *
   */
  long ReferenceInstrument::instrument_cmd(const std::string &cmd,
                                           const std::string &args,
                                           std::string &retstring) {
    if ( cmd == REFERENCE_REFVALUE ) {
      return this->refvalue(args, retstring);
    }
    else {
      retstring="unrecognized command";
      return ERROR;
    }
  }
  /***** Camera::ReferenceInstrument::instrument_cmd **************************/


  /***** Camera::ReferenceInstrument::refvalue ********************************/
  /**
   * @brief      report the value configure_instrument() read
   * @param[in]  args       not used
   * @param[out] retstring  REFERENCE_VALUE
   * @return     ERROR|NO_ERROR|HELP
   *
   */
  long ReferenceInstrument::refvalue(const std::string &args, std::string &retstring) {
    const std::string function("Camera::ReferenceInstrument::refvalue");

    // Help
    if (args=="?" || args=="help") {
      retstring = REFERENCE_REFVALUE;
      retstring.append( "\n" );
      retstring.append( "  report REFERENCE_VALUE from the config file\n" );
      return HELP;
    }

    if (this->reference_value.empty()) {
      return fail(function, retstring, "REFERENCE_VALUE not configured");
    }

    retstring = this->reference_value;
    return NO_ERROR;
  }
  /***** Camera::ReferenceInstrument::refvalue ********************************/


  /***** Camera::ReferenceInstrument::get_exposure_modes **********************/
  /**
   * @brief      Archon's exposure modes and the one this instrument adds
   * @return     vector<string>
   *
   */
  std::vector<std::string> ReferenceInstrument::get_exposure_modes() {
    auto modes = ArchonInterface::get_exposure_modes();
    modes.push_back(ReferenceExposureMode::REFERENCE);
    return modes;
  }
  /***** Camera::ReferenceInstrument::get_exposure_modes **********************/


  /***** Camera::ReferenceInstrument::set_exposure_mode ***********************/
  /**
   * @brief      constructs the mode this instrument defines, else the parent's
   * @param[in]  modein    string representing the exposure mode
   * @param[in]  modeargs  optional mode-specific args
   * @return     ERROR|NO_ERROR
   *
   */
  long ReferenceInstrument::set_exposure_mode(const std::string &modein, const std::vector<std::string> &modeargs) {
    if (caseCompareString(modein, ReferenceExposureMode::REFERENCE)) {
      this->exposuremode = std::make_shared<ExposureModeReference>(this);
      return NO_ERROR;
    }
    return ArchonInterface::set_exposure_mode(modein, modeargs);
  }
  /***** Camera::ReferenceInstrument::set_exposure_mode ***********************/

}
