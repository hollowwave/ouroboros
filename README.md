# OUROBOROS

> A DIY wireless security research tool built on ESP32 DevKit v1.
> Red on black. Snake eating its tail. Signals in, signals out.

Built from scratch — no Marauder fork, clean architecture, every line intentional.

---

## Hardware

| Component | Part |
|---|---|
| MCU | ESP32 DevKit v1 (WROOM-32) |
| Display | 1.44" ST7735 TFT (128×128) |
| Sub-GHz Radio | CC1101 |
| Navigation | 4× Tactile buttons |

---

## Features

### WiFi
- AP scanner (hidden SSIDs included)
- Deauth — scrollable target picker, broadcast or single AP
- Beacon spam (rotating SSIDs, all channels)
- Probe request sniffer (channel-hopping)
- RSSI mapper — live AP list + channel graph, auto-rescan every 3s

### Bluetooth
- BLE device scanner
- BLE advertisement spam — Apple, Samsung (Fast Pair), Windows (Swift Pair)

### Sub-GHz (CC1101)
- Signal scanner with RSSI bar graph
- Raw signal capture
- Raw signal replay
- Rolling code detector — analyzes captures, tells you if a remote is fixed or rolling code
- Full config: frequency (300–928 MHz), modulation, bandwidth, TX power, packet length
- NVS persistence — config survives reboots

### UI
- Animated Ouroboros splash screen (rotating snake ring)
- Red on black theme throughout
- Status bar with active module indicator dot
- Hint bar on every screen
- Signal bar icons (5-bar WiFi style) for RSSI
- Ouroboros spinner on waiting screens (capture, probe sniff)

---

## Wiring

### ST7735 Display (SPI)
| Pin | ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| CS | GPIO 5 |
| RESET | GPIO 4 |
| DC/A0 | GPIO 2 |
| SDA/MOSI | GPIO 23 |
| SCK | GPIO 18 |
| LED/BL | 3.3V |

### CC1101 (shared SPI bus)
| Pin | ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| SCK | GPIO 18 |
| CSN | GPIO 15 |
| GDO0 | GPIO 22 |
| GDO2 | GPIO 21 |

> Both devices share MOSI/SCK on GPIO 23/18. CS pins (5 and 15) keep them separate.

### Buttons (active LOW, internal pull-up — one leg to GPIO, other to GND)
| Button | ESP32 |
|---|---|
| UP | GPIO 12 |
| DOWN | GPIO 13 |
| SELECT | GPIO 14 |
| BACK | GPIO 27 |

---

## Button Controls

| Button | Press | Long Press | Double Click |
|---|---|---|---|
| UP | Cursor up / value +1 | Fast increment | Jump to top |
| DOWN | Cursor down / value -1 | Fast decrement | Jump to bottom |
| SELECT | Enter / confirm | **Start/stop module** | Secondary action |
| BACK | Go back | **Jump to main menu** | — |

---

## Project Structure

```
ouroboros/
├── src/
│   ├── main.cpp
│   ├── ui/
│   │   ├── Display.h/cpp         # TFT_eSPI wrapper + themed draw calls
│   │   ├── Menu.h/cpp            # State machine menu
│   │   ├── Ouroboros.h/cpp       # Animated snake spinner
│   │   └── ConfigScreen.h/cpp    # Sub-GHz interactive config editor
│   ├── modules/
│   │   ├── WiFiAttack.h/cpp      # Scan, deauth, beacon spam, probe sniff
│   │   ├── WiFiMapper.h/cpp      # RSSI mapper — AP list + channel graph
│   │   ├── BLEModule.h/cpp       # BLE scan + spam
│   │   ├── SubGHz.h/cpp          # CC1101 scan / capture / replay
│   │   ├── RollingCodeDetector.h/cpp  # Fixed vs rolling code analysis
│   │   ├── DeauthPicker.h/cpp    # AP target selector
│   │   └── NVSConfig.h/cpp       # Persistent config via ESP32 NVS
│   └── utils/
│       └── Buttons.h/cpp         # OneButton — click / long / double
├── include/
│   └── config.h                  # Pins, colors, constants
├── platformio.ini
└── README.md
```

---

## Architecture

```
[ Buttons ] → [ Menu State Machine ] → [ Feature Modules ] → [ Display ]
                      ↕                        ↕
              [ ConfigScreen ]           [ Ouroboros ]
              [ DeauthPicker ]           (spinner overlay)
              [ RollingCodeDetector ]
              [ WiFiMapper ]
```

- Every screen is a state — adding a feature = new enum value + module file
- `SELECT_LONG` universally starts/stops the active module
- `BACK_LONG` universally goes home
- All modules log to Serial at 115200 for easy debugging
- Sub-GHz config auto-saves to NVS on exit

---

## Dependencies

```ini
lib_deps =
    bodmer/TFT_eSPI
    lsatan/SmartRC-CC1101-Driver-Lib
    mathertel/OneButton
```

---

## Build & Flash

```bash
git clone https://github.com/yourusername/ouroboros
cd ouroboros
pio run --target upload
pio device monitor --baud 115200
```

---

## Legal

For **authorized security testing and educational use only**.
Always test on networks and devices you own or have written authorization to test.

---

## License

MIT
