//
// Created by mike-langmayr on 10/7/24.
//

#include "zmq.hpp"
#include "write-to-zmq.h"

WriteToZmq::WriteToZmq() = default;

template <class T>
long WriteToZmq::write_image(T *imageData, Camera::Information &info) {
  std::string timestamp = get_timestamp("");
  std::string zmq_message = timestamp + ": Image data goes here";
  std::cout << "Sending: " << zmq_message << std::endl;

  // Send the message to the server (asynchronously)
  this->push_socket.send_data(zmq::buffer(zmq_message));

  return NO_ERROR;
}

long WriteToZmq::open(bool writekeys, Camera::Information &info) {
  this->push_socket.connect("tcp://localhost:5555");
  return NO_ERROR;
}

void WriteToZmq::close(bool writekeys, Camera::Information &info) {
  return;
}

bool WriteToZmq::is_open() {
  return true;
}