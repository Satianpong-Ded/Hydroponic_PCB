Hydroponic_PCB

แผงวงจร (PCB) ควบคุมระบบไฮโดรโปนิกส์ด้วย **ESP32-S3** วัดค่า pH, อุณหภูมิ และความชื้น แสดงผลบนจอ OLED ส่งข้อมูลขึ้น **ThingsBoard** ผ่าน MQTT และควบคุมปั๊มน้ำผ่านรีเลย์ ทั้งแบบตั้งเวลาอัตโนมัติและสั่งจากระยะไกล

ออกแบบด้วย **EasyEDA** โดย satianpong.ded (REV 1.0)

---

Features

- วัดค่า **pH** ด้วยวงจร Analog Front-end (Op-amp TLV2372) พร้อม Wheatstone Bridge และ Comparator แจ้งเตือนค่าเกินเกณฑ์
- วัด **อุณหภูมิ / ความชื้น** ด้วย DHT22
- แสดงผลบนจอ **OLED SSD1306 (I2C)**
- ส่งข้อมูล Telemetry ขึ้น **ThingsBoard** ผ่าน MQTT
- ควบคุมปั๊มน้ำด้วย **รีเลย์ 2 ช่อง** (Relay1, Relay2)
- ระบบจ่ายไฟครบในบอร์ด: 12 V → 5 V (LM7805) → 3.3 V (AP2112K)
- ต่อ USB-C สำหรับอัปโหลดโค้ด / จ่ายไฟ
- มีปุ่ม **RESET** และ **BOOT**

---

โครงสร้างวงจร (Schematics)

วงจรแบ่งเป็น 3 แผ่น

### 1) `Esp32_Controller` (2025-11-05)

![Esp32_Controller](Schematic/Esp32_Controller.png)

| ส่วนวงจร | รายละเอียด |
|---|---|
| Controller | ESP32-S3-WROOM-1 (N8R2) |
| Button | ปุ่ม RESET (EN) และ BOOT (GPIO0) พร้อม R pull-up 12 kΩ + C 100 nF / 10 µF ตัดสัญญาณรบกวน |
| USB_Port | USB Type-C (USB1) มี R 5.1 kΩ ที่ CC1/CC2 ต่อ D+/D- เข้า ESP32-S3 |
| I2C_Pin | Pull-up 4.7 kΩ ที่ SDA / SCL เข้า 3.3 V |
| Power Indication | LED แสดงสถานะไฟ 5 V |

2) `Ph_sensor` (2025-11-04)

![Ph_sensor](Schematic/Ph_sensor.png)

วงจร Analog Front-end สำหรับอ่านค่า pH และอุณหภูมิ ใช้ Op-amp **TLV2372IDGKG4** (Dual Op-amp, Rail-to-rail)

| ส่วนวงจร | หน้าที่ |
|---|---|
| Voltage Follower (บน) | สร้างแรงดันอ้างอิงจาก **TL431** ปรับด้วย RP1 แล้ว Buffer เป็น `pH-` |
| Inverting Op-amp (บน) | รับ `pH+` จากขั้ว BNC ขยาย/กลับเฟส ได้เป็นสัญญาณ **Po** |
| Comparator + Reference | เทียบ Po กับแรงดันอ้างอิง (ปรับด้วย RP3) ขับ LED และสัญญาณ **Do** (Digital Out) |
| Voltage Follower (ล่าง) + Wheatstone Bridge | สร้างแรงดันอ้างอิงให้บริดจ์ ปรับสมดุลด้วย RP2 |
| Differential Op-amp | ขยายผลต่างของบริดจ์ |
| Inverting Op-amp (ล่าง) | ขยายขั้นสุดท้ายได้เป็นสัญญาณ **To** |
| Connectors | BNC (RF1) สำหรับหัววัด pH และ Header 1x6: `+5V, GND, GND, To, Po, Do` |

3) `Power & Relay` (2025-10-30)

![Power_Relay](Schematic/Power_Relay.png)

| ส่วนวงจร | รายละเอียด |
|---|---|
| Rectifier / Regulator | 12 V DC → **LM7805** → 5 V → **AP2112K-3.3** → 3.3 V, มี LED_B แสดงสถานะ 5 V |
| Connectors | OLED (I2C), DHT22 (R pull-up 4.7 kΩ), RELAY_IN, 12V_IN (CONN-TH 2P 5.00) |
| Relay x2 | ขับด้วยทรานซิสเตอร์ **2N3904** (R base 470 Ω), ไดโอด **1N4007** กัน Back-EMF, LED แสดงสถานะ, ขั้ว LINE สำหรับโหลด |

---

Pin Mapping

ตามวงจร Controller (ชื่อ net → ขา ESP32-S3):

| สัญญาณ | ขา ESP32-S3 | หมายเหตุ |
|---|---|---|
| `Po` (pH) | IO4 | อ่านแบบ Analog |
| `SDA` / `SCL` | IO8 / IO9 | I2C สำหรับ OLED (ที่อยู่ 0x3C) |
| `DHT22` | IO40 | |
| `Relay1` | IO35 | |
| `Relay2` | IO36 | |
| `USB_DP` / `USB_DN` | IO20 / IO19 | USB-C |
| `ESP32_GPIO0` | IO0 | ปุ่ม BOOT |

---

Bill of Materials (หลัก ๆ)

