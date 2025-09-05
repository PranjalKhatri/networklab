#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// ---------- Message Header ----------
struct MessageHeader
{
    uint64_t send_time_ns;
    uint32_t payload_size;
};

// ---------- Time helper ----------
uint64_t now_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

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
size_t msg_size;
// ---------- TCP Client ----------
void run_tcp(const char *server_ip, int port, size_t total_kb)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return;
    }

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sockfd);
        return;
    }

    if (connect(sockfd, (sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect");
        close(sockfd);
        return;
    }

    size_t total_bytes = total_kb * 1024;
    std::vector<char> buffer(msg_size);
    memset(buffer.data(), 'A', msg_size);

    // ----- Upload -----
    auto start = now_ns();
    size_t sent = 0;
    // std::cout << "will send " << total_bytes << std::endl;
    while (sent < total_bytes)
    {
        MessageHeader hdr{now_ns(), (uint32_t)msg_size};
        std::vector<char> packet(sizeof(hdr) + msg_size);
        memcpy(packet.data(), &hdr, sizeof(hdr));
        memcpy(packet.data() + sizeof(hdr), buffer.data(), msg_size);
        if (send_all(sockfd, packet.data(), packet.size()) <= 0)
            break;
        sent += msg_size;
    }
    // Send DONE
    MessageHeader done{now_ns(), 0};
    send_all(sockfd, (char *)&done, sizeof(done));
    auto end = now_ns();
    double upload_time = (end - start) / 1e9;
    double upload_tp = (total_bytes / 1024.0) / upload_time; // KB/s
    // std::cout << "[TCP] Upload throughput: " << upload_tp << " KB/s\n";

    // ----- Download -----
    size_t received = 0;
    uint64_t first_recv = 0, last_recv = 0;
    std::vector<char> recvbuf(msg_size + sizeof(MessageHeader));
    while (true)
    {
        MessageHeader hdr;
        if (recv_all(sockfd, (char *)&hdr, sizeof(hdr)) <= 0)
            break;
        if (hdr.payload_size == 0)
            break; // DONE
        std::vector<char> payload(hdr.payload_size);
        if (recv_all(sockfd, payload.data(), hdr.payload_size) <= 0)
            break;
        if (first_recv == 0)
            first_recv = now_ns();
        last_recv = now_ns();
        received += hdr.payload_size;
    }
    double dl_time = (last_recv - first_recv) / 1e9;
    // std::cout << "client downloaded" << received << "\n";
    double dl_tp = (received / 1024.0) / dl_time;
#ifdef TXT
    std::cout << received / 1024.0 << " " << dl_tp << "\n";
#else
    std::cout << "[TCP] Download throughput: " << dl_tp << " KB/s\n";
#endif
    close(sockfd);
}

// ---------- UDP Client ----------
void run_udp(const char *server_ip, int port, size_t total_kb)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return;
    }

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sockfd);
        return;
    }

    size_t total_bytes = total_kb * 1024;
    std::vector<char> buffer(msg_size);
    memset(buffer.data(), 'B', msg_size);

    // ----- Upload -----
    auto start = now_ns();
    size_t sent = 0;
    while (sent < total_bytes)
    {
        MessageHeader hdr{now_ns(), (uint32_t)msg_size};
        std::vector<char> packet(sizeof(hdr) + msg_size);
        memcpy(packet.data(), &hdr, sizeof(hdr));
        memcpy(packet.data() + sizeof(hdr), buffer.data(), msg_size);
        sendto(sockfd, packet.data(), packet.size(), 0,
               (sockaddr *)&servaddr, sizeof(servaddr));
        sent += msg_size;
    }
    // Send DONE
    MessageHeader done{now_ns(), 0};
    sendto(sockfd, &done, sizeof(done), 0, (sockaddr *)&servaddr, sizeof(servaddr));
    auto end = now_ns();
    double upload_time = (end - start) / 1e9;
    double upload_tp = (total_bytes / 1024.0) / upload_time;
    // std::cout << "[UDP] Upload throughput: " << upload_tp << " KB/s\n";

    // ----- Download -----
    size_t received = 0;
    uint64_t first_recv = 0, last_recv = 0;
    std::vector<char> recvbuf(msg_size + sizeof(MessageHeader));
    sockaddr_in fromaddr{};
    socklen_t fromlen = sizeof(fromaddr);
    while (true)
    {
        ssize_t n = recvfrom(sockfd, recvbuf.data(), recvbuf.size(), 0,
                             (sockaddr *)&fromaddr, &fromlen);
        if (n <= 0)
            break;
        MessageHeader *hdr = (MessageHeader *)recvbuf.data();
        if (hdr->payload_size == 0)
            break; // DONE
        if (first_recv == 0)
            first_recv = now_ns();
        last_recv = now_ns();
        received += hdr->payload_size;
    }
    double dl_time = (last_recv - first_recv) / 1e9;
    double dl_tp = (received / 1024.0) / dl_time;
#ifdef TXT
    std::cout << received / 1024.0 << " " << dl_tp << "\n";
#else
    std::cout << "[UDP] Download throughput: " << dl_tp << " KB/s\n";
#endif
    close(sockfd);
}

// ---------- Main ----------
int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <tcp|udp> <server_ip> <port> <msg_sz> <total_kb>\n";
        return 1;
    }

    std::string mode = argv[1];
    const char *server_ip = argv[2];
    int port = std::stoi(argv[3]);
    size_t total_kb = std::stoul(argv[5]);
    msg_size = std::stoul(argv[4]) * 1024;
    // Prompt for message size
    if (mode == "tcp")
    {
        // std::cout << "tcp run" << std::endl;
        run_tcp(server_ip, port, total_kb);
    }
    else if (mode == "udp")
    {
        run_udp(server_ip, port, total_kb);
    }
    else
    {
        std::cerr << "Invalid mode: use tcp or udp\n";
        return 1;
    }
    return 0;
}