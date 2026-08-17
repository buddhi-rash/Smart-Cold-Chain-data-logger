# Smart Cold-Chain Data Logger

A rechargeable, multi-use, cellular-connected temperature and humidity logger for pharmaceutical cold-chain monitoring.  
Designed to solve the **$35 billion annual spoilage problem** by providing real-time alerts, ±0.1 °C accuracy, and direct USB‑C PDF/CSV export—without proprietary software or cloud lock-in.

---

## 🔍 Problem

Cold‑chain failures in pharmaceuticals and biologics lead to massive product loss and regulatory risk. Existing loggers are either passive (USB‑only, no real‑time visibility) or expensive real‑time trackers locked into proprietary platforms. This project delivers an **open, rechargeable, and accurate** alternative.

---

## ✨ Key Features

- **±0.1 °C accuracy** (Sensirion SHT45) for GDP/regulatory compliance  
- **Real‑time LTE‑M/NB‑IoT alerts** via SIM7070G / A7670C cellular module  
- **60‑day battery life** (5000 mAh 21700 Li‑Ion, ultra‑low‑power design)  
- **USB‑C mass‑storage export** – plug into any PC, get PDF + CSV automatically  
- **Condition‑based monitoring** – logging frequency adapts near alarm thresholds  
- **External PT100 RTD probe** support for payload‑core temperature  
- **Minimalistic UI** – 1.3″ OLED, 3 tactile buttons, RGB status LED, piezo buzzer  
- **Open MQTT cloud** – no vendor lock‑in  
- **4‑layer PCB** with controlled impedance (90 Ω USB), hand‑solderable components  

---

## 🧰 Repository Structure

---

## 🔧 Hardware Overview

| Subsystem | Component | Notes |
|-----------|-----------|-------|
| MCU | STM32L412CBT6 (Cortex‑M4, 80 MHz) | Ultra‑low‑power Stop 2 mode (0.7 µA) |
| Temperature/Humidity | Sensirion SHT45 (or SHT40) | ±0.1 °C typical |
| External RTD | MAX31865 + PT100 probe | 3‑wire, –200 to +200 °C |
| Cellular | SIM7070G HAT/ A7670C | LTE‑M/NB‑IoT, 2G fallback, GNSS |
| Storage | W25Q128/W25R128 16 MB SPI flash | ~800,000 records |
| Fuel Gauge | MAX17048 | ModelGauge, 4 µA hibernate |
| Light / Motion | OPT3001, LIS3DH | Door‑open detection, tilt/shock |
| Display | 1.3″ OLED (I²C) | 128×64, industrial temp |
| Power | MCP73831 charger, TPS631000 buck‑boost, S‑8211CAH protection | Load‑sharing circuit for USB operation |
| Battery | 21700 Li‑Ion 5000 mAh | 60‑day runtime with condition‑based monitoring |

The PCB is a **4‑layer S‑G‑P‑S** stackup (JLCPCB JLC04161H‑7628) with controlled impedance for USB. All components are hand‑solderable (LQFP, SOIC, SOT, 0603).

---

## 📟 Firmware Overview

Written in **C** using STM32 HAL and FreeRTOS (optional). Key modules:

- **Sensor drivers** – I²C/SPI for all sensors, with interrupt‑driven wake‑up.
- **Power manager** – Stop 2 mode scheduling, RTC wake‑up, condition‑based logging.
- **Modem manager** – AT command parser, MQTT publisher (TLS), PSM control.
- **USB MSC** – TinyUSB + FatFS for PDF/CSV generation on the fly.
- **UI manager** – 3‑button menu state machine, OLED rendering, RGB LED control.

Build with **STM32CubeIDE**; project file located in `/firmware/core`.

---

## 🚀 Getting Started

### Prerequisites
- STM32CubeIDE, ST‑LINK programmer
- IoT SIM card (1.8 V, LTE‑M/NB‑IoT)
- USB‑C cable
- Li‑Ion 18650 cell

### Flashing
1. Connect ST‑LINK to the SWD header.
2. Open the project in STM32CubeIDE.
3. Build and flash the firmware.

### First Power‑Up
1. Insert the IoT SIM card (observing orientation).
2. Charge the battery via USB‑C (about 10 hours full).
3. Press **MENU** to power on and navigate the UI.
4. Configure logging interval, upload interval, and alarm thresholds.
5. Mount the logger in the shipment; data will upload automatically.

### Data Retrieval
- Plug the logger into any PC via USB‑C. It appears as a mass‑storage drive.
- Copy `TRIP_REPORT.PDF` and `TRIP_DATA.CSV`.

---

## 📄 Documentation

Detailed documents are in the `docs/` folder:
- `architecture.md` – full block diagram and pin mapping.
- `user-manual.md` – complete user guide.
- `testing.md` – power consumption measurements, validation results.

---

## 🧪 Testing & Validation

- **24‑hour current profile:** average 8.4 mA (10‑min logging, 2‑hr upload) → ~20 days worst‑case, 60 days with condition‑based monitoring.
- **Sensor accuracy:** verified against reference thermometer in climate chamber.
- **Cellular reliability:** 100% registration on A7670C (SIM7070G had intermittent failures – see note in docs).
- **USB export:** PDF/CSV generation tested on Windows, macOS, Linux.

---


## 📜 License

This project is licensed under the **MIT License** – see the `LICENSE` file for details.

