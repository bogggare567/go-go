// GO-GO — settings module (split 1:1 from v15 monolith).
#include "gogo.h"

void savePairedRxId(uint32_t id) {
  prefs.begin("radio", false);
  prefs.putUInt("pair_rx", id);
  prefs.end();
  pairedRxId = id;
}

void loadPairedRxId() {
  prefs.begin("radio", true);
  pairedRxId = prefs.getUInt("pair_rx", 0);
  prefs.end();
}

// ============================================================================
// Preferences
// ============================================================================
void loadOscConfig() {
  prefs.begin("osc", true);
  String ip = prefs.getString("ip", "192.168.1.100");
  String addr = prefs.getString("addr", "/go");
  String panic = prefs.getString("panic", "/panic");
  int port = prefs.getInt("port", 53000);
  prefs.end();

  safeCopy(config.osc_ip, ip.c_str(), sizeof(config.osc_ip));
  safeCopy(config.osc_address, addr.c_str(), sizeof(config.osc_address));
  safeCopy(config.panic_address, panic.c_str(), sizeof(config.panic_address));
  config.osc_port = port;
}

void saveOscConfig() {
  prefs.begin("osc", false);
  prefs.putString("ip", config.osc_ip);
  prefs.putString("addr", config.osc_address);
  prefs.putString("panic", config.panic_address);
  prefs.putInt("port", config.osc_port);
  prefs.end();
}

void saveKeymap() {
  prefs.begin("system", false);
  prefs.putUChar("gokey", goKeyCode);
  prefs.putUChar("pankey", panicKeyCode);
  prefs.end();
}

bool loadControlMode() {
  prefs.begin("system", true);
  bool saved = prefs.getBool("mode_set", false);
  int value = prefs.getInt("mode", MODE_OSC_WIFI);
  gatewayOutputMode = prefs.getUChar("out", OUT_BLE);
  goKeyCode = prefs.getUChar("gokey", 0x2C);    // Space
  panicKeyCode = prefs.getUChar("pankey", 0x29); // Esc
  prefs.end();

  if (!saved) {
    modeConfigured = false;
    controlMode = MODE_OSC_WIFI;
    selectedMode = MODE_OSC_WIFI;
    return false;
  }

  if (value < MODE_OSC_WIFI || value > MODE_LORA_GATEWAY) value = MODE_OSC_WIFI;
  controlMode = (ControlMode)value;
  selectedMode = controlMode;
  modeConfigured = true;

  if (gatewayOutputMode > OUT_OSC) gatewayOutputMode = OUT_BLE;
  return true;
}

void saveControlMode(uint8_t mode) {
  prefs.begin("system", false);
  prefs.putBool("mode_set", true);
  prefs.putInt("mode", (int)mode);
  prefs.end();
}

void saveGatewayOutputMode() {
  prefs.begin("system", false);
  prefs.putUChar("out", gatewayOutputMode);
  prefs.end();
}


void saveControlModeAndOutput(uint8_t mode, uint8_t out) {
  prefs.begin("system", false);
  prefs.putBool("mode_set", true);
  prefs.putInt("mode", (int)mode);
  prefs.putUChar("out", out);
  prefs.end();
}

void loadRadioConfig() {
  prefs.begin("radio", true);
  radioCfg.freq = prefs.getFloat("freq", 868.0f);
  radioCfg.bw = prefs.getFloat("bw", 125.0f);
  radioCfg.sf = prefs.getUChar("sf", 7);
  radioCfg.cr = prefs.getUChar("cr", 5);
  radioCfg.power = prefs.getChar("pwr", 14);
  radioCfg.counter = prefs.getUShort("cnt", 0);
  radioCfg.region = prefs.getUChar("region", 0);
  radioCfg.chan = prefs.getUChar("chan", 255);
  radioCfg.tuneKhz = 0;  // fine-tune UI removed in v16.4; field kept for compat
  regionConfigured = prefs.getBool("regset", false);
  prefs.end();

  if (radioCfg.chan == 255) {
    // Migration from v15 configs that stored a bare frequency:
    // pick the nearest EU868 channel so an updated device keeps its link.
    radioCfg.region = 0;
    radioCfg.tuneKhz = 0;
    const RegionPlan& r = regionPlan(0);
    uint8_t best = 0;
    for (uint8_t i = 1; i < r.numChannels; i++) {
      if (fabsf(radioCfg.freq - r.channels[i]) < fabsf(radioCfg.freq - r.channels[best])) best = i;
    }
    radioCfg.chan = best;
  }
  applyRadioFreq();

  // Reserve a counter block once per boot so sendPacket never writes NVS.
  // The counter jumps forward after a reboot; receivers only need it to be
  // monotonically increasing (wraparound-safe compare in acceptNewPacket).
  radioCfg.counter += 1000;
  prefs.begin("radio", false);
  prefs.putUShort("cnt", radioCfg.counter);
  prefs.end();
}

void saveRadioConfig() {
  prefs.begin("radio", false);
  prefs.putFloat("freq", radioCfg.freq);
  prefs.putFloat("bw", radioCfg.bw);
  prefs.putUChar("sf", radioCfg.sf);
  prefs.putUChar("cr", radioCfg.cr);
  prefs.putChar("pwr", radioCfg.power);
  prefs.putUShort("cnt", radioCfg.counter);
  prefs.putUChar("region", radioCfg.region);
  prefs.putUChar("chan", radioCfg.chan);
  prefs.putBool("regset", regionConfigured);
  prefs.end();
}
