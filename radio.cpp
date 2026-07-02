// GO-GO — radio module (split 1:1 from v15 monolith).
#include "gogo.h"

struct PeerState {
  uint32_t senderId;
  uint16_t lastCounter;
  bool used;
};
PeerState peerTable[8];

void IRAM_ATTR loraSetFlag() {
  loraEventFlag = true;
}

String peerTargetString() {
  if (pairedRxId == 0) return String("ANY");
  return shortIdString(pairedRxId);
}

int discoveredPeerCount() {
  int n = 0;
  unsigned long now = millis();
  for (auto &p : discoveredPeers) {
    if (p.used && (now - p.lastSeen < 10000)) n++;
  }
  return n;
}

int discoveredIndexByVisibleOrder(int visibleIndex) {
  int n = 0;
  unsigned long now = millis();
  for (int i = 0; i < 6; i++) {
    if (discoveredPeers[i].used && (now - discoveredPeers[i].lastSeen < 10000)) {
      if (n == visibleIndex) return i;
      n++;
    }
  }
  return -1;
}

void rememberDiscoveredPeer(uint32_t id, int16_t rssi, float snr) {
  if (id == 0 || id == deviceId) return;
  unsigned long now = millis();

  for (auto &p : discoveredPeers) {
    if (p.used && p.id == id) {
      p.rssi = rssi;
      p.snr = snr;
      p.lastSeen = now;
      return;
    }
  }

  for (auto &p : discoveredPeers) {
    if (!p.used) {
      p.used = true;
      p.id = id;
      p.rssi = rssi;
      p.snr = snr;
      p.lastSeen = now;
      return;
    }
  }

  // Replace the oldest entry if the list is full.
  int oldest = 0;
  for (int i = 1; i < 6; i++) {
    if (discoveredPeers[i].lastSeen < discoveredPeers[oldest].lastSeen) oldest = i;
  }
  discoveredPeers[oldest] = {id, rssi, snr, now, true};
}

bool loraPeerFound() {
  if (!isLoRaMode() || !loraInitialized) return false;
  if (lastPeerId == 0) return false;
  if (millis() - lastLinkSeen > LINK_TIMEOUT) return false;
  if (isLoRaRemote() && pairedRxId != 0 && lastPeerId != pairedRxId) return false;
  return true;
}

// ============================================================================
// LoRa
// ============================================================================
bool isLoRaMode() {
  return controlMode == MODE_LORA_REMOTE || controlMode == MODE_LORA_GATEWAY;
}

bool isLoRaRemote() {
  return controlMode == MODE_LORA_REMOTE;
}

bool isLoRaGateway() {
  return controlMode == MODE_LORA_GATEWAY;
}

bool initLoRa() {
  if (loraInitialized) return true;

  DBGLN("[LORA] begin");
  int state = lora.begin(radioCfg.freq, radioCfg.bw, radioCfg.sf, radioCfg.cr);
  if (state != RADIOLIB_ERR_NONE) {
    DBG("[LORA] begin failed: ");
    DBGLN(state);
    loraInitialized = false;
    loraInitFailed = true;
    return false;
  }

  lora.setOutputPower(radioCfg.power);
  lora.setDio1Action(loraSetFlag);
  state = lora.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    DBG("[LORA] RX failed: ");
    DBGLN(state);
    loraInitialized = false;
    loraInitFailed = true;
    return false;
  }

  loraInitialized = true;
  loraInitFailed = false;
  lastLinkSeen = millis();
  return true;
}

void stopLoRa() {
  if (!loraInitialized) return;
  lora.sleep();
  loraInitialized = false;
  loraInitFailed = false;
}

void restartLoRa() {
  stopLoRa();
  delay(50);
  initLoRa();
}

bool canSendNow() {
  return (millis() - lastTxTimestamp) >= 100;
}

bool sendPacket(uint8_t type, uint32_t targetId, uint8_t ackCmd) {
  if (!loraInitialized) return false;
  if (!canSendNow()) return false;
  lastTxTimestamp = millis();

  LoRaPacket p{};
  p.version = 1;
  p.type = type;
  p.senderId = deviceId;
  p.targetId = targetId;
  p.counter = radioCfg.counter++;
  p.ackCommand = ackCmd;
  p.crc = crc16_ccitt((const uint8_t*)&p, offsetof(LoRaPacket, crc));

  int state = lora.transmit((uint8_t*)&p, sizeof(p));
  lora.startReceive();
  saveRadioConfig();

  if (state != RADIOLIB_ERR_NONE) {
    DBG("[LORA] TX failed: ");
    DBGLN(state);
    return false;
  }

  txPacketCount++;
  return true;
}

bool acceptNewPacket(uint32_t sender, uint16_t counter) {
  for (auto &peer : peerTable) {
    if (peer.used && peer.senderId == sender) {
      if (counter <= peer.lastCounter) return false;
      peer.lastCounter = counter;
      return true;
    }
  }
  for (auto &peer : peerTable) {
    if (!peer.used) {
      peer.used = true;
      peer.senderId = sender;
      peer.lastCounter = counter;
      return true;
    }
  }
  return true;
}

