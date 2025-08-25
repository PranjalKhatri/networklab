#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <unistd.h>
#include "common.hpp"

using namespace std;

#define PORT "5080"
#define BACKLOGS 10
// get socket address IPv4 / IPv6
void *get_in_addr(sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((sockaddr_in *)sa)->sin_addr);
    }
    return &(((sockaddr_in6 *)sa)->sin6_addr);
}

int main()
{

    int sockfd, new_fd; // listening on sockfd and new connection on newfd
    addrinfo hints{}, *servinfo{}, *ptr{};
    hints.ai_family = AF_INET; // both ipv4 and 6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // allows server to bind
    int yes{};
    int rv{};                    // return value
    sockaddr_storage their_addr; // connectors address info
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
    {
        cerr << "getaddrinfo : " << gai_strerror(rv);
    }
    for (ptr = servinfo; ptr != nullptr; ptr = ptr->ai_next)
    {
        if ((sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) == -1)
        {
            clog << "socket error!\n";
            continue; // try next
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            cerr << "sockopt!\n";
            exit(1);
        }
        if (bind(sockfd, ptr->ai_addr, ptr->ai_addrlen) == -1)
        {
            close(sockfd);
            clog << "server: bind!\n";
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);
    if (ptr == nullptr)
    {
        cerr << "failed to bind!\n";
        exit(1);
    }
    if (listen(sockfd, BACKLOGS) == -1)
    {
        cerr << "listen!\n";
        exit(1);
    }
    cout << "server: Listening\n";
    while (1)
    { // main accept() loop
        sin_size = sizeof(their_addr);
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr,
                        &sin_size);
        if (new_fd == -1)
        {
            clog << "accept";
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof(s));
        printf("server: got connection from %s\n", s);

        if (!fork())
        {                  // this is the child process
            close(sockfd); // child doesn't need the listener
            server_handshake(new_fd,"9080");
            close(new_fd);
            exit(0);
        }
        close(new_fd); // parent doesn't need this
    }
}