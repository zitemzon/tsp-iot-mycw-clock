/* =======================================================================
   MyCW - Rainbow Clock v2   |   Toy Station Plus+
   NodeMCU V2 (ESP8266) + 2.8" TFT ILI9341

   สิ่งที่ทำได้
     - นาฬิกา NTP ตั้ง WiFi จากมือถือ ไม่ต้องคอมไพล์ใหม่เวลาย้ายสถานที่
     - ปุ่มสัมผัส 2 ปุ่ม เปิด/ปิด/หรี่ไฟ และเรียก scene ของ Philips Hue
     - โชว์ยอดขายวันนี้ 8 สาขา พร้อมธงว่าสาขาไหน "ยังไม่ส่งรายงาน"
     - อัปเดตเฟิร์มแวร์และตั้งค่าทุกอย่างผ่านหน้าเว็บบนมือถือ (OTA)

   กฎเหล็ก: ข้อความบนจอทุกตัวเป็น English ASCII เท่านั้น (ดู CLAUDE.md ข้อ 1)
            ทุกการวาดข้อความต้องผ่าน uiText() ใน ui.cpp
   ======================================================================= */
#include "config.h"
#include "store.h"
#include "ui.h"
#include "net_time.h"
#include "touch.h"
#include "hue.h"
#include "tsp.h"
#include "webui.h"

static uint32_t gHueNextPoll  = 0;
static uint32_t gPageSaveAt   = 0;     /* 0 = ไม่มีอะไรรอเซฟ */
static bool     gRampActive   = false;
static bool     gRampUp       = true;
static uint32_t gRampNext     = 0;

/* ------------------------------------------------------------------ helpers */
static void syncUiState() {
  uid.wifiOk    = netOnline();
  uid.rssi      = uid.wifiOk ? netRssi() : 0;
  uid.ntpOk     = netNtpOk();
  uid.huePaired = hueIsPaired();
  uid.hueOn     = hueStateOn();
  uid.hueBri    = hueStateBri();
  uid.hueOk     = hueLastOk();
  uid.hueApi    = cfg.hue.api;
  strncpy(uid.ipStr, netIp().toString().c_str(), sizeof(uid.ipStr) - 1);
  uid.ipStr[sizeof(uid.ipStr) - 1] = '\0';
}

/* จำหน้าที่ผู้ใช้เปิดค้างไว้ แต่หน่วงเขียนแฟลช 10 วินาที
   ถ้าเซฟทุกครั้งที่กดเปลี่ยนหน้า แฟลชจะสึกเร็วโดยไม่จำเป็น */
static void schedulePageSave() { gPageSaveAt = millis() + 10000; }

static void servicePageSave() {
  if (!gPageSaveAt || (int32_t)(millis() - gPageSaveAt) < 0) return;
  gPageSaveAt = 0;
  if (cfg.ui.page != uiPage()) { cfg.ui.page = uiPage(); storeSave(); }
}

/* pairing กิน 30 วินาที จึงต้องเดินเซิร์ฟเวอร์เว็บต่อระหว่างรอ
   ไม่งั้นมือถือที่เปิดหน้าตั้งค่าค้างอยู่จะเจอ connection timeout */
static void runHuePairing() {
  huePairStart();
  int  secs = 30;
  HuePairResult r = HUE_PAIR_WAIT;

  while (true) {
    r = huePairPoll(secs);
    if (r != HUE_PAIR_WAIT) break;
    uiHuePairScreen(secs, false);
    for (int i = 0; i < 15; i++) { webuiLoop(); netLoop(); delay(100); }
  }

  if (r == HUE_PAIR_OK) {
    uiHuePairScreen(0, false);
    uiToast("HUE PAIRED", C_OK, 2000);
    uiSetPage(PAGE_LIGHTS);
  } else {
    uiHuePairScreen(0, true);
    delay(2500);
    uiSetPage(uiPage());
  }
}

/* กดปุ่ม B ค้าง = ไล่ความสว่างขึ้นหรือลง สลับทิศทุกครั้งที่กดค้างใหม่ */
static void serviceBrightnessRamp() {
  if (!gRampActive) return;

  if (!touchHeldB()) {          /* ปล่อยนิ้วแล้ว */
    gRampActive = false;
    return;
  }
  if ((int32_t)(millis() - gRampNext) < 0) return;
  gRampNext = millis() + 300;

  int bri = hueStateBri() + (gRampUp ? 30 : -30);
  if (bri > 254) { bri = 254; gRampUp = false; }
  if (bri < 10)  { bri = 10;  gRampUp = true;  }
  hueSetGroup(true, (uint8_t)bri);
  uiMarkDirty();
}

