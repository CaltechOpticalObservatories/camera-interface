/**
 * @file    network.cpp
 * @brief   network functions, specifically TCP/IP sockets
 * @details TcpSocket class for client and server communications
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * This contains the class member functions for a TcpSocket class,
 * suitable for both client and server network communications.
 *
 * For a client:
 * set this->host, this->port and call Connect(). 
 *
 * For a server:
 * at minimum, set this->port, this->blocking or instantiate using
 * (port, blocking, totime, id). Call Listen() then Accept(). 
 *
 * For both:
 * Call Read() and Write() to read/write from/to the socket. 
 * Use Poll() to wait for data to be ready to read.
 *
 */

#include "network.h"

namespace Network {


  /**************** Network::TcpSocket::TcpSocket *****************************/
  /**
   * @fn     TcpSocket
   * @brief  TcpSocket class constructor
   * @param  port_in, port which server will listen on
   * @param  block_in, true|false -- will the connection be blocking?
   * @param  totime_in, timeout time for poll, in msec
   * @param  id_in, optional ID number (for keeping track of threads)
   * @return none
   *
   * Use this to construct a server's listening socket object
   *
   */
  TcpSocket::TcpSocket(int port_in, bool block_in, int totime_in, int id_in) {
    this->port = port_in;
    this->blocking = block_in;
    this->totime = totime_in;
    this->id = id_in;
    this->fd = -1;
    this->listenfd = -1;
    this->host = "";
    this->addrs = NULL;
    this->connection_open = false;
  };
  /**************** Network::TcpSocket::TcpSocket *****************************/


  /**************** Network::TcpSocket::TcpSocket *****************************/
  /**
   * @fn     TcpSocket
   * @brief  default TcpSocket class constructor
   * @param  none
   * @return none
   *
   */
  TcpSocket::TcpSocket() {
    this->port = -1;
    this->blocking = false;
    this->totime = POLLTIMEOUT;    //!< default Poll timeout in msec
    this->id = -1;
    this->fd = -1;
    this->listenfd = -1;
    this->host = "";
    this->addrs = NULL;
    this->connection_open = false;
  };
  /**************** Network::TcpSocket::TcpSocket *****************************/


  /**************** Network::TcpSocket::TcpSocket *****************************/
  /**
   * @fn     TcpSocket
   * @brief  TcpSocket class copy constructor
   * @param  obj reference to class object
   * @return none
   *
   */
  TcpSocket::TcpSocket(const TcpSocket &obj) {
    port = obj.port;
    blocking = obj.blocking;
    totime = obj.totime;
    id = obj.id;
    fd = obj.fd;
    listenfd = obj.listenfd;
    host = obj.host;
    addrs = obj.addrs;
    connection_open = &obj.connection_open;
  };
  /**************** Network::TcpSocket::~TcpSocket ****************************/


  /**************** Network::TcpSocket::~TcpSocket ****************************/
  /**
   * @fn     ~TcpSocket
   * @brief  TcpSocket class deconstructor
   * @param  none
   * @return none
   *
   */
  TcpSocket::~TcpSocket() {
    this->Close();
  };
  /**************** Network::TcpSocket::~TcpSocket ****************************/


  /**************** Network::TcpSocket::Accept ********************************/
  /**
   * @fn     Accept
   * @brief  creates a new connected socket for pending connection
   * @param  none
   * @return fd (new connected socket file descriptor) or -1 on error
   *
   * Create new listening socket for pending connection on this->listenfd
   * and returns a new connected socket this->fd
   *
   */
  int TcpSocket::Accept() {
    this->clilen = (socklen_t)sizeof(this->cliaddr);
    this->fd = accept(this->listenfd, (struct sockaddr *) &this->cliaddr, &this->clilen);
    if (this->fd < 0) { perror("(Network::TcpSocket::Accept) error calling accept"); return -1; }
    return (this->fd);
  }
  /**************** Network::TcpSocket::Accept ********************************/


