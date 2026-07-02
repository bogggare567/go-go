// LoRa link: init, packet protocol, heartbeat/beacon, pairing helpers.
#pragma once
#include "config.h"

bool isLoRaMode();
bool isLoRaRemote();
bool isLoRaGateway();

// Region frequency plans. TX and RX must use the same region/channel/tune.
struct RegionPlan {
  const char* name;
  const float* channels;  // MHz
  uint8_t numChannels;
  int8_t maxPower;        // dBm cap for the region
};
uint8_t regionCount();
const RegionPlan& regionPlan(uint8_t idx);
const RegionPlan& currentRegion();
void applyRadioFreq();    // recompute radioCfg.freq from region/chan/tune
void cycleRegion();
void cycleTune();

bool initLoRa();
void stopLoRa();
void restartLoRa();
bool canSendNow();
bool sendPacket(uint8_t type, uint32_t targetId, uint8_t ackCmd = 0);
bool sendCommand(uint8_t type, uint32_t targetId);
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
