/**
 * @file    archon.cpp
 * @brief   common interface functions
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include "logentry.h"
#include "common.h"
#include "archon.h"
#include "tcplinux.h"

namespace Archon {

  // Archon::Archon constructor
  //
  Interface::Interface() {
  }

  // Archon::
  //
  Interface::~Interface() {
  }

  long Interface::connect_to_controller() {
    const char* function = "Archon::Interface::connect_to_controller";
    Logf("(%s)\n", function);
    connect_to_server("10.0.0.2", 4242);
    return 0;
  }

  long Interface::close_connection() {
    const char* function = "Archon::Interface::close_connection";
    Logf("(%s)\n", function);
    return 0;
  }

  long Interface::load_config(std::string cfgfile) {
    const char* function = "Archon::Interface::load_config";
    Logf("(%s)\n", function);
    return 0;
  }
}
