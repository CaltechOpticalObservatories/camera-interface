//
// Created by mike-langmayr on 10/2/24.
//

#include "image-output-factory.h"

std::unique_ptr<ImageOutput> ImageOutputFactory::create_image_output_object(std::string output_type) {
  const std::string function = "ImageOutputFactory::create_image_output";
  logwrite(function, "creating image output: " + output_type);

  if (output_type == "disk") {
    return std::make_unique<WriteToDisk>();
  }
  if (output_type == "zmq") {
    logwrite(function, "return zmq");

    return std::make_unique<WriteToZmq>();
  }

  logwrite(function, "Error: Unknown output type '" + output_type + "' provided.");
  return nullptr;
}