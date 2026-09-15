/* =======================================================================
   tsp.h - ดึงยอดขายรายสาขาจากระบบ Toy Station Plus+

   ปลายทางคือ Apps Script "webapp tsp meter" ที่ใช้งานอยู่แล้ว
   โดยเรียก action ใหม่ที่เพิ่มเข้าไป (ดู apps_script/clock_action.gs)

     GET <cfg.sales.url>?action=clock&t=<token>

   ตอบกลับเป็น JSON ขนาดเล็ก (< 700 ไบต์) เพราะ ESP8266 ต้อง parse ทั้งก้อนใน RAM
     {"ok":1,"d":"2026-09-15","tot":48250,"mtd":712400,"mc":86,
      "b":[["ROBIN CLG",12450,1], ... ]}

   ยอดทั้งหมดคำนวณฝั่ง Apps Script ด้วย meterAll_()/isRevOk_() ตัวเดียวกับที่
   Dashboard และ AppSheet ใช้ - ตัวเลขจึงตรงกันเสมอ ห้ามมาคำนวณซ้ำฝั่งนี้
   ======================================================================= */
#pragma once
#include <Arduino.h>

void     tspBegin();
void     tspLoop();        /* ดึงตามรอบเวลาที่ตั้งไว้ (cfg.sales.every)  */
void     tspRequestNow();  /* สั่งดึงทันที (ปุ่ม B บนหน้า SALES)          */
bool     tspConfigured();
