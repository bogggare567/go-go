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

// Build and transmit one packet with an explicit counter value.
// The counter is NOT persisted here: loadRadioConfig() reserves a block on
// boot, so per-packet NVS writes (flash wear, TX latency) are avoided.
static bool transmitPacket(uint8_t type, uint32_t targetId, uint8_t ackCmd, uint16_t counter) {
  if (!loraInitialized) return false;
  lastTxTimestamp = millis();

  LoRaPacket p{};
  p.version = 1;
  p.type = type;
  p.senderId = deviceId;
  p.targetId = targetId;
  p.counter = counter;
  p.ackCommand = ackCmd;
  p.crc = crc16_ccitt((const uint8_t*)&p, offsetof(LoRaPacket, crc));

  int state = lora.transmit((uint8_t*)&p, sizeof(p));
  lora.startReceive();

  if (state != RADIOLIB_ERR_NONE) {
    DBG("[LORA] TX failed: ");
    DBGLN(state);
    return false;
  }

  txPacketCount++;
  return true;
}

bool sendPacket(uint8_t type, uint32_t targetId, uint8_t ackCmd) {
  if (!loraInitialized) return false;
  if (!canSendNow()) return false;
  return transmitPacket(type, targetId, ackCmd, radioCfg.counter++);
}

bool sendCommand(uint8_t type, uint32_t targetId) {
  // GO/PANIC path: bypass the heartbeat throttle and send 3 copies with the
  // SAME counter — the gateway dedupes repeats, extra copies only add
  // resilience against a single lost packet.
  if (!loraInitialized) return false;
  uint16_t counter = radioCfg.counter++;
  bool ok = false;
  for (int i = 0; i < 3; i++) {
    if (transmitPacket(type, targetId, 0, counter)) ok = true;
    delay(8);
  }
  return ok;
}

bool acceptNewPacket(uint32_t sender, uint16_t counter) {
  for (auto &peer : peerTable) {
    if (peer.used && peer.senderId == sender) {
      // uint16 wraparound-safe monotonic check (window of 32767).
      int16_t diff = (int16_t)(counter - peer.lastCounter);
      if (diff <= 0) return false;
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
    sendKeyPress((type == PKT_GO) ? goKeyCode : panicKeyCode);
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

// ============================================================================
// Region frequency plans
// ============================================================================
// EU868 keeps the legacy v15 channel list, so an updated device still hears
// devices running the old firmware without any reconfiguration.
static const float CH_EU868[] = {868.0f, 868.1f, 868.3f, 868.5f, 869.0f};
static const float CH_RU864[] = {864.1f, 864.3f, 864.5f, 864.7f, 864.9f};
static const float CH_US915[] = {903.9f, 906.5f, 909.1f, 911.7f, 914.2f};
static const float CH_AU915[] = {915.2f, 916.8f, 918.4f, 920.0f, 921.6f};
static const float CH_AS923[] = {923.2f, 923.4f, 923.6f, 923.8f, 924.0f};
static const float CH_IN865[] = {865.0625f, 865.4025f, 865.985f};
static const float CH_KR920[] = {922.1f, 922.3f, 922.5f, 922.7f, 922.9f};
static const float CH_CN470[] = {470.3f, 471.9f, 473.5f, 475.1f, 476.7f};

static const RegionPlan REGIONS[] = {
  {"EU868", CH_EU868, 5, 14, 863.0f, 870.0f},
  {"RU864", CH_RU864, 5, 14, 864.0f, 870.0f},
  {"US915", CH_US915, 5, 22, 902.0f, 928.0f},
  {"AU915", CH_AU915, 5, 22, 915.0f, 928.0f},
  {"AS923", CH_AS923, 5, 16, 920.0f, 925.0f},
  {"IN865", CH_IN865, 3, 20, 865.0f, 867.0f},
  {"KR920", CH_KR920, 5, 14, 920.9f, 923.3f},
  {"CN470", CH_CN470, 5, 17, 470.0f, 490.0f},  // needs the 470-510 MHz hardware variant
};

uint8_t regionCount() {
  return sizeof(REGIONS) / sizeof(REGIONS[0]);
}

const RegionPlan& regionPlan(uint8_t idx) {
  return REGIONS[idx < regionCount() ? idx : 0];
}

const RegionPlan& currentRegion() {
  return regionPlan(radioCfg.region);
}

void applyRadioFreq() {
  if (radioCfg.region >= regionCount()) radioCfg.region = 0;
  const RegionPlan& r = currentRegion();
  if (radioCfg.chan >= r.numChannels) radioCfg.chan = 0;
  // Always run at the region's legal maximum (owner request: max safe power).
  radioCfg.power = r.maxPower;
  radioCfg.freq = r.channels[radioCfg.chan];
}

void cycleFrequency() {
  radioCfg.chan = (radioCfg.chan + 1) % currentRegion().numChannels;
  applyRadioFreq();
  saveRadioConfig();
  restartLoRa();
}

// ============================================================================
// Spectrum view: sweep the region band, sample instantaneous RSSI per bin.
// ============================================================================
static bool spectrumActive = false;

void enterSpectrum() {
  if (!loraInitialized) initLoRa();
  spectrumActive = true;
  spectrumPos = 0;
  memset(spectrumLevels, 0, sizeof(spectrumLevels));
}

void exitSpectrum() {
  spectrumActive = false;
  if (loraInitialized) {
    lora.standby();
    lora.setFrequency(radioCfg.freq);
    lora.startReceive();
  }
}

void spectrumSweepStep() {
  if (!spectrumActive || !loraInitialized) return;
  const RegionPlan& r = currentRegion();
  // A few bins per loop pass keeps the button responsive.
  for (int i = 0; i < 8; i++) {
    float f = r.scanFrom + (r.scanTo - r.scanFrom) * spectrumPos / 63.0f;
    lora.standby();
    lora.setFrequency(f);
    lora.startReceive();
    delay(2);
    float rssi = lora.getRSSI(false);   // instantaneous, not packet RSSI
    int h = (int)((rssi + 130.0f) * (40.0f / 70.0f));  // -130..-60 dBm -> 0..40 px
    if (h < 0) h = 0;
    if (h > 40) h = 40;
    spectrumLevels[spectrumPos] = (uint8_t)h;
    spectrumPos = (spectrumPos + 1) % 64;
  }
}

void cycleOutput() {
  gatewayOutputMode = (gatewayOutputMode + 1) % 2;
  saveGatewayOutputMode();
  if (controlMode == MODE_LORA_GATEWAY) {
    // Re-init so exactly one RX output is active: BLE HID OR OSC WiFi.
    initSelectedMode(false);
  }
}
