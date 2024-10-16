//
// Created by mike-langmayr on 10/7/24.
//

#ifndef ZMQSOCKET_H
#define ZMQSOCKET_H

class ZmqSocket {  // Ensure the class name is correct
public:
  // Constructor with correct name (matches class name) and no return type
  ZmqSocket()
   : context(1),
     publisher(context, zmq::socket_type::xpub)
  {}

  void send_data(zmq::mutable_buffer message) {
    publisher.send(message, zmq::send_flags::none);  // Example usage of push_socket
  }

  void connect(std::string uri) {
    publisher.bind(uri);
  }

private:
  zmq::context_t context;       // zmq context as a class member
  zmq::socket_t publisher;    // zmq socket as a class member

};

#endif //ZMQSOCKET_H
