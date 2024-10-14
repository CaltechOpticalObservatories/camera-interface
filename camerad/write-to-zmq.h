//
// Created by mike-langmayr on 10/7/24.
//

#ifndef WRITE_TO_ZMQ_H
#define WRITE_TO_ZMQ_H

#include "zmq.hpp"
#include "image-output/image-output.h"
#include "zmq-push-socket.h"

class WriteToZmq: public ImageOutput {
public:
  explicit WriteToZmq();

  ZmqPushSocket push_socket;

  template<class T>
  long write_image(T* imageData, Camera::Information &info);

  long open(bool writekeys, Camera::Information &info) override;

  void close(bool writekeys, Camera::Information &info) override;

  bool is_open() override;
};


#endif //WRITE_TO_ZMQ_H
