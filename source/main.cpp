#include <RF24/RF24.h>

#include <ctime>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>

static constexpr uint8_t  DEMO_PAYLOAD_SIZE  = 10;
static constexpr uint8_t  CHANNEL_CHANGE_ID  = 80;
static constexpr int      DEFAULT_CHANNEL    = 90;
static constexpr uint16_t UDP_PORT           = 60005;
static constexpr const char* MULTICAST_GROUP = "224.5.92.5";

// CE pin 22, SPI device 0 (spidev0.0)
RF24 radio(22, 0);

int     channel             = DEFAULT_CHANNEL;
bool    firstPacketReceived = false;
uint8_t address[5]          = {110, 110, 8, 110, 110};

void sendToRobot(uint8_t robot_id, const uint8_t* payload, uint8_t len)
{
    address[2] = robot_id;
    radio.openWritingPipe(address);

    const bool result = radio.write(payload, len);
    if (!result)
    {
        printf("[ERROR] Failed to send %d bytes to robot %d\n", len, robot_id);
    }

    if (radio.failureDetected)
    {
        printf("[ERROR] Radio failure detected, reinitializing...\n");
        radio.begin();
        radio.setDataRate(RF24_2MBPS);
        radio.enableDynamicPayloads();
        radio.setPALevel(RF24_PA_MAX);
        radio.setAutoAck(false);
        radio.setChannel(channel);
        radio.stopListening();
        radio.flush_tx();
        radio.failureDetected = false;
    }
}

void demo()
{
    uint8_t payload[DEMO_PAYLOAD_SIZE] = {};
    payload[0] = 1;
    sendToRobot(25, payload, DEMO_PAYLOAD_SIZE);
}

void processPacket(std::span<const uint8_t> packet)
{
    firstPacketReceived = true;

    unsigned head = 0;
    while (head < packet.size())
    {
        const uint8_t id = packet[head];

        if (id == CHANNEL_CHANGE_ID)
        {
            // need at least 8 bytes for the channel change packet
            if (head + 8 > packet.size())
                break;

            const uint8_t new_channel = packet[head + 1];
            // validate: byte 1 must equal byte 7
            if (packet[head + 7] == new_channel && channel != new_channel)
            {
                channel = new_channel;
                printf("[INFO] Setting NRF channel to %d\n", channel);
                radio.setChannel(channel);
            }

            head += 10;
        }
        else
        {
            // need at least 2 bytes for id + len
            if (head + 2 > packet.size())
                break;

            const uint8_t packet_len = packet[head + 1];

            // copy only what's available, zero-pad the rest
            uint8_t payload[256] = {};
            const uint8_t available = static_cast<uint8_t>(
                std::min<size_t>(packet_len, packet.size() - head - 2));
            memcpy(payload, packet.data() + head + 2, available);

            sendToRobot(id, payload, packet_len);

            head += packet_len + 2;
        }
    }
}

int main()
{
    printf("[INFO] Initializing NRF24\n");

    bool init_result;
    try
    {
        init_result = radio.begin();
    }
    catch (const std::runtime_error& e)
    {
        printf("[CRITICAL] Failed to initialize NRF24: %s\n", e.what());
        return -1;
    }

    if (!init_result)
    {
        printf("[CRITICAL] Failed to initialize NRF24\n");
        return -1;
    }

    radio.setDataRate(RF24_2MBPS);
    radio.enableDynamicPayloads();
    radio.setPALevel(RF24_PA_MAX);
    radio.setAutoAck(false);
    radio.setChannel(channel);

    address[2] = 8;
    radio.openWritingPipe(address);

    address[2] = 30;
    radio.openReadingPipe(1, address);

    // TX mode
    radio.stopListening();
    radio.flush_tx();

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

    static uint8_t buf[4096];
    sockaddr_in    sender{};
    socklen_t      sender_len = sizeof(sender);

    time_t last_diagnostics = time(nullptr);

    while (true)
    {
        const time_t now = time(nullptr);
        if (now - last_diagnostics >= 5)
        {
            radio.printPrettyDetails();
            last_diagnostics = now;
        }

        const ssize_t n = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
                                   reinterpret_cast<sockaddr*>(&sender), &sender_len);
        if (n > 0)
        {
            printf("[DEBUG] Received %zd bytes from %s:%d\n",
                   n, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
            processPacket({buf, static_cast<size_t>(n)});
        }

        if (!firstPacketReceived)
        {
            demo();
            delay(10);
        }
    }

    close(sock);
    return 0;
}