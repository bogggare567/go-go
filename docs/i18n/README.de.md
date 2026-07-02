# GO-GO (Deutsch)

GO-GO ist eine Arduino-Firmware für das **Heltec WiFi LoRa 32 V3** Board, die es in eine kabellose Ein-Knopf-Fernbedienung für Theater, Konzerte und Live-Events verwandelt.

### Was es macht

Ein einfacher Tastendruck sendet ein **GO**-Kommando (nächsten Cue starten, Ton abspielen, Effekt auslösen). Langes Halten sendet **PANIC** (alles stoppen, alle Cues unterbrechen). Drei schnelle Klicks öffnen das Einstellungsmenü auf dem OLED-Display.

### Betriebsmodi

| Modus | Beschreibung |
|-------|-------------|
| **OSC / WiFi** | Sendet OSC-Nachrichten per UDP — kompatibel mit QLab, Ableton Live und jeder OSC-fähigen Software |
| **BLE / HID** | Funktioniert als Bluetooth-Tastatur — GO = Leertaste, PANIC = Escape. Keine Software-Konfiguration nötig |
| **LoRa Remote** | Überträgt Befehle per LoRa-Funk an einen gekoppelten Gateway — für große Reichweiten |
| **LoRa Gateway** | Empfängt LoRa-Pakete und leitet sie als OSC oder BLE weiter |

### Hardware

- **Board:** Heltec WiFi LoRa 32 V3 (ESP32-S3, SX1262 LoRa, SSD1306 OLED 128×64)
- **Taste:** eingebaute PRG-Taste (GPIO 0)
- **LED-Feedback:** eingebaute LED (GPIO 25)
- **Optionaler Akku** mit Spannungsüberwachung

### Einrichtung

1. Alle erforderlichen Bibliotheken im Arduino IDE installieren
2. Board auswählen: **Heltec WiFi LoRa 32 V3**
3. `LoRa_soundkorb.ino` flashen
4. Beim ersten Start: WiFi-AP `GO-GO-XXXXXX` verbinden, um WiFi und OSC-Ziel zu konfigurieren
5. Betriebsmodus im Gerätemenü wählen (3 schnelle Klicks)

---

[English README](../../README.md)
