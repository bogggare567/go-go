// LoRa link: init, packet protocol, heartbeat/beacon, pairing helpers.
#pragma once
#include "config.h"

bool isLoRaMode();
bool isLoRaRemote();
bool isLoRaGateway();

bool initLoRa();
void stopLoRa();
void restartLoRa();
bool canSendNow();
bool sendPacket(uint8_t type, uint32_t targetId, uint8_t ackCmd = 0);
bool acceptNewPacket(uint32_t sender, uint16_t counter);
void emitGatewayAction(uint8_t type, uint32_t fromSender);
void processLoRaPacket();
void sendHeartbeat();
void sendGatewayBeacon();
void retryPendingAck();
void cycleFrequency();
void cyclePower();
void cycleOutput();

bool loraPeerFound();
String peerTargetString();
int discoveredPeerCount();
int discoveredIndexByVisibleOrder(int visibleIndex);
void rememberDiscoveredPeer(uint32_t id, int16_t rssi, float snr);
