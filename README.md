Hydroponic_PCB

A custom PCB for controlling a hydroponic system, built around the **ESP32-S3**. It measures **pH**, **temperature** and **humidity**, shows readings on an OLED display, publishes telemetry to **ThingsBoard** over MQTT, and drives a water pump through relays, either on an automatic timer or by remote command.

Designed in **EasyEDA** by satianpong.ded (REV 1.0).

---
Features

- **pH measurement** through an analog front-end (TLV2372 op-amps) with a Wheatstone bridge and a comparator for threshold alerts
- **Temperature / humidity** sensing with a DHT22
- **OLED (SSD1306, I2C)** local display
- **ThingsBoard** telemetry over MQTT
- **Two relay channels** (Relay1, Relay2) for pumps or other loads
- Onboard power chain: 12 V → 5 V (LM7805) → 3.3 V (AP2112K)
- **USB-C** for programming and power
- **RESET** and **BOOT** buttons

---
Schematics

The design is split into three sheets.

### 1) `Esp32_Controller` (2025-11-05)

| Block | Description |
|---|---|
| Controller | ESP32-S3-WROOM-1 (N8R2) |
| Button | RESET (EN) and BOOT (GPIO0) buttons with 12 kΩ pull-ups and 100 nF / 10 µF debounce capacitors |
| USB_Port | USB Type-C (USB1) with 5.1 kΩ resistors on CC1/CC2, D+/D- wired to the ESP32-S3 |
| I2C_Pin | 4.7 kΩ pull-ups on SDA / SCL to 3.3 V |
| Power Indication | LED showing the 5 V rail is live |

### 2) `Ph_sensor` (2025-11-04)

Analog front-end for pH and temperature sensing, using **TLV2372IDGKG4** dual rail-to-rail op-amps.

| Block | Function |
|---|---|
| Voltage Follower (top) | Reference voltage from a **TL431**, adjustable with RP1, buffered as `pH-` |
| Inverting Op-amp (top) | Takes `pH+` from the BNC probe connector and produces the **Po** signal |
| Comparator + Reference | Compares Po against an adjustable reference (RP3); drives an LED and the digital output **Do** |
| Voltage Follower (bottom) + Wheatstone Bridge | Supplies the bridge reference; balanced with RP2 |
| Differential Op-amp | Amplifies the bridge difference |
| Inverting Op-amp (bottom) | Final gain stage, producing the **To** signal |
| Connectors | BNC (RF1) for the pH probe; 1x6 header: `+5V, GND, GND, To, Po, Do` |

### 3) `Power & Relay` (2025-10-30)

| Block | Description |
|---|---|
| Rectifier / Regulator | 12 V DC → **LM7805** → 5 V → **AP2112K-3.3** → 3.3 V, with a status LED on the 5 V rail |
| Connectors | OLED (I2C), DHT22 (4.7 kΩ pull-up), RELAY_IN, 12V_IN (CONN-TH 2P 5.00) |
| Relay x2 | Driven by **2N3904** transistors (470 Ω base resistors), **1N4007** flyback diodes, status LEDs, and a LINE terminal for the load |

---
Pin Mapping

Net names to ESP32-S3 pins, per the Controller sheet:

| Signal | ESP32-S3 pin | Notes |
|---|---|---|
| `Po` (pH) | IO4 | Analog input |
| `SDA` / `SCL` | IO8 / IO9 | I2C for the OLED (address 0x3C) |
| `DHT22` | IO40 | |
| `Relay1` | IO35 | |
| `Relay2` | IO36 | |
| `USB_DP` / `USB_DN` | IO20 / IO19 | USB-C |
| `ESP32_GPIO0` | IO0 | BOOT button |

---

Bill of Materials (main parts)

| Part | Description |
|---|---|
| U15 | ESP32-S3-WROOM-1 (N8R2) |
| U2.1, U5.1, U8.1, U9.1, U10.1, U13.1 | TLV2372IDGKG4 (op-amp) |
| U11 | TL431ACLPRE3 (voltage reference) |
| U1 | LM7805 |
| U4 | AP2112K-3.3TRG1 |
| Q5, Q6 | 2N3904 |
| D5, D6 | 1N4007 |
| RLY5, RLY6 | Relay |
| RP1, RP2, RP3 | R_3386P_EU (3386P trimmer) |
| RF1 | BNC-KYWE-295-W4-N |
| USB1 | USB Type-C |
| Other | DHT22, SSD1306 128x64 I2C OLED, 2.54 mm headers, resistors and capacitors per the schematics |

---

Firmware

### Required libraries (Arduino IDE / PlatformIO)

- `PubSubClient`
- `DHT sensor library` (Adafruit)
- `Adafruit GFX Library`
- `Adafruit SSD1306`

Board: **ESP32S3 Dev Module** (esp32 core by Espressif)

### How it works

1. Connects to Wi-Fi, then to MQTT at `demo.thingsboard.io` (port 1883) using the Access Token as the username.
2. Reads pH, temperature and humidity and shows them on the OLED.
3. Publishes JSON telemetry to `v1/devices/me/telemetry`.
4. Runs the pump on a cycle: **ON for 10 s out of every 15 s**.
5. Manual override with the messages `ON` / `OFF` (after `ON`, the automatic timer is suspended until `OFF` is received).

ThingsBoard Setup

1. Create a device on [ThingsBoard](https://demo.thingsboard.io) and copy its **Access Token**.
2. Put the token in the `tbToken` variable.
3. Telemetry keys sent: `ph`, `temperature`, `humidity`.
4. Build a dashboard with charts / gauges as needed.

---
Known Issues / Things to Check

- **Relay logic:** the relay driver uses an NPN 2N3904 (active-HIGH: a HIGH GPIO turns the relay on), but the firmware uses `LOW = ON`, which is the convention for active-LOW relay modules. Swap HIGH/LOW to match the actual circuit.
- **ADC voltage range:** the pH op-amps run from 5 V, so their output can exceed the 3.3 V limit of the ESP32-S3 ADC. Check the output range or add a voltage divider.
- **pH formula:** `readPH()` uses an approximate formula. **Calibrate** with standard buffer solutions (e.g. pH 4.0 / 7.0) and adjust it.
- **Remote commands:** the firmware never calls `client.subscribe("v1/devices/me/rpc/request/+")`, so the callback receives nothing. ThingsBoard RPC payloads are also JSON, not a bare `ON` / `OFF` string.
- **Publish rate:** telemetry goes out roughly every 0.5 s, which is too frequent for the demo server. Use 5–10 s.
- **Relay 2:** the hardware has a second relay channel, but the firmware only controls Relay1.
- **Safety:** the relay LINE terminals can switch mains voltage. Use proper insulation and creepage distances.

