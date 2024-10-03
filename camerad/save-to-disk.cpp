//
// Created by mike-langmayr on 10/1/24.
//

#include "save-to-disk.h"

SaveToDisk::SaveToDisk() = default;

template <class T>
long SaveToDisk::write_image(T *imageData, Camera::Information &info) {
  return this->fits_file.write_image(imageData, info);
}

long SaveToDisk::open(bool writekeys, Camera::Information &info) {
  return this->fits_file.open_file(writekeys, info);
}

void SaveToDisk::close(bool writekeys, Camera::Information &info) {
  return this->fits_file.close_file(writekeys, info);
}
