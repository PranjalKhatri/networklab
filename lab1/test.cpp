#include <iostream>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

void showip(const std::string& host, const std::string& service) {
    addrinfo hints{}, *addr = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host.c_str(), service.c_str(), &hints, &addr);
    if (status != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        return;
    }

    std::cout << "Successfully resolved: " << host << std::endl;
    int i = 0;
    for (addrinfo *ptr = addr; ptr != nullptr; ptr = ptr->ai_next) {
        std::cout << "Response #" << i++ << ":\n";

        // Create a buffer large enough for both IPv4 and IPv6
        char ipstr[INET6_ADDRSTRLEN];
        void *ip_addr = nullptr;

        if (ptr->ai_family == AF_INET) {
            sockaddr_in *ipv4 = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
            ip_addr = &(ipv4->sin_addr);
            inet_ntop(AF_INET, ip_addr, ipstr, sizeof(ipstr));
            std::cout << "  IPv4 address: " << ipstr << std::endl;
        } else if (ptr->ai_family == AF_INET6) {
            sockaddr_in6 *ipv6 = reinterpret_cast<sockaddr_in6*>(ptr->ai_addr);
            ip_addr = &(ipv6->sin6_addr);
            inet_ntop(AF_INET6, ip_addr, ipstr, sizeof(ipstr));
            std::cout << "  IPv6 address: " << ipstr << std::endl;
        } else {
            std::cout << "  Unknown address family.\n";
        }
        std::cout << "\n";
    }

    // Crucial: Free the memory allocated by getaddrinfo
    freeaddrinfo(addr);
}

int main() {
    showip("www.google.com", "http");
    return 0;
}