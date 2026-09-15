#include "hue.h"
#include "store.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

static bool     gOn      = false;
static uint8_t  gBri     = 0;
static bool     gLastOk  = true;
static uint32_t gPairEnd = 0;

/* เก็บ TLS session ไว้ใช้ซ้ำ (session resumption) การกดปุ่มครั้งถัด ๆ ไป
   จะไม่ต้อง handshake เต็มรูปแบบ ลดเวลาจาก ~600ms เหลือ ~150ms */
static BearSSL::Session gTlsSession;

void hueBegin() {
  gOn = false; gBri = 0; gLastOk = true;
}

bool hueIsPaired()   { return cfg.hue.ip.length() && cfg.hue.user.length(); }
bool hueLastOk()     { return gLastOk; }
bool hueStateOn()    { return gOn; }
uint8_t hueStateBri(){ return gBri; }

bool hueDiscover(String& ipOut) {
  int n = MDNS.queryService("hue", "tcp");
  if (n <= 0) return false;
  ipOut = MDNS.IP(0).toString();
  return ipOut.length() > 0;
}

/* --------------------------------------------------------------- transport
   ห่อ HTTP/HTTPS ไว้ที่เดียว ผู้เรียกส่งแค่ method/path/body
   คืน HTTP status (< 0 = ต่อไม่ติด) และเติม respOut ถ้าต้องการอ่านคำตอบ */
static int hueRequest(bool secure, const char* method, const String& path,
                      const String& body, String* respOut) {
  if (secure && ESP.getFreeHeap() < TLS_MIN_FREE_HEAP) return -100;

  HTTPClient http;
  int code = -1;

  if (secure) {
    /* Bridge ใช้ self-signed cert จึงตรวจไม่ได้อยู่แล้ว - ปลอดภัยพอเพราะ
       ทั้งหมดอยู่ใน LAN และ token ไม่ได้วิ่งออกอินเทอร์เน็ต */
    std::unique_ptr<BearSSL::WiFiClientSecure> cli(new BearSSL::WiFiClientSecure);
    cli->setInsecure();
    cli->setBufferSizes(1024, 512);
    cli->setSession(&gTlsSession);
    if (!http.begin(*cli, "https://" + cfg.hue.ip + path)) return -1;
    http.addHeader("hue-application-key", cfg.hue.user);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(6000);
    code = http.sendRequest(method, (uint8_t*)body.c_str(), body.length());
    if (respOut && code > 0) *respOut = http.getString();
    http.end();
  } else {
    WiFiClient cli;
    if (!http.begin(cli, "http://" + cfg.hue.ip + path)) return -1;
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(4000);
    code = http.sendRequest(method, (uint8_t*)body.c_str(), body.length());
    if (respOut && code > 0) *respOut = http.getString();
    http.end();
  }
  return code;
}

/* ------------------------------------------------------------------ pairing */
void huePairStart() { gPairEnd = millis() + 30000; }

HuePairResult huePairPoll(int& secsLeft) {
  if (!cfg.hue.ip.length()) { secsLeft = 0; return HUE_PAIR_FAIL; }

  long left = (long)(gPairEnd - millis());
  secsLeft = left > 0 ? (int)(left / 1000) : 0;

  const String body = "{\"devicetype\":\"mycw#rainbowclock\",\"generateclientkey\":true}";
  String resp;

  /* endpoint /api ใช้สร้าง key ได้ทั้ง v1 และ v2 - ลอง HTTP ก่อนเพราะเบากว่า
     ถ้าบริดจ์ปิด HTTP แล้ว (v1 ถูกเลิกรองรับ) ค่อยลอง HTTPS */
  int code = hueRequest(false, "POST", "/api", body, &resp);
  bool viaTls = false;
  if (code <= 0 || code == 404 || code == 410) {
    resp = "";
    code = hueRequest(true, "POST", "/api", body, &resp);
    viaTls = true;
  }

  if (code > 0 && resp.length()) {
    StaticJsonDocument<512> d;
    if (!deserializeJson(d, resp) && d.is<JsonArray>() && d.size() > 0) {
      JsonObject o = d[0];
      if (o.containsKey("success")) {
        cfg.hue.user = (const char*)o["success"]["username"];
        /* ตรวจว่าบริดจ์ยังรับ v1 อยู่ไหม โดยลองอ่าน config ผ่าน HTTP จริง ๆ
           ถูกกว่าการเดาจากเฟิร์มแวร์เวอร์ชัน */
        cfg.hue.api = viaTls ? 2 : 1;
        if (!viaTls) {
          String probe;
          int pc = hueRequest(false, "GET", "/api/" + cfg.hue.user + "/groups/0", "", &probe);
          if (pc != 200) cfg.hue.api = 2;
        }
        storeSave();
        hueRefreshState();
        return HUE_PAIR_OK;
      }
      if (o.containsKey("error")) {
        int type = o["error"]["type"] | 0;
        if (type == 101) {                       /* ยังไม่ได้กดปุ่มบน Bridge */
          return (left > 0) ? HUE_PAIR_WAIT : HUE_PAIR_FAIL;
        }
        return HUE_PAIR_FAIL;
      }
    }
  }
  return (left > 0) ? HUE_PAIR_WAIT : HUE_PAIR_FAIL;
}

