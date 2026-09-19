# MyCW Rainbow Clock v2

นาฬิกา NTP + จอมอนิเตอร์หน้าร้านสำหรับ **Toy Station Plus+**
NodeMCU V2 (ESP8266) + จอ TFT 2.8" ILI9341 + ปุ่มสัมผัส + Philips Hue

📖 **เอกสารเต็มอยู่ที่ [`MyCW/README.md`](MyCW/README.md)**

## 🔒 กฎเหล็กของโปรเจกต์นี้

ข้อความที่แสดงบนจอ TFT ต้องเป็น **English ASCII เท่านั้น** (`0x20`–`0x7E`)
เพราะฟอนต์ของ TFT_eSPI ไม่รองรับการวางสระและวรรณยุกต์ไทย
รายละเอียดและวิธีบังคับใช้ 3 ชั้นอยู่ใน [`CLAUDE.md`](CLAUDE.md)

## โครงสร้าง

| ไฟล์ / โฟลเดอร์ | เนื้อหา |
|---|---|
| `MyCW/MyCW.ino` | สเก็ตช์หลัก — เปิดใน Arduino IDE ได้เลย |
| `MyCW/ui.cpp` | ทุกอย่างที่วาดลงจอ (จุดเดียวที่เรียก `tft.drawString()` ได้) |
| `MyCW/hue.cpp` | Philips Hue ผ่าน LAN — mDNS discovery, รองรับทั้ง API v1 และ v2 |
| `MyCW/tsp.cpp` | ดึงยอดขาย 8 สาขาจากระบบ TSP เดิม |
| `MyCW/touch.cpp` | TTP223 สองตัว — tap / double-tap / long-press |
| `MyCW/webui.cpp` | หน้าเว็บตั้งค่า + OTA (หลัง HTTP basic auth) |
| `MyCW/apps_script/` | action ที่เพิ่มเข้าไปใน Apps Script เดิมฝั่งเซิร์ฟเวอร์ |
| `CLAUDE.md` | กฎถาวรของโปรเจกต์ |

> โฟลเดอร์ต้องชื่อ `MyCW` ให้ตรงกับ `MyCW.ino` ตามข้อบังคับของ Arduino IDE

## ความปลอดภัย

ไม่มี secret ในซอร์ส — WiFi เก็บใน WiFiManager, ส่วน Hue token / feed URL /
รหัสหน้าเว็บ อยู่ใน `/config.json` บน LittleFS ซึ่ง gitignore ไว้แล้ว

## ที่มา

เดิมอยู่เป็น branch ใน repo `tsp-wheel` (กิจกรรมหมุนวงล้อ) แยกออกมาเป็นโปรเจกต์ของตัวเอง
