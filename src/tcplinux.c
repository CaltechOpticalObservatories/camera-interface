/** ---------------------------------------------------------------------------
 * @file     p3k/aoca/src/tcplinux.c
 * @brief    generic TCP/IP communications functions for Linux (not nesC)
 * @author   David Hale, CIT
 * @date     2008-08-18
 * @modified extensively (see below)
 * 
 * Modification History
 * --------------------
 * 2008-08-18 dsh  created
 * 2008-11-03 dsh  add error check to bind()
 *     -11-04 dsh  fixed Poll(), removed SO_KEEPALIVE option
 * 2009-01-16 dsh  added timeout arg to Poll()
 *     -03-23 dsh  added Tcp_listen
 *     -07-01 dsh  added some new wrappers
 * 2010-01-22 dsh  always return -1 on error (a few places didn't do this)
 * 2010-05-14 dsh  change bzero -> memset
 * 2010-10-13 dsh  fixed possible bug in ms_pause
 * 2010-10-18 dsh  bug fixes
 * 
 */

#include "tcplinux.h"

/** ---------------------------------------------------------------------------
 * @fn     int tcp_listen()
 * @brief  create socket and listen for connections on that socket
 * @param  port
 * @return socket file descriptor, or -1 on error
 *
 * Create bidirectional socket byte stream, bind to that socket,
 * and listen for connections on that socket
 *
 */
int tcp_listen(int port)
  {
  struct sockaddr_in  servaddr;
  int                 sockfd;
  int                 on=1;

  /**
   * get file descriptor for listening socket
   *
   */
  if ( (sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == ERROR)
    {
    fprintf(stderr, "tcp_listen: error %d creating socket: %s\n", errno,
                    strerror(errno));
    return(-1);
    }

  /**
   * allow re-binding to port while previous connection is in TIME_WAIT
   *
   */
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  /**
   * get address information and bind it to the socket
   *
   */
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family      = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port        = htons(port);

  if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))<0)
    {
    fprintf(stderr, "tcp_listen: error %d binding to %d: %s\n", errno, sockfd, 
                     strerror(errno));
    return(-1);
    }

  if (listen(sockfd, LISTENQ)!=0)
    {
    fprintf(stderr, "tcp_listen: error %d calling listen on fd %d: %s\n", errno,
                    sockfd, strerror(errno));
    return(-1);
    }

  return(sockfd);
  }

/** ---------------------------------------------------------------------------
 * @fn     Tcp_listen(const char *host, const char *serv, socklen_t *addrlenp)
 * @brief  Listen for connection on host:service
 * @param  host NULL pointer or NULL-terminated string, either valid host
 *              name or numeric host dotted decimal address string.
 * @param  serv either decimal port number or a name listed in services(5).
 * @return open file descriptor, or -1 on error
 *
 * This function creates a bidirectional socket byte stream, binds to that
 * socket, and listens for a connection on that socket.
 *
 */
int Tcp_listen(const char *host, const char *serv, socklen_t *addrlenp)
  {
  int             listenfd, n;
  const int       on=1;
  struct addrinfo hints, *res, *ressave;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags    = AI_PASSIVE;
  hints.ai_family   = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ( (n = getaddrinfo(host, serv, &hints, &res)) != 0)
    printf("error for %s, %s: %s\n", host, serv, gai_strerror(n));

  ressave = res;

  do{
    listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listenfd == -1)
      continue;

    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
      break;

    close(listenfd);
    } while ( (res = res->ai_next) != NULL);

  if (res == NULL) fprintf(stderr, "tcp listen error for %s, %s", host, serv);

  if (listen(listenfd, LISTENQ)==ERROR) perror("listen");

  if (addrlenp)
    *addrlenp = res->ai_addrlen;

  freeaddrinfo(ressave);

  return(listenfd);
  }

/** ---------------------------------------------------------------------------
 * @fn      connect_to_server(*host, port)
 * @brief   opens, connects to a given host/port and returns a file descriptor
 * @param   host
 * @param   port
 * @returns int file descriptor, or -1 on error
 *
 */
int connect_to_server(char *host, int port)
  {
  struct sockaddr_in  servaddr;
  struct hostent     *hostPtr=NULL;
  int  sockfd, flags;

  if ( (sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == ERROR)
    {
    fprintf(stderr, "connect_to_server: error %d creating socket: %s\n",
                     errno, strerror(errno));
    return(-1);
    }

  if ((hostPtr=gethostbyname(host))==NULL)
    if ((hostPtr=gethostbyaddr(host, strlen(host), AF_INET))==NULL)
      {
      fprintf(stderr, "error resolving host %s\n", host);
      return(-1);
      }

  servaddr.sin_family = AF_INET;
  servaddr.sin_port   = htons(port);
  (void) memcpy(&servaddr.sin_addr, hostPtr->h_addr, hostPtr->h_length);

  if ( (connect(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr)))==ERROR)
    {
    fprintf(stderr, "connect_to_server: error %d:%s connecting to %s:%d\n",
                    errno, strerror(errno), host, port);
    return(-1);
    }

  if ((flags = fcntl(sockfd, F_GETFL, 0)) < 0)
    {
    fprintf(stderr, "F_GETFL error\n");
    return(-1);
    }

  flags |= O_NONBLOCK;

  if (fcntl(sockfd, F_SETFL, flags) < 0)
    {
    fprintf(stderr, "F_SETFL error\n");
    return(-1);
    }

  return(sockfd);
  }

