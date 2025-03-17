//
// Simple listener.cpp program for UDP multicast
//
// Adapted from:
// http://ntrg.cs.tcd.ie/undergrad/4ba2/multicast/antony/example.html
//
// Changes:
// * Compiles for Linux
// * Takes the port and group on the command line
// * adopted c++ idiomatic practices
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <cstring>
#include <iostream>

constexpr size_t MSGBUFSIZE=256;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "Command line args should be multicast group and port\n";
        std::cout << "e.g. for SSDP, `listener 239.255.255.250 1900`)\n";
        return 1;
    }

    char *group = argv[1]; // e.g. 239.255.255.250 for SSDP
    int port;
    try {
      port = std::stoi(argv[2]);
      if ( port < 1 ) throw std::out_of_range("invalid port number");
    }
    catch (...) {
      std::cerr << "invalid port number\n";
      return 1;
    }

    char *filterstr = nullptr;

    if (argc == 4) filterstr = argv[3];

    // create what looks like an ordinary UDP socket
    //
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        std::cerr << "socket: " << std::strerror(errno) << "\n";
        return 1;
    }

    // allow multiple sockets to use the same PORT number
    //
    u_int yes = 1;
    if (
        setsockopt(
            fd, SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(yes)
        ) < 0
    ) {
        std::cerr << "Reusing ADDR: " << std::strerror(errno) << "\n";
        return 1;
    }

    // set up destination address
    //
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // differs from sender
    addr.sin_port = htons(port);

    // bind to receive address
    //
    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        std::cerr << "bind: " << std::strerror(errno) << "\n";
        return 1;
    }

    // use setsockopt() to request that the kernel join a multicast group
    //
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (
        setsockopt(
            fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof(mreq)
        ) < 0
    ) {
        std::cerr << "setsockopt: " << std::strerror(errno) << "\n";
        return 1;
    }

    // now just enter a read-print loop
    //
    while (1) {
        char msgbuf[MSGBUFSIZE];
        socklen_t addrlen = sizeof(addr);
        int nbytes = recvfrom(
            fd,
            msgbuf,
            MSGBUFSIZE,
            0,
            (struct sockaddr *) &addr,
            &addrlen
        );
        if (nbytes < 0) {
            std::cerr << "recvfrom: " << std::strerror(errno) << "\n";
            return 1;
        }
        msgbuf[nbytes] = '\0';
        if (filterstr != nullptr) {
            if (strstr(msgbuf, filterstr) != nullptr) std::cout << msgbuf << "\n";
        } else std::cout << msgbuf << "\n";
    }

    return 0;
}
