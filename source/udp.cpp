#include "udp.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>

static constexpr uint16_t    UDP_PORT       = 60005;
static constexpr const char* MULTICAST_GROUP = "224.5.92.5";

int udpInit()
{
    const int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        printf("[CRITICAL] Failed to create UDP socket\n");
        return -1;
    }

    const int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(UDP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        printf("[CRITICAL] Failed to bind UDP socket\n");
        close(sock);
        return -1;
    }

    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        printf("[CRITICAL] Failed to join multicast group\n");
        close(sock);
        return -1;
    }

    printf("[INFO] Listening on %s:%d\n", MULTICAST_GROUP, UDP_PORT);
    return sock;
}