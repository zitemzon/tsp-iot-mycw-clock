/* =======================================================================
   net_time.h - WiFi (WiFiManager), NTP, mDNS

   ไม่มี SSID/รหัสผ่านฝังในโค้ด ตั้งค่าจากมือถือผ่าน captive portal
   ======================================================================= */
#pragma once
#include <Arduino.h>
#include "config.h"

/* คืน true ถ้าต่อ WiFi ได้ (จาก config เดิมหรือหลังผู้ใช้ตั้งค่าเสร็จ)
   forcePortal = true -> เข้าโหมดตั้งค่าทันทีแม้จะมี WiFi เดิมอยู่ */
bool netBegin(bool forcePortal);

/* เข้าโหมด captive portal ตามคำสั่ง (long-press ปุ่ม A หรือ double-reset) */
void netStartPortal();

/* ---- double-reset detect ----------------------------------------------
   กดปุ่ม RESET บนบอร์ด 2 ครั้ง (ครั้งที่สองภายใน 6 วินาที) = เข้าโหมดตั้งค่า WiFi
   ใช้ RTC user memory ไม่ต้องพึ่งไลบรารีเพิ่ม

   ข้อจำกัด: RTC RAM หายเมื่อไฟดับสนิท จึงใช้ได้กับ "ปุ่ม RESET" เท่านั้น
   การถอดปลั๊กเสียบใหม่ 2 ครั้งจะไม่ทำงาน - ระบุไว้ใน README แล้ว          */
bool netDrdBegin();   /* คืน true ถ้าตรวจพบการกดรีเซ็ตซ้ำ */
void netDrdLoop();    /* ล้าง flag หลังผ่านไป 6 วินาที      */

void     netLoop();
bool     netOnline();
int      netRssi();
bool     netNtpOk();
time_t   netLocalNow();          /* epoch ที่บวก cfg.tz แล้ว                */
IPAddress netIp();
