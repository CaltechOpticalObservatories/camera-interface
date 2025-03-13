//
// Created by mike-langmayr on 10/1/24.
//

#include "write-to-disk.h"

WriteToDisk::WriteToDisk() {
  std::string function = "WriteToDisk::WriteToDisk";
  logwrite(function, "WriteToDisk contructor");
};

long WriteToDisk::open(bool writekeys, Camera::Information &info) {
  logwrite("WriteToDisk::open", "WriteToDisk::open");
  return this->fits_file.open_file(writekeys, info);
}

void WriteToDisk::close(bool writekeys, Camera::Information &info) {
  logwrite("WriteToDisk::close", "WriteToDisk::close");
  return this->fits_file.close_file(writekeys, info);
}

bool WriteToDisk::is_open() {
  return this->fits_file.isopen();
}

