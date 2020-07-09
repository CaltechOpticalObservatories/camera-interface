#ifndef ARC66_H
#define ARC66_H

#include "common.h"
#include "config.h"
#include "logentry.h"
#include "utilities.h"
#include "ArcDeviceCAPI.h"

namespace Arc66 {

  class Interface {

    private:

    public:
      Interface();
      ~Interface();

      Common::Common    common;              //!< instantiate a Common object

      long interface(std::string &iface);
      long connect_controller();
      long disconnect_controller();
      long native(std::string cmd);
      long expose();

  };

}

#endif
