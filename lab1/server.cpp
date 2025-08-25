#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <unistd.h>
#include "common.hpp"
#include <thread>
#include <vector>
#include <mutex>
using namespace std;

#define PORT "5080"
#define BACKLOGS 10

struct ClientInfo
{
    std::string ip; // client IP address
    uint16_t port;  // client UDP port
    message msg;    // the Type 3 message payload
    bool done;      // has the job finished?
};

vector<ClientInfo> clients;
mutex clients_mtx;

std::mutex print_mutex;

template <typename... Args>
void ts_print(Args &&...args)
{
    std::lock_guard<std::mutex> lock(print_mutex);
    (std::cout << ... << args);
}

// get socket address IPv4 / IPv6
void *get_in_addr(sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((sockaddr_in *)sa)->sin_addr);
    }
    return &(((sockaddr_in6 *)sa)->sin6_addr);
}

int16_t UDP_PORT = 9080;

void udp_server()
{
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0)
    {
        ts_print("[UDP] Socket creation failed!\n");
        return;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_sock, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        ts_print("[UDP] bind failed\n");
        close(udp_sock);
        return;
    }

    ts_print("[UDP] Listening on ", UDP_PORT, "\n");

    char buf[1024];
    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        ssize_t n = recvfrom(udp_sock, buf, sizeof(buf) - 1, 0,
                             (sockaddr *)&client_addr, &addrlen);
        if (n < 0)
        {
            ts_print("[UDP] recvfrom error\n");
            continue;
        }
        buf[n] = '\0';
        message msg{};
        msg.parseFromBuf(buf, n);

        char ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ipstr, sizeof(ipstr));
        int cport = ntohs(client_addr.sin_port);
        ts_print("udp connection from ", ipstr, "\n");

        {
            lock_guard<mutex> lock(clients_mtx);
            bool known = false;
            for (auto &c : clients)
            {
                // ts_print("comparing ", ipstr, " ", c.ip, "\n");
                if (c.ip == ipstr)
                {
                    c.port = cport; // update UDP port
                    known = true;
                    c.msg.parseFromBuf(buf, n);
                    c.done = true;
                    break;
                }
            }
            if (!known)
            {
                ts_print("[UDP] Got packet from unknown client ", ipstr, ":", cport, " ", msg.print(false), "\n");
            }
            else
            {
                ts_print("[UDP] Got packet from client ", ipstr, ":", cport, " ", msg.print(false), "\n");
                msg.set(msg_type::TYPE_4, "Hi from udp server!\n");
                sendto(udp_sock, buf, sizeof(buf), 0, (sockaddr *)&client_addr, addrlen);
            }
        }
    }
}

void tcp_server()
{
    int sockfd, new_fd;
    addrinfo hints{}, *servinfo{}, *ptr{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int yes{1};
    int rv{};
    sockaddr_storage their_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
    {
        ts_print("[TCP] getaddrinfo : ", gai_strerror(rv), "\n");
    }
    for (ptr = servinfo; ptr != nullptr; ptr = ptr->ai_next)
    {
        if ((sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) == -1)
        {
            ts_print("[TCP] socket error!\n");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            ts_print("[TCP] setsockopt failed!\n");
            exit(1);
        }
        if (bind(sockfd, ptr->ai_addr, ptr->ai_addrlen) == -1)
        {
            close(sockfd);
            ts_print("[TCP] server: bind failed!\n");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);
    if (ptr == nullptr)
    {
        ts_print("[TCP] failed to bind!\n");
        exit(1);
    }
    if (listen(sockfd, BACKLOGS) == -1)
    {
        ts_print("[TCP] listen failed!\n");
        exit(1);
    }
    ts_print("[TCP] Listening on ", PORT, "\n");

    while (1)
    {
        sin_size = sizeof(their_addr);
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            ts_print("[TCP] accept error\n");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof(s));
        ts_print("[TCP] Got connection from ", s, "\n");

        std::thread([new_fd, s]()
                    {
                    if (server_handshake(new_fd, to_string(UDP_PORT).c_str()) < 0) {
                        ts_print("[TCP] Handshake unsuccessful!\n");
                        close(new_fd);
                        return;
                    }
                    {
                        lock_guard<mutex> lock(clients_mtx);
                        // ts_print("pushing\n");
                        clients.push_back({s, 0, {}, false});
                    }
                    close(new_fd); })
            .detach();
    }
}

int main()
{
    thread udp_thread(udp_server);
    thread tcp_thread(tcp_server);

    udp_thread.join();
    tcp_thread.join();
}
