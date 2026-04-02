#include <RF24/RF24.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <span>
#include <stdexcept>

RF24 radio(22, 0);

int  Channel             = 110;
bool firstPacketRecieved = false;

uint8_t address[5] = {110, 110, 8, 110, 110};

void demo()
{
    uint8_t data[11];
    data[0]  = 25;
    data[1]  = 1;
    data[2]  = 0;
    data[3]  = 0;
    data[4]  = 0;
    data[5]  = 0;
    data[6]  = 0;
    data[7]  = 0;
    data[8]  = 0;
    data[9]  = 0;
    data[10] = 0;

    address[2] = data[0];
    radio.openWritingPipe(address);

    const bool result = radio.write(data + 1, 10);
    if (!result)
    {
        printf("[ERROR] Failed to send demo bytes\n");
    }
}

void processRecievedPacket(std::span<char> packet)
{
    firstPacketRecieved = true;

    unsigned head = 0;
    while (head < packet.size())
    {
        if (packet[head] == 80)
        {
            if (packet[head + 1] == packet[head + 7] && Channel != packet[head + 1])
            {
                Channel = packet[head + 1];
                printf("[INFO] setting nrf channel to %d\n", Channel);
                radio.setChannel(Channel);
                head += 10;
            }
        }
        else
        {
            address[2] = packet[head];
            radio.openWritingPipe(address);

            const int  packet_len = packet[head + 1];
            const bool result     = radio.write(packet.data() + head + 2, packet_len);
            if (!result)
            {
                printf("[ERROR] Failed to send %d bytes to %d\n", packet_len, address[2]);
            }

            head += packet_len + 2;
        }
    }
}

int main(void)
{
    printf("[INFO] Initializing nrf24\n");

    bool nrf_init_result;
    try
    {
        nrf_init_result = radio.begin();
    }
    catch (const std::runtime_error &error)
    {
        printf("[CRITICAL] Failed to initialize nrf24: %s\n", error.what());
        return -1;
    }

    if (!nrf_init_result)
    {
        printf("[CRITICAL] Failed to initialize nrf24\n");
        return -1;
    }

    // TODO: verify
    radio.setPayloadSize(10);

    radio.setPALevel(RF24_PA_MAX);

    address[2] = 8;
    radio.openWritingPipe(address);

    address[2] = 30;
    radio.openReadingPipe(1, address);

    radio.setChannel(Channel);

    // TODO: verify
    radio.setAutoAck(false);

    // put radio in TX mode
    radio.stopListening();

    radio.printPrettyDetails();

    // UDP socket setup
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
    addr.sin_port        = htons(60005);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        printf("[CRITICAL] Failed to bind UDP socket\n");
        close(sock);
        return -1;
    }

    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr("224.5.92.5");
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        printf("[CRITICAL] Failed to join multicast group\n");
        close(sock);
        return -1;
    }

    static char buf[4096];
    sockaddr_in sender{};
    socklen_t   sender_len = sizeof(sender);

    while (true)
    {
        const ssize_t n = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
                                   reinterpret_cast<sockaddr *>(&sender), &sender_len);
        if (n > 0)
        {
            printf("[DEBUG] received %zd bytes from %s:%d\n",
                   n, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
            processRecievedPacket({buf, static_cast<size_t>(n)});
        }

        if (!firstPacketRecieved)
        {
            printf("[DEBUG] sending demo bytes\n");
            demo();
            delay(10);
        }
    }

    close(sock);
    return 0;
}