//
// Created by mike-langmayr on 9/30/24.
//

#ifndef IMAGE_OUTPUT_H
#define IMAGE_OUTPUT_H

#include "camera.h"

class ImageOutput {
  template<class T>
  void writeImage(const T* imageData, Camera::Information &info) {
    // Will be hidden
    std::cout << "Processing data: " << std::endl;
  };

  virtual ~ImageOutput() = default;
};

#endif //IMAGE_OUTPUT_H
