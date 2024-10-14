//
// Created by mike-langmayr on 10/7/24.
//

#ifndef ZMQPUSHSOCKET_H
#define ZMQPUSHSOCKET_H

class ZmqPushSocket {  // Ensure the class name is correct
public:
  // Constructor with correct name (matches class name) and no return type
  ZmqPushSocket()
   : context(1),
     push_socket(context, zmq::socket_type::push)
  {}

  void send_data(zmq::mutable_buffer message) {
    push_socket.send(message, zmq::send_flags::none);  // Example usage of push_socket
  }

  void connect(std::string uri) {
    push_socket.connect(uri);
  }

private:
  zmq::context_t context;       // zmq context as a class member
  zmq::socket_t push_socket;    // zmq socket as a class member

};

#endif //ZMQPUSHSOCKET_H
