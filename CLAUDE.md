# Project Rules — tsp-wheel / MyCW

กฎถาวรของโปรเจกต์ ใช้กับทุก session โดยไม่ต้องสั่งซ้ำ

---

## 1. 🔒 ข้อความบนจอ TFT ต้องเป็น English ASCII เท่านั้น

**กฎเหล็ก ห้ามละเมิดทุกกรณี**

ทุกข้อความที่แสดงบนจอ TFT ของ MyCW ต้องเป็นอักษร ASCII พิมพ์ได้เท่านั้น (`0x20`–`0x7E`)
ห้ามมีอักษรไทย อีโมจิ หรืออักขระ non-ASCII ใด ๆ ออกจอ ไม่ว่าจะมาจาก string literal
ในโค้ด หรือจากข้อมูลที่ดึงมาจาก Google Sheets / Hue Bridge / API ใด ๆ

### เหตุผลทางเทคนิค
ฟอนต์ในตัวของ TFT_eSPI เป็น ASCII ล้วน และต่อให้แปลงฟอนต์ไทยเป็น VLW (smooth font)
ก็ยังวางสระบน/ล่างและวรรณยุกต์ผิดตำแหน่ง เพราะ VLW เรียงกลิฟไปทางขวาอย่างเดียว
ไม่มี glyph composition — "โ-ร-บิ-น" จะออกมาเป็น "โรบ ิน"

### บังคับใช้ 3 ชั้น

| ชั้น | กลไก | ที่อยู่ |
|---|---|---|
| 1 | คอลัมน์ `Short_Name` (ละติน) ในชีต `Branches` | Google Sheets |
| 2 | Apps Script `ascii_()` strip non-ASCII ก่อนส่ง JSON | `MyCW/apps_script/clock_action.gs` |
| 3 | **ทุกข้อความวาดผ่าน `uiText()` / `uiTextR()` เท่านั้น** | `MyCW/ui.cpp` |

### ข้อห้ามระดับโค้ด
- **ห้ามเรียก `tft.drawString()` / `tft.print()` / `tft.drawNumber()` นอกไฟล์ `ui.cpp`**
  ทุกข้อความต้องผ่าน `uiText()` ซึ่งกรอง byte `> 0x7F` เป็น `?`
- **ห้ามใส่ string literal ภาษาไทยในโค้ด `.ino` / `.h` / `.cpp`** — คอมเมนต์ภาษาไทยได้ แต่
  ข้อความที่ออกจอต้องเป็นอังกฤษ
- ห้ามใช้ `฿` ใช้ `THB` แทน

### คำศัพท์มาตรฐานบนจอ (ใช้ชุดนี้ ห้ามคิดคำใหม่)

| ความหมาย | คำบนจอ |
|---|---|
| วันในสัปดาห์ | `MON TUE WED THU FRI SAT SUN` |
| เดือน | `JAN FEB MAR APR MAY JUN JUL AUG SEP OCT NOV DEC` |
| ยอดวันนี้ / ยอดสะสมเดือน | `TODAY` / `MTD` |
| จำนวนตู้ที่บันทึกวันนี้ | `MACHINES` |
| สาขายังไม่ส่งรายงาน | `PENDING` (จุดแดง) |
| สาขาส่งรายงานแล้ว | `OK` (จุดเขียว) |
| ข้อมูลเก่า ดึงไม่สำเร็จ | `STALE HH:MM` |
| สถานะไฟ | `LIGHTS ON` / `LIGHTS OFF` |
| ข้อผิดพลาด Hue | `HUE ERROR` |
| หน่วยเงิน | `THB` |

### วิธีตรวจก่อน commit ทุกครั้ง
```bash
# 1) ไม่มี non-ASCII ใน string literal ที่ออกจอ (เจอได้เฉพาะในคอมเมนต์)
grep -rnP '[^\x00-\x7F]' --include='*.ino' --include='*.h' --include='*.cpp' MyCW/

# 2) ไม่มีการวาดข้อความนอก ui.cpp
grep -rn 'tft\.drawString\|tft\.print\|tft\.drawNumber' MyCW/ | grep -v 'MyCW/ui\.cpp'
```
คำสั่งที่ 2 ต้องไม่คืนผลลัพธ์ใด ๆ

---

## 2. ห้าม hardcode secret ในซอร์สโค้ด

- WiFi SSID/password → WiFiManager เก็บใน flash ของ ESP8266
- Hue username token, Web App URL, CLOCK_TOKEN, รหัสหน้าเว็บ → `/config.json` บน LittleFS
- ฝั่ง Apps Script → `PropertiesService.getScriptProperties()` เท่านั้น
- `/config.json` และไฟล์ `secrets*` ต้องอยู่ใน `.gitignore`

---

## 3. ฮาร์ดแวร์ MyCW (อ้างอิงเวลาแก้โค้ด)

- บอร์ด: **NodeMCU V2 (ESP-12E, ESP8266)** — ไม่มี capacitive touch ในตัว ต้องใช้ TTP223 ภายนอก
- จอ: **2.8" TFT ILI9341 SPI** 320×240 แนวนอน
- RAM ว่างจริง ~40 KB → **ห้ามใช้ full-screen sprite** (320×240×2 = 153 KB)
- TLS handshake กิน heap 16–22 KB → ต้องเช็ค `ESP.getFreeHeap()` ก่อนทุกครั้ง
- Pin map อยู่ใน `MyCW/config.h` — แก้ที่เดียว

---

## 4. ระบบข้อมูล Toy Station Plus+

- Spreadsheet: **TSP_Master_Database Ver.2** (`1FU1iosNzzepz8AVAFOz-ZRwzPzi_O5wgiOV3TYwcomk`)
- Apps Script: **"webapp tsp meter"** — เป็น single source of truth ของการคำนวณยอด
- **ห้ามคำนวณยอดขายเองใหม่** ให้เรียกใช้ `meterAll_()` / `isRevOk_()` / `branches_()` ที่มีอยู่แล้ว
  ไม่งั้นตัวเลขจะไม่ตรงกับ Dashboard และ AppSheet
- การเพิ่มฟีเจอร์ใน Apps Script ให้ **เพิ่ม `case` ใหม่ใน `routeGet_()`** ไม่แก้ case เดิม

---

## 5. Git

- พัฒนาและ push บนแบรนช์ `claude/wifi-setup-mobile-mycw-ewf9e3` เท่านั้น
- `git push -u origin <branch>` เสมอ
- ห้ามเปิด Pull Request เว้นแต่ผู้ใช้สั่งชัดเจน
