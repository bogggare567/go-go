# GO-GO (LoRa_soundkorb) — контекст проекта

Прошивка для Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262 + SSD1306): однокнопочный
театральный пульт GO/PANIC. Режимы: OSC/WiFi (QLab), BLE HID-клавиатура,
LoRa TX (пульт), LoRa RX (гейтвей → OSC или BLE).

## Главное

- **Все планы и идеи — в [docs/ROADMAP.md](docs/ROADMAP.md).** Перед обсуждением новых
  фич читать его; новые договорённости записывать туда же, а не только в чат.
- Текущая версия: v15, бета. Работает, но UX сырой. Владелец: Богдан, звукорежиссёр
  (Москва), сайт soundkorb.ru. Общение на русском.
- GitHub: https://github.com/bogggare567/go-go. Пуш — только после явного «ок» владельца.

## Правила работы

- **В коммитах и PR — никаких `Co-Authored-By: Claude` и другой атрибуции.**
  Владелец просил не появляться в соавторах; история уже переписана.

- **Каждую правку прошивки проверять компиляцией** (arduino-cli 1.5.1 установлен, brew):
  ```
  arduino-cli compile --fqbn "Heltec-esp32:esp32:heltec_wifi_lora_32_V3:UploadSpeed=921600,CPUFreq=240,DebugLevel=none,LoopCore=1,EventsCore=1,LORAWAN_REGION=0,LoRaWanDebugLevel=0,LORAWAN_PREAMBLE_LENGTH=0,SLOW_CLK_TPYE=0" .
  ```
  Прошивает владелец через Arduino IDE или esptool.
- ⚠ В тулчейне Heltec esp-14.2.0_20260121 не хватало стандартного заголовка `<numbers>`
  (баг упаковки, ломает любую сборку) — восстановлен вручную в
  `~/Library/Arduino15/packages/Heltec-esp32/tools/xtensa-esp-elf-gcc/.../include/c++/14.2.0/numbers`.
  После переустановки/обновления ядра файл пропадёт — вернуть.
- Перед крупными правками прошивки делать копию `.bak` (в .gitignore).
- `build/` в git не добавлять. Бинарники релизов — через GitHub Releases.

## Структура прошивки (v16, после разбиения)

- `LoRa_soundkorb.ino` — глобальное состояние + setup/loop (кнопочная логика пока тут)
- `config.h` — пины, константы, enum'ы, структуры, extern-объявления глобалов
- `gogo.h` — зонтичный инклюд; каждый .cpp включает только его
- Модули: `util`, `settings` (NVS), `battery` (заряд+LED), `osc_wifi`, `ble_hid`,
  `radio` (LoRa-протокол: LoRaPacket, PKT_*, CRC16, sendCommand 3x), `modes`
  (жизненный цикл режимов, performGO/performPanic), `ui_screens` (экраны, меню, boot-анимация)
- Кнопка: 1 клик = GO, удержание 1.4 с = PANIC, 3 быстрых клика = меню
- Настройки в Preferences (неймспейсы: osc, system, radio); счётчик пакетов
  резервируется блоком +1000 на старте — в sendPacket записи во флеш нет
