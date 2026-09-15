#include "webui.h"
#include "store.h"
#include "hue.h"
#include "tsp.h"
#include "ui.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

static ESP8266WebServer       server(80);
static ESP8266HTTPUpdateServer updater;

static bool gPairReq  = false;
static bool gSalesReq = false;

bool webuiTakeHuePairRequest()      { bool r = gPairReq;  gPairReq  = false; return r; }
bool webuiTakeSalesRefreshRequest() { bool r = gSalesReq; gSalesReq = false; return r; }

static bool guard() {
  if (!cfg.web.pass.length()) return true;          /* ยังไม่ตั้งรหัส - ปล่อยผ่าน */
  if (server.authenticate(cfg.web.user.c_str(), cfg.web.pass.c_str())) return true;
  server.requestAuthentication();
  return false;
}

/* escape ค่าที่เอาไปใส่ใน value="..." กัน token ที่มี " หรือ & ทำ HTML พัง */
static String esc(const String& s) {
  String o; o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&':  o += F("&amp;");  break;
      case '<':  o += F("&lt;");   break;
      case '>':  o += F("&gt;");   break;
      case '"':  o += F("&quot;"); break;
      default:   o += c;
    }
  }
  return o;
}

static const char CSS[] PROGMEM =
  "<style>body{font-family:system-ui,sans-serif;margin:0;padding:16px;"
  "background:#111;color:#eee;max-width:560px}h2{color:#5cf;margin:18px 0 6px;"
  "font-size:1.05rem;border-bottom:1px solid #333;padding-bottom:4px}"
  "label{display:block;margin:10px 0 3px;font-size:.82rem;color:#9ab}"
  "input,select{width:100%;padding:9px;border:1px solid #444;border-radius:6px;"
  "background:#1c1c1c;color:#eee;font-size:1rem;box-sizing:border-box}"
  "button,.b{display:inline-block;margin:6px 6px 0 0;padding:10px 16px;border:0;"
  "border-radius:6px;background:#2a6;color:#fff;font-size:.95rem;cursor:pointer;"
  "text-decoration:none}.g{background:#357}.r{background:#a33}"
  ".s{font-size:.8rem;color:#9ab;margin:4px 0}</style>";

static void sendPage() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  server.sendContent(F("<!doctype html><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>MyCW Settings</title>"));
  server.sendContent_P(CSS);
  server.sendContent(F("<h1 style='font-size:1.2rem'>MyCW Rainbow Clock</h1>"));

  String s;
  s  = F("<p class=s>Firmware "); s += FW_VERSION;
  s += F(" &middot; IP "); s += WiFi.localIP().toString();
  s += F(" &middot; Heap "); s += String(ESP.getFreeHeap());
  s += F("</p><form method=POST action=/save>");
  server.sendContent(s);

  /* ---- time ---- */
  s  = F("<h2>Time</h2><label>UTC offset (seconds)</label>"
         "<input name=tz value='"); s += String(cfg.tz);
  s += F("'><label>NTP server</label><input name=ntp value='");
  s += esc(cfg.ntp); s += F("'>");
  server.sendContent(s);

  /* ---- hue ---- */
  s  = F("<h2>Philips Hue</h2><p class=s>Status: ");
  s += hueIsPaired() ? F("paired, API v") : F("not paired");
  if (hueIsPaired()) s += String(cfg.hue.api);
  s += F("</p><label>Bridge IP</label><input name=hip value='");
  s += esc(cfg.hue.ip);
  s += F("'><label>Group / grouped_light id (0 = all lights)</label><input name=hgr value='");
  s += esc(cfg.hue.group);
  s += F("'><label>Scene A id</label><input name=hsa value='");
  s += esc(cfg.hue.sceneA);
  s += F("'><label>Scene B id</label><input name=hsb value='");
  s += esc(cfg.hue.sceneB);
  s += F("'>");
  server.sendContent(s);

  /* ---- sales ---- */
  s  = F("<h2>Sales feed</h2>"
         "<label>Web App URL (ends with /exec)</label><input name=surl value='");
  s += esc(cfg.sales.url);
  s += F("'><label>CLOCK_TOKEN</label><input name=stok type=password value='");
  s += esc(cfg.sales.token);
  s += F("'><label>Refresh every (seconds, min 60)</label><input name=sev value='");
  s += String(cfg.sales.every);
  s += F("'>");
  server.sendContent(s);

  /* ---- web auth ---- */
  s  = F("<h2>Web access</h2><label>User</label><input name=wu value='");
  s += esc(cfg.web.user);
  s += F("'><label>Password (blank keeps current)</label>"
         "<input name=wp type=password value=''>");
  s += F("<p><button type=submit>Save</button></p></form>");
  server.sendContent(s);

  /* ---- actions ---- */
  server.sendContent(F(
    "<h2>Actions</h2>"
    "<a class='b g' href=/discover>Find bridge</a>"
    "<a class='b g' href=/pair>Pair with bridge</a>"
    "<a class='b g' href=/salesnow>Refresh sales</a>"
    "<a class='b' href=/update>Firmware update</a>"
    "<a class='b r' href=/restart>Restart</a>"
    "<a class='b r' href=/wifireset>Forget WiFi</a>"
    "<p class=s>Pairing: tap \"Pair with bridge\", then press the round button "
    "on the Hue Bridge within 30 seconds. The clock screen counts down.</p>"));
}

