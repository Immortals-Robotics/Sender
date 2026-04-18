#include "nrf.h"
#include "udp.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <ctime>
#include <span>

static double now()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

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

    double lastPacketTime = -1.0;

    while (true)
    {
        const ssize_t n = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
                                   reinterpret_cast<sockaddr*>(&sender), &sender_len);
        if (n > 0)
        {
            printf("[DEBUG] Received %zd bytes from %s:%d\n",
                   n, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
            lastPacketTime = now();
            processPacket({buf, static_cast<size_t>(n)});
        }

        if (lastPacketTime < 0.0 || (now() - lastPacketTime) > 1.0)
        {
            demo();
            usleep(10000); // 10ms
        }
    }

    return 0;
}
