//
// Created by mike-langmayr on 10/7/24.
//

#ifndef ZMQSOCKET_H
#define ZMQSOCKET_H
#include <set>

class ZmqSocket {  // Ensure the class name is correct
public:
  // Constructor with correct name (matches class name) and no return type
  ZmqSocket() = default;

  void unbind(std::string const& addr) {
    try {
      publisher.unbind(addr);
      boundAddresses.erase(addr);
      std::cout << "Unbound from " << addr << std::endl;
    } catch (const zmq::error_t& e) {
      std::cerr << "Unbinding failed: " << e.what() << std::endl;
    }
  }

  void unbindAll() {
    for (const auto& addr : boundAddresses) {
      unbind(addr);
    }
  }

  void bind(std::string const& addr) {
    try {
      logwrite("ZmqSocket::bind", "Binding to zmq socket: " + addr);
      publisher.bind(addr);
      logwrite("ZmqSocket::bind", "Publisher bound to: " + addr);
      boundAddresses.insert(addr);
      logwrite("ZmqSocket::bind", "Publisher bound to: " + addr);
    } catch(const zmq::error_t& e) {
      std::cerr << "Binding failed: " << e.what() << std::endl;
    }
  }

  void waitForSubscribers() {
    std::cout << "Waiting for subscribers..." << std::endl;
    while (true) {
      // XPUB receives subscription messages (start with a byte indicating subscription status)
      zmq::message_t message;
      publisher.recv(message);
      // First byte: 1 is a subscription, 0 is an unsubscription
      if (static_cast<uint8_t*>(message.data())[0] == 1) {
        std::cout << "A subscriber has connected." << std::endl;
        break;
      }
    }
  }

  void send(const string& message) {
    waitForSubscribers();

    logwrite("ZmqSocket::send", "Sending message: " + message);
    zmq::message_t payload(message);
    try {
      publisher.send(payload, zmq::send_flags::none);
      std::cout << 'Sent message: ' << payload << std::endl;
    } catch(const zmq::error_t& e) {
      std::cerr << "Send failed: " << e.what() << std::endl;
    }
  }

private:
  zmq::context_t context;
  zmq::socket_t publisher;
  std::pmr::set<std::string> boundAddresses;
};

#endif //ZMQSOCKET_H
