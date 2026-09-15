/* =======================================================================
   webui.h - หน้าเว็บตั้งค่า + OTA

   ใช้ ESP8266WebServer (แบบ sync) + ESP8266HTTPUpdateServer ซึ่งมากับ core
   อยู่แล้ว ไม่ต้องลงไลบรารีเพิ่ม และเสถียรกว่า async บน ESP8266

   ทุก endpoint บังคับ HTTP Basic Auth เพราะเครื่องอยู่บนเน็ตเวิร์กร้าน
   ที่มีพนักงานและลูกค้าใช้ร่วมกัน รอบแรกที่ยังไม่ตั้งรหัสจะสุ่ม 6 หลัก
   แล้วแสดงบนจอ - คนที่เห็นจอเท่านั้นถึงจะเข้าได้

   หมายเหตุ: หน้าเว็บใช้ภาษาอังกฤษเช่นกัน เพื่อให้กฎ ASCII ของโปรเจกต์
   เป็นกฎเดียวตรวจได้ด้วย grep ครั้งเดียว และเลี่ยงปัญหา encoding ของ
   ไฟล์ sketch ใน Arduino IDE
   ======================================================================= */
#pragma once
#include <Arduino.h>

void webuiBegin();
void webuiLoop();

/* หน้าเว็บสั่งงานที่ต้องทำใน main loop (ห้ามทำใน handler เพราะ blocking) */
bool webuiTakeHuePairRequest();
bool webuiTakeSalesRefreshRequest();
