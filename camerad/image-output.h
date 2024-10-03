//
// Created by mike-langmayr on 9/30/24.
//

#ifndef IMAGE_OUTPUT_H
#define IMAGE_OUTPUT_H

#include "camera.h"

class ImageOutput {
public:
  virtual ~ImageOutput() = default;

  template <class T>
  long write_image(const T* imageData, Camera::Information &info) {
    // Will be hidden
    std::cout << "Processing data: " << std::endl;
    return NO_ERROR;
  };

  virtual long open(bool writekeys, Camera::Information &info) = 0;

  virtual void close(bool writekeys, Camera::Information &info) = 0;
};

#endif //IMAGE_OUTPUT_H