/** ---------------------------------------------------------------------------
 * @fn     Poll(fd, to)
 * @brief  polls a single file descriptor to wait for incoming data to read
 * @param  fd a file descriptor to poll
 * @param  to the timeout
 * @return 0 on timeout, -1 on error, a non-negative value on success
 *
 */
int Poll(int fd, int to)
  {
  struct pollfd poll_struct;
  poll_struct.events = POLLIN;
  poll_struct.fd     = fd;
  return poll(&poll_struct, 1, to);
  }

/** ---------------------------------------------------------------------------
 * @fn     ms_pause(ms)
 * @brief  pause indicated number of milliseconds
 * @param  ms number of milliseconds to pause
 * @return nothing
 *
 */
void ms_pause(int ms) {
  struct pollfd poll_struct;
  poll_struct.fd=0;
  poll_struct.events=0;
  poll_struct.revents=0;
  poll(&poll_struct, 1, ms);
  }

/** ---------------------------------------------------------------------------
 * @fn     sock_rbputs(sockfd, *str)
 * @brief  Raw Byte writes a character string to a socket
 * @param  sockfd an open socket file descriptor
 * @param *str pointer to a buffer containing data
 * @return -1 if connection is closed while trying to write
 * @see    sock_puts()
 *
 * This is similar to sock_puts except it writes all bytes in a buffer,
 * up to and including the terminating null character.
 *
 */
int sock_rbputs(int sockfd, char *str)
  {
  int wrote,i=0;
  while (str[i]!='\0') if ((wrote=write(sockfd,&str[i++],1))<=0) return wrote;
  if ((wrote=write(sockfd,&str[i],1))<=0) return wrote;
  return i;
  }

/** ---------------------------------------------------------------------------
 * @fn     sock_puts(sockfd, *str)
 * @brief  writes a character string to a socket
 * @param  sockfd an open socket file descriptor
 * @param *str pointer to a buffer containing data
 * @return -1 if connection is closed while trying to write
 * @see    sock_write()
 *
 */
int sock_puts(int sockfd, char *str)
  {
  return sock_write(sockfd, str, strlen(str));
  }

/** ---------------------------------------------------------------------------
 * @fn     sock_gets(sockfd, str, count)
 * @brief  read from socket until linefeed reached, or up to count bytes
 * @param  sockfd an open socket file descriptor
 * @param *str pointer to a buffer containing data
 * @param  count max num of bytes to read
 * @return -1 if connection is closed while trying to write
 *
 */
size_t sock_gets(int sockfd, char *str, size_t count)
  {
  int    bytes_read;
  size_t total=0;
  char    buf=0;

  if (count>0) {               /** hopefully asked to read more than 0 bytes */
    do {
      if ((bytes_read=read(sockfd, &buf, 1))<1) return(-1);   /** sets errno */

      /**
       * always increment total at least once, so this counts the linefeed
       * as a byte read.
       */
      if (total++<count) {
        str[0]=buf;
        str++;
        }
printf("buf=%x\n",buf);
      } while ((total<count) && (buf!='\n') ); // && (buf!='\r'));
    }
  else str[0]=0;    /** set return buffer NULL if not asked to read anything */
  return(total);
  }

/** ---------------------------------------------------------------------------
 * @fn     sock_write(sockfd, *buf, count)
 * @brief  writes data to a socket
 * @param  sockfd an open socket file descriptor
 * @param *buf pointer to a buffer containing data
 * @param  count number of bytes in buffer to write
 * @return int number of bytes written to socket
 *
 */
int sock_write(int sockfd, char *buf, size_t count)
  {
  size_t bytes_sent = 0;
  int    this_write;

  while (bytes_sent < count) {
    do
      this_write = write(sockfd, buf, count - bytes_sent);
    while ((this_write < 0) && (errno == EINTR));
    if (this_write <= 0) return this_write;
    bytes_sent += this_write;
    buf += this_write;
    }
  return count;
  }

