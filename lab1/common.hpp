#include <string>
#include <cstdint>
#include <cstring>     // for memcpy
#include <arpa/inet.h> // for htonl, ntohl
#define MSG_LEN 150
using namespace std;
enum class msg_type : int32_t
{
    TYPE_1 = 1,
    TYPE_2,
    TYPE_3,
    TYPE_4
};
struct message
{
    msg_type type;
    int32_t length;
    char message[MSG_LEN];
    int set(msg_type tp, string_view msg)
    {
        int sz = msg.size();
        if (sz > MSG_LEN)
            return -1;
        length = sz;
        type = tp;
        for (int i = 0; i < sz; i++)
        {
            message[i] = msg[i];
        }
        return 0;
    }
    int printToBuf(char *buf, int sz) const
    {
        if (length > MSG_LEN)
            return -2; // invalid length

        int req = sizeof(type) + sizeof(length) + length;
        if (sz < req)
        {
            return -1; // not enough space
        }

        // convert to network byte order
        int32_t net_type = htonl(static_cast<underlying_type_t<msg_type>>(type));
        int32_t net_len = htonl(length);

        memcpy(buf, &net_type, sizeof(net_type));
        memcpy(buf + sizeof(net_type), &net_len, sizeof(net_len));
        memcpy(buf + sizeof(net_type) + sizeof(net_len), message, length);

        return req; // number of bytes written
    }

    int parseFromBuf(const char *buf, int sz)
    {
        if (sz < (int)(sizeof(type) + sizeof(length)))
            return -1;

        memcpy(&type, buf, sizeof(type));
        memcpy(&length, buf + sizeof(type), sizeof(length));

        type = static_cast<msg_type>(ntohl(static_cast<underlying_type_t<msg_type>>(type)));
        length = ntohl(length);

        if (length > MSG_LEN || sz < (int)(sizeof(type) + sizeof(length) + length))
        {
            return -2; // invalid size
        }

        memcpy(message, buf + sizeof(type) + sizeof(length), length);
        message[length] = '\0'; // null terminate for safety

        return 0;
    }

    string print(bool print = true)
    {

        string res = "[type=" + to_string((int)type) +
                     ", length=" + to_string(length) +
                     ", message=" + std::string(message, length) + "]";

        if (print)
        {
            cout<<res;
        }
        return res;
    }
};
// ---- Handshake helpers ----

// send a message over a TCP socket
inline int send_message(int sockfd, const message &msg)
{
    char buf[sizeof(int32_t) * 2 + MSG_LEN];
    int n = msg.printToBuf(buf, sizeof(buf));
    if (n < 0)
        return n;

    int sent = send(sockfd, buf, n, 0);
    return (sent == n) ? 0 : -1;
}

// receive a message over a TCP socket
inline int recv_message(int sockfd, message &msg)
{
    char buf[sizeof(int32_t) * 2 + MSG_LEN];
    int n = recv(sockfd, buf, sizeof(buf), 0);
    if (n <= 0)
        return -1; // connection closed or error
    return msg.parseFromBuf(buf, n);
}

// client handshake: send HELLO, expect WELCOME
inline int client_handshake(int sockfd)
{
    message msg;
    msg.set(msg_type::TYPE_1, "");
    if (send_message(sockfd, msg) < 0)
        return -1;

    if (recv_message(sockfd, msg) < 0)
        return -1;
    if (msg.type != msg_type::TYPE_2)
        return -2;
    return atoi(msg.message);
}

// server handshake: expect HELLO, reply WELCOME
inline int server_handshake(int sockfd, const char *UDP_PORT)
{
    message msg;
    if (recv_message(sockfd, msg) < 0)
        return -1;
    if (msg.type != msg_type::TYPE_1)
        return -2;

    msg.set(msg_type::TYPE_2, UDP_PORT);
    if (send_message(sockfd, msg) < 0)
        return -1;
    return 0;
}