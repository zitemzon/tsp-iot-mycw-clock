#include "net_time.h"
#include "store.h"
#include "ui.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <time.h>

static bool     gNtpOk    = false;
static uint32_t gNtpCheck = 0;
static char     gWebPin[8] = "";

/* ------------------------------------------------- double reset detect (DRD) */
#define DRD_MAGIC   0x7B1DC10C
#define DRD_WINDOW_MS 6000
static uint32_t gDrdStart = 0;
static bool     gDrdArmed = false;

bool netDrdBegin() {
  uint32_t magic = 0;
  ESP.rtcUserMemoryRead(0, &magic, sizeof(magic));
  bool detected = (magic == DRD_MAGIC);

  uint32_t w = DRD_MAGIC;
  ESP.rtcUserMemoryWrite(0, &w, sizeof(w));
  gDrdStart = millis();
  gDrdArmed = true;
  return detected;
}

void netDrdLoop() {
  if (!gDrdArmed || millis() - gDrdStart < DRD_WINDOW_MS) return;
  uint32_t zero = 0;
  ESP.rtcUserMemoryWrite(0, &zero, sizeof(zero));
  gDrdArmed = false;
}

/* --------------------------------------------------------------- web PIN
   ถ้ายังไม่เคยตั้งรหัสหน้าเว็บ ให้สุ่ม 6 หลักแล้วโชว์บนจอ
   คนที่มองเห็นจอเท่านั้นถึงจะเข้าหน้าตั้งค่าได้ - สำคัญเพราะเครื่องอยู่บน
   เน็ตเวิร์กร้านที่มีพนักงานและลูกค้าใช้ร่วมกัน */
static void ensureWebPin() {
  if (cfg.web.pass.length()) { gWebPin[0] = '\0'; return; }
  randomSeed(ESP.getCycleCount() ^ micros());
  snprintf(gWebPin, sizeof(gWebPin), "%06lu", (unsigned long)random(0, 1000000));
  cfg.web.pass = gWebPin;
  storeSave();
}

static void onPortal(WiFiManager* wm) {
  (void)wm;
  uiWifiSetupScreen(gWebPin[0] ? gWebPin : cfg.web.pass.c_str());
}

static void startMdns() {
  if (MDNS.begin(DEV_NAME)) MDNS.addService("http", "tcp", 80);
}

bool netBegin(bool forcePortal) {
  ensureWebPin();

  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEV_NAME);

  WiFiManager wm;
  wm.setAPCallback(onPortal);
  wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_S);
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setDebugOutput(false);

  bool ok;
  if (forcePortal) {
    uiWifiSetupScreen(gWebPin[0] ? gWebPin : cfg.web.pass.c_str());
    ok = wm.startConfigPortal(AP_SSID, AP_PASS);
  } else {
    uiBootScreen("CONNECTING WIFI");
    ok = wm.autoConnect(AP_SSID, AP_PASS);
  }

  if (!ok) {
    /* หมดเวลา portal แล้วยังไม่ได้ตั้งค่า - รีสตาร์ตไปลอง WiFi เดิมใหม่
       ดีกว่าค้างอยู่ในโหมด AP ตลอดไปโดยที่นาฬิกาไม่แสดงเวลา */
    uiBootScreen("SETUP TIMEOUT - RESTARTING");
    delay(1500);
    ESP.restart();
  }

  /* configTime(0,0,...) = ขอเวลา UTC แล้วเราบวก offset เอง
     ไม่ใช้ TZ string ของ newlib เพราะไทยไม่มี DST ไม่ต้องการตารางโซนเวลา */
  configTime(0, 0, cfg.ntp.c_str());
  startMdns();
  return true;
}

void netStartPortal() {
  WiFiManager wm;
  wm.setAPCallback(onPortal);
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setDebugOutput(false);

  uiWifiSetupScreen(gWebPin[0] ? gWebPin : cfg.web.pass.c_str());
  if (!wm.startConfigPortal(AP_SSID, AP_PASS)) {
    uiBootScreen("SETUP TIMEOUT - RESTARTING");
    delay(1500);
    ESP.restart();
  }
  configTime(0, 0, cfg.ntp.c_str());
  startMdns();
}

void netLoop() {
  MDNS.update();
  netDrdLoop();

  /* เช็คว่า NTP ซิงก์แล้วหรือยังทุก 2 วินาที ไม่ต้องถี่กว่านี้ */
  if (millis() - gNtpCheck >= 2000) {
    gNtpCheck = millis();
    gNtpOk = (time(nullptr) > 1600000000UL);   /* > ก.ย. 2020 = ซิงก์แล้ว */
  }
}

bool      netOnline()  { return WiFi.status() == WL_CONNECTED; }
int       netRssi()    { return WiFi.RSSI(); }
bool      netNtpOk()   { return gNtpOk; }
IPAddress netIp()      { return WiFi.localIP(); }

time_t netLocalNow() {
  time_t utc = time(nullptr);
  return utc + (time_t)cfg.tz;
}
