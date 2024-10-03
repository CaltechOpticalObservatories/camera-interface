//
// Created by mike-langmayr on 10/1/24.
//

#ifndef SAVE_TO_DISK_H
#define SAVE_TO_DISK_H

#include "image-output.h"
#include "fits.h"

class SaveToDisk : public ImageOutput {
public:
  FITS_file fits_file; //!< instantiate a FITS container object

  explicit SaveToDisk();

  template<class T>
  long write_image(T* imageData, Camera::Information &info);

  long open(bool writekeys, Camera::Information &info) override;

  void close(bool writekeys, Camera::Information &info) override;

  bool is_open() override;
};

#endif //SAVE_TO_DISK_H
