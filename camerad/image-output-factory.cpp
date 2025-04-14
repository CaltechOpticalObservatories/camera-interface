//
// Created by mike-langmayr on 10/2/24.
//

#include "image-output-factory.h"
#include "save-to-disk.h"


std::unique_ptr<ImageOutput> ImageOutputFactory::create_image_output(const std::string& output_type) {
  if (output_type == "disk") {
    return std::make_unique<SaveToDisk>();
  }
  // else if (outputType == "message_queue") {
    // Return a MessageQueueSender, assuming it takes a connection string or queue name
    // return std::make_unique<MessageQueueSender>(queueName);
  // }
  else {
    std::cerr << "Error: Unknown output type '" << output_type << "' provided." << std::endl;
    throw std::invalid_argument("Unknown output type: " + output_type);
  }
}
