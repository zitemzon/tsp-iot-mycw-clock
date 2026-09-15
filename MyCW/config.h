/* =======================================================================
   MyCW - Rainbow Clock v2  |  Toy Station Plus+
   config.h - pin map, build flags, compile-time defaults

   Board : NodeMCU V2 (ESP-12E / ESP8266)
   Panel : 2.8" TFT ILI9341 SPI, 320x240 landscape

   RULE: every string that reaches the panel must be printable ASCII.
         See CLAUDE.md section 1. All drawing goes through uiText() in ui.cpp.
   ======================================================================= */
#pragma once
#include <Arduino.h>

#define FW_VERSION "2.0.0"
#define DEV_NAME   "mycw"          /* mDNS host -> http://mycw.local        */
#define AP_SSID    "RainbowClock"  /* captive portal SSID (ASCII only)      */
#define AP_PASS    "12345678"

/* ---------------------------------------------------------------- display
   จอต่อผ่าน HSPI ฮาร์ดแวร์ ขาจริงถูกกำหนดใน TFT_eSPI/User_Setup.h
   (copy มาจาก MyCW/TFT_eSPI_User_Setup.h) ค่าด้านล่างไว้อ้างอิง/ตรวจสอบ
   ให้ตรงกันเท่านั้น - เปลี่ยนที่นี่อย่างเดียวไม่มีผล ต้องแก้ User_Setup.h ด้วย

     TFT_CS  D8 / GPIO15      TFT_MOSI D7 / GPIO13
     TFT_DC  D3 / GPIO0       TFT_SCLK D5 / GPIO14
     TFT_RST -1  (ต่อเข้าขา RST ของบอร์ด)
     LED     3V3 (ดู README เรื่อง MOSFET ถ้าจะทำ PWM backlight)
   ---------------------------------------------------------------------- */
#define SCR_W 320
#define SCR_H 240
#define SCR_ROTATION 1             /* 1 = landscape, USB อยู่ทางซ้าย        */

/* ------------------------------------------------------------------ touch
   ESP8266 ไม่มี capacitive touch ในตัว (ต่างจาก ESP32) จึงมี 2 โหมด:

     TOUCH_TTP223 (ค่าเริ่มต้น) - โมดูล TTP223 2 ตัว ต่อ D1 / D2
     TOUCH_XPT2046             - จอที่มีทัชสกรีนในตัว (มีขา T_CS/T_IRQ ...)

   เลือกโดย comment/uncomment บรรทัด USE_XPT2046 ด้านล่าง
   ---------------------------------------------------------------------- */
/* #define USE_XPT2046 */

#ifndef USE_XPT2046
  #define PIN_TOUCH_A 5            /* D1 - ปุ่ม A : เปลี่ยนหน้า / เข้า WiFi setup */
  #define PIN_TOUCH_B 4            /* D2 - ปุ่ม B : คุมไฟ Hue / refresh ยอดขาย   */
#else
  #define PIN_T_CS    4            /* D2 */
  #define PIN_T_IRQ   5            /* D1 */
  /* T_CLK/T_DIN/T_DO ใช้ SPI bus ร่วมกับจอ (D5/D7/D6) */
#endif

/* gesture timing (ms) - TTP223 มี debounce ในตัวแล้ว ค่านี้กัน noise ซ้ำ */
#define TOUCH_DEBOUNCE_MS   50
#define TOUCH_TAP_MAX_MS    400    /* กดสั้นกว่านี้ = tap                  */
#define TOUCH_DBL_GAP_MS    350    /* tap ที่สองต้องมาภายในช่วงนี้         */
#define TOUCH_LONG_MS      1500    /* กดค้างนานกว่านี้ = long press        */

/* ------------------------------------------------------------------- misc */
#define TZ_OFFSET_DEFAULT  25200   /* UTC+7 - ไทยไม่มี DST ตั้งคงที่ได้    */
#define NTP_SERVER_DEFAULT "th.pool.ntp.org"
#define WIFI_CONNECT_TIMEOUT_S  20
#define PORTAL_TIMEOUT_S       300 /* ไม่ตั้งค่าใน 5 นาที -> รีสตาร์ต       */

#define SALES_POLL_DEFAULT_S   300 /* ดึงยอดขายทุก 5 นาที                  */
#define SALES_MAX_BRANCHES       8

/* TLS handshake บน ESP8266 กิน heap 16-22KB ถ้าเหลือน้อยกว่านี้ให้ข้ามรอบ
   ไม่งั้นบอร์ดจะ brownout/รีบูตกลางทาง */
#define TLS_MIN_FREE_HEAP    20000

/* ------------------------------------------------------------------ pages */
enum Page { PAGE_CLOCK = 0, PAGE_SALES, PAGE_LIGHTS, PAGE_SYSTEM, PAGE_COUNT };

/* ----------------------------------------------------------- touch events */
enum TouchEvent {
  EV_NONE = 0,
  EV_A_TAP, EV_A_LONG,
  EV_B_TAP, EV_B_DOUBLE, EV_B_LONG
};

/* ------------------------------------------------------------ colour theme
   ค่า RGB565 - ตั้งไว้ที่เดียวเพื่อให้ทุกหน้าใช้ชุดสีเดียวกัน */
#define C_BG      0x0000           /* black                                */
#define C_TEXT    0xFFFF           /* white                                */
#define C_DIM     0x8410           /* grey - label รอง                      */
#define C_OK      0x07E0           /* green - ส่งรายงานแล้ว                 */
#define C_PENDING 0xF800           /* red   - ยังไม่ส่ง                     */
#define C_WARN    0xFFE0           /* yellow - STALE                       */
#define C_ACCENT  0x05FF           /* cyan  - หัวข้อ                        */
#define C_LINE    0x2104           /* dark grey - เส้นคั่น                  */
