#include "astrocam.h"

namespace AstroCam {

  CommonInterface::CommonInterface() {
    this->config.filename = CONFIG_FILE;  //!< default config file set in CMakeLists.txt
  }

  CommonInterface::~CommonInterface() {
  }

  /** Arc66::CommonInterface::configure_controller ****************************/
  long CommonInterface::configure_controller() {
    std::string function = "AstroCam::CommonInterface::configure_controller";

    // loop through the entries in the configuration file, stored in config class
    //
    for (int entry=0; entry < this->config.n_entries; entry++) {

      if (config.param[entry].compare(0, 5, "IMDIR")==0) {
        this->common.imdir( config.arg[entry] );
      }

      if (config.param[entry].compare(0, 6, "IMNAME")==0) {
        this->common.imname( config.arg[entry] );
      }

    }
    logwrite(function, "successfully applied configuration");
    return NO_ERROR;
  }
}
