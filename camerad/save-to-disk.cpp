//
// Created by mike-langmayr on 10/1/24.
//

#include "save-to-disk.h"
#include <filesystem>

SaveToDisk::SaveToDisk(const std::string& outputDir) : outputDirectory(outputDir) {
  // Ensure the directory exists or create it
  if (!std::filesystem::exists(outputDirectory)) {
    std::filesystem::create_directories(outputDirectory);
  }
}

template <class T>
void SaveToDisk::writeImage(T *imageData, Camera::Information &info) {

}
