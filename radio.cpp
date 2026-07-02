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

// --- auto-frequency state (logic lives near the end of this file) ---
static uint8_t autoGridIdx = 0;
static bool searchActive = false;
static bool searchDwelling = false;
static uint8_t searchIdx = 0;
static unsigned long searchDwellStart = 0;
static float searchFreqNow = 0;
static void tuneTo(float f);
static uint8_t pickCleanestBin();
static void masterAdoptBin(uint8_t idx);

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

  // Auto frequency: the gateway (master) starts on the cleanest bin it can
  // find; remotes will locate it by scanning the same grid.
  if (freqAuto && isLoRaGateway()) {
    masterAdoptBin(pickCleanestBin());
  } else if (freqAuto && isLoRaRemote()) {
    searchActive = false;
    lastLinkSeen = 0;  // start the grid search on the first loop pass
  }
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
    } else if (p.type == PKT_HOP && freqAuto) {
      // Master announced a channel change: follow immediately. The master
      // keeps announcing on the old channel briefly, then joins us.
      if ((pairedRxId == 0 || pairedRxId == p.senderId) &&
          p.ackCommand < gridCount()) {
        radioCfg.freq = gridFreq(p.ackCommand);
        tuneTo(radioCfg.freq);
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
  if (searchActive) return;  // don't spray pings across bins while scanning
  unsigned long now = millis();

  // Jittered interval: a fixed 2 s cycle drifts into alignment with the
  // gateway's 2 s beacon and the packets then collide in bursts.
  static unsigned long pingJitter = 0;
  if (now - lastPingTime >= PING_INTERVAL + pingJitter) {
    lastPingTime = now;
    pingJitter = (unsigned long)random(0, 500);
    sendPacket(PKT_PING, pairedRxId ? pairedRxId : 0xFFFFFFFF, 0);
  }

  if (lastLinkSeen == 0 || now - lastLinkSeen > LINK_TIMEOUT) {
    linkOK = false;
  }
}

void sendGatewayBeacon() {
  if (!isLoRaGateway() || !loraInitialized) return;
  unsigned long now = millis();
  // Beacon is a discovery aid. While a remote is actively pinging us there
  // is nothing to discover, and beaconing only risks colliding with pings.
  bool linkAlive = (lastPeerId != 0) && (now - lastLinkSeen < 3000);
  if (!linkAlive && now - lastBeaconTime >= BEACON_INTERVAL) {
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
  // Auto -> ch0 -> ch1 -> ... -> last -> Auto. Picking a fixed channel
  // disables all hopping/searching (manual override, owner requirement).
  if (freqAuto) {
    freqAuto = false;
    radioCfg.chan = 0;
    applyRadioFreq();
  } else if (radioCfg.chan + 1 < currentRegion().numChannels) {
    radioCfg.chan++;
    applyRadioFreq();
  } else {
    freqAuto = true;
  }
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
  // Re-link fast: the remote pings immediately instead of waiting a cycle.
  lastPingTime = 0;
}

// ============================================================================
// Auto frequency: master/slave on a dense grid
// ============================================================================
// Grid: 0.2 MHz steps across the region band, inset 0.1 MHz from the edges.
// A LoRa signal is 125 kHz wide, so a finer grid would just overlap itself.
uint8_t gridCount() {
  const RegionPlan& r = currentRegion();
  int n = (int)((r.scanTo - r.scanFrom - 0.2f) / 0.2f) + 1;
  if (n < 1) n = 1;
  if (n > 255) n = 255;
  return (uint8_t)n;
}

float gridFreq(uint8_t idx) {
  const RegionPlan& r = currentRegion();
  return r.scanFrom + 0.1f + 0.2f * idx;
}

// master hop hysteresis: 6 s of sustained noise, quiet link, >=60 s between hops
static float noiseWin[20];
static uint8_t noiseCount = 0, noiseIdx = 0;
static unsigned long lastNoiseSample = 0;
static unsigned long lastHopTime = 0;

bool slaveSearching() { return searchActive; }
float searchStatusFreq() { return searchFreqNow; }

static void tuneTo(float f) {
  lora.standby();
  lora.setFrequency(f);
  lora.startReceive();
}

// The master picks the quietest bin of the FULL region grid (owner request:
// check all channels). Scoring takes the worst of 3 samples, so continuously
// busy segments (wireless audio at 863-865) and bursty interferers both look
// bad and are avoided.
static uint8_t pickCleanestBin() {
  uint8_t best = 0;
  float bestRssi = 1000.0f;
  for (uint8_t i = 0; i < gridCount(); i++) {
    lora.standby();
    lora.setFrequency(gridFreq(i));
    lora.startReceive();
    // Worst of 3 samples: a bursty interferer OFF during one sample must
    // not make a channel look clean.
    float worst = -200.0f;
    for (int k = 0; k < 3; k++) {
      delay(4);
      float rssi = lora.getRSSI(false);
      if (rssi > worst) worst = rssi;
    }
    if (worst < bestRssi) {
      bestRssi = worst;
      best = i;
    }
  }
  return best;
}

// Master: park on a grid bin (boot / after hop decision).
static void masterAdoptBin(uint8_t idx) {
  autoGridIdx = idx;
  radioCfg.freq = gridFreq(idx);
  tuneTo(radioCfg.freq);
}

static void masterAnnounceAndHop(uint8_t newIdx) {
  // Announce on the old channel several times, then move. A slave that
  // misses every copy simply re-finds us with its grid scan.
  uint16_t counter = radioCfg.counter++;
  for (int i = 0; i < 5; i++) {
    LoRaPacket p{};
    p.version = 1;
    p.type = PKT_HOP;
    p.senderId = deviceId;
    p.targetId = 0xFFFFFFFF;
    p.counter = counter;
    p.ackCommand = newIdx;
    p.crc = crc16_ccitt((const uint8_t*)&p, offsetof(LoRaPacket, crc));
    lora.transmit((uint8_t*)&p, sizeof(p));
    delay(60);
  }
  masterAdoptBin(newIdx);
  lastHopTime = millis();
  noiseCount = 0;
  DBG("[HOP] moved to "); DBGLN(radioCfg.freq);
}

static void gatewayAutoStep() {
  unsigned long now = millis();

  // A working link is sacred: while a remote is pinging us we NEVER retune,
  // no matter how the channel looks (owner rule: no ether experiments
  // during a show). Hops happen only when nobody is talking to us anyway.
  bool linkAlive = (lastPeerId != 0) && (now - lastLinkSeen < 8000);
  if (linkAlive) {
    noiseCount = 0;
    return;
  }

  if (now - lastNoiseSample < 300) return;
  lastNoiseSample = now;

  noiseWin[noiseIdx] = lora.getRSSI(false);
  noiseIdx = (noiseIdx + 1) % 20;
  if (noiseCount < 20) { noiseCount++; return; }

  // Noise floor = the QUIETEST sample in the window: stray packets spike
  // single samples, but the floor between packets stays low on a healthy
  // channel.
  float floorRssi = 0;
  for (int i = 0; i < 20; i++) floorRssi = (i == 0 || noiseWin[i] < floorRssi) ? noiseWin[i] : floorRssi;

  bool sustainedNoise = floorRssi > -85.0f;
  bool cooledDown = (lastHopTime == 0) || (now - lastHopTime > 60000);

  if (sustainedNoise && cooledDown) {
    uint8_t candidate = pickCleanestBin();
    tuneTo(gridFreq(autoGridIdx));  // back to the old bin to announce
    if (candidate != autoGridIdx) masterAnnounceAndHop(candidate);
    else noiseCount = 0;
  }
}

static uint8_t binIndexOf(float freq) {
  const RegionPlan& r = currentRegion();
  int idx = (int)((freq - r.scanFrom - 0.1f) / 0.2f + 0.5f);
  if (idx < 0) idx = 0;
  if (idx >= gridCount()) idx = gridCount() - 1;
  return (uint8_t)idx;
}

// Active search: the master's beacon fills only ~1.5% of airtime, so passive
// sniffing (CAD) almost always misses it. Instead the slave ASKS: it sends a
// PING on each bin and listens for the PONG the gateway always returns.
// Starts at the last known bin, so a reconnect is typically instant.
static void slaveAutoStep() {
  unsigned long now = millis();

  if (linkOK) {
    if (searchActive) {
      // Locked onto the master on this bin.
      radioCfg.freq = searchFreqNow;
      searchActive = false;
      searchDwelling = false;
      ledPlayMorseGo();
    }
    return;
  }

  if (!searchActive) {
    // Give the normal ping/pong 6 s to recover before we start hunting.
    if (lastLinkSeen != 0 && now - lastLinkSeen < 6000) return;
    searchActive = true;
    searchDwelling = false;
    searchIdx = binIndexOf(radioCfg.freq);
  }

  static unsigned long searchPauseUntil = 0;
  if (searchPauseUntil) {
    if (now < searchPauseUntil) return;
    searchPauseUntil = 0;
  }

  if (searchDwelling) {
    if (now - searchDwellStart < 250) return;  // PONG listen window
    searchDwelling = false;
    searchIdx = (searchIdx + 1) % gridCount();
    // Full pass done without an answer: breathe briefly, then retry.
    if (searchIdx == binIndexOf(radioCfg.freq)) searchPauseUntil = now + 700;
    return;
  }

  searchFreqNow = gridFreq(searchIdx);
  lora.standby();
  lora.setFrequency(searchFreqNow);
  lora.startReceive();
  transmitPacket(PKT_PING, pairedRxId ? pairedRxId : 0xFFFFFFFF, 0, radioCfg.counter++);
  searchDwelling = true;
  searchDwellStart = now;
}

void radioAutoStep() {
  if (!freqAuto || !loraInitialized) return;
  if (isLoRaGateway()) gatewayAutoStep();
  else if (isLoRaRemote()) slaveAutoStep();
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
