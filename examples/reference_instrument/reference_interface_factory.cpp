/**
 * @file    reference_interface_factory.cpp
 * @brief   the seam between camerad and an instrument
 *
 * camerad calls this to obtain its interface, so every instrument supplies one
 * translation unit defining it.
 */

#include "reference_instrument.h"
#include "camera_interface.h"

namespace Camera {

  std::unique_ptr<Interface> Interface::create() {
    return std::make_unique<ReferenceInstrument>();
  }

}
