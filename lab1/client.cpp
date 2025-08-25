#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#include "common.hpp"

using namespace std;

#define PORT "5080"

int udp_conv(int server_port, const char *server_ip)
{
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0)
    {
        perror("socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    message msg{};
    msg.set(msg_type::TYPE_3, "Hello from UDP client!");
    char buf[MSG_LEN];
    int n = msg.printToBuf(buf, sizeof buf);
    cout<<"msg"<<msg.length<<msg.message<<"\n";
    ssize_t sent = sendto(udp_sock, buf, n, 0,
                          (sockaddr *)&server_addr, sizeof(server_addr));
    if (sent < 0)
    {
        perror("sendto");
    }
    else
    {
        cout << "Sent " << sent << " bytes over UDP" << endl;
    }

    socklen_t addrlen = sizeof(server_addr);
    n = recvfrom(udp_sock, buf, sizeof(buf) - 1, 0,
                         (sockaddr *)&server_addr, &addrlen);

    if (n >= 0)
    {
        msg.parseFromBuf(buf,n);
        cout << "Received: " << buf << endl;
    }

    close(udp_sock);
    return 0;
}
int tcp_handshake()
{
    int sockfd, rv;
    struct addrinfo hints{}, *servinfo, *p;

    // Setup hints for getaddrinfo
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // IPv4 (server is AF_INET)
    hints.ai_socktype = SOCK_STREAM; // TCP

    // Resolve localhost:5080
    if ((rv = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) != 0)
    {
        cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
        return 1;
    }

    // Loop through results and connect
    for (p = servinfo; p != nullptr; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("connect");
            close(sockfd);
            continue;
        }
        break;
    }

    if (p == nullptr)
    {
        cerr << "client: failed to connect\n";
        return 2;
    }

    freeaddrinfo(servinfo); // Done with address info
    // Receive data
    int bufsz = 150;
    char buf[bufsz];
    message st1;
    st1.set(msg_type::TYPE_1, "Hi from client");

    rv = client_handshake(sockfd);
    close(sockfd);
    return rv;
}
int main()
{
    int port = tcp_handshake();
    sleep(1);
    udp_conv(port, "localhost");

    return 0;
}
