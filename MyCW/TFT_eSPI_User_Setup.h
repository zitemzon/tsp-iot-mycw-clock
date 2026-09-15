/* =========================================================================
   TFT_eSPI User_Setup สำหรับ MyCW
   NodeMCU V2 (ESP-12E) + จอ 2.8" ILI9341 SPI

   *** ขั้นตอนบังคับ ***
   copy ไฟล์นี้ไปทับ
       Windows : Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
       macOS   : ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
       Linux   : ~/Arduino/libraries/TFT_eSPI/User_Setup.h

   ถ้าไม่ทำขั้นนี้จอจะขาวหรือไม่ขึ้นภาพเลย เพราะ TFT_eSPI กำหนดขาใน
   ไฟล์ของไลบรารีเอง ไม่ได้รับค่าจาก sketch

   หมายเหตุ: ถ้าอัปเดตไลบรารี TFT_eSPI ในอนาคต User_Setup.h จะถูกเขียนทับ
   ต้อง copy ไฟล์นี้กลับไปใหม่ทุกครั้ง
   ========================================================================= */

#define USER_SETUP_INFO "MyCW NodeMCU ILI9341"

#define ILI9341_DRIVER

/* ---- ขา (ตรงกับตารางใน MyCW/README.md และคอมเมนต์ใน config.h) ----------
     TFT_MISO  D6 / GPIO12  ต่อเฉพาะเมื่อจอมีทัชสกรีน XPT2046 ในตัว
     TFT_MOSI  D7 / GPIO13
     TFT_SCLK  D5 / GPIO14
     TFT_CS    D8 / GPIO15
     TFT_DC    D3 / GPIO0
     TFT_RST   -1  ต่อขา RESET ของจอเข้ากับขา RST ของบอร์ด
   ---------------------------------------------------------------------- */
#define TFT_MISO PIN_D6
#define TFT_MOSI PIN_D7
#define TFT_SCLK PIN_D5
#define TFT_CS   PIN_D8
#define TFT_DC   PIN_D3
#define TFT_RST  -1

/* ---- ฟอนต์ ----------------------------------------------------------
   โหลดเฉพาะที่ใช้จริงเพื่อประหยัดแฟลช (ต้องเหลือที่ให้ OTA ด้วย)
     GLCD   ฟอนต์ 1 - ใช้เป็น fallback ของไลบรารี
     FONT2  16px - แถวสาขา แถบสถานะ
     FONT4  26px - หัวข้อ ยอดรวม
     FONT7  48px 7-segment - เวลาตัวใหญ่หน้า CLOCK
   ทุกฟอนต์เป็น ASCII ล้วน จึงแสดงภาษาไทยไม่ได้ (ดู CLAUDE.md ข้อ 1)
   ---------------------------------------------------------------------- */
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT7
#define LOAD_GFXFF

/* 80MHz จะเพี้ยนถ้าใช้สายจัมเปอร์ยาว 40MHz นิ่งกว่าและเร็วพอ */
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