  /**************** Network::TcpSocket::Listen ********************************/
  /**
   * @fn     Listen
   * @brief  create a TCP listening socket
   * @param  none
   * @return listenfd (listening file descriptor), or -1 on error
   *
   * This creates a TCP listening socket that will be used to accept
   * incomming connections.
   *
   */
  int  TcpSocket::Listen() {
    std::string function = "Network::TcpSocket::Listen";

    // create the socket
    //
    if ( (this->listenfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1 ) {
      std::cerr << "(" << function << ") error creating socket: " << strerror(errno) << "\n";
      return(-1);
    }

    // allow re-binding to port while previous connection is in TIME_WAIT
    //
    int on=1;
    setsockopt(this->listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // get address information and bind it to the socket
    //
    struct sockaddr_in servaddr = {0};
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(this->port);

    if ( bind(this->listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
      std::cerr << "(" << function << ") error binding to fd" << this->listenfd << ": " << strerror(errno) << "\n";
      return(-1);
    }

    // start listening
    //
    if (listen(this->listenfd, LISTENQ)!=0) {
      std::cerr << "(" << function << ") error listening to fd" << this->listenfd << ": " << strerror(errno) << "\n";
      return(-1);
    }

    return(this->listenfd);
  }
  /**************** Network::TcpSocket::Listen ********************************/


  /**************** Network::TcpSocket::Poll **********************************/
  /**
   * @fn     Poll
   * @brief  polls a single file descriptor to wait for incoming data to read
   * @param  none
   * @return 0 on timeout, -1 on error, a non-negative value on success
   *
   */
  int TcpSocket::Poll() {
    struct pollfd poll_struct;
    poll_struct.events = POLLIN;
    poll_struct.fd     = this->fd;
    return poll(&poll_struct, 1, this->totime);
  }
  /**************** Network::TcpSocket::Poll **********************************/


  /**************** Network::TcpSocket::Connect *******************************/
  /**
   * @fn     Connect
   * @brief  connect to this->host on this->port
   * @param  none
   * @return 0 on success, -1 on error
   *
   * this->host and this->port need to be specified prior to calling Connect()
   * On success, this function will set this->fd with the open socket file descriptor.
   *
   * getaddrinfo() will allocate memory which must be freed by TcpSocket::Close()
   *
   */
  int TcpSocket::Connect() {
    std::string function = "Network::TcpSocket::Connect";

    struct addrinfo hints = {0};       //!< destination for getaddrinfo

    hints.ai_family = AF_INET;         //!< IPv4 address only
    hints.ai_socktype = SOCK_STREAM ;  //!< non-blocking, connection-based socket
    hints.ai_protocol = IPPROTO_TCP;   //!< TCP socket
    hints.ai_flags = AI_NUMERICSERV;   //!< numeric port number

    // Return one (or more) socket address structures which identify the requested host and port
    // Call TcpSocket::Close() to free memory allocated by this call.
    //
    int status = getaddrinfo(this->host.c_str(), std::to_string(this->port).c_str(), &hints, &this->addrs);

    if ( status != 0 ) {
      std::cerr << "(" << function << ") error connecting to " << this->host << "/" << this->port 
                << " : " << gai_strerror(status) << "\n";
      return(-1);
    }

    // Loop though the results returned by getaddrinfo and attempt to create a socket and connect to it.
    // There should only be one when a host:port is specified but this form is more general.
    //
    for (struct addrinfo *sa = this->addrs; sa != NULL; sa = sa->ai_next) {
      // create the socket, which returns a file descriptor on success
      //
      this->fd = socket(sa->ai_family, sa->ai_socktype, sa->ai_protocol);

      if (this->fd == -1) continue;    // try another entry in the sock addr struct

      // connect to the socket file descriptor
      //
      if ( connect( this->fd, sa->ai_addr, sa->ai_addrlen ) == -1 ) {
        std::cerr << "(" << function << ") error connecting to " << this->host << "/" << this->port << " on fd " << this->fd 
                  << ": " << std::strerror(errno) << "\n";
        return (-1);
      } else break;
    }

    int flags;
    if ((flags = fcntl(this->fd, F_GETFL, 0)) < 0) {
      std::cerr << "(" << function << ") error getting socket file descriptor flags: " << std::strerror(errno) << "\n";
      return(-1);
    }

    flags |= O_NONBLOCK;

    if (fcntl(this->fd, F_SETFL, flags) < 0) {
      std::cerr << "(" << function << ") error setting socket file descriptor flags: " << std::strerror(errno) << "\n";
      return(-1);
    }

    this->connection_open = (this->fd >= 0 ? true : false);

    return (this->fd >= 0 ? 0 : -1);
  }
  /**************** Network::TcpSocket::Connect *******************************/


  /**************** Network::TcpSocket::Close *********************************/
  /**
   * @fn     Close
   * @brief  close a socket connection and free memory allocated by getaddrinfo()
   * @param  none
   * @return 0 on success, -1 on error
   *
   */
  int TcpSocket::Close() {
    int error = -1;
    if (this->fd >= 0) {               // if the file descriptor is valid
      if (close(this->fd) == 0) {      // then close it
        error = 0;
        this->fd = -1;
      }
      else {
        error = -1;                    // error closing file descriptor
     }
    }
    else {
      error = 0;                       // didn't start with a valid file descriptor
    }

    if (this->addrs != NULL) {         // free memory allocated by getaddrinfo()
      freeaddrinfo(this->addrs);
      this->addrs = NULL;
    }

    this->connection_open = false;     // clear the connection_open flag
    return (error);
  }
  /**************** Network::TcpSocket::Close *********************************/


  /**************** Network::TcpSocket::Write *********************************/
  /**
   * @fn     Write
   * @brief  write data to a socket
   * @param  msg_in
   * @return number of bytes written
   *
   */
  int TcpSocket::Write(std::string msg_in) {
    size_t bytes_sent;       // total bytes sent
    int    this_write;       // bytes sent with this write()
    const char* buf = msg_in.c_str();
    bytes_sent = 0;

    while (bytes_sent < msg_in.length()) {
      do
        this_write = write(this->fd, buf, msg_in.length() - bytes_sent);
      while ((this_write < 0) && (errno == EINTR));
      if (this_write <= 0) return this_write;
      bytes_sent += this_write;
      buf += this_write;
    }
    return bytes_sent;
  }
  /**************** Network::TcpSocket::Write *********************************/


  /**************** Network::TcpSocket::Read **********************************/
  /**
   * @fn     Read
   * @brief  read data from connected socket
   * @param  buf, pointer to buffer
   * @param  count, number of bytes to read
   * @return number of bytes read
   *
   */
  int TcpSocket::Read(void* buf, size_t count) {
    return read(this->fd, buf, count);
  }
  /**************** Network::TcpSocket::Read **********************************/


  /**************** Network::TcpSocket::Bytes_ready ***************************/
  /**
   * @fn     Bytes_ready
   * @brief  get the number of bytes available on the socket file descriptor this->fd
   * @param  buf, pointer to buffer
   * @param  count, number of bytes to read
   * @return number of bytes read
   *
   */
  int TcpSocket::Bytes_ready() {
    int bytesready=-1;
    if ( ioctl(this->fd, FIONREAD, &bytesready) < 0 ) {
      perror("(Network::TcpSocket::Bytes_ready) ioctl error");
    }
    return(bytesready);
  }
  /**************** Network::TcpSocket::Bytes_ready ***************************/

}
