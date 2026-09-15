#include "tsp.h"
#include "store.h"
#include "ui.h"
#include "net_time.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

static uint32_t gNextMs   = 0;
static bool     gForce    = false;

bool tspConfigured() { return cfg.sales.url.length() && cfg.sales.token.length(); }

void tspBegin() {
  gNextMs = millis() + 3000;      /* ดึงครั้งแรกหลังบูต 3 วิ ให้ WiFi/NTP นิ่งก่อน */
  uid.salesNextMs = gNextMs;
}

void tspRequestNow() { gForce = true; }

/* กรองให้เหลือ ASCII พิมพ์ได้ และตัดความยาวให้พอดีช่องบนจอ
   (ชั้นป้องกันที่ 3 อยู่ใน ui.cpp อยู่แล้ว อันนี้เป็นชั้นกลางกันข้อมูลบวม) */
static void copyAscii(char* dst, size_t cap, const char* src) {
  size_t o = 0;
  if (src) {
    for (size_t i = 0; src[i] && o < cap - 1; i++) {
      uint8_t c = (uint8_t)src[i];
      if (c >= 0x20 && c <= 0x7E) dst[o++] = (char)c;
    }
  }
  dst[o] = '\0';
}

static bool fetch() {
  if (!tspConfigured() || !netOnline()) return false;

  /* TLS handshake กิน heap 16-22KB ถ้าเหลือไม่พอให้ข้ามรอบนี้ไปเลย
     ดีกว่าปล่อยให้ handshake ล้มกลางทางแล้วบอร์ด brownout */
  if (ESP.getFreeHeap() < TLS_MIN_FREE_HEAP) return false;

  bool ok = false;

  {   /* บล็อกแยกเพื่อให้ client/http ถูกทำลายและคืน RAM ทันทีที่จบ */
    std::unique_ptr<BearSSL::WiFiClientSecure> cli(new BearSSL::WiFiClientSecure);
    /* root CA ของ Google หมุนเวียนบ่อยเกินกว่าจะ pin ไว้ในเฟิร์มแวร์
       ชดเชยด้วย CLOCK_TOKEN + ฝั่งเซิร์ฟเวอร์เป็น read-only และส่งแต่ยอดสรุป
       (ไม่มีข้อมูลพนักงานหรือลูกค้าอยู่ใน payload) */
    cli->setInsecure();
    cli->setBufferSizes(2048, 512);

    HTTPClient http;
    String url = cfg.sales.url + "?action=clock&t=" + cfg.sales.token;

    if (http.begin(*cli, url)) {
      /* script.google.com ตอบ 302 ข้ามโดเมนไป script.googleusercontent.com
         ค่า HTTPC_STRICT_FOLLOW_SAME_HOST จะล้มเสมอ ต้องใช้ FORCE */
      http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
      http.setTimeout(15000);
      http.useHTTP10(true);          /* ปิด chunked ให้ ArduinoJson อ่าน stream ได้ตรง */

      int code = http.GET();
      if (code == 200) {
        StaticJsonDocument<1024> d;
        if (!deserializeJson(d, http.getStream()) && (d["ok"] | 0) == 1) {
          uid.total    = d["tot"] | 0L;
          uid.mtd      = d["mtd"] | 0L;
          uid.machines = d["mc"]  | 0;

          uint8_t n = 0;
          /* แต่ละสมาชิกของ "b" เป็น array ["ชื่อย่อ", ยอด, ส่งแล้ว 0/1]
             ต้องรับเป็น JsonVariantConst แล้วค่อย index - ArduinoJson ไม่ได้
             คืนค่าเป็น JsonArrayConst ตรง ๆ ตอน iterate */
          for (JsonVariantConst b : d["b"].as<JsonArrayConst>()) {
            if (n >= SALES_MAX_BRANCHES) break;
            copyAscii(uid.br[n].name, sizeof(uid.br[n].name), b[0] | "");
            uid.br[n].rev  = b[1] | 0L;
            uid.br[n].done = (b[2] | 0) != 0;
            n++;
          }
          uid.brCount = n;
          ok = true;
        }
      }
      http.end();
    }
  }

  if (ok) {
    uid.salesEver     = true;
    uid.salesStale    = false;
    uid.salesAtEpoch  = (uint32_t)netLocalNow();
  } else if (uid.salesEver) {
    /* คงตัวเลขเดิมไว้ แค่ติดธง STALE - ห้ามล้างเป็น 0 เพราะบนจอจะอ่านได้ว่า
       "วันนี้ขายไม่ได้เลย" ซึ่งคนละเรื่องกับ "ดึงข้อมูลไม่สำเร็จ" */
    uid.salesStale = true;
  }
  uiMarkDirty();
  return ok;
}

void tspLoop() {
  if (!tspConfigured()) return;

  uint32_t now = millis();
  if (!gForce && (int32_t)(now - gNextMs) < 0) return;

  gForce = false;
  bool ok = fetch();

  /* ดึงไม่สำเร็จให้ลองใหม่เร็วขึ้น (60 วิ) แต่ไม่ถี่กว่านั้น กัน quota
     ของ Apps Script และกันไม่ให้จอค้างบ่อย ๆ เพราะ HTTPS เป็น blocking */
  uint32_t nextGap = ok ? (uint32_t)cfg.sales.every * 1000UL : 60000UL;
  gNextMs = millis() + nextGap;
  uid.salesNextMs = gNextMs;
}
