//
// Created by mike-langmayr on 10/1/24.
//

#include "write-to-disk.h"

WriteToDisk::WriteToDisk() {
  std::string function = "WriteToDisk::WriteToDisk";
  logwrite(function, "WriteToDisk contructor");
};

template <class T>
long WriteToDisk::write_image(T *imageData, Camera::Information &info) {
  logwrite("WriteToDisk::write_image", "WriteToDisk::write_image");
  return this->fits_file.write_image(imageData, info);
}

long WriteToDisk::open(bool writekeys, Camera::Information &info) {
  return this->fits_file.open_file(writekeys, info);
}

void WriteToDisk::close(bool writekeys, Camera::Information &info) {
  return this->fits_file.close_file(writekeys, info);
}

bool WriteToDisk::is_open() {
  return this->fits_file.isopen();
}

