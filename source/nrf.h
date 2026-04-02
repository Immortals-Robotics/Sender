#pragma once

#include <cstdint>
#include <span>

bool nrfInit();

void sendToRobot(uint8_t robot_id, const uint8_t* payload, uint8_t len);
void demo();
void processPacket(std::span<const uint8_t> packet);

bool firstPacketReceived();

void nrfPrintDiagnostics();