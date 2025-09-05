#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <chrono>
#include <vector>
// ---------- Message Header ----------
// The same header struct is used on both client and server
struct MessageHeader
{
    uint64_t send_time_ns;
    uint32_t payload_size; // 0 => DONE
};

// ---------- Time helper ----------
uint64_t now_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

// ---------------- TCP ----------------
// Helper to ensure all bytes are sent
ssize_t send_all(int sock, const char *buffer, size_t len)
{
    size_t total_sent = 0;
    while (total_sent < len)
    {
        ssize_t n = send(sock, buffer + total_sent, len - total_sent, 0);
        if (n <= 0)
            return n;
        total_sent += n;
    }
    return total_sent;
}

// Helper to ensure all bytes are received
ssize_t recv_all(int sock, char *buffer, size_t len)
{
    size_t total_received = 0;
    while (total_received < len)
    {
        ssize_t n = recv(sock, buffer + total_received, len - total_received, 0);
        if (n <= 0)
            return n;
        total_received += n;
    }
    return total_received;
}

void tcp_server(int port, size_t msg_size, size_t total_kb)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return;
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind");
        close(server_fd);
        return;
    }
    if (listen(server_fd, 1) < 0)
    {
        perror("listen");
        close(server_fd);
        return;
    }
#ifndef TXT
    std::cout << "[TCP] Waiting for connection on port " << port << "...\n";
#endif
    int sock = accept(server_fd, nullptr, nullptr);
    if (sock < 0)
    {
        perror("accept");
        close(server_fd);
        return;
    }

    char *buffer = new char[msg_size + sizeof(MessageHeader)];
    uint64_t first_send_time = 0, last_arrival_time = 0;
    size_t total_payload = 0;

    // ---- Receive upload ----
    while (true)
    {
        MessageHeader hdr;
        if (recv_all(sock, (char *)&hdr, sizeof(hdr)) <= 0)
            break;
        if (hdr.payload_size == 0)
            break; // DONE

        std::vector<char> payload(hdr.payload_size);
        if (recv_all(sock, payload.data(), hdr.payload_size) <= 0)
            break;

        if (first_send_time == 0)
            first_send_time = now_ns();
        // if (first_send_time == 0) first_send_time = hdr.send_time_ns;
        last_arrival_time = now_ns();
        total_payload += hdr.payload_size;
    }

    double dur = (last_arrival_time - first_send_time) / 1e9;
#ifdef TXT
    std::cout << total_payload / 1024.0 << " " << (total_payload / 1024.0) / dur << "\n";
#else
    std::cout << "[TCP] Upload: " << total_payload / 1024.0
              << " KB in " << dur << "s => "
              << (total_payload / 1024.0) / dur << " KB/s\n";
#endif

    // ---- Send download ----
    size_t total_bytes = total_kb * 1024;
    size_t sent = 0;
    std::vector<char> send_buffer(msg_size + sizeof(MessageHeader));
    while (sent < total_bytes)
    {
        MessageHeader hdr{now_ns(), (uint32_t)msg_size};
        memcpy(send_buffer.data(), &hdr, sizeof(hdr));
        memset(send_buffer.data() + sizeof(hdr), 'X', msg_size);
        if (send_all(sock, send_buffer.data(), sizeof(hdr) + msg_size) <= 0)
            break;
        sent += msg_size;
    }
    // send DONE
    MessageHeader done{now_ns(), 0};
    send_all(sock, (char *)&done, sizeof(done));

    close(sock);
    close(server_fd);
    delete[] buffer;
}

// ---------------- UDP ----------------
void udp_server(int port, size_t msg_size, size_t total_kb)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return;
    }

    sockaddr_in addr{}, client{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(sock);
        return;
    }
    socklen_t clen = sizeof(client);

    char *buffer = new char[msg_size + sizeof(MessageHeader)];
    uint64_t first_send_time = 0, last_arrival_time = 0;
    size_t total_payload = 0;

    // ---- Receive upload ----
    while (true)
    {
        ssize_t n = recvfrom(sock, buffer, msg_size + sizeof(MessageHeader), 0,
                             (sockaddr *)&client, &clen);
        if (n <= 0)
            break;

        MessageHeader *hdr = (MessageHeader *)buffer;
        if (hdr->payload_size == 0)
            break; // DONE
        if (first_send_time == 0)
            first_send_time = now_ns();
        // if (first_send_time == 0) first_send_time = hdr->send_time_ns;
        last_arrival_time = now_ns();
        total_payload += hdr->payload_size;
    }

    double dur = (last_arrival_time - first_send_time) / 1e9;
#ifdef TXT
    std::cout << total_payload / 1024.0 << " " << (total_payload / 1024.0) / dur << "\n";
#else
    std::cout << "[UDP] Upload: " << total_payload / 1024.0
              << " KB in " << dur << "s => "
              << (total_payload / 1024.0) / dur << " KB/s\n";
#endif

    // ---- Send download ----
    size_t total_bytes = total_kb * 1024;
    size_t sent = 0;
    while (sent < total_bytes)
    {
        MessageHeader hdr{now_ns(), (uint32_t)msg_size};
        memcpy(buffer, &hdr, sizeof(hdr));
        memset(buffer + sizeof(hdr), 'X', msg_size);
        sendto(sock, buffer, sizeof(hdr) + msg_size, 0,
               (sockaddr *)&client, clen);
        sent += msg_size;
    }
    // send DONE
    MessageHeader done{now_ns(), 0};
    sendto(sock, &done, sizeof(done), 0, (sockaddr *)&client, clen);

    close(sock);
    delete[] buffer;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        std::cerr << "Usage: ./server tcp|udp port msg_size_kb total_kb\n";
        return 1;
    }
    std::string mode = argv[1];
    int port = std::stoi(argv[2]);
    size_t msg_size = std::stoul(argv[3]) * 1024;
    size_t total_kb = std::stoul(argv[4]);

    if (mode == "tcp")
        tcp_server(port, msg_size, total_kb);
    else if (mode == "udp")
        udp_server(port, msg_size, total_kb);
    else
    {
        std::cerr << "Invalid mode: use tcp or udp\n";
        return 1;
    }
    return 0;
}