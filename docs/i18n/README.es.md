# GO-GO (Español)

GO-GO es un firmware de Arduino para el módulo **Heltec WiFi LoRa 32 V3** que lo convierte en un mando a distancia inalámbrico de un botón para teatro, conciertos y eventos en directo.

### Qué hace

Una pulsación simple envía el comando **GO** (avanzar cue, reproducir sonido, activar efecto). Una pulsación larga envía **PANIC** (cortar todo, detener todos los cues). Tres clics rápidos abren el menú de configuración en la pantalla OLED.

### Modos de operación

| Modo | Descripción |
|------|-------------|
| **OSC / WiFi** | Envía mensajes OSC por UDP — compatible con QLab, Ableton Live y cualquier software con OSC |
| **BLE / HID** | Funciona como teclado Bluetooth — GO = Espacio, PANIC = Escape. Sin configuración de software |
| **LoRa Remote** | Transmite comandos por radio LoRa a una pasarela emparejada — para largas distancias |
| **LoRa Gateway** | Recibe paquetes LoRa y los reenvía como OSC o BLE |

### Hardware

- **Módulo:** Heltec WiFi LoRa 32 V3 (ESP32-S3, SX1262 LoRa, OLED SSD1306 128×64)
- **Botón:** botón PRG integrado (GPIO 0)
- **LED de estado:** LED integrado (GPIO 25)
- **Batería opcional** con monitoreo de voltaje

### Configuración

1. Instalar todas las bibliotecas requeridas en Arduino IDE
2. Seleccionar placa: **Heltec WiFi LoRa 32 V3**
3. Flashear `LoRa_soundkorb.ino`
4. En el primer arranque: conectarse al AP WiFi `GO-GO-XXXXXX` para configurar WiFi y destino OSC
5. Elegir el modo de operación en el menú del dispositivo (3 clics rápidos)

---

[English README](../../README.md)
