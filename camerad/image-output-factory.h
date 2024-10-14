//
// Created by mike-langmayr on 10/2/24.
//

#ifndef IMAGEOUTPUTFACTORY_H
#define IMAGEOUTPUTFACTORY_H

#include "image-output.h"
#include "save-to-disk.h"

class ImageOutputFactory {
public:
  static std::unique_ptr<ImageOutput> create_image_output(const std::string& output_type);
};



#endif //IMAGEOUTPUTFACTORY_H
