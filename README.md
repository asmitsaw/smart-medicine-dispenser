# 💊 Smart Medicine Dispenser

> An IoT-powered automatic medicine dispenser with AI camera detection, IR sensor confirmation, real-time push notifications, and a Progressive Web App dashboard — built on ESP32 + Blynk + Python + MediaPipe.

---

## 📋 Table of Contents

- [Overview](#overview)
- [System Architecture](#system-architecture)
- [Hardware Components](#hardware-components)
- [Wiring Reference](#wiring-reference)
- [Virtual Pin Map (Blynk)](#virtual-pin-map-blynk)
- [File Structure](#file-structure)
- [Setup Guide](#setup-guide)
  - [1. Blynk Console Setup](#1-blynk-console-setup)
  - [2. ESP32 Firmware](#2-esp32-firmware)
  - [3. Python Detection Script](#3-python-detection-script)
  - [4. Web Dashboard (PWA)](#4-web-dashboard-pwa)
- [How It Works — Full Flow](#how-it-works--full-flow)
- [Features](#features)
- [Push Notifications](#push-notifications)
- [Troubleshooting](#troubleshooting)
- [Dependencies](#dependencies)

---

## Overview

The Smart Medicine Dispenser automatically dispenses medicine at scheduled times and verifies whether the patient actually took it using:

1. **IR Sensors** (primary — physical pickup detection)
2. **AI Camera** (secondary — hand-to-mouth gesture via MediaPipe)

Results are synced to a beautiful Neo-Brutal PWA dashboard in real time, with native browser push notifications for every event.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                        BLYNK CLOUD                           │
│   V0/V4/V5 Timers  ·  V2 Trigger  ·  V3 Status  ·  V10 Push │
└────────────┬───────────────────────────┬─────────────────────┘
             │                           │
     ┌───────▼────────┐         ┌────────▼──────────┐
     │   ESP32 (C++)  │◄────────│  detect.py (PC)   │
     │   semi.ino     │ V2,V9   │  MediaPipe + CV2  │
     │                │────────►│                   │
     └───────┬────────┘         └───────────────────┘
             │
     ┌───────▼────────────────────────────┐
     │         HARDWARE                   │
     │  Servo · LCD · Buzzer · IR1 · IR2  │
     │  RTC DS1307 · LED indicators       │
     └────────────────────────────────────┘
             │
     ┌───────▼──────────────┐
     │   PWA Dashboard       │
     │   index.html (Browser)│
     │   Push Notifications  │
     └───────────────────────┘
```

---

## Hardware Components

| # | Component | Purpose |
|---|-----------|---------|
| 1 | **ESP32 Dev Module** | Main MCU — WiFi, logic, servo control |
| 2 | **DS1307 RTC Module** | Real-time clock (backed by battery) |
| 3 | **SG90 Servo Motor** | Rotates to dispense medicine |
| 4 | **JHD162A LCD 16×2** | Display time and status messages |
| 5 | **I2C Adapter (PCF8574)** | LCD I2C interface (4-wire connection) |
| 6 | **IR Sensor × 2** | Detect hand/medicine pickup physically |
| 7 | **Active Buzzer** | Audio alert during dose time |
| 8 | **ESP32-CAM (optional)** | Camera module for AI detection |
| 9 | **PC Webcam** | Used by `detect.py` for gesture detection |

---

## Wiring Reference

### I2C Bus (shared by RTC + LCD)

| Signal | ESP32 GPIO |
|--------|-----------|
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### Full Pin Map

| Component | Pin | ESP32 GPIO |
|-----------|-----|-----------|
| Servo (SG90) | Signal | GPIO 13 |
| Buzzer | + | GPIO 27 |
| IR Sensor 1 | OUT | GPIO 34 |
| IR Sensor 2 | OUT | GPIO 35 |
| LCD/RTC SDA | SDA | GPIO 21 |
| LCD/RTC SCL | SCL | GPIO 22 |
| RTC VCC | 3.3 V | 3V3 |
| LCD VCC | 5 V | VIN |
| All GND | GND | GND |

> ⚠️ **Critical:** All GND must share a common rail. Servo may need external 5 V if ESP resets during rotation.

---

## Virtual Pin Map (Blynk)

| Pin | Direction | Type | Description |
|-----|-----------|------|-------------|
| **V0** | Web → ESP32 | Integer (Time) | Morning dose time (seconds from midnight) |
| **V4** | Web → ESP32 | Integer (Time) | Afternoon dose time |
| **V5** | Web → ESP32 | Integer (Time) | Night dose time |
| **V1** | Web → ESP32 | Integer | Manual dispense trigger (write 1) |
| **V2** | ESP32 → Python | Integer | Dispensed flag (1=dispensed, 0=done) |
| **V3** | ESP32 → Web | String | Human-readable status string |
| **V9** | Python → ESP32 | Integer | Camera result (1=Taken, 0=Missed) |
| **V10** | ESP32/Python → Web | String | Push notification message |

> **Timer encoding:** Web sends `H × 3600 + M × 60` (seconds from midnight).  
> Example: `08:30` → `30600` · `23:51` → `85860`

---

## File Structure

```
sensor/
├── semi/
│   └── semi.ino          # ESP32 firmware (Arduino C++)
├── detect.py             # Python AI detection script (runs on PC)
├── index.html            # PWA dashboard (Neo-Brutal UI)
├── sw.js                 # Service Worker (PWA + push handler)
├── manifest.json         # PWA manifest
├── reqirements.txt       # Python dependencies
├── image.png             # PWA icon
└── README.md             # This file
```

---

## Setup Guide

### 1. Blynk Console Setup

1. Go to [blynk.cloud](https://blynk.cloud) → **Templates**
2. Open template `TMPL39IHNO8eS` (SMART MEDICINE DISPENSARY)
3. Go to **Datastreams** and ensure these exist:

| Datastream | Pin | Type | Min | Max |
|------------|-----|------|-----|-----|
| Morning Time | V0 | Integer | 0 | 86400 |
| Afternoon Time | V4 | Integer | 0 | 86400 |
| Night Time | V5 | Integer | 0 | 86400 |
| Manual Dispense | V1 | Integer | 0 | 1 |
| Cam Trigger | V2 | Integer | 0 | 1 |
| Status | V3 | String | — | — |
| Cam Result | V9 | Integer | 0 | 1 |
| Push Message | V10 | String | — | — |

> ⚠️ **Min/Max for V0/V4/V5 must be 0–86400** or the HTTP API will return `HTTP 400`.

---

### 2. ESP32 Firmware

#### Libraries Required (Arduino IDE)

Install via **Sketch → Include Library → Manage Libraries**:

| Library | Author |
|---------|--------|
| `BlynkSimpleEsp32` | Blynk |
| `LiquidCrystal I2C` | Frank de Brabander |
| `RTClib` | Adafruit |
| `ESP32Servo` | Kevin Harrington |
| `Time` (TimeLib) | Paul Stoffregen |

#### Configure WiFi credentials in `semi.ino`

```cpp
char ssid[] = "YourWiFiName";
char pass[] = "YourWiFiPassword";
```

#### Upload

1. Open `semi/semi.ino` in Arduino IDE
2. Select **Board**: `ESP32 Dev Module`
3. Select correct **COM Port**
4. Click **Upload**
5. Open **Serial Monitor** at `115200` baud — you should see:
   ```
   [BLYNK] Connected - syncing V0/V4/V5...
   RTC set HH:MM:SS
   Status: System Ready
   ```

---

### 3. Python Detection Script

#### Requirements

- Python **3.11** (required — MediaPipe is not compatible with 3.12+)
- A working webcam

#### Install dependencies

```bash
py -3.11 -m pip install opencv-python mediapipe==0.10.9 protobuf==3.20.3 requests
```

> If you get a `protobuf` conflict, pin it to `3.20.3` as above.

#### Run

```bash
# From the sensor/ directory
py -3.11 detect.py
```

The script runs **headlessly** in the background, polling Blynk `V2` every 2 seconds. It only opens the camera window when the ESP32 signals that medicine has been dispensed.

#### What detect.py does

```
Loop every 2s:
  Poll V2
  If V2 == 1:
    Open webcam
    Watch 18s for hand-to-mouth gesture (MediaPipe Hands + Face)
    If detected → write V9=1, V3="Medicine Taken", V10="Camera confirmed"
    If timeout   → write V9=0, V3="Missed Dose",   V10="MISSED!"
    Reset V2=0
  If V2 becomes 0 mid-detection (IR confirmed first):
    Close camera early (background watcher thread)
```

---

### 4. Web Dashboard (PWA)

Open `index.html` directly in a browser (Chrome recommended).

#### First time setup

1. Click **🔔 Notify** button in the header
2. Allow browser notifications when prompted
3. Notifications will now appear natively for every dose event

#### Setting Timers

1. In the **Schedule** section, pick a time using the time picker
2. Click **Set Morning / Set Afternoon / Set Night**
3. Toast shows `✅ Morning alarm set for 08:30`
4. Serial Monitor confirms: `[TIMER] Morning → 08:30`

#### Manual Dispense

Click **⚡ Dispense Medicine** to immediately trigger one dose release.

---

## How It Works — Full Flow

```
Scheduled dose time arrives (or Manual Dispense clicked)
      │
      ▼
ESP32 → servo rotates 90° → medicine drops
ESP32 → V2 = 1             (signals Python cam)
ESP32 → V3 = "Take Medicine!"
ESP32 → V10 = "Time to take your medicine!"
ESP32 → buzzer starts beeping (400ms toggle)
      │
      ├──── PRIORITY 1: IR Sensors ─────────────────────────────
      │     Both IR1 & IR2 LOW = hand in dispenser
      │     → buzzer OFF, V2=0 (camera closes early)
      │     → V3 = "Medicine Taken", V10 = "IR confirmed"
      │     → alertActive = false  ✓
      │
      ├──── PRIORITY 2: Camera (detect.py) ────────────────────
      │     Hand-to-mouth detected within 18s
      │     → V9=1, V3="Medicine Taken", V10="Cam confirmed"
      │     → ESP32 BLYNK_WRITE(V9) → alertActive = false  ✓
      │
      ├──── PRIORITY 3: Camera Timeout (20s) ──────────────────
      │     detect.py never responded → re-reads IR snapshot
      │     If IR LOW → "Medicine Taken (IR Fallback)"
      │     If IR HIGH → "Missed Dose (Timeout)"
      │
      └──── PRIORITY 4: Overall Timeout (30s) ─────────────────
            → V3 = "Missed Dose", V10 = "MISSED!"
            → alertActive = false  ✓

Web Dashboard (polling every 2–3s):
  V3 → Status card + toast
  V2 → Camera card animation (pulsing yellow while watching)
  V10 → nativeNotify() browser popup
```

---

## Features

| Feature | Implementation |
|---------|---------------|
| 3 daily dose timers | RTC-based, set remotely via PWA |
| Servo dispense | SG90 on GPIO 13, 90° rotation |
| NTP time sync | Syncs RTC to IST on every boot |
| IR pickup detection | GPIO 34 & 35, priority-1 confirmation |
| AI camera detection | MediaPipe Hands + Face, 18s window |
| Camera early exit | Background thread watches V2; exits if IR confirms first |
| Buzzer alert | Non-blocking 400ms toggle, stops on any confirmation |
| LCD status | I2C 16×2, throttled to 1s refresh |
| Live status on web | V3 polling every 2s |
| Camera state card | V2 polling every 2.5s, animated pulsing |
| Push notifications | Web Notifications API + V10 polling every 3s |
| PWA / installable | `manifest.json` + `sw.js` service worker |
| Dose stats chart | Chart.js doughnut, persisted in localStorage |
| Manual dispense | V1 button in PWA |
| Timer persistence | `BLYNK_CONNECTED()` + `syncVirtual` on reconnect |

---

## Push Notifications

The system has **two notification layers**:

### 1. Browser (Web Notifications API)
- Click **🔔 Notify** in the header → grant permission
- Works on desktop Chrome/Edge/Firefox
- Fires on: dose time, medicine taken, missed dose, camera active
- Requires the web page to be open (or PWA installed)

### 2. Blynk V10 Polling
- ESP32 and `detect.py` write human-readable messages to V10
- Dashboard polls V10 every 3s and calls `nativeNotify()`
- Deduplicates (only notifies if message changed)

---

## Troubleshooting

### Serial Monitor shows nothing on timer set
**Cause:** Blynk datastream Min/Max too narrow (e.g. max=23 for hours).  
**Fix:** Set V0/V4/V5 Max = `86400` in Blynk Console → Datastreams.

### `HTTP 400` when setting timer
**Cause:** Value out of datastream range.  
**Fix:** Same as above — update Min=0, Max=86400 for V0/V4/V5.

### `[TIMER] Morning → 00:00` on boot
**Cause:** Old firmware stored `10` (integer hour) in V0; new firmware reads it as 10 seconds = 00:00.  
**Fix:** Simply click **Set Morning** once from the web to overwrite with correct value.

### Camera not opening
**Cause:** `detect.py` not running, or wrong Python version.  
**Fix:** Run with `py -3.11 detect.py`. Check webcam index (default `0`).

### Mediapipe ImportError / protobuf conflict
```bash
py -3.11 -m pip install mediapipe==0.10.9 protobuf==3.20.3
```

### RTC shows wrong time
**Cause:** `if (!rtc.isrunning())` guard prevented update.  
**Fix:** Already fixed — firmware now always NTP-syncs on boot after WiFi connects.

### Servo jitters / ESP32 resets during dispense
**Cause:** Servo drawing too much current from ESP32's 5V pin.  
**Fix:** Power servo from external 5V supply; share GND with ESP32.

### Buzzer never stops
**Cause:** `alertActive` stuck true.  
**Fix:** Upload latest firmware — timeout (30s) auto-resolves.

---

## Dependencies

### Arduino Libraries

```
BlynkSimpleEsp32
LiquidCrystal I2C (Frank de Brabander)
RTClib (Adafruit)
ESP32Servo
Time (TimeLib, Paul Stoffregen)
WiFi (built-in ESP32)
time.h (ESP32 IDF, built-in)
```

### Python (3.11)

```
opencv-python
mediapipe==0.10.9
protobuf==3.20.3
requests
```

### Web / PWA

```
Chart.js (CDN)      — dose statistics doughnut chart
Web Notifications API — native push alerts
Service Worker API  — PWA, offline cache
Blynk HTTP API      — all IoT communication
Google Fonts        — Space Grotesk
```

---

## Project Info

| Item | Value |
|------|-------|
| Platform | ESP32 Dev Module |
| Blynk Template | `TMPL39IHNO8eS` — SMART MEDICINE DISPENSARY |
| Auth Token | `9IZnV6T6HxYj6A4wMb6dM-aoUAo0J8eU` |
| WiFi | Asmit's F55 |
| Timezone | IST (UTC+5:30, offset 19800s) |
| RTC | DS1307 at I2C address `0x27` |
| LCD | 16×2 at I2C address `0x27` |

---

*Built with ❤️ — Smart Medicine Dispenser v2.0*
