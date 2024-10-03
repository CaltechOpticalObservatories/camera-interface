//
// Created by mike-langmayr on 10/2/24.
//

#ifndef IMAGEOUTPUTFACTORY_H
#define IMAGEOUTPUTFACTORY_H

#include "image-output.h"
#include "save-to-disk.h"

class ImageOutputFactory {
public:
  // Static method to create ImageOutput objects
  static std::unique_ptr<ImageOutput> createImageOutput(const std::string& outputType);
};



#endif //IMAGEOUTPUTFACTORY_H
