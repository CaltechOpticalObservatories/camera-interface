/**
 * @file   image_process.h
 * @brief  composition model for simple image processing
 *
 */

#pragma once

#include "common.h"

namespace Camera {

  /**
   * @brief    DeInterlacer abstract base class
   */
  class DeInterlacer {
    public:
      virtual ~DeInterlacer() = default;
      virtual void deinterlace(char* in, char* out) {
        throw std::runtime_error("deinterlace(char*, char*) not supported");
      }
      virtual void deinterlace(char* in, uint16_t* out) {
        throw std::runtime_error("deinterlace(char*, uint16_t*) not supported");
      }
      virtual void deinterlace(char* in, uint16_t* out1, uint16_t* out2) {
        throw std::runtime_error("deinterlace(char*, uint16_t*, uint16_t*) not supported");
      }
  };

  class Subtractor {
    public:
      virtual ~Subtractor() = default;
      virtual void subtract(uint16_t* in1, uint16_t* in2, int16_t* out) {
        throw std::runtime_error("subtract(uint16_t*, uint16_t*, int16_t*) not supported");
      }
      virtual void subtract(uint16_t* in1, uint16_t* in2, int32_t* out) {
        throw std::runtime_error("subtract(uint16_t*, uint16_t*, int32_t*) not supported");
      }
  };

  class Coadder {
    public:
      virtual ~Coadder() = default;
      virtual void coadd(uint16_t* in, uint16_t* out) {
        throw std::runtime_error("coadd(uint16_t*, uint16_t*) not supported");
      }
      virtual void coadd(uint16_t* in, int16_t* out) {
        throw std::runtime_error("coadd(uint16_t*, int16_t*) not supported");
      }
      virtual void coadd(uint16_t* in, int32_t* out) {
        throw std::runtime_error("coadd(uint16_t*, int32_t*) not supported");
      }
  };

  class ImageProcessor {
    private:
      std::unique_ptr<DeInterlacer> _deinterlacer;
      std::unique_ptr<Subtractor> _subtractor;
      std::unique_ptr<Coadder> _coadder;

    public:
      ImageProcessor(std::unique_ptr<DeInterlacer> d,
                     std::unique_ptr<Subtractor> s,
                     std::unique_ptr<Coadder> c)
        : _deinterlacer(std::move(d)),
          _subtractor(std::move(s)),
          _coadder(std::move(c)) { }

      DeInterlacer* deinterlacer() const { return _deinterlacer.get(); }
      Subtractor* subtractor() const { return _subtractor.get(); }
      Coadder* coadder() const { return _coadder.get(); }
  };

  /**
   * @brief    factory function creates appropriate deinterlacer object
   */
  std::unique_ptr<ImageProcessor> make_image_processor(const std::string &mode);

}
