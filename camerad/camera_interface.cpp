#include "camera_interface.h"

namespace Camera {

  // this is the code for shared functions common to all implementations

  void Interface::set_server(Camera::Server* s) {
    this->server=s;
  }

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
}
