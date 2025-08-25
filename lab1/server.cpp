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

#define BACKLOGS 10

struct ClientInfo
{
    enum class State
    {
        NOT_ARRIVED,
        ARRIVED,
        DONE
    };
    uint16_t socket;
    std::string ip; // client IP address
    uint16_t port;  // client UDP port
    message msg;    // the Type 3 message payload
    State state = ClientInfo::State::NOT_ARRIVED;
    sockaddr_in addr;
    socklen_t addrlen;
};

vector<ClientInfo> clients;
mutex clients_mtx;

std::mutex print_mutex;
string ack_msg = "ACK FROM SERVER!!";
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

uint16_t UDP_PORT = 9080;

int udp_for_client(std::string ip, uint16_t udp_port)
{
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0)
    {
        ts_print("[UDP] Socket creation failed for client ", ip, "\n");
        return -1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(udp_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_sock, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        ts_print("[UDP] bind failed on port ", udp_port, "\n");
        close(udp_sock);
        return -1;
    }

    ts_print("[UDP] Dedicated UDP server for ", ip, " on port ", udp_port, "\n");

    char buf[1024];
    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        ssize_t n = recvfrom(udp_sock, buf, sizeof(buf) - 1, 0,
                             (sockaddr *)&client_addr, &addrlen);
        if (n < 0)
        {
            ts_print("[UDP] recvfrom error for ", ip, "\n");
            continue;
        }

        message msg{};
        msg.parseFromBuf(buf, n);

        {
            std::lock_guard<std::mutex> lock(clients_mtx);
            for (auto &i : clients)
            {
                if (i.ip == ip && i.state == ClientInfo::State::NOT_ARRIVED)
                {
                    i.msg = msg;
                    i.port = ntohs(client_addr.sin_port);
                    i.addr = client_addr;
                    i.addrlen = addrlen;
                    i.socket = udp_sock; // set FD before ARRIVED
                    i.state = ClientInfo::State::ARRIVED;
                    return 0; // do NOT close udp_sock here, FCFS will
                }
            }
        }
        ts_print("No client for ip ", ip);
        return -1;
    }
}

int udp_send_and_close(int udp_sock, const std::string& ack_msg,
                       const sockaddr_in& client_addr, socklen_t addrlen) {
    message ack{};
    ack.set(msg_type::TYPE_4, ack_msg.c_str());      // <-- pass c_str + size
    char buf[sizeof(int32_t)*2 + MSG_LEN];
    int n = ack.printToBuf(buf, sizeof(buf));
    if (n < 0) return -1;

    ssize_t sent = sendto(udp_sock, buf, n, 0,
                          (const sockaddr*)&client_addr, addrlen);
    if (sent < 0) { perror("sendto failed"); return -1; }

    close(udp_sock);
    return 0;
}


void fcfs()
{
    size_t cur = 0;
    for (;;)
    {
        std::unique_lock<std::mutex> lock(clients_mtx);
        if (cur < clients.size() && clients[cur].state == ClientInfo::State::ARRIVED)
        {
            auto cli = clients[cur]; // copy the info you need
            lock.unlock();           // release lock while sending

            udp_send_and_close(cli.socket, ack_msg, cli.addr, cli.addrlen);

            lock.lock();
            clients[cur].state = ClientInfo::State::DONE;
            ts_print(clients[cur].msg.print(false),"\n");
            ++cur;
        }
        else
        {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // tiny backoff
        }
    }
}

void tcp_server(const char* PORT)
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
                        clients.push_back(ClientInfo{0,std::string(s), 0, message{}, ClientInfo::State::NOT_ARRIVED});

                        thread client_thread([s, port=UDP_PORT++]() {
                            udp_for_client(std::string(s), port);
                        });
                        client_thread.detach();
                    }
                    close(new_fd); })
            .detach();
    }
}

int main(int argc,char**argv)
{
    if(argc != 2){
        cerr<<"USAGE: .\\server [PORT]\n";
        return 1;
    }
    // thread udp_thread(udp_server);
    thread tcp_thread(tcp_server,argv[1]);
    thread fcfs_thread(fcfs);
    // udp_thread.join();
    fcfs_thread.join();
    tcp_thread.join();
}
