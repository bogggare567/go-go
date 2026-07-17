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

// Recodes UTF-8 (source files are UTF-8) into font_cyr.h's compact glyph
// table so display.print()/getTextBounds() index the right bitmap. ASCII
// passes through untouched - safe to wrap around any string, Cyrillic or
// not. Returns a pointer to a shared static buffer (single-threaded loop,
// consumed synchronously by each drawCenteredText()/print() call).
const char* ru(const char* utf8);
