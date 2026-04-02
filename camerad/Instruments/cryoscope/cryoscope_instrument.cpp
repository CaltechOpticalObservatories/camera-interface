/**
 * @file    Instruments/cryoscope/cryoscope_instrument.cpp
 * @brief   implementation for CryoScope-specific properties
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "cryoscope_instrument.h"
#include "cryoscope_exposure_modes.h"

namespace Camera {

  /***** Camera::CryoScope::instrument_cmd ************************************/
  /**
   * @brief      dispatcher for CryoScope-specific instrument commands
   * @details    This allows dispatching instrument specific commands by receiving
   *             the command and args and calling the appropriate instrument
   *             specific function.
   * @param[in]  cmd        command
   * @param[in]  args       any number of arguments
   * @param[out] retstring  return string
   * @return     ERROR|NO_ERROR|HELP
   *
   */
  long CryoScope::instrument_cmd(const std::string &cmd,
                                 const std::string &args,
			         std::string &retstring) {
    if (false) {
    }
    else {
      retstring = "unrecognized command";
      return ERROR;
    }
  }
  /***** Camera::CryoScope::instrument_cmd ************************************/


  /***** Camera::CryoScope::configure_instrument ******************************/
  /**
   * @brief      extract+apply instrument-specific parameters from config file
   * @throws     std::runtime_error
   *
   */
  void CryoScope::configure_instrument() {
    const std::string function("Camera::CryoScope::configure_instrument");
    int numapplied=0, lastapplied=0;

    if (this->configfile.n_rows < 1) throw std::runtime_error("empty configuration");

    // iterate through each row in config file
    for (int row=0; row < this->configfile.n_rows; row++) {

      lastapplied=numapplied;

      // START_PARAM
      if (this->configfile.param[row]=="START_PARAM") {
        this->start_param = this->configfile.arg[row];
        numapplied++;
      }

      if (numapplied > lastapplied) {
        std::ostringstream oss;
        oss << "config:" << this->configfile.param[row] << "=" << this->configfile.arg[row];
        logwrite(function, oss.str());
      }
    }
  }
  /***** Camera::CryoScope::configure_instrument ******************************/


  /***** Camera::CryoScope::power *********************************************/
  /**
   * @brief      set/get power and set Start=1 when powered on
   * @details    This overrides ArchonInterface::power. It calls the standard function,
   *             but if powered on then it also sets the Start parameter = 1
   * @param[in]  args       requested state or help, on|off|?
   * @param[out] retstring  contains power_status string on success
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long CryoScope::power(const std::string args, std::string &retstring) {

    // first call standard ArchonInterface power
    long error = this->ArchonInterface::power(args, retstring);

    // if power is turned on set Start=1 and setup detector
    if (error==NO_ERROR && !args.empty() && this->controller->power_status=="ON") {
      error = this->controller->set_parameter(this->start_param, 1);
      if (error==NO_ERROR) error = this->setup_detector();
    }
    else
    // if power is turned off set Start=0
    if (error==NO_ERROR && !args.empty() && this->controller->power_status=="OFF") {
      error = this->controller->set_parameter(this->start_param, 0);
    }

    return error;
  }
  /***** Camera::CryoScope::power *********************************************/


  /***** Camera::CryoScope::get_exposure_modes ********************************/
  /**
   * @brief      return a vector of strings of recognized exposure modes
   * @details    This adds CryoScope exposure modes to the base exposure modes.
   * @return     vector<string>
   *
   */
  std::vector<std::string> CryoScope::get_exposure_modes() {
    // base exposure modes
    auto modes = this->ArchonInterface::get_exposure_modes();

    // add cryoscope exposure modes
    for (const auto &mode : Camera::CryoScopeExposureMode::ALLMODES) { modes.push_back(mode); }

    return modes;
  }
  /***** Camera::CryoScope::get_exposure_modes ********************************/


  /***** Camera::CryoScope::set_exposure_mode *********************************/
  /**
   * @brief      actually sets the exposure mode
   * @details    This creates the appropriate exposure mode object for the
   *             requested exposure mode, providing access to that mode's functions.
   *             This is cryoscope-specific but gets called by ArchonInterface because
   *             it overrides. If the requested mode is not a cryoscope mode then
   *             this will call the set_exposure_mode in the base class.
   * @param[in]  modein    desired exposure mode
   * @param[in]  modeargs  optional arguments for the mode
   * @return     ERROR|NO_ERROR
   *
   */
  long CryoScope::set_exposure_mode(const std::string &modein, const std::vector<std::string> &modeargs) {

    // check for specialized cryoscope-specific expose modes
    if (caseCompareString(modein, CryoScopeExposureMode::XXX)) {
      this->exposuremode = std::make_unique<ExposureModeXXX>(this, modeargs);
    }
    // otherwise it's a standard ArchonInterface exposure mode
    else {
      return this->ArchonInterface::set_exposure_mode(modein, modeargs);
    }

    return NO_ERROR;
  }
  /***** Camera::CryoScope::set_exposure_mode *********************************/


  /***** Camera::CryoScope::setup_detector ************************************/
  /**
   * @brief      setup detector
   * @details    This is a hard-coded placeholder for a better version but
   *             here just so the detector works.
   * @return     ERROR|NO_ERROR
   *
   */
  long CryoScope::setup_detector() {
    long error=NO_ERROR;
    std::vector<std::string> commands = { "10 1 16402",
                                          "10 0 1",
                                          "10 0 0" };
    for (const auto &cmd : commands) {
      error = this->controller->set_vcpu_inreg(cmd);
      if (error!=NO_ERROR) break;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return error;
  }
  /***** Camera::CryoScope::setup_detector ************************************/

}
