#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#include"common.hpp"

using namespace std;

#define PORT "5080"

int main() {
    int sockfd, rv;
    struct addrinfo hints{}, *servinfo, *p;

    // Setup hints for getaddrinfo
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;      // IPv4 (server is AF_INET)
    hints.ai_socktype = SOCK_STREAM; // TCP

    // Resolve localhost:5080
    if ((rv = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) != 0) {
        cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
        return 1;
    }

    // Loop through results and connect
    for (p = servinfo; p != nullptr; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("connect");
            close(sockfd);
            continue;
        }
        break;
    }

    if (p == nullptr) {
        cerr << "client: failed to connect\n";
        return 2;
    }

    freeaddrinfo(servinfo); // Done with address info
    // Receive data
    int bufsz=150;
    char buf[bufsz];
    message st1;
    st1.set(msg_type::TYPE_1,"Hi from client",15);
    client_handshake(sockfd);

    close(sockfd);
    return 0;
}
