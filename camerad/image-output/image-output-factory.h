//
// Created by mike-langmayr on 10/2/24.
//

#ifndef IMAGEOUTPUTFACTORY_H
#define IMAGEOUTPUTFACTORY_H

#include "image-output.h"
#include "../write-to-disk.h"
#include "../write-to-zmq.h"

class ImageOutputFactory {
public:
  static std::unique_ptr<ImageOutput> create_image_output_object(std::string output_type);
};



#endif //IMAGEOUTPUTFACTORY_H
