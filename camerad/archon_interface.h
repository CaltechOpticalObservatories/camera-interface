/**
 * @file    archon_interface.h
 * @brief   this defines the Archon implementation of Camera::Interface
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include "camera_interface.h"          //!< defines Camera::Interface base class
#include "archon_controller.h"

constexpr int MAXADCCHANS =   16;              //!< max number of ADC channels per controller (4 mod * 4 ch/mod)
constexpr int MAXADMCHANS =   72;              //!< max number of ADM channels per controller (4 mod * 18 ch/mod)
constexpr int BLOCK_LEN   = 1024;              //!< Archon block size
constexpr int REPLY_LEN   =  100 * BLOCK_LEN;  //!< Reply buffer size (over-estimate)

// Archon commands
//
const std::string  SYSTEM        = "SYSTEM";
const std::string  STATUS        = "STATUS";
const std::string  FRAME         = "FRAME";
const std::string  CLEARCONFIG   = "CLEARCONFIG";
const std::string  POLLOFF       = "POLLOFF";
const std::string  POLLON        = "POLLON";
const std::string  APPLYALL      = "APPLYALL";
const std::string  POWERON       = "POWERON";
const std::string  POWEROFF      = "POWEROFF";
const std::string  APPLYCDS      = "APPLYCDS";
const std::string  APPLYSYSTEM   = "APPLYSYSTEM";
const std::string  RESETTIMING   = "RESETTIMING";
const std::string  LOADTIMING    = "LOADTIMING";
const std::string  HOLDTIMING    = "HOLDTIMING";
const std::string  RELEASETIMING = "RELEASETIMING";
const std::string  LOADPARAMS    = "LOADPARAMS";
const std::string  TIMER         = "TIMER";
const std::string  FETCHLOG      = "FETCHLOG";
const std::string  UNLOCK        = "LOCK0";

// Minimum required backplane revisions for certain features
//
const std::string REV_RAMP           = "1.0.548";
const std::string REV_SENSORCURRENT  = "1.0.758";
const std::string REV_HEATERTARGET   = "1.0.1087";
const std::string REV_FRACTIONALPID  = "1.0.1054";
const std::string REV_VCPU           = "1.0.784";

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
      long load_firmware( const std::string args, std::string &retstring ) override;
      long native( const std::string args, std::string &retstring ) override;
      long power( const std::string args, std::string &retstring ) override;
      long test( const std::string args, std::string &retstring ) override;

      // These functions are specific to the Archon Interface and are not
      // found in the base class.
      //
      long load_timing(std::string cmd, std::string &reply);

    private:
      Controller controller;
      std::string_view QUIET = "quiet";  // allows sending commands without logging
      const int nmods = 12; //!< number of modules per controller

      // These functions are specific to the Archon Interface and are not
      // found in the base class.
      //
      long connect_controller(const std::string& devices_in);
      template <class T> long get_configmap_value(std::string key_in, T& value_out);
      long get_status_key(std::string key, std::string &value);
      long load_acf(std::string cmd);
      long load_timing(std::string cmd);
      long send_cmd(std::string cmd, std::string &reply);
      long send_cmd(std::string cmd);
      long fetchlog();
  };

}
