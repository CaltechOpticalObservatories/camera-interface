
#pragma once

#include "camera_interface.h"
#include "archon_controller.h"

#define  SYSTEM        std::string("SYSTEM")
#define  FETCHLOG      std::string("FETCHLOG")

#define MAXADCCHANS 16             //!< max number of ADC channels per controller (4 mod * 4 ch/mod)
#define MAXADMCHANS 72             //!< max number of ADM channels per controller (4 mod * 18 ch/mod)
#define SNPRINTF(VAR, ...) { snprintf(VAR, sizeof(VAR), __VA_ARGS__); }

namespace Camera {
  class ArchonInterface : public Interface {
    public:
      ArchonInterface() = default;
      ~ArchonInterface() override;

      void myfunction() override;

      long connect_controller( const std::string args, std::string &retstring );
      long connect_controller(const std::string& devices_in);
      long disconnect_controller( const std::string args, std::string &retstring ) override;
      long send_cmd(std::string cmd, std::string &reply);
      long send_cmd(std::string cmd);
      long fetchlog();

    private:
      Controller controller;
      std::string_view QUIET = "quiet";  // allows sending commands without logging
      const int nmods = 12; //!< number of modules per controller
  };

}

