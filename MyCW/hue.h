/* =======================================================================
   hue.h - Philips Hue Bridge (local LAN เท่านั้น ไม่ผ่านคลาวด์)

   โค้ดส่วนอื่นเรียกผ่านฟังก์ชันในไฟล์นี้อย่างเดียว ไม่ต้องรู้ว่าข้างในใช้
   API v1 หรือ v2 - ตัวเลือกถูกตรวจอัตโนมัติตอน pair แล้วจำไว้ใน cfg.hue.api

     v1  PUT http://<ip>/api/<user>/groups/<id>/action        (HTTP, เร็ว)
     v2  PUT https://<ip>/clip/v2/resource/grouped_light/<rid> (TLS, กิน heap)

   Signify ทยอยเลิกรองรับ v1 บริดจ์เฟิร์มแวร์ใหม่บางรุ่นอาจตอบ 404/410
   โค้ดจะลอง v1 ก่อนเพราะเบากว่ามาก แล้วค่อยถอยไป v2 ถ้าโดนปฏิเสธ
   ======================================================================= */
#pragma once
#include <Arduino.h>

enum HuePairResult { HUE_PAIR_WAIT = 0, HUE_PAIR_OK = 1, HUE_PAIR_FAIL = -1 };

void hueBegin();

/* ค้นหา Bridge ด้วย mDNS (_hue._tcp) - ไม่ใช้ discovery.meethue.com
   เพราะเป็นคลาวด์ ถ้าเน็ตนอกบ้านล่มจะหาไม่เจอทั้งที่ Bridge อยู่ใน LAN */
bool hueDiscover(String& ipOut);

void          huePairStart();            /* เริ่มนับถอยหลัง 30 วินาที        */
HuePairResult huePairPoll(int& secsLeft);/* เรียกซ้ำจนกว่าจะไม่ใช่ HUE_PAIR_WAIT */

bool hueRefreshState();                  /* อ่าน on/bri ปัจจุบันจาก Bridge   */
bool hueSetGroup(bool on, uint8_t bri);  /* bri 0-254 (สเกลของ Hue v1)      */
bool hueToggle();
bool hueRecallScene(const String& sceneId);

bool    hueIsPaired();
bool    hueLastOk();
bool    hueStateOn();
uint8_t hueStateBri();