static void handleTouch(TouchEvent e) {
  switch (e) {
    case EV_A_TAP:
      uiNextPage();
      schedulePageSave();
      break;

    case EV_A_LONG:
      /* ทางเข้าโหมดตั้งค่า WiFi หลัก - ยังคงกลไกกด RESET 2 ครั้งไว้ด้วย
         เผื่อวันหนึ่งปุ่มสัมผัสเสีย จะได้ไม่ต้องรื้อเครื่องไปต่อคอม */
      uiToast("OPENING WIFI SETUP", C_WARN, 800);
      delay(800);
      netStartPortal();
      uiSetPage(uiPage());
      break;

    case EV_B_TAP:
      if (uiPage() == PAGE_SALES) {
        uiToast("REFRESHING SALES", C_ACCENT, 1200);
        tspRequestNow();
      } else if (hueIsPaired()) {
        /* วาดไอคอนใหม่ทันทีแบบ optimistic แล้วค่อยยิงคำสั่ง
           ถ้าล้มเหลว hueLastOk() จะเป็น false แล้วแถบสถานะขึ้น HUE ERROR เอง */
        bool ok = hueToggle();
        uiMarkDirty();
        if (!ok) uiToast("HUE ERROR", C_PENDING, 2500);
      } else {
        uiToast("HUE NOT PAIRED", C_DIM, 1500);
      }
      break;

    case EV_B_DOUBLE:
      if (uiPage() != PAGE_SALES && hueIsPaired()) {
        uid.hueScene = (uid.hueScene == 'A') ? 'B' : 'A';
        const String& id = (uid.hueScene == 'A') ? cfg.hue.sceneA : cfg.hue.sceneB;
        if (id.length()) {
          if (hueRecallScene(id)) uiToast("SCENE RECALLED", C_OK, 1500);
          else                    uiToast("HUE ERROR", C_PENDING, 2500);
        } else {
          uiToast("SCENE NOT SET", C_DIM, 1500);
        }
        uiMarkDirty();
      }
      break;

    case EV_B_LONG:
      if (uiPage() != PAGE_SALES && hueIsPaired()) {
        gRampActive = true;
        gRampUp     = !gRampUp;
        gRampNext   = 0;
        uiToast(gRampUp ? "BRIGHTER" : "DIMMER", C_ACCENT, 900);
      }
      break;

    default: break;
  }
}

/* ===================================================================== setup */
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n[MyCW] boot " FW_VERSION));

  uiBegin();
  uiBootScreen("STARTING");

  if (!storeBegin()) {
    /* ไม่มี filesystem = ตั้ง Flash Size ใน IDE ผิด (ต้องมี FS partition)
       ปล่อยให้เดินต่อได้ แต่ config จะไม่ถูกจำข้ามการรีบูต */
    Serial.println(F("[MyCW] LittleFS mount failed - check Flash Size (FS:1MB)"));
    uiBootScreen("NO FILESYSTEM - CHECK FLASH SIZE");
    delay(2500);
  }

  bool doublePress = netDrdBegin();
  if (doublePress) Serial.println(F("[MyCW] double reset -> wifi setup"));

  netBegin(doublePress);

  touchBegin();
  hueBegin();
  webuiBegin();
  tspBegin();

  if (hueIsPaired()) hueRefreshState();
  syncUiState();

  uiSetPage(cfg.ui.page);
  Serial.print(F("[MyCW] ready at http://"));
  Serial.println(netIp());
}

/* ====================================================================== loop */
void loop() {
  netLoop();
  webuiLoop();
  tspLoop();

  TouchEvent e = touchPoll();
  if (e != EV_NONE) {
    Serial.printf("[touch] event %d\n", (int)e);
    handleTouch(e);
  }
  serviceBrightnessRamp();

  if (webuiTakeHuePairRequest())      runHuePairing();
  if (webuiTakeSalesRefreshRequest()) tspRequestNow();

  /* อ่านสถานะไฟซ้ำทุก 30 วินาที เพื่อให้จอตรงกับความจริงแม้จะมีคนไปสั่งไฟ
     จากแอป Hue หรือสวิตช์อื่น โดยที่นาฬิกาไม่รู้ */
  if (hueIsPaired() && (int32_t)(millis() - gHueNextPoll) >= 0) {
    gHueNextPoll = millis() + 30000;
    hueRefreshState();
    uiMarkDirty();
  }

  servicePageSave();
  syncUiState();
  uiTick(netLocalNow());
}
