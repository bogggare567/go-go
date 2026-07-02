// Small helpers: strings, ids, validation, CRC.
#pragma once
#include "config.h"

void safeCopy(char* dst, const char* src, size_t dstSize);
void VextON();
uint32_t hardwareId32();
String shortIdString(uint32_t id);
String getUniqueName();
bool isValidIP(const char* ip);
uint16_t crc16_ccitt(const uint8_t* data, size_t len);