void emitGatewayAction(uint8_t type, uint32_t fromSender) {
  lastLinkSeen = millis();
  linkOK = true;

  const char* cmd = (type == PKT_GO) ? "GO" : "PANIC";

  if (DEBUG_SERIAL) {
    DBG("[RX LORA] ");
    DBG(cmd);
    DBG(" from ");
    DBGLN(shortIdString(fromSender));
  }


  if (outputWantsOsc() && WiFi.status() == WL_CONNECTED) {
    sendOSC((type == PKT_GO) ? config.osc_address : config.panic_address);
  }

  if (outputWantsBle() && bleConnected) {
    sendKeyPress((type == PKT_GO) ? 0x2C : 0x29);
  }

  blinkLedOnce();
}

void processLoRaPacket() {
  if (!loraEventFlag || !loraInitialized) return;
  loraEventFlag = false;

  LoRaPacket p{};
  int state = lora.readData((uint8_t*)&p, sizeof(p));
  if (state != RADIOLIB_ERR_NONE) return;

  uint16_t calc = crc16_ccitt((const uint8_t*)&p, offsetof(LoRaPacket, crc));
  if (calc != p.crc || p.version != 1) return;
  if (p.targetId != 0xFFFFFFFF && p.targetId != deviceId) return;
  if (!acceptNewPacket(p.senderId, p.counter)) return;

  lastPacket = p;
  lastPeerId = p.senderId;
  lastPeerRssi = (int16_t)lora.getRSSI();
  lastPeerSnr = lora.getSNR();
  rememberDiscoveredPeer(p.senderId, lastPeerRssi, lastPeerSnr);
  lastLinkSeen = millis();
  linkOK = true;
  rxPacketCount++;

  if (isLoRaGateway()) {
    if (p.type == PKT_PING) {
      sendPacket(PKT_PONG, p.senderId, 0);
    } else if (p.type == PKT_GO || p.type == PKT_PANIC) {
      lastRxCommand = p.type;
      lastCommandSeen = millis();
      emitGatewayAction(p.type, p.senderId);
      setScreen((p.type == PKT_GO) ? SCREEN_GO_SENT : SCREEN_PANIC_SENT);
      messageTime = millis();
    }
  } else if (isLoRaRemote()) {
    if (p.type == PKT_PONG || p.type == PKT_BEACON) {
      if (pairedRxId == 0 || pairedRxId == p.senderId) {
        linkOK = true;
        lastLinkSeen = millis();
      }
    } else if (p.type == PKT_ACK && waitingForAck && p.targetId == deviceId && p.ackCommand == pendingCommand) {
      waitingForAck = false;
      retryCount = 0;
      ackOk = true;
      messageTime = millis();
      setScreen((pendingCommand == PKT_GO) ? SCREEN_GO_SENT : SCREEN_PANIC_SENT);
    }
  }
}

void sendHeartbeat() {
  if (!isLoRaRemote() || !loraInitialized) return;
  unsigned long now = millis();

  if (now - lastPingTime >= PING_INTERVAL) {
    lastPingTime = now;
    sendPacket(PKT_PING, pairedRxId ? pairedRxId : 0xFFFFFFFF, 0);
  }

  if (lastLinkSeen == 0 || now - lastLinkSeen > LINK_TIMEOUT) {
    linkOK = false;
  }
}

void sendGatewayBeacon() {
  if (!isLoRaGateway() || !loraInitialized) return;
  unsigned long now = millis();
  if (now - lastBeaconTime >= BEACON_INTERVAL) {
    lastBeaconTime = now;
    sendPacket(PKT_BEACON, 0xFFFFFFFF, 0);
  }
  if (lastLinkSeen == 0 || now - lastLinkSeen > LINK_TIMEOUT) {
    linkOK = false;
  }
}

void retryPendingAck() {
  if (!waitingForAck || !isLoRaRemote()) return;

  unsigned long now = millis();
  if (now - lastTxTime < ACK_TIMEOUT) return;

  if (retryCount < MAX_RETRIES - 1) {
    retryCount++;
    lastTxTime = now;
    sendPacket(pendingCommand, pendingTarget, 0);
  } else {
    waitingForAck = false;
    ackOk = false;
    linkOK = false;
    setScreen(SCREEN_NO_CONNECTION);
    messageTime = now;
  }
}

void cycleFrequency() {
  static const float freqs[] = {868.0f, 868.1f, 868.3f, 868.5f, 869.0f};
  const int n = sizeof(freqs) / sizeof(freqs[0]);
  int idx = 0;
  for (int i = 0; i < n; i++) {
    if (fabsf(radioCfg.freq - freqs[i]) < 0.01f) {
      idx = i;
      break;
    }
  }
  idx = (idx + 1) % n;
  radioCfg.freq = freqs[idx];
  saveRadioConfig();
  restartLoRa();
}

void cyclePower() {
  static const int8_t powers[] = {10, 14, 17, 20};
  const int n = sizeof(powers) / sizeof(powers[0]);
  int idx = 0;
  for (int i = 0; i < n; i++) {
    if (powers[i] == radioCfg.power) {
      idx = i;
      break;
    }
  }
  idx = (idx + 1) % n;
  radioCfg.power = powers[idx];
  saveRadioConfig();
  restartLoRa();
}

void cycleOutput() {
  gatewayOutputMode = (gatewayOutputMode + 1) % 2;
  saveGatewayOutputMode();
  if (controlMode == MODE_LORA_GATEWAY) {
    // Re-init so exactly one RX output is active: BLE HID OR OSC WiFi.
    initSelectedMode(false);
  }
}
