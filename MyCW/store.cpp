#include "store.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

Config cfg;

static const char* CFG_PATH = "/config.json";

/* JSON เล็กมาก (< 700 ไบต์) ใช้ StaticJsonDocument บน stack ไม่แตะ heap
   ซึ่งสำคัญเพราะ heap ต้องเหลือไว้ให้ TLS handshake */
static const size_t JDOC = 1024;

bool storeBegin() {
  if (!LittleFS.begin()) {
    /* ครั้งแรกหลัง flash - ยังไม่มี filesystem ต้อง format ก่อน
       ถ้า format ไม่ผ่านแปลว่าตั้ง Flash Size ใน IDE ไม่มี FS partition */
    if (!LittleFS.format() || !LittleFS.begin()) return false;
  }
  storeLoad();
  return true;
}

bool storeLoad() {
  if (!LittleFS.exists(CFG_PATH)) return false;

  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) return false;

  StaticJsonDocument<JDOC> d;
  DeserializationError err = deserializeJson(d, f);
  f.close();
  if (err) return false;

  cfg.tz  = d["tz"]  | TZ_OFFSET_DEFAULT;
  cfg.ntp = d["ntp"] | NTP_SERVER_DEFAULT;

  JsonObjectConst h = d["hue"];
  cfg.hue.ip     = h["ip"]     | "";
  cfg.hue.user   = h["user"]   | "";
  cfg.hue.api    = h["api"]    | 1;
  cfg.hue.group  = h["group"]  | "0";
  cfg.hue.sceneA = h["sceneA"] | "";
  cfg.hue.sceneB = h["sceneB"] | "";

  JsonObjectConst s = d["sales"];
  cfg.sales.url   = s["url"]   | "";
  cfg.sales.token = s["token"] | "";
  cfg.sales.every = s["every"] | SALES_POLL_DEFAULT_S;
  if (cfg.sales.every < 60) cfg.sales.every = 60;   /* กันตั้งถี่จนโดน quota */

  JsonObjectConst w = d["web"];
  cfg.web.user = w["user"] | "admin";
  cfg.web.pass = w["pass"] | "";

  JsonObjectConst u = d["ui"];
  cfg.ui.bright = u["bright"] | 255;
  cfg.ui.page   = u["page"]   | (uint8_t)PAGE_CLOCK;
  if (cfg.ui.page >= PAGE_COUNT) cfg.ui.page = PAGE_CLOCK;

  return true;
}

bool storeSave() {
  StaticJsonDocument<JDOC> d;

  d["tz"]  = cfg.tz;
  d["ntp"] = cfg.ntp;

  JsonObject h = d.createNestedObject("hue");
  h["ip"]     = cfg.hue.ip;
  h["user"]   = cfg.hue.user;
  h["api"]    = cfg.hue.api;
  h["group"]  = cfg.hue.group;
  h["sceneA"] = cfg.hue.sceneA;
  h["sceneB"] = cfg.hue.sceneB;

  JsonObject s = d.createNestedObject("sales");
  s["url"]   = cfg.sales.url;
  s["token"] = cfg.sales.token;
  s["every"] = cfg.sales.every;

  JsonObject w = d.createNestedObject("web");
  w["user"] = cfg.web.user;
  w["pass"] = cfg.web.pass;

  JsonObject u = d.createNestedObject("ui");
  u["bright"] = cfg.ui.bright;
  u["page"]   = cfg.ui.page;

  /* เขียนลงไฟล์ชั่วคราวก่อนแล้วค่อย rename - ถ้าไฟดับกลางคัน config เดิม
     จะยังอยู่ครบ ไม่กลายเป็นไฟล์ครึ่ง ๆ ที่ parse ไม่ได้ */
  File f = LittleFS.open("/config.tmp", "w");
  if (!f) return false;
  bool ok = (serializeJson(d, f) > 0);
  f.close();
  if (!ok) { LittleFS.remove("/config.tmp"); return false; }

  LittleFS.remove(CFG_PATH);
  return LittleFS.rename("/config.tmp", CFG_PATH);
}

void storeReset() {
  LittleFS.remove(CFG_PATH);
  cfg = Config();
}
