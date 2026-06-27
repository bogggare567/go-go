# GO-GO

**Theatre remote control firmware for Heltec WiFi LoRa 32 V3**

---

## Flash without Arduino IDE (Mac / Linux)

No IDE needed — just Python and a USB cable.

**1. Install esptool**
```bash
pip install esptool
```

**2. Connect the board via USB, find the port**
```bash
ls /dev/cu.usb*
# Usually: /dev/cu.usbserial-XXXX  or  /dev/cu.SLAB_USBtoUART
```

**3. Flash**
```bash
esptool.py \
  --chip esp32s3 \
  --port /dev/cu.usbserial-XXXX \
  --baud 921600 \
  --flash-mode dio \
  --flash-freq 80m \
  --flash-size 8MB \
  write_flash \
  0x0    firmware/bootloader.bin \
  0x8000 firmware/partitions.bin \
  0xe000 firmware/boot_app0.bin \
  0x10000 firmware/go-go.bin
```

Replace `/dev/cu.usbserial-XXXX` with your actual port from step 2.

After flashing, the board resets automatically and boots GO-GO.

---

## English

GO-GO is an Arduino firmware for the **Heltec WiFi LoRa 32 V3** board that turns it into a one-button wireless show control remote for live theatre, concerts, and events.

### What it does

A single button press sends a **GO** command (advance cue, play sound, fire effect). A long hold sends a **PANIC** command (cut everything, stop all cues). Three fast clicks open the settings menu on the OLED display.

### Operating modes

| Mode | Description |
|------|-------------|
| **OSC / WiFi** | Sends OSC messages over UDP — works directly with QLab, QLab Standby, Ableton Live, and any OSC-capable software |
| **BLE / HID** | Acts as a Bluetooth keyboard — GO = Space, PANIC = Escape. No software setup needed |
| **LoRa Remote** | Transmits commands via LoRa radio to a paired gateway — for long range or RF-only environments |
| **LoRa Gateway** | Receives LoRa packets and forwards them as OSC or BLE |

### Hardware

- **Board:** Heltec WiFi LoRa 32 V3 (ESP32-S3, SX1262 LoRa, SSD1306 OLED 128×64)
- **Single button:** built-in PRG button (GPIO 0)
- **LED feedback:** built-in LED (GPIO 25)
- **Optional battery** with voltage monitoring

### Libraries required

- `Heltec ESP32 Dev-Boards` (board package)
- `WiFiManager`
- `OSCMessage` (CNMAT)
- `NimBLE-Arduino`
- `Adafruit SSD1306` + `Adafruit GFX`
- `SX126x-Arduino`

### Setup

1. Install all required libraries in Arduino IDE
2. Select board: **Heltec WiFi LoRa 32 V3**
3. Flash `LoRa_soundkorb.ino`
4. On first boot, connect to the WiFi AP `GO-GO-XXXXXX` to configure WiFi + OSC target
5. Choose your operating mode in the on-device menu (3 fast clicks)

---

## Deutsch

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

## Français

GO-GO est un firmware Arduino pour le module **Heltec WiFi LoRa 32 V3** qui le transforme en télécommande sans fil à un bouton pour le théâtre, les concerts et les événements en direct.

### Ce que ça fait

Un simple appui envoie la commande **GO** (avancer le cue, jouer un son, déclencher un effet). Un maintien prolongé envoie **PANIC** (tout couper, arrêter tous les cues). Trois clics rapides ouvrent le menu de configuration sur l'écran OLED.

### Modes de fonctionnement

| Mode | Description |
|------|-------------|
| **OSC / WiFi** | Envoie des messages OSC par UDP — compatible avec QLab, Ableton Live et tout logiciel OSC |
| **BLE / HID** | Fonctionne comme un clavier Bluetooth — GO = Espace, PANIC = Échap. Aucune configuration logicielle requise |
| **LoRa Remote** | Transmet les commandes via radio LoRa vers une passerelle couplée — pour les grandes distances |
| **LoRa Gateway** | Reçoit les paquets LoRa et les relaie en OSC ou BLE |

### Matériel

- **Module:** Heltec WiFi LoRa 32 V3 (ESP32-S3, SX1262 LoRa, OLED SSD1306 128×64)
- **Bouton:** bouton PRG intégré (GPIO 0)
- **Retour LED:** LED intégrée (GPIO 25)
- **Batterie optionnelle** avec surveillance de tension

### Installation

1. Installer toutes les bibliothèques requises dans Arduino IDE
2. Sélectionner la carte : **Heltec WiFi LoRa 32 V3**
3. Flasher `LoRa_soundkorb.ino`
4. Au premier démarrage : se connecter au point d'accès WiFi `GO-GO-XXXXXX` pour configurer le WiFi et la cible OSC
5. Choisir le mode de fonctionnement dans le menu embarqué (3 clics rapides)

---

## Español

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

## Русский

GO-GO — прошивка Arduino для модуля **Heltec WiFi LoRa 32 V3**, которая превращает его в беспроводной одно-кнопочный пульт управления для театра, концертов и живых мероприятий.

### Что делает

Одно нажатие кнопки отправляет команду **GO** (следующий кью, запуск звука, триггер эффекта). Долгое удержание отправляет **PANIC** (стоп всему, сброс всех кью). Три быстрых клика открывают меню настроек на OLED-экране.

### Режимы работы

| Режим | Описание |
|-------|----------|
| **OSC / WiFi** | Отправляет OSC-сообщения по UDP — работает с QLab, Ableton Live и любым OSC-совместимым ПО |
| **BLE / HID** | Работает как Bluetooth-клавиатура — GO = Пробел, PANIC = Escape. Никакой настройки ПО |
| **LoRa Remote** | Передаёт команды по радио LoRa на сопряжённый шлюз — для большого расстояния |
| **LoRa Gateway** | Принимает LoRa-пакеты и пересылает их как OSC или BLE |

### Железо

- **Плата:** Heltec WiFi LoRa 32 V3 (ESP32-S3, SX1262 LoRa, OLED SSD1306 128×64)
- **Кнопка:** встроенная кнопка PRG (GPIO 0)
- **LED-индикация:** встроенный светодиод (GPIO 25)
- **Опциональный аккумулятор** с контролем заряда

### Установка

1. Установить все необходимые библиотеки в Arduino IDE
2. Выбрать плату: **Heltec WiFi LoRa 32 V3**
3. Прошить `LoRa_soundkorb.ino`
4. При первом запуске: подключиться к точке доступа `GO-GO-XXXXXX`, настроить WiFi и адрес OSC-получателя
5. Выбрать режим работы в меню устройства (3 быстрых клика)

---

## License

MIT
