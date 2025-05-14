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
   * @return     unique_ptr to DeInterlace derived object
   * @throws     std::invalid_argument
   *
   */
  std::unique_ptr<DeInterlace> make_deinterlacer(const std::string &mode) {
    if (mode=="none") return std::make_unique<DeInterlace_None>();
    if (mode=="rxrv") return std::make_unique<DeInterlace_RXRV>();
    throw std::invalid_argument("unknown mode "+mode);
  }
  /***** Camera::make_deinterlacer ********************************************/


  /***** Camera::deinterlace **************************************************/
  /**
   * @brief      specialization for deinterlacing None
   * @param[in]  bufin   pointer to input buffer
   * @param[out] bufout  pointer to deinterlaced buffer
   */
  void DeInterlace_None::deinterlace(char* bufin, uint16_t* bufout) {
    const std::string function("Camera::DeInterlace_None::deinterlace");
    logwrite(function, "here");
  }
  /***** Camera::deinterlace **************************************************/


  /***** Camera::deinterlace **************************************************/
  /**
   * @brief      specialization for deinterlacing RXRV
   * @param[in]  imgbuf  pointer to input buffer
   * @param[out] sigbuf  pointer to deinterlaced signal frame from imgbuf
   * @param[out] resbuf  pointer to deinterlaced reset frame from imgbuf
   */
  void DeInterlace_RXRV::deinterlace(char* imgbuf, uint16_t* sigbuf, uint16_t* resbuf) {
    const std::string function("Camera::DeInterlace_RXRV::deinterlace");
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
  /***** Camera::deinterlace **************************************************/
}
