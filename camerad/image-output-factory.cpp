//
// Created by mike-langmayr on 10/2/24.
//

#include "image-output-factory.h"
#include "save-to-disk.h"


std::unique_ptr<ImageOutput> ImageOutputFactory::createImageOutput(const std::string& outputType) {
  if (outputType == "disk") {
    return std::make_unique<SaveToDisk>();
  }
  // else if (outputType == "queue") {
    // Return a MessageQueueSender, assuming it takes a connection string or queue name
    // return std::make_unique<MessageQueueSender>(queueName);
  // }
  else {
    std::cerr << "Error: Unknown output type '" << outputType << "' provided." << std::endl;
    throw std::invalid_argument("Unknown output type: " + outputType);
  }
}
