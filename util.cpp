// GO-GO — util module (split 1:1 from v15 monolith).
#include "gogo.h"

// ============================================================================
// Helpers
// ============================================================================
void safeCopy(char* dst, const char* src, size_t dstSize) {
  if (!dst || dstSize == 0) return;
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

void VextON() {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
}

uint32_t hardwareId32() {
  uint64_t mac = ESP.getEfuseMac();
  return (uint32_t)((mac & 0xFFFFFFFFULL) ^ ((mac >> 24) & 0xFFFFFFFFULL));
}

String shortIdString(uint32_t id) {
  char buf[10];
  snprintf(buf, sizeof(buf), "%06lX", (unsigned long)(id & 0xFFFFFF));
  return String(buf);
}

String getUniqueName() {
  return "GO-GO-" + shortIdString(hardwareId32());
}

bool isValidIP(const char* ip) {
  int parts[4];
  int count = sscanf(ip, "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2], &parts[3]);
  if (count != 4) return false;
  for (int i = 0; i < 4; i++) {
    if (parts[i] < 0 || parts[i] > 255) return false;
  }
  return true;
}

uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
      else crc <<= 1;
    }
  }
  return crc;
}
