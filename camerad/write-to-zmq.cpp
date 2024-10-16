//
// Created by mike-langmayr on 10/7/24.
//

#include "write-to-zmq.h"
#include "zmq.hpp"

long WriteToZmq::open(bool writekeys, Camera::Information &info) {
  // std::string function = "WriteToZmq::open";

  // logwrite(function, "opening ZMQ push socket");

  // this->push_socket.connect("tcp://localhost:5555");
  return NO_ERROR;
}

void WriteToZmq::close(bool writekeys, Camera::Information &info) {

}

bool WriteToZmq::is_open() {
  return true;
}