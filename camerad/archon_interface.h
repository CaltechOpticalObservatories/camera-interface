/**
 * @file    archon_interface.h
 * @brief   this defines the Archon implementation of Camera::Interface
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include "camera_interface.h"          //!< defines Camera::Interface base class
#include "archon_controller.h"

#define  SYSTEM        std::string("SYSTEM")
#define  FETCHLOG      std::string("FETCHLOG")

#define MAXADCCHANS 16             //!< max number of ADC channels per controller (4 mod * 4 ch/mod)
#define MAXADMCHANS 72             //!< max number of ADM channels per controller (4 mod * 18 ch/mod)
#define SNPRINTF(VAR, ...) { snprintf(VAR, sizeof(VAR), __VA_ARGS__); }

namespace Camera {
  class Controller;

  class ArchonInterface : public Interface {
    friend Controller;

    public:
      ArchonInterface();
      ~ArchonInterface() override;

      // These are virtual functions inherited by the Camera::Interface base class
      // and have their own controller-specific implementations which are
      // implemented in archon_interface.cpp.
      //
      long abort( const std::string args, std::string &retstring ) override;
      long autodir( const std::string args, std::string &retstring ) override;
      long basename( const std::string args, std::string &retstring ) override;
      long bias( const std::string args, std::string &retstring ) override;
      long bin( const std::string args, std::string &retstring ) override;
      long connect_controller( const std::string args, std::string &retstring ) override;
      long disconnect_controller( const std::string args, std::string &retstring ) override;
      long exptime( const std::string args, std::string &retstring ) override;
      long test( const std::string args, std::string &retstring ) override;

    private:
      Controller controller;
      std::string_view QUIET = "quiet";  // allows sending commands without logging
      const int nmods = 12; //!< number of modules per controller

      // These functions are specific to the Archon Interface and are not
      // found in the base class.
      //
      long connect_controller(const std::string& devices_in);
      template <class T>
        long get_configmap_value(std::string key_in, T& value_out);
      long send_cmd(std::string cmd, std::string &reply);
      long send_cmd(std::string cmd);
      long fetchlog();
  };

}
