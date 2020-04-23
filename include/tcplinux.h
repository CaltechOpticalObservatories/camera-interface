/**
 * @file     p3k/aoca/include/tcplinux.h
 * @brief    include file for tcplinux functions
 * @author   David Hale <dhale@astro.caltech.edu>
 * @date     2009-02-03
 * @modified 2010-05-14
 * 
 */
#ifndef TCPLINUX_H
#define TCPLINUX_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <poll.h>
#include <sys/un.h>
#include <signal.h>                    /*!< for sigaction, catching SIGPIPE */

#include <sys/resource.h>              /*!< for wait3(2) */
#include <sys/wait.h>                  /*!< for wait3(2) */
#include <sys/ioctl.h>                 /*!< for fion_read */
#include <ctype.h>
#include <limits.h>

/*---------------------------------------------------------------------------
| Definitions
|----------------------------------------------------------------------------*/

#ifndef _BSD_SOURCE
#define _BSD_SOURCE          /*!< provides prototype for wait3() and friends */
#endif

#ifndef  TCP_ERROR
#define  TCP_ERROR    -1
#endif

#define  LISTENQ       64              /*!< listen(3n) backlog */
#ifndef  ENDCHAR
#define  ENDCHAR       '\0'
#endif


int  tcp_listen(int port);
int  Poll(int fd, int to);
int  Accept(int fd, struct sockaddr *sa, socklen_t *slptr);
void ms_pause(int ms);
int  Tcp_listen(const char *host, const char *serv, socklen_t *addrlenp);
int  connect_to_server(const char *host, int port);
int  sock_puts(int sockfd, char *str);
int  sock_rbputs(int sockfd, char *str);
size_t sock_gets(int sockfd, char *str, size_t count);
int  sock_write(int sockfd, char *buf);
int  fion_read(int fd);
void ignore_sigpipe(void);
int  get_connection(int socket_type, unsigned short port, int *listener);
int  atoport(char *service, char *proto);

#endif
