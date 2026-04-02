/**
 * @file    Instruments/cryoscope/cryoscope_interface_factory.cpp
 * @brief   CryoScope Interface Factory
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include "cryoscope_instrument.h"
#include "camera_interface.h"

namespace Camera {

  /***** Camera::Interface::create ********************************************/
  /**
   * @brief      factory function to create pointer to CryoScope
   * @return     unique_ptr<CryoScope>
   *
   */
  std::unique_ptr<Interface> Interface::create() {
    return std::make_unique<CryoScope>();
  }
  /***** Camera::Interface::create ********************************************/

}
