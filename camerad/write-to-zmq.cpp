//
// Created by mike-langmayr on 10/7/24.
//

#include "write-to-zmq.h"
#include "zmq.hpp"

long WriteToZmq::open(bool writekeys, Camera::Information &info) {
  std::string function = "WriteToZmq::open";
  logwrite(function, "open");
  return NO_ERROR;
}

long WriteToZmq::open_socket() {
  std::string function = "WriteToZmq::open_socket";
  logwrite(function, "opening ZMQ socket");
  //
  // std::string addr = "tcp://localhost:5555";
  //
  // try {
  //   publisher.bind(addr);
  is_socket_open = true;
  //   std::cout << "Publisher bound to " << addr << std::endl;
  // } catch(const zmq::error_t& e) {
  //   std::cerr << "Binding failed: " << e.what() << std::endl;
  //   return ERROR;
  // }

  return NO_ERROR;
}

void WriteToZmq::close(bool writekeys, Camera::Information &info) {
  std::string function = "WriteToZmq::close";
  logwrite(function, "close");
}

void WriteToZmq::close_socket() {
  std::string function = "WriteToZmq::close_socket";
  logwrite(function, "closing ZMQ socket");
  //
  // try {
  //   publisher.unbindAll();
  is_socket_open = false;
  // } catch(const zmq::error_t& e) {
  //   std::cerr << "Unbinding failed: " << e.what() << std::endl;
  // }
}

bool WriteToZmq::is_open() {
  return this->is_socket_open;
}