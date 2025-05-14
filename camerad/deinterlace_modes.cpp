/**
 * @file   deinterlace_modes.cpp
 * @brief  implementation of deinterlacing factory and specializations
 *
 */

#include "deinterlace_modes.h"

namespace Camera {

  /***** Camera::make_deinterlacer ********************************************/
  /**
   * @brief      factory function creates appropriate deinterlacer object
   * @param[in]  mode
   * @return     unique_ptr to DeInterlaceBase derived object
   * @throws     std::invalid_argument
   *
   */
  std::unique_ptr<DeInterlaceBase> make_deinterlacer(const std::string &mode) {
    if (mode=="none") {
      logwrite("Camera::make_deinterlacer", "factory made deinterlacer for \'none\'");
      return std::make_unique<DeInterlaceMode<char, uint16_t, ModeNone>>();
    }
    else
    if (mode=="rxrv") {
      logwrite("Camera::make_deinterlacer", "factory made deinterlacer for \'rxrv\'");
      return std::make_unique<DeInterlaceMode<char, uint16_t, ModeRXRV>>();
    }
    else {
      logwrite("Camera::make_deinterlacer", "ERROR factory got unknown mode \'"+mode+"\'");
      throw std::invalid_argument("unknown mode "+mode);
    }
  }
  /***** Camera::make_deinterlacer ********************************************/


  /***** Camera::DeInterlaceMode::test ****************************************/
  /**
   * @brief      specialization for ModeRXRV test
   */
  template <>
  void DeInterlaceMode<char, uint16_t, ModeRXRV>::test() {
    const std::string function("Camera::DeInterlaceMode::ModeNone::test");
    logwrite(function, "here");
  }
  /***** Camera::DeInterlaceMode::test ****************************************/


  /***** Camera::DeInterlaceMode::deinterlace *********************************/
  /**
   * @brief      specialization for ModeNone deinterlacing
   * @param[in]  bufin   pointer to input buffer
   * @param[out] bufout  pointer to deinterlaced buffer
   */
  template <>
  void DeInterlaceMode<char, uint16_t, ModeNone>::deinterlace(char* bufin, uint16_t* bufout) {
    const std::string function("Camera::DeInterlaceMode::ModeNone::deinterlace");
    logwrite(function, "here");
  }
  /***** Camera::DeInterlaceMode::deinterlace *********************************/


  /***** Camera::DeInterlaceMode::deinterlace *********************************/
  /**
   * @brief      specialization for ModeRXRV deinterlacing
   * @param[in]  imgbuf  pointer to input buffer
   * @param[out] sigbuf  pointer to deinterlaced signal frame from imgbuf
   * @param[out] resbuf  pointer to deinterlaced reset frame from imgbuf
   */
  template <>
  void DeInterlaceMode<char, uint16_t, ModeRXRV>::deinterlace(char* imgbuf, uint16_t* sigbuf, uint16_t* resbuf) {
    const std::string function("Camera::DeInterlaceMode::ModeRXRV::deinterlace");
    logwrite(function, "here");
    std::cerr << "(" << function << ") buffer contents:";
    for (int i=0; i<10; i++) {
      std::cerr << " " << i;
      // modify contents of output buffers
      sigbuf[i]=i;
      resbuf[i]=100-i;
    }
    std::cerr << std::endl;
  }
}
