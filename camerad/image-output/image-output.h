//
// Created by mike-langmayr on 9/30/24.
//

#ifndef IMAGE_OUTPUT_H
#define IMAGE_OUTPUT_H

#include "../camera.h"

class ImageOutput {
public:
  virtual ~ImageOutput() = default;

  template <class T>
  long write_image(const T* image_data, Camera::Information &info) {
    const std::string function = "ImageOutput::write_image";
    logwrite(function, "member function should not be used");
    return NO_ERROR;
  };

  virtual long open(bool writekeys, Camera::Information &info) {
    const std::string function = "ImageOutput::open";
    logwrite(function, "member function should not be used");
  }

  virtual void close(bool writekeys, Camera::Information &info) {
    const std::string function = "ImageOutput::close";
    logwrite(function, "member function should not be used");
  }

  virtual bool is_open() = 0;
};

#endif //IMAGE_OUTPUT_H
