#include "nrf.h"
#include "udp.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <span>

int main()
{
    if (!nrfInit())
        return -1;

    const int sock = udpInit();
    if (sock < 0)
        return -1;

    static uint8_t buf[4096];
    sockaddr_in    sender{};
    socklen_t      sender_len = sizeof(sender);

    while (true)
    {
        const ssize_t n = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
                                   reinterpret_cast<sockaddr*>(&sender), &sender_len);
        if (n > 0)
        {
            printf("[DEBUG] Received %zd bytes from %s:%d\n",
                   n, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
            processPacket({buf, static_cast<size_t>(n)});
        }

        if (!firstPacketReceived())
        {
            demo();
            usleep(10000); // 10ms
        }
    }

    return 0;
}