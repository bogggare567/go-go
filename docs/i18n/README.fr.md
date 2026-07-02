# GO-GO (Français)

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

[English README](../../README.md)
