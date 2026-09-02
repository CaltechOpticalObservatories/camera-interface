#include "camera_interface.h"
#include "frame_output_factory.h"

#include <algorithm>
#include <cctype>

namespace Camera {

  // this is the code for shared functions common to all implementations

  void Interface::set_server(Camera::Server* s) {
    this->server=s;
  }

  /***** Camera::Interface::configure_frame_outputs ***************************/
  /**
   * @brief      build the in-band frame output sinks (FITS, shared memory)
   * @details    dispatch_frame fans every retrieved frame out to these
   *
   */
  void Interface::configure_frame_outputs() {
    const std::string function("Camera::Interface::configure_frame_outputs");
    FrameOutputsConfig outcfg;
    apply_config_overrides(outcfg, this->configfile);
    this->frame_outputs = make_frame_outputs(outcfg);
    logwrite(function, "configured "+std::to_string(this->frame_outputs.size())+" frame output(s)");
  }
  /***** Camera::Interface::configure_frame_outputs ***************************/

  void Interface::func_shared() {
    std::string function("Camera::Interface::func_shared");
    logwrite(function, "common implementation function");
  }

  /***** Camera::Interface::disconnect_controller *****************************/
  /**
   * @brief      disconnect camera controller
   * @details    use this to disconnect before exiting because it takes
   *             no arguments and returns nothing
   *
   */
  void Interface::disconnect_controller() {
    std::string retstring;
    this->disconnect_controller("", retstring);
  }
  /***** Camera::Interface::disconnect_controller *****************************/


  /***** Camera::Interface::key *************************************************/
  /**
   * @brief      add/list a custom FITS keyword written into every exposure
   * @param[in]  args       "list", or "KEYWORD=VALUE//COMMENT" (comment optional)
   * @param[out] retstring  usage string on help/error
   * @return     NO_ERROR | ERROR | HELP
   *
   * KEYWORD=. deletes that keyword (see Common::FitsKeys::addkey).
   *
   */
  long Interface::key(std::string args, std::string &retstring) {
    const std::string function("Camera::Interface::key");

    if (args.empty() || args == "?" || args == "help") {
      retstring = "key KEYWORD=VALUE//COMMENT | key list";
      return HELP;
    }

    if (args == "list") {
      logwrite(function, "systemkeys:");
      this->camera_info.systemkeys.listkeys();
      logwrite(function, "userkeys:");
      this->camera_info.userkeys.listkeys();
      return NO_ERROR;
    }

    long error = this->camera_info.userkeys.addkey(args);
    if (error != NO_ERROR) {
      logwrite(function, "ERROR bad syntax: expected KEYWORD=VALUE//COMMENT");
    }
    return error;
  }
  /***** Camera::Interface::key *************************************************/


  /***** Camera::Interface::datacube *********************************************/
  /**
   * @brief      set/report whether reads are written as one multi-extension FITS file
   * @param[in]  args       "true", "false" (case-insensitive), or empty to report state
   * @param[out] retstring  current state ("true"/"false"), or usage on error
   * @return     NO_ERROR | ERROR | HELP
   *
   */
  long Interface::datacube(std::string args, std::string &retstring) {
    const std::string function("Camera::Interface::datacube");

    if (args == "?" || args == "help") {
      retstring = "datacube [ true | false ]";
      return HELP;
    }

    if (!args.empty()) {
      std::transform(args.begin(), args.end(), args.begin(),
                      [](unsigned char c) { return std::tolower(c); });
      if (args != "true" && args != "false") {
        retstring = "datacube [ true | false ]";
        logwrite(function, "ERROR expected true or false, got " + args);
        return ERROR;
      }
      this->camera_info.is_datacube = (args == "true");
      for (auto &output : this->frame_outputs) {
        output->set_option("datacube", args);
      }
    }

    retstring = (this->camera_info.is_datacube ? "true" : "false");
    return NO_ERROR;
  }
  /***** Camera::Interface::datacube *********************************************/


  /***** Camera::Interface::configure_frame_outputs *******************************/
  /**
   * @brief      build frame_outputs (SHM/FITS) from the config file
   * @details    Called unconditionally from camerad.cpp after configure_instrument(),
   *             so no derived override can silently skip wiring frame_outputs.
   *
   */
  void Interface::configure_frame_outputs() {
    Camera::FrameOutputsConfig fo_cfg;
    Camera::apply_config_overrides(fo_cfg, this->configfile);
    this->frame_outputs = Camera::make_frame_outputs(fo_cfg);
  }
  /***** Camera::Interface::configure_frame_outputs *******************************/
}
