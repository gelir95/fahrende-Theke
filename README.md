# Fahrende Theke — ESP32 Motorsteuerung per Fernbedienung

ESP-NOW-basierte Funk-Fernsteuerung für einen Elektromotor, entwickelt für die "Theke".

## Projektübersicht

Zwei ESP32-Boards kommunizieren über ESP-NOW (kein Router nötig):

- **Sender**: Liest zwei Joysticks (VR = vor/zurück, RL = links/rechts) und einen Geschwindigkeits-Poti. Sendet alle 50ms ein Paket an den Receiver.
- **Receiver**: Empfängt die Joystick-Werte, verarbeitet sie und gibt Steuerbefehle an einen nachgelagerten PID-Motorregler aus (über Serial, 9600 Baud).

### Funktionen

- **Expo-Kurve**: Nicht-linearer Joystick-Response — kleine Ausschläge reagieren sanft, voller Ausschlag gibt vollen Wert
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

### 1. lib_deps in platformio.ini eintragen

```ini
[env:receiver]
...
lib_deps = https://github.com/ricardoquesada/bluepad32.git
```

### 2. BP32.update() im Loop aufrufen

```cpp
#include <Bluepad32.h>

void loop() {
  BP32.update();
  // Controller-Daten über BP32.myControllers() abrufen
}
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