| อุปกรณ์ | รายละเอียด |
|---|---|
| U15 | ESP32-S3-WROOM-1 (N8R2) |
| U2.1, U5.1, U8.1, U9.1, U10.1, U13.1 | TLV2372IDGKG4 (Op-amp) |
| U11 | TL431ACLPRE3 (Voltage Reference) |
| U1 | LM7805 |
| U4 | AP2112K-3.3TRG1 |
| Q5, Q6 | 2N3904 |
| D5, D6 | 1N4007 |
| RLY5, RLY6 | Relay |
| RP1, RP2, RP3 | R_3386P_EU (Trimmer 3386P) |
| RF1 | BNC-KYWE-295-W4-N |
| USB1 | USB Type-C |
| อื่น ๆ | DHT22, OLED SSD1306 128x64 (I2C), Header 2.54 mm, ตัวต้านทาน/ตัวเก็บประจุตามแบบ |

---
Firmware

### Library ที่ต้องติดตั้ง (Arduino IDE / PlatformIO)

- `PubSubClient`
- `DHT sensor library` (Adafruit)
- `Adafruit GFX Library`
- `Adafruit SSD1306`

Board: **ESP32S3 Dev Module** (esp32 core by Espressif)

### การทำงาน

1. เชื่อมต่อ Wi-Fi และ MQTT ไปที่ `demo.thingsboard.io` (พอร์ต 1883) ใช้ Access Token เป็น username
2. อ่านค่า pH, อุณหภูมิ, ความชื้น แล้วแสดงบน OLED
3. ส่ง Telemetry ไปที่ `v1/devices/me/telemetry` ในรูป JSON
4. ปั๊มทำงานตามรอบ: **เปิด 10 วินาที ทุก ๆ 15 วินาที**
5. สั่งปั๊มแบบ Manual ได้ด้วยข้อความ `ON` / `OFF` (เมื่อสั่ง ON ระบบตั้งเวลาอัตโนมัติจะหยุดจนกว่าจะสั่ง OFF)

ThingsBoard

1. สร้าง Device บน [ThingsBoard](https://demo.thingsboard.io) แล้วคัดลอก **Access Token**
2. ใส่ token ในตัวแปร `tbToken`
3. ข้อมูล Telemetry ที่ส่ง: `ph`, `temperature`, `humidity`
4. สร้าง Dashboard แสดงกราฟ / Gauge ตามต้องการ

---

ข้อควรตรวจสอบ / Known Issues

- **ลอจิกรีเลย์:** วงจรขับรีเลย์ใช้ NPN 2N3904 (Active-HIGH: ขา GPIO = HIGH รีเลย์ทำงาน) แต่โค้ดปัจจุบันใช้ `LOW = ON` ซึ่งเป็นลอจิกของโมดูลรีเลย์แบบ Active-LOW ต้องสลับ HIGH/LOW ให้ตรงกับวงจรจริง
- **ระดับแรงดัน ADC:** Op-amp ของวงจร pH จ่ายไฟ 5 V เอาต์พุตอาจสูงเกิน 3.3 V ซึ่งเป็นขีดจำกัดของ ADC ESP32-S3 ควรตรวจสอบหรือเพิ่มตัวหารแรงดัน
- **สูตร pH:** `readPH()` ใช้สูตรประมาณ ต้อง **Calibrate** ด้วยน้ำยามาตรฐาน (เช่น pH 4.0 / 7.0) แล้วปรับสูตรให้ตรง
- **คำสั่งจาก ThingsBoard:** โค้ดยังไม่ได้ `client.subscribe("v1/devices/me/rpc/request/+")` ทำให้ callback ยังไม่ได้รับคำสั่ง และ payload RPC ของ ThingsBoard เป็น JSON ไม่ใช่ข้อความ `ON`/`OFF` ตรง ๆ
- **ความถี่ส่งข้อมูล:** ตอนนี้ส่งทุก ~0.5 วินาที ซึ่งถี่เกินไปสำหรับ demo server ควรเพิ่มเป็น 5–10 วินาที
- **รีเลย์ที่ 2:** วงจรมี Relay2 แต่โค้ดยังควบคุมแค่ Relay1
- **ความปลอดภัย:** ขั้ว LINE ของรีเลย์ใช้กับไฟบ้านได้ ต้องมีฉนวนและระยะห่างเหมาะสม

---

Hardware / การสั่งผลิต PCB

ไฟล์ทั้งหมดอยู่ในโฟลเดอร์ [`Hardware/`](Hardware)

| ไฟล์ | ใช้ทำอะไร |
|---|---|
| [`Gerber_Hydroponic.zip`](Hardware/Gerber_Hydroponic.zip) | ไฟล์สำหรับสั่งผลิตแผ่น PCB เปล่า (JLCPCB, PCBWay ฯลฯ) |
| [`BOM_Hydroponic.csv`](Hardware/BOM_Hydroponic.csv) | รายการชิ้นส่วนทั้งหมด ใช้สั่งซื้อหรือส่งประกอบ |
| [`PickAndPlace_PCB.csv`](Hardware/PickAndPlace_PCB.csv) | พิกัดและมุมวางชิ้นส่วน สำหรับบริการประกอบ SMT |
| `PCB_layout.png` | รูปลายวงจร PCB |
| `3D_img.png` | รูปโมเดล 3D ของบอร์ด |

**สั่งแค่บอร์ดเปล่า:** อัปโหลด `Gerber_Hydroponic.zip` ให้โรงงาน แล้วบัดกรีเองตาม BOM
**ให้โรงงานประกอบให้:** ใช้ Gerber + BOM + Pick & Place ครบทั้ง 3 ไฟล์

PCB Layout

![PCB_layout](Hardware/PCB_layout.png)

3D Preview

![3D_img](Hardware/3D_img.png)


