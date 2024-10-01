//
// Created by mike-langmayr on 10/1/24.
//

#ifndef SAVE_TO_DISK_H
#define SAVE_TO_DISK_H

#include "image-output.h"
#include <string>

class SaveToDisk : public ImageOutput {
public:
  explicit SaveToDisk(const std::string& outputDirectory);
  void processImage(const char* imageData) override;
  ~SaveToDisk();

private:
  std::string outputDirectory;
};

#endif //SAVE_TO_DISK_H
