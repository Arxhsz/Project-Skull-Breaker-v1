<div align="center">

![](https://media0.giphy.com/media/v1.Y2lkPTc5MGI3NjExcTFyYXU1YXNmcTN3a290OG1hMnJuaWI3cXZqbGoxd284OThmYXB1YSZlcD12MV9pbnRlcm5hbF9naWZfYnlfaWQmY3Q9Zw/VWUeGlXiBrQsnmwTOv/giphy.gif)

# 💀 Skull Breaker

### ESP32 Multi-Radio Wireless Security "Research" Device

[![Platform](https://img.shields.io/badge/platform-ESP32-blue?style=flat-square)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/framework-Arduino-teal?style=flat-square)](https://www.arduino.cc/)
[![Build](https://img.shields.io/badge/build-PlatformIO-orange?style=flat-square)](https://platformio.org/)
[![Version](https://img.shields.io/badge/version-v1-red?style=flat-square)](#)
[![Status](https://img.shields.io/badge/status-work%20in%20progress-yellow?style=flat-square)](#known-issues-v1)

> **WiFi · BLE · 2.4GHz · Sub-GHz RF — all in one handheld unit**

</div>

---

> ⚠️ **This is v1 — a working proof of concept.** A lot of things are broken, half-baked, or just vibes right now. v2 is where the real polish happens. That said, the core is solid and it's already a pretty cool piece of kit.

---

## Hardware

| Component | Details |
|---|---|
| MCU | ESP32 (esp32dev) |
| Display | ILI9341 2.4" TFT (240×320) |
| Radio 1 | NRF24L01+ (2.4GHz primary) |
| Radio 2 | CC1101 (sub-GHz: 315 / 433 / 868 / 915 MHz) |
| Radio 3 | NRF24L01+ (2.4GHz secondary — dual jammer) |
| Storage | SD card + LittleFS (internal flash) |
| Power | LiPo battery with ADC monitoring |
| Input | 5-button directional pad (UP / DOWN / LEFT / RIGHT / SELECT) |
| Backlight | PWM-controlled via GPIO 13 |

### Pin Map

```
TFT CS:     15    TFT DC:      2    TFT RST:    4
SD CS:       5    CC1101 CS:  22    CC1101 GDO0: 12
BTN UP:     32    BTN DOWN:   33    BTN LEFT:   26
BTN RIGHT:  27    BTN SELECT: 25
BATTERY:    34    CHARGER:    35    BACKLIGHT:  13
```

---

## Features

### WiFi

**Sniffers**
- AP Scanner, Beacon Sniffer, Probe Sniffer, Deauth Sniffer
- EAPOL / PMKID Capture, Station Sniffer, Signal Monitor
- Channel Analyzer, Raw Packet Capture, Packet Counter / Monitor

**Attacks**
- Deauth, Beacon Flood, Rick Roll
- Evil Portal (SD-hosted custom HTML)
- Probe Flood, AP Clone, Channel Switch
- Association Sleep, Bad Message, Quiet Attack

---

### Bluetooth / BLE

**Sniffers**
- General BLE Scanner, BLE Analyzer, Car Skimmer Detector
- Flipper Zero Sniffer, AirTag Tracker, Flock Detector
- Meta Device Sniffer, Manufacturer Filter, BLE Radar, BLE Signal Logger

**Attacks**
- Sour Apple, Swift Pair Spam, Samsung Spam
- BLE Beacon Spam, BT Spam All, BLE Jammer
- BLE Spoofer (AirPods, PowerBeats, AirTags, Galaxy Buds, HomePod, Apple TV, Surface, Swift Pair)

---

### 2.4GHz (NRF24)

**Sniffers**
- Channel Scanner, Noise Analyzer, Protocol Scan

**Attacks**
- 2.4GHz Jammer — BLE / BT Classic / WiFi / Drone / Constant Carrier modes

---

### Sub-GHz RF (CC1101)

**Sniffers**
- RF Scanner, RF Monitor, Signal Capture, Rolling Code Capture
- Frequency Sweep with waterfall display

**Transmit**
- Signal Replay (TX), RF Jammer, Squelch Open, Sub file playback

---

### System
- OTA wireless firmware updates
- SD card file manager (rename, delete, text viewer)
- LittleFS internal storage
- Battery percentage + charger detection
- Screen saver with configurable timeout and styles
- Icon cache system
- Boot splash with custom image support
- FreeRTOS task management

---

## Building

Built with [PlatformIO](https://platformio.org/).

```bash
# Build
pio run

# Upload (default COM4 — change in platformio.ini)
pio run --target upload

# Serial monitor
pio device monitor
```

Dependencies are managed automatically by PlatformIO:

```ini
lib_deps =
  adafruit/Adafruit GFX Library
  adafruit/Adafruit ILI9341
  nrf24/RF24
  h2zero/NimBLE-Arduino
```

Partition scheme: `partitions_orion_ota.csv` (custom, supports OTA)

---

## Known Issues (v1)

There are **50+ known bugs** across the system. This is v1 — it's a starting point, not a finished product. The big ones:

### Hardware / Boot
- CC1101 (Radio 2) consistently fails to initialize at boot (`version=0x0`) — likely SPI timing or wiring issue
- Secondary NRF24 (Radio 3) also fails to detect — may not be present on all builds
- Radio 1 TX test fails at boot on some units

### WiFi
- Beacon sniffer shows garbled text (`Aa!@&#&bff`) when duplicate SSIDs are present
- Several WiFi AP list screens show a stray debug line (random AP / BSSID / RSSI in small text at the bottom)
- Hidden SSID app: unresponsive buttons on first load, slow, flickery, hard to exit
- Deauth sniffer captures too many false positives
- EAPOL / PMKID capture doesn't display results
- Signal Monitor shows no data
- Probe Flood, Bad Message, and Quiet attacks have no observable effect

### BLE
- BLE Sniffer screen flickers during background scans
- Flipper Zero detection only matches on device name — misses most Flippers
- BLE Manufacturer Sniffer apps hang on exit and require a hardware reset
- Flipper Sniff requires multiple left-presses to exit
- All BLE attacks have limited range compared to dedicated hardware
- BLE Spoofer only works reliably with AirPods and PowerBeats profiles
- Beacon Spam doesn't show up on mobile devices

### 2.4GHz
- Scanner and Noise Analyzer graphs show no data (regression — was working)
- Jammer UI flickers
- Wrong back button icon in submenus

### RF
- Most RF sniffer screens flicker
- Freq Sweep only renders outlines — full UI is broken (regression after RF updates)
- RF Jammer signal is weak
- Captured signal replay doesn't match original device output
- Squelch Open produces no audio — and a previous fix attempt broke the entire RF subsystem

### System
- Low memory warnings frequently block tool launches (`Mode guard blocked: Low memory`)
- Icon cache not always released between mode switches
- BLE stack requires warmup retries on init
- WiFi init logs interface errors on startup
- Some error states require manual hardware reset to recover

---

## Roadmap (v2)

v2 is a full ground-up redesign — new hardware, new firmware architecture, all the v1 bugs squashed.

### New Hardware Platform

The v2 board is a custom dual-layer perfboard build with a completely redesigned component layout:

| Component | v1 | v2 |
|---|---|---|
| MCU | ESP32 (generic dev board) | ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM, BLE 5.0) |
| Display | 2.4" ILI9341 (240×320) | 3.2" TFT — ST7789 or ILI9488 (up to 480×320) |
| 2.4GHz Radios | 1-2x bare NRF24L01+ | 2x E01-2G4M27D (NRF24L01+ with PA/LNA + SMA antenna) |
| Sub-GHz Radios | 1x CC1101 (unreliable) | 2x CC1101 433MHz with SMA antennas |
| GPS | None | ATGM336H (GPS / BeiDou / GLONASS / Galileo / QZSS) |
| Power | Basic LiPo + linear reg | TP4056 USB-C charging + TPS63020 buck-boost → stable 3.3V |
| Battery | Unknown | 3000mAh 3.7V LiPo |
| Construction | Dev board + breadboard | 8×12cm double-sided black perfboard, M3 nylon standoffs |

**Architecture — 2 physical layers:**
- **Layer 1 (top):** 3.2" TFT display, 5 tactile nav buttons, optional buzzer, optional status LED
- **Layer 2 (bottom):** ESP32-S3, power system, all 4 radio modules, GPS

### Firmware Fixes (all v1 bugs)

**Hardware / Radio Init**
- Proper SPI sequencing with per-device CS arbitration — only one CS active at a time
- Dedicated power filtering caps per radio (100µF + 10µF + 0.1µF each)
- CC1101 initialization with correct timing and SPI delays
- Dual CC1101 support — independent CS lines, verified probe sequence
- Dual NRF24 PA/LNA support — both radios initialized cleanly with power-down gaps

**UI / Display**
- Smarter redraw logic — only redraw when data actually changes, eliminating all flickering
- Fix beacon sniffer text corruption (buffer overflow on duplicate SSIDs)
- Remove stray debug line from all WiFi AP list screens
- Fix Freq Sweep UI — restore full spectrum graph, waterfall, labels, and controls
- Fix Car Skimmers text clipping and 2.4GHz submenu back button icons

**WiFi**
- Fix Hidden SSID app: button responsiveness on first load, single-press exit, performance pass
- Fix EAPOL / PMKID capture display
- Fix Signal Monitor data rendering
- Fix Probe Flood, Bad Message, and Quiet attacks — proper packet rates and construction
- Fix Deauth sniffer false positive rate

**BLE**
- Flipper Zero detection via UUID and BLE characteristics (not just name matching)
- Fix BLE Manufacturer Sniffer exit hang — proper scan task teardown on back press
- Maximize TX power (9 dBm) and optimize advertising intervals across all BLE attacks
- Fix BLE Spoofer for all profiles: AirTags, Galaxy Buds, Swift Pair, Surface, Apple TV, HomePod
- Fix Beacon Spam mobile visibility

**2.4GHz**
- Restore Scanner and Noise Analyzer graph data rendering (regression fix)
- Fix Jammer UI flickering
- Improve single-radio BLE jamming effectiveness

**RF**
- Fix Squelch Open and restore the entire RF subsystem broken by that patch
- Increase RF Jammer output power (PA table tuning)
- Fix captured signal replay timing and frequency correction
- Restore Freq Sweep full UI

**System**
- PSRAM-backed icon cache — 8MB PSRAM eliminates low memory blocking
- Graceful error recovery — no more manual resets required
- Reliable BLE stack init — proper WiFi shutdown delay before BLE bring-up
- Fix WiFi interface registration error on init
- Modular firmware architecture — cleaner separation between radio drivers, UI, and app logic

### New in v2

- **GPS integration** — location tagging for captured signals and scan results
- **Dual CC1101** — simultaneous sub-GHz monitoring and transmission
- **Dual NRF24 PA/LNA** — stronger 2.4GHz jamming and scanning with real antennas
- **USB-C charging** — no more proprietary connectors
- **Buck-boost regulation** — stable 3.3V even as battery drains
- **Passive buzzer** — audio feedback for captures and alerts
- **Bigger display** — more screen real estate for waterfall, graphs, and device lists
- **Skull-inspired industrial UI** — minimal black/gray aesthetic, fast navigation, compact tactical interface

---

## Disclaimer

This device is intended for **authorized security research and educational use only**. Using these tools against networks or devices you do not own or have explicit permission to test is illegal in most jurisdictions. The authors take no responsibility for misuse.

---

<div align="center">

*💀 Skull Breaker v1 — v2 is coming.*

</div>
