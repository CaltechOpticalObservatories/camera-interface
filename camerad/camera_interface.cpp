#include "camera_interface.h"
#include "frame_output_factory.h"

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
}