static void hRoot()  { if (!guard()) return; sendPage(); }

static void hSave() {
  if (!guard()) return;

  if (server.hasArg("tz"))  cfg.tz  = server.arg("tz").toInt();
  if (server.hasArg("ntp")) cfg.ntp = server.arg("ntp");

  cfg.hue.ip     = server.arg("hip");
  cfg.hue.group  = server.arg("hgr");
  cfg.hue.sceneA = server.arg("hsa");
  cfg.hue.sceneB = server.arg("hsb");
  if (!cfg.hue.group.length()) cfg.hue.group = "0";

  cfg.sales.url   = server.arg("surl");
  cfg.sales.token = server.arg("stok");
  long ev = server.arg("sev").toInt();
  cfg.sales.every = (ev < 60) ? 60 : (uint16_t)ev;

  if (server.arg("wu").length()) cfg.web.user = server.arg("wu");
  /* ช่องรหัสผ่านว่าง = ไม่เปลี่ยน ป้องกันการเซฟฟอร์มแล้วรหัสหายโดยไม่ตั้งใจ */
  if (server.arg("wp").length()) cfg.web.pass = server.arg("wp");

  storeSave();
  uiMarkDirty();
  server.sendHeader("Location", "/");
  server.send(303);
}

static void hDiscover() {
  if (!guard()) return;
  String ip;
  if (hueDiscover(ip)) {
    cfg.hue.ip = ip;
    storeSave();
    server.send(200, "text/html",
      "<meta http-equiv=refresh content='2;url=/'>Bridge found at " + ip);
  } else {
    server.send(200, "text/html",
      F("<meta http-equiv=refresh content='3;url=/'>No bridge found on this network. "
        "Enter the IP manually (Hue app &rarr; Settings &rarr; Bridge)."));
  }
}

static void hPair() {
  if (!guard()) return;
  if (!cfg.hue.ip.length()) {
    server.send(200, "text/html",
      F("<meta http-equiv=refresh content='3;url=/'>Set the bridge IP first."));
    return;
  }
  gPairReq = true;      /* ให้ main loop ทำ - handler ต้องไม่ block นาน */
  server.send(200, "text/html",
    F("<meta http-equiv=refresh content='35;url=/'>"
      "Press the round button on the Hue Bridge now. Watch the clock screen."));
}

static void hSalesNow() {
  if (!guard()) return;
  gSalesReq = true;
  server.send(200, "text/html", F("<meta http-equiv=refresh content='3;url=/'>Refreshing..."));
}

static void hRestart() {
  if (!guard()) return;
  server.send(200, "text/html", F("Restarting..."));
  delay(300);
  ESP.restart();
}

static void hWifiReset() {
  if (!guard()) return;
  server.send(200, "text/html", F("WiFi credentials cleared. Restarting into setup mode..."));
  delay(300);
  WiFi.disconnect(true);
  delay(200);
  ESP.restart();
}

void webuiBegin() {
  /* หน้า /update มี auth ของตัวเองแยกต่างหาก */
  updater.setup(&server, "/update", cfg.web.user.c_str(), cfg.web.pass.c_str());

  server.on("/",         hRoot);
  server.on("/save",     HTTP_POST, hSave);
  server.on("/discover", hDiscover);
  server.on("/pair",     hPair);
  server.on("/salesnow", hSalesNow);
  server.on("/restart",  hRestart);
  server.on("/wifireset",hWifiReset);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(303); });
  server.begin();
}

void webuiLoop() { server.handleClient(); }
