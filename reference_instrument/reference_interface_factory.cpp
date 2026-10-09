/**
 * @file    reference_interface_factory.cpp
 * @brief   Reference Instrument Interface Factory
 *
 */
#include "reference_instrument.h"
#include "camera_interface.h"

namespace Camera {
  /***** Camera::Interface::create ********************************************/
  /**
   * @brief      factory function to create pointer to ReferenceInstrument
   * @return     unique_ptr<ReferenceInstrument>
   *
   */
  std::unique_ptr<Interface> Interface::create() {
    return std::make_unique<ReferenceInstrument>();
  }
  /***** Camera::Interface::create ********************************************/
}
