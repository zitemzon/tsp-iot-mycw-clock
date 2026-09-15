/* =======================================================================
   store.h - persistent settings on LittleFS (/config.json)

   ไม่มี secret ใดอยู่ในซอร์สโค้ด ทุกอย่างอยู่ในไฟล์นี้บนแฟลชของบอร์ด
   แก้ได้จากหน้าเว็บ http://mycw.local โดยไม่ต้องคอมไพล์ใหม่
   ======================================================================= */
#pragma once
#include <Arduino.h>
#include "config.h"

struct HueCfg {
  String   ip;        /* ว่าง = ยังไม่ได้ pair / ให้ค้นด้วย mDNS            */
  String   user;      /* username (v1) หรือ application key (v2)          */
  uint8_t  api = 1;   /* 1 = REST v1 (HTTP), 2 = CLIP v2 (HTTPS)          */
  String   group = "0";   /* v1: group id ("0" = ทุกดวง) / v2: rid         */
  String   sceneA;
  String   sceneB;
};

struct SalesCfg {
  String   url;       /* .../exec ของ Web App "webapp tsp meter"          */
  String   token;     /* ตรงกับ CLOCK_TOKEN ใน Script Properties          */
  uint16_t every = SALES_POLL_DEFAULT_S;
};

struct WebCfg {
  String user = "admin";
  String pass;        /* ว่าง = สุ่มรหัส 6 หลักตอนบูต แล้วโชว์บนจอ         */
};

struct UiCfg {
  uint8_t bright = 255;
  uint8_t page   = PAGE_CLOCK;
};

struct Config {
  int32_t  tz  = TZ_OFFSET_DEFAULT;
  String   ntp = NTP_SERVER_DEFAULT;
  HueCfg   hue;
  SalesCfg sales;
  WebCfg   web;
  UiCfg    ui;
};

extern Config cfg;

bool storeBegin();   /* mount LittleFS (format ถ้าจำเป็น) + โหลด config    */
bool storeLoad();
bool storeSave();
void storeReset();   /* ลบ config.json แล้วกลับไปใช้ค่า default           */
