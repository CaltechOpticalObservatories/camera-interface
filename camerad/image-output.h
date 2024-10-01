//
// Created by mike-langmayr on 9/30/24.
//

#ifndef IMAGE_OUTPUT_H
#define IMAGE_OUTPUT_H

#include <vector>
#include <cstdint> // For uint8_t

class ImageOutput {
  // Pure virtual function to be implemented by derived classes
  virtual void processImage(const char* imageData) = 0;

  // Virtual destructor to allow derived classes to clean up properly
  virtual ~ImageOutput() = default;
};

#endif //IMAGE_OUTPUT_H
