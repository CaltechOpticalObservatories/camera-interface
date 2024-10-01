//
// Created by mike-langmayr on 10/1/24.
//

#include "save-to-disk.h"
#include <filesystem>
#include <iostream>

SaveToDisk::SaveToDisk(const std::string& outputDir) : outputDirectory(outputDir) {
  // Ensure the directory exists or create it
  if (!std::filesystem::exists(outputDirectory)) {
    std::filesystem::create_directories(outputDirectory);
  }
}

void SaveToDisk::processImage(const char* imageData) {

}