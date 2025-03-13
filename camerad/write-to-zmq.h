//
// Created by mike-langmayr on 10/7/24.
//

#ifndef WRITE_TO_ZMQ_H
#define WRITE_TO_ZMQ_H

#include "image-output/image-output.h"
// #include "zmq-socket.h"
#include <cstdint>
#include <zmq.hpp>
#include <netinet/in.h>

class WriteToZmq: public ImageOutput {
private:
  // zmq::context_t context; // ZeroMQ context
  // zmq::socket_t publisher;   // ZeroMQ socket
  bool is_socket_open = false;

  // void convertToNetworkByteOrder(unsigned short int* data, size_t length) {
  //   for (size_t i = 0; i < length; i++) {
  //     // Cast to unsigned short to match htons() expected type
  //     ((unsigned short*)data)[i] = htons(((unsigned short*)data)[i]);
  //   }
  // }

public:
  WriteToZmq() = default;
  // WriteToZmq() : context(1), publisher(context, ZMQ_XPUB) {
  //   const std::string function = "WriteToZmq::WriteToZmq";
  //   std::string addr = "tcp://localhost:5555";
  //
  //   try {
  //     publisher.bind(addr);
  //     is_socket_open = true;
  //     logwrite(function, "Publisher bound to " + addr);
  //   } catch(const zmq::error_t& e) {
  //     std::cerr << "Binding failed: " << e.what() << std::endl;
  //   }
  // }

  // Delete copy constructor and copy assignment operator
  // WriteToZmq(const WriteToZmq&) = delete;
  // WriteToZmq& operator=(const WriteToZmq&) = delete;
  //
  // // Move constructor
  // WriteToZmq(WriteToZmq&& other) noexcept
  //     : context(std::move(other.context)), publisher(std::move(other.publisher)) {
  //   // Optionally handle other state transitions here
  // }
  //
  // // Move assignment operator
  // WriteToZmq& operator=(WriteToZmq&& other) noexcept {
  //   context = std::move(other.context);
  //   publisher = std::move(other.publisher);
  //   return *this;
  // }

  template<class T>
  long write_image(T* imageData, Camera::Information &info, std::string info_json = "") {
    const std::string function = "WriteToZmq::write_image";
    logwrite(function, "writing image to ZMQ");

    // short int data[] = {100, 200, 300, 400};
    // size_t dataLength = sizeof(imageData) / sizeof(short int);

    // convertToNetworkByteOrder(imageData, dataLength);

    zmq::context_t context(1);
    zmq::socket_t publisher(context, ZMQ_XPUB);

    std::string timestamp = get_timestamp("");
    logwrite(function, "Sending image data...");

    zmq::message_t header(info_json);

    logwrite(function, "image data section_size: " + std::to_string(info.section_size) + ", axis 0: " + std::to_string(info.axes[0]) + ", axis 1: " + std::to_string(info.axes[1]));
    logwrite(function, "image data first element: " + std::to_string(imageData[0]) + ", second element: " + std::to_string(imageData[1]));

    zmq::message_t payload(info.section_size * sizeof(int16_t));
    memcpy(payload.data(), imageData, info.section_size * sizeof(int16_t));

//    zmq::message_t payload(imageData, info.section_size);
    std::string addr = "tcp://localhost:5555";

    try {
      publisher.bind(addr);
      is_socket_open = true;
      std::cout << "Publisher bound to " << addr << std::endl;
    } catch(const zmq::error_t& e) {
      std::cerr << "Binding failed: " << e.what() << std::endl;
      return ERROR;
    }

    while (true) {
      logwrite(function, "Waiting for subscribers...");
      // XPUB receives subscription messages (start with a byte indicating subscription status)
      zmq::message_t response;
      publisher.recv(response);
      // First byte: 1 is a subscription, 0 is an unsubscription
      if (static_cast<uint8_t*>(response.data())[0] == 1) {
        logwrite(function, "A subscriber has connected.");
        break;
      }
    }

    // Send the message to the server (asynchronously)
    try {
      publisher.send(header, zmq::send_flags::none);
      publisher.send(payload, zmq::send_flags::none);
      logwrite(function, "Message sent successfully.");
    } catch (const zmq::error_t& e) {
      logwrite(function, "Error while sending message: " + std::string(e.what()));
    }

    return NO_ERROR;
  };

  long open(bool writekeys, Camera::Information &info) override;
  long open_socket();

  void close(bool writekeys, Camera::Information &info) override;
  void close_socket();

  bool is_open() override;
};


#endif //WRITE_TO_ZMQ_H
