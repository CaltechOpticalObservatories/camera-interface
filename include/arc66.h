#ifndef ARC66_H
#define ARC66_H

#include "common.h"
#include "config.h"
#include "logentry.h"
#include "utilities.h"
#include "ArcDeviceCAPI.h"
#include "astrocam.h"     // defines namespace "AstroCam"

namespace Arc66 {

  class Interface : public AstroCam::Information, public AstroCam::CommonInterface {

    private:

    public:
      Interface();
      ~Interface();

//    Common::Common    common;              //!< instantiate a Common object

      CommonInterface astrocam;              //!< common astrocam interface

      Information camera_info;               //!< this is the main camera_info object
      Information fits_info;                 //!< used to copy the camera_info object to preserve info for FITS writing

      long interface(std::string &iface);
      long connect_controller();
      long disconnect_controller();
      long native(std::string cmd);
      long expose();

  };

}

#endif
