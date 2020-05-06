/**
 * @file    network.h
 * @brief   network functions, specifically TCP/IP sockets
 * @details TcpSocket class definitions for client and server communications
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * This contains the class member definitions for a TcpSocket class,
 * suitable for both client and server network communications.
 *
 */

#ifndef NEWTCPLINUX_H
#define NEWTCPLINUX_H

#include <cstdio>
#include <string>
#include <cstring>
#include <iostream>

#include <sys/ioctl.h>                 // for ioctl, FIONREAD
#include <poll.h>                      // for pollfd
#include <unistd.h>
#include <fcntl.h>

// for addrinfo
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define POLLTIMEOUT 5000               //!< default Poll timeout in msec
#define LISTENQ 64                     //!< listen(3n) backlog 

namespace Network {

  class TcpSocket {

    private:
      int port;
      bool blocking;
      int totime;                      //!< timeout time for poll
      int fd;                          //!< connected socket file descriptor
      int listenfd;                    //!< listening socket file descriptor
      std::string host;
      bool connection_open;

      struct sockaddr_in cliaddr;        //!< socket address structure used for Accept
      socklen_t clilen;                  //!< size of the socket address structure

    public:
      TcpSocket();                       //!< basic class constructor
      TcpSocket(int port_in, bool block_in, int totime_in, int id_in);  //!< useful constructor for a server
      TcpSocket(const TcpSocket &obj);   //!< copy constructor
      ~TcpSocket();                      //!< class destructor

      struct addrinfo *addrs;            //!< dynamically allocated linked list returned by getaddrinfo()

      int id;                            //!< id may be useful for tracking multiple threads, no real requirement here

      int getfd() { return this->fd; };
      bool isblocking() { return this->blocking; };
      bool isconnected() { return this->connection_open; };
      int polltimeout() { return this->totime; };
      void sethost(std::string host_in) { this->host = host_in; };
      std::string gethost() { return this->host; };
      void setport(int port_in) { this->port = port_in; };
      int getport() { return this->port; };

      int Accept();                      //!< creates a new connected socket for pending connection
      int Listen();                      //!< create a TCP listening socket
      int Poll();                        //!< polls a single file descriptor to wait for incoming data to read
      int Connect();                     //!< connect to this->host on this->port
      int Close();                       //!< close a socket connection
      int Write(std::string msg_in);     //!< write data to a socket
      int Read(void* buf, size_t count); //!< read data from connected socket
      int Bytes_ready();                 //!< get the number of bytes available on the socket descriptor this->fd

  };
}
#endif
