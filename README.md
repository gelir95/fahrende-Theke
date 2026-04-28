# Fahrende Theke — ESP32 Motorsteuerung per Fernbedienung

ESP-NOW-basierte Funk-Fernsteuerung für einen Elektromotor, entwickelt für die "Theke".

## Projektübersicht

Zwei ESP32-Boards kommunizieren über ESP-NOW (kein Router nötig):

- **Sender**: Liest zwei Joysticks (VR = vor/zurück, RL = links/rechts) und einen Geschwindigkeits-Poti. Sendet alle 50ms ein Paket an den Receiver.
- **Receiver**: Empfängt die Joystick-Werte, verarbeitet sie und gibt Steuerbefehle an einen nachgelagerten PID-Motorregler aus (über Serial, 9600 Baud).

### Funktionen

- **Expo-Kurve**: Nicht-linearer Joystick-Response — kleine Ausschläge reagieren sanft, voller Ausschlag gibt vollen Wert
- **Totzone (Deadzone)**: Eingaben unter einem konfigurierbaren Schwellwert werden als 0 behandelt — verhindert Drift bei losgelassenem Stick (`DEADZONE_VR`, `DEADZONE_RL` in Prozent des Maximalwerts)
- **Beschleunigungs-/Verzögerungs-Ramp**: Werte ändern sich schrittweise, kein abruptes Umschalten
- **Signalverlust-Failsafe**: Kein Paket seit 200ms → Motor bremst kontrolliert auf 0
- **Notbremsung**: Joystick ≥98% in Gegenrichtung → sofortiger Stopp
- **MAC-Lock**: Receiver akzeptiert nur den ersten Sender der sich meldet, ignoriert andere

### Pinbelegung

**Sender:**
| Pin | Funktion |
|-----|----------|
| GPIO35 | Joystick VR (vor/zurück) |
| GPIO34 | Joystick RL (links/rechts) |
| GPIO32 | Geschwindigkeit (Poti) |
| GPIO33 | Hupe (Button) |

**Receiver:**
| Pin | Funktion |
|-----|----------|
| GPIO5 | Hupe (LED/Summer) |
| TX0 | Serieller Output an PID-Regler |

### Serielle Ausgabe (Receiver)

```
VR:-1842    <- vor/zurück (-2047 bis +2047)
RL:+650     <- rechts/links (-1300 bis +1300)
```

---

## Entwicklungsumgebung

Das Projekt nutzt **PlatformIO** (VS Code Extension) mit dem [pioarduino](https://github.com/pioarduino/platform-espressif32) Platform-Fork für ESP32 Arduino Core 3.x.

### Voraussetzungen

- VS Code mit PlatformIO Extension
- Kein separater Arduino-Core nötig — PlatformIO lädt alles automatisch

### Bauen und Flashen

```bash
# Sender bauen und flashen
pio run -e sender --target upload

# Receiver bauen und flashen
pio run -e receiver --target upload

# Nur bauen (ohne Upload)
pio run -e sender
pio run -e receiver
```

### MAC-Adresse auslesen

Vor dem ersten Einsatz die MAC-Adresse des Receiver-Boards auslesen und in `src/sender/main.cpp` eintragen:

```bash
# tools/get_mac auf den Receiver flashen
# MAC wird im Serial Monitor ausgegeben (115200 Baud)
# Dann broadcastAddress in src/sender/main.cpp anpassen
```

---

## Bluepad32 installieren (PlatformIO)

Bluepad32 ermöglicht die Verbindung eines Xbox-Controllers per Bluetooth direkt auf dem Receiver-ESP32, parallel zu ESP-NOW.

### Aktuelle Einschränkung

Das offizielle Bluepad32-Arduino-Paket basiert derzeit auf **Arduino-ESP32 Core 2.x**. Der Receiver nutzt deshalb ein angepasstes Framework-Paket (Repackaging von maxgerhardt), das Bluepad32 bereits eingebaut hat.

Das ist in `platformio.ini` so eingebunden:

```ini
[env:receiver]
platform = espressif32@6.10.0
platform_packages =
    framework-arduinoespressif32@https://github.com/maxgerhardt/pio-framework-bluepad32/archive/refs/heads/main.zip
```

### Upgrade auf Core 3.x (sobald verfügbar)

Sobald [ricardoquesada/esp32-arduino-lib-builder](https://github.com/ricardoquesada/esp32-arduino-lib-builder/releases) ein Core-3.x-Paket veröffentlicht:

1. `platform_packages` aus `platformio.ini` entfernen
2. `platform` zurück auf pioarduino setzen:
   ```ini
   platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip
   ```
3. ESP-NOW Callback in `src/receiver/main.cpp` auf Core 3.x anpassen:
   ```cpp
   // Core 2.x (aktuell):
   void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)

   // Core 3.x (nach Upgrade):
   void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
   // mac = recv_info->src_addr
   ```

> **Hinweis**: Bluepad32 und ESP-NOW können parallel laufen — der ESP32 teilt das 2,4-GHz-Radio zwischen WiFi (ESP-NOW) und Bluetooth. Beide funktionieren gleichzeitig, können aber gelegentlich zu leicht erhöhter Latenz führen.

---

## Bluepad32 installieren (Arduino IDE)

Falls Arduino IDE statt PlatformIO verwendet wird:

Bluepad32 ist keine normale Arduino-Bibliothek — es wird als eigene Board-Plattform installiert.

### 1. Board-Manager-URL hinzufügen

**File → Preferences** → Feld "Additional boards manager URLs":

```
https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json
```

### 2. Bluepad32-Plattform installieren

**Tools → Board → Boards Manager** → `bluepad32` suchen → **ESP32 + Bluepad32** installieren.

### 3. Richtiges Board auswählen

**Tools → Board → ESP32 Bluepad32 Arduino** → `ESP32 Dev Module`

> Das Board muss aus der **Bluepad32**-Gruppe gewählt werden, nicht aus der normalen ESP32-Gruppe.

---

## Xbox Controller verbinden

### Unterstützte Controller
- Xbox One (Modell 1708 und neuer) mit Bluetooth
- Xbox Series X/S

### Pairing
1. Controller einschalten (Xbox-Taste)
2. **Pairing-Taste** (kleine runde Taste oben) **3 Sekunden** gedrückt halten
3. Xbox-Logo blinkt schnell → Pairing-Modus aktiv

Bluepad32 verbindet sich beim ersten Start automatisch mit dem ersten gefundenen Controller. Nach dem ersten Pairing wird der Controller beim nächsten Einschalten automatisch wiederverbunden.