/** ---------------------------------------------------------------------------
 * @fn     Accept(fd, *sa, *slptr)
 * @brief  wrapper for POSIX accept(P) function
 * @param  fd an open socket file descriptor
 * @param *sa struct sockaddr pointer
 * @param *slptr socklen_t pointer
 * @return int file descriptor of accepted socket
 *
 */
int Accept(int fd, struct sockaddr *sa, socklen_t *slptr)
  {
  int n;
again:
  if ((n=accept(fd, sa, slptr))<0) {
#ifdef EPROTO
    if (errno==EPROTO || errno==ECONNABORTED)
#else
    if (errno==ECONNABORTED)
#endif
      goto again;
    else
      perror("Accept error.");
    }
  return(n);
  }
  
/** ---------------------------------------------------------------------------
 * @fn     ignore_sigpipe()
 * @brief  ignore SIGPIPE signal
 * @param  none
 * @return none
 *
 */
void sig_pipe(int sig_type) {
  fprintf(stderr,"in sig_pipe, type=%d\n", sig_type);
  }
void ignore_sigpipe(void)
  {
  struct sigaction sig;
  sig.sa_handler=SIG_IGN;    /** specifies that the signal should be ignored */
  sig.sa_flags=0;
  sigemptyset(&sig.sa_mask);
  if (sigaction(SIGPIPE,&sig,NULL)) perror("ignore_sigpipe");
/*
  struct sigaction act, oldact;
  sigemptyset(&act.sa_mask);
  act.sa_flags=0;
  act.sa_handler=sig_pipe;
  sigaction(SIGPIPE,&act,&oldact);
*/
/*signal(SIGPIPE, sig_pipe);*/
  }

/** ---------------------------------------------------------------------------
 * @fn     get_connection(type, port, listener)
 * @brief  listens on a port and returns connections, forking for each.
 * @param  type
 * @param  port
 * @param  listener
 * @return int socket or -1 on error
 *
 * This function forks internally and creates a new process for each
 * incomming connection.
 *
 */
int get_connection(int socket_type, u_short port, int *listener)
  {
  struct sockaddr_in address;
  int    listening_socket;
  int    connected_socket=-1;
  int    new_process;
  int    on=1;

  ignore_sigpipe();

  /**
   * construct internet address
   */
  memset((char *) &address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = port;
  address.sin_addr.s_addr = htonl(INADDR_ANY);

  listening_socket = socket(AF_INET, socket_type, 0);
  if (listening_socket < 0) {
    perror("socket");
    return(-1);
    }

  if (listener != NULL) *listener = listening_socket;

  setsockopt(listening_socket,SOL_SOCKET,SO_REUSEADDR,(void *)&on,sizeof(on));

  if (bind(listening_socket, (struct sockaddr *) &address,
    sizeof(address)) < 0) {
    perror("bind");
    close(listening_socket);
    return(-1);
    }

  if (socket_type==SOCK_STREAM) {
    listen(listening_socket, LISTENQ);
    while(connected_socket < 0) {
      connected_socket = accept(listening_socket, NULL, NULL);
      if (connected_socket < 0) {
        /**
         * Either a real error occured, or blocking was interrupted for
         * some reason.  Only abort execution if a real error occured. 
         */
        if (errno != EINTR) {
          perror("accept");
          close(listening_socket);
          return(-1);
          } 
        else
          continue;                    /** don't fork - do the accept again */
        }

      new_process = fork();
      if (new_process < 0) {
        perror("fork");
        close(connected_socket);
        connected_socket = -1;
        }
      else {                           /** We have a new process... */
        if (new_process==0) {
          /**
           * This is the new process.
           */
          close(listening_socket);     /** Close our copy of this socket */
          if (listener != NULL)
            *listener = -1;            /** Closed in this process.           */
          }                            /** We are not responsible for it.    */
        else {
          /**
           * This is the main loop.  Close copy of connected socket,
           * and continue loop. 
           */
          close(connected_socket);
          connected_socket = -1;
          }
        }
      }                              /** end while(connected_socket<0) */
    return connected_socket;
    }                                /** end if (socket_type == SOCK_STREAM) */
  else
    return listening_socket;
  }

/** ---------------------------------------------------------------------------
 * @fn     atoport(service, proto)
 * @brief  returns a port number from given service name and type
 * @param  service
 * @param  proto
 * @return int port
 *
 */
int atoport(char *service, char *proto)
  {
  struct  servent *serv;
  char   *errpos;
  long    int lport;
  int     port;
  /**
   * first try /etc/services
   */
  if ((serv=getservbyname(service,proto))!=NULL) port=serv->s_port;
  else {
    /*
     * not in /etc/services ... try as a decimal number
     */
    lport=strtol(service,&errpos,0);
    if ((errpos[0]!=0) || (lport<1) || (lport>65535)) return -1; /* invalid */
    port=htons(lport);
    }
  return(port);
  }

