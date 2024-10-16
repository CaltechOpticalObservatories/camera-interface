//
// Created by mike-langmayr on 10/1/24.
//

#ifndef WRITE_TO_DISK_H
#define WRITE_TO_DISK_H

#include "image-output/image-output.h"
#include "fits.h"

class WriteToDisk : public ImageOutput {
public:
  explicit WriteToDisk();

  FITS_file fits_file; //!< instantiate a FITS container object

  template<class T>
  long write_image(T* imageData, Camera::Information &info);

  long open(bool writekeys, Camera::Information &info) override;

  void close(bool writekeys, Camera::Information &info) override;

  bool is_open() override;
};

#endif //WRITE_TO_DISK_H
