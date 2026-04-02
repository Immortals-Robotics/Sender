#include "nrf.h"

#include <RF24/RF24.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdexcept>

static constexpr uint8_t     DEMO_PAYLOAD_SIZE = 10;
static constexpr uint8_t     CHANNEL_CHANGE_ID = 80;
static constexpr int         DEFAULT_CHANNEL   = 90;

// CE pin 22, SPI device 0 (spidev0.0)
static RF24 radio(22, 0);

static int     s_channel             = DEFAULT_CHANNEL;
static bool    s_firstPacketReceived = false;
static uint8_t s_address[5]          = {110, 110, 8, 110, 110};

static void applyConfig()
{
    radio.setDataRate(RF24_2MBPS);
    radio.enableDynamicPayloads();
    radio.setPALevel(RF24_PA_MAX);
    radio.setAutoAck(false);
    radio.setChannel(s_channel);
}

bool nrfInit()
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
        return false;
    }

    if (!init_result)
    {
        printf("[CRITICAL] Failed to initialize NRF24\n");
        return false;
    }

    applyConfig();

    s_address[2] = 8;
    radio.openWritingPipe(s_address);

    s_address[2] = 30;
    radio.openReadingPipe(1, s_address);

    // TX mode
    radio.stopListening();
    radio.flush_tx();

    radio.printPrettyDetails();

    return true;
}

void sendToRobot(uint8_t robot_id, const uint8_t* payload, uint8_t len)
{
    s_address[2] = robot_id;
    radio.openWritingPipe(s_address);

    const bool result = radio.write(payload, len);
    if (!result)
    {
        printf("[ERROR] Failed to send %d bytes to robot %d\n", len, robot_id);
    }

    if (radio.failureDetected)
    {
        printf("[ERROR] Radio failure detected, reinitializing...\n");
        radio.begin();
        applyConfig();
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
    s_firstPacketReceived = true;

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
            if (packet[head + 7] == new_channel && s_channel != new_channel)
            {
                s_channel = new_channel;
                printf("[INFO] Setting NRF channel to %d\n", s_channel);
                radio.setChannel(s_channel);
            }

            head += 10;
        }
        else
        {
            // need at least 2 bytes for id + len
            if (head + 2 > packet.size())
                break;

            const uint8_t packet_len = packet[head + 1];

            if (packet_len == 0)
                break; // prevent infinite loop

            // send the full slot from head (including robot_id and length bytes),
            // since the nRF MCU uses payload[1] to know how many bytes to forward
            // to the FPGA, and the FPGA dispatches on payload[2] as the command code
            uint8_t payload[256] = {};
            const uint8_t available = static_cast<uint8_t>(
                std::min<size_t>(packet_len, packet.size() - head));
            memcpy(payload, packet.data() + head, available);

            sendToRobot(id, payload, packet_len);

            head += packet_len; // stride is packet_len, not packet_len + 2
        }
    }
}

bool firstPacketReceived()
{
    return s_firstPacketReceived;
}

// Sends a halt command followed by an echo request to the given robot,
// then briefly switches to RX mode to read and log the response.
// The echo command (case 7) makes the FPGA copy the received payload back
// and send it via UART to the nRF MCU, which forwards it to us over NRF.
void debugRequestFeedback(uint8_t robot_id)
{
    static constexpr uint8_t LEN       = 10;
    static constexpr uint8_t CMD_HALT  = 6;
    static constexpr uint8_t CMD_ECHO  = 7;

    // Halt: [robot_id, LEN, CMD_HALT, 0, ...]
    uint8_t halt_payload[LEN] = {};
    halt_payload[0] = robot_id;
    halt_payload[1] = LEN;
    halt_payload[2] = CMD_HALT;
    sendToRobot(robot_id, halt_payload, LEN);

    // Echo: [robot_id, LEN, CMD_ECHO, 0, ...]
    uint8_t echo_payload[LEN] = {};
    echo_payload[0] = robot_id;
    echo_payload[1] = LEN;
    echo_payload[2] = CMD_ECHO;
    sendToRobot(robot_id, echo_payload, LEN);

    // Wait for the pipeline to process:
    // nRF MCU UART write + FPGA processing + FPGA UART write back ~= 15-25ms at 31250 baud
    delay(10);

    // Switch to RX and give a generous window for the response
    radio.startListening();
    delay(100);

    if (radio.available())
    {
        uint8_t response[32] = {};
        radio.read(response, sizeof(response));

        printf("[DEBUG] Feedback from robot %d:", robot_id);
        for (int i = 0; i < 32; i++)
            printf(" %02X", response[i]);
        printf("\n");
    }
    else
    {
        printf("[DEBUG] No feedback received from robot %d\n", robot_id);
    }

    radio.stopListening();
}