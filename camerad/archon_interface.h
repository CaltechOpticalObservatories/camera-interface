
#pragma once

#include "camera_interface.h"

namespace Camera {

  class ArchonInterface : public Interface {
    public:
      ArchonInterface() = default;
      ~ArchonInterface() override;

      void myfunction() override;
      long connect_controller( std::string args, std::string &retstring ) override {
        std::string function("Camera::ArchonInterface::connect_controller");
        std::stringstream message;
        long error = NO_ERROR;

        std::cout << "Connect Controller";
        return error;
      }

      long disconnect_controller( std::string args, std::string &retstring ) override {
        std::string function("Camera::ArchonInterface::connect_controller");
        std::stringstream message;
        long error = NO_ERROR;

        std::cout << "Disconnect Controller";
        return error;
      }
  };

}

