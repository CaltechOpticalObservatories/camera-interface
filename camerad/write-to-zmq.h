//
// Created by mike-langmayr on 10/7/24.
//

#ifndef WRITE_TO_ZMQ_H
#define WRITE_TO_ZMQ_H

#include "zmq.hpp"
#include "image-output/image-output.h"
#include "zmq-socket.h"

class WriteToZmq: public ImageOutput {
public:
  WriteToZmq() {
    const std::string function = "WriteToZmq::WriteToZmq";
    logwrite(function, "WriteToZmq constructor");

    const std::string zmq_port = "5555";
    logwrite(function, "Connecting to ZMQ push socket on port " + zmq_port);

    try {
      // Attempt to connect
      this->zmq_socket.connect("tcp://localhost:" + zmq_port);
      logwrite(function, "Socket connected successfully.");
    } catch (const zmq::error_t& e) {
      logwrite(function, "Error while connecting: " + std::string(e.what()));
    }
  }

  ZmqSocket zmq_socket;

  template<class T>
  long write_image(T* imageData, Camera::Information &info) {
    const std::string function = "WriteToZmq::write_image";
    logwrite(function, "writing image to ZMQ");

    std::string timestamp = get_timestamp("");
    std::string zmq_message = timestamp + ", image data: " + imageData;
    logwrite(function, "Sending image data...");

    // Send the message to the server (asynchronously)
    try {
      this->zmq_socket.send_data(zmq::buffer(zmq_message));
      logwrite(function, "Message sent successfully.");
    } catch (const zmq::error_t& e) {
      logwrite(function, "Error while sending message: " + std::string(e.what()));
    }

    return NO_ERROR;
  };

  long open(bool writekeys, Camera::Information &info) override;

  void close(bool writekeys, Camera::Information &info) override;

  bool is_open() override;
};


#endif //WRITE_TO_ZMQ_H