/* -------------------------------------------------------------- read state */
bool hueRefreshState() {
  if (!hueIsPaired()) return false;
  String resp;
  int code;

  if (cfg.hue.api == 1) {
    code = hueRequest(false, "GET", "/api/" + cfg.hue.user + "/groups/" + cfg.hue.group,
                      "", &resp);
    if (code == 200) {
      StaticJsonDocument<512> d;
      if (!deserializeJson(d, resp)) {
        JsonObject a = d["action"];
        if (!a.isNull()) {
          gOn  = a["on"]  | false;
          gBri = a["bri"] | 0;
          gLastOk = true;
          return true;
        }
      }
    }
  } else {
    code = hueRequest(true, "GET", "/clip/v2/resource/grouped_light/" + cfg.hue.group,
                      "", &resp);
    if (code == 200) {
      StaticJsonDocument<768> d;
      if (!deserializeJson(d, resp)) {
        JsonObject o = d["data"][0];
        if (!o.isNull()) {
          gOn = o["on"]["on"] | false;
          float pct = o["dimming"]["brightness"] | 0.0f;
          gBri = (uint8_t)(pct * 254.0f / 100.0f + 0.5f);
          gLastOk = true;
          return true;
        }
      }
    }
  }
  gLastOk = false;
  return false;
}

/* ------------------------------------------------------------ write state */
bool hueSetGroup(bool on, uint8_t bri) {
  if (!hueIsPaired()) return false;
  int code;

  if (cfg.hue.api == 1) {
    String body = "{\"on\":" + String(on ? "true" : "false");
    if (on) body += ",\"bri\":" + String((int)bri);
    body += "}";
    code = hueRequest(false, "PUT",
                      "/api/" + cfg.hue.user + "/groups/" + cfg.hue.group + "/action",
                      body, nullptr);
    /* บริดจ์ที่ปิด v1 แล้วจะตอบ 404/410 - สลับไป v2 แล้วจำไว้ ไม่ต้องให้ผู้ใช้
       มาตั้งค่าเอง และครั้งต่อไปจะยิง v2 ตรง ๆ เลย */
    if (code == 404 || code == 410) {
      cfg.hue.api = 2;
      storeSave();
    } else {
      gLastOk = (code == 200);
      if (gLastOk) { gOn = on; if (on) gBri = bri; }
      return gLastOk;
    }
  }

  /* v2: dimming.brightness เป็นเปอร์เซ็นต์ 0-100 (ไม่ใช่ 0-254 แบบ v1) */
  String body = "{\"on\":{\"on\":" + String(on ? "true" : "false") + "}";
  if (on) {
    float pct = bri * 100.0f / 254.0f;
    body += ",\"dimming\":{\"brightness\":" + String(pct, 1) + "}";
  }
  body += "}";
  code = hueRequest(true, "PUT",
                    "/clip/v2/resource/grouped_light/" + cfg.hue.group, body, nullptr);
  gLastOk = (code == 200);
  if (gLastOk) { gOn = on; if (on) gBri = bri; }
  return gLastOk;
}

bool hueToggle() {
  uint8_t bri = gBri ? gBri : 200;    /* ถ้าไม่เคยอ่านค่ามา ให้เปิดที่ ~80% */
  return hueSetGroup(!gOn, bri);
}

bool hueRecallScene(const String& sceneId) {
  if (!hueIsPaired() || !sceneId.length()) return false;
  int code;

  if (cfg.hue.api == 1) {
    code = hueRequest(false, "PUT",
                      "/api/" + cfg.hue.user + "/groups/" + cfg.hue.group + "/action",
                      "{\"scene\":\"" + sceneId + "\"}", nullptr);
    if (code != 404 && code != 410) {
      gLastOk = (code == 200);
      if (gLastOk) { gOn = true; hueRefreshState(); }
      return gLastOk;
    }
    cfg.hue.api = 2;
    storeSave();
  }

  code = hueRequest(true, "PUT", "/clip/v2/resource/scene/" + sceneId,
                    "{\"recall\":{\"action\":\"active\"}}", nullptr);
  gLastOk = (code == 200);
  if (gLastOk) { gOn = true; hueRefreshState(); }
  return gLastOk;
}
