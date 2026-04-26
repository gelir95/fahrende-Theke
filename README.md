# Bluepad32 auf dem ESP32 installieren (Arduino IDE)

Bluepad32 ist keine normale Arduino-Bibliothek – es wird als eigene Board-Plattform installiert und ersetzt dabei die Standard-ESP32-Plattform für den Sender-Sketch.

## Voraussetzungen

- Arduino IDE 2.x
- ESP32 Arduino Core bereits installiert (falls nicht: wird durch Bluepad32 mitgebracht)

---

## Installation

### 1. Board-Manager-URL hinzufügen

Arduino IDE öffnen → **File → Preferences** (Windows/Linux) oder **Arduino IDE → Settings** (macOS)

Im Feld **"Additional boards manager URLs"** folgende URL eintragen:

```
https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json
```

> Falls bereits andere URLs eingetragen sind, mit einem Komma trennen.

### 2. Bluepad32-Plattform installieren

**Tools → Board → Boards Manager** öffnen → nach `bluepad32` suchen → **ESP32 + Bluepad32** installieren.

### 3. Richtiges Board auswählen

**Tools → Board → ESP32 Bluepad32 Arduino** → passendes Board wählen, z.B.:
- `ESP32 Dev Module` für generische ESP32-Boards

> Wichtig: Das Board muss aus der **Bluepad32**-Gruppe gewählt werden, nicht aus der normalen ESP32-Gruppe – sonst fehlen die Bluepad32-Funktionen.

---

## Xbox Controller verbinden

### Unterstützte Controller
- Xbox One (Modell 1708 und neuer) mit Bluetooth
- Xbox Series X/S

### Pairing-Modus aktivieren
1. Controller einschalten (Xbox-Taste)
2. **Pairing-Taste** (kleine runde Taste oben an der Vorderseite) **3 Sekunden** gedrückt halten
3. Xbox-Logo blinkt schnell → Controller ist im Pairing-Modus

Bluepad32 verbindet sich beim ersten Start automatisch mit dem ersten gefundenen Controller. Nach dem ersten Pairing wird der Controller beim nächsten Einschalten automatisch wiederverbunden.

---

## Hinweise

- **WiFi + Bluetooth gleichzeitig**: ESP32 teilt das 2,4-GHz-Radio zwischen WiFi (ESP-NOW) und Bluetooth. Beides funktioniert parallel, kann aber gelegentlich zu leicht erhöhter Latenz führen.
- **Bluepad32 ersetzt `loop()`-Timing nicht**: Der eigene `loop()` läuft normal weiter, `BP32.update()` muss regelmäßig aufgerufen werden.
- **Aktuelle URL prüfen**: Die Board-Manager-URL kann sich bei neuen Releases ändern. Aktuelle URL immer auf der [offiziellen Bluepad32-GitHub-Seite](https://github.com/ricardoquesada/bluepad32) nachschlagen.
