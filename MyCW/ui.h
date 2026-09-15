/* =======================================================================
   ui.h - การวาดจอทั้งหมด

   *** ไฟล์ ui.cpp เป็นไฟล์เดียวในโปรเจกต์ที่ได้รับอนุญาตให้เรียก
       tft.drawString() / tft.print() / tft.drawNumber() ได้ ***

   ทุกข้อความต้องผ่าน uiText() ซึ่งกรอง byte > 0x7F ทิ้งเป็น '?'
   เพราะฟอนต์ของ TFT_eSPI เป็น ASCII ล้วน - อักษรไทยจะออกมาเป็นขยะ
   ดูกฎเต็มใน CLAUDE.md ข้อ 1
   ======================================================================= */
#pragma once
#include <Arduino.h>
#include "config.h"

/* ข้อมูลที่โมดูลอื่นเติมให้ UI อ่าน - UI ไม่ไปดึงข้อมูลเอง */
struct BranchRow {
  char name[12];   /* ASCII ล้วน มาจาก Short_Name ในชีต (ยาวสุด 10 ตัว) */
  long rev;        /* ยอดวันนี้ (บาท ปัดเป็นจำนวนเต็ม)                  */
  bool done;       /* true = ส่งรายงานแล้ว, false = PENDING             */
};

struct UiData {
  /* --- clock / net --- */
  bool     wifiOk    = false;
  int      rssi      = 0;
  bool     ntpOk     = false;

  /* --- sales --- */
  BranchRow br[SALES_MAX_BRANCHES];
  uint8_t  brCount   = 0;
  long     total     = 0;
  long     mtd       = 0;
  int      machines  = 0;
  bool     salesEver = false;   /* เคยดึงสำเร็จอย่างน้อย 1 ครั้งหรือยัง  */
  bool     salesStale= false;   /* ดึงรอบล่าสุดไม่สำเร็จ                 */
  uint32_t salesAtEpoch = 0;    /* เวลาที่ดึงสำเร็จครั้งล่าสุด           */
  uint32_t salesNextMs  = 0;

  /* --- hue --- */
  bool     huePaired = false;
  bool     hueOn     = false;
  uint8_t  hueBri    = 0;       /* 0-254 ตามสเกลของ Hue                 */
  uint8_t  hueApi    = 1;
  bool     hueOk     = true;    /* false = คำสั่งล่าสุดล้มเหลว           */
  char     hueScene  = 'A';

  /* --- system --- */
  char     ipStr[16] = "0.0.0.0";
};

extern UiData uid;

void    uiBegin();
void    uiSetPage(uint8_t page);       /* บังคับ full redraw               */
uint8_t uiPage();
void    uiNextPage();
void    uiMarkDirty();                 /* ข้อมูลเปลี่ยน -> วาดใหม่รอบหน้า  */
void    uiTick(time_t localNow);       /* เรียกจาก loop() ทุกรอบ           */

/* หน้าจอเฉพาะกิจ (วาดเต็มจอทันที ไม่ผ่าน page router) */
void uiBootScreen(const char* line);
void uiWifiSetupScreen(const char* webPin);
void uiHuePairScreen(int secondsLeft, bool failed);
void uiToast(const char* msg, uint16_t colour, uint16_t ms);
