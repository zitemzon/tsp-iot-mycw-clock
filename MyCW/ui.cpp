/* =======================================================================
   ui.cpp - ไฟล์เดียวที่ได้รับอนุญาตให้วาดข้อความลงจอ (ดู CLAUDE.md ข้อ 1)
   ======================================================================= */
#include "ui.h"
#include "store.h"
#include <TFT_eSPI.h>

static TFT_eSPI tft = TFT_eSPI();

UiData uid;

static uint8_t  gPage      = PAGE_CLOCK;
static bool     gFullRedraw= true;
static bool     gDirty     = true;
static uint32_t gToastUntil= 0;
/* toast วาดทับพื้นที่เดียวกับแถบสถานะ พอ toast หายต้องบังคับวาดแถบสถานะใหม่
   ไม่งั้น cache ของ drawStatusBar จะคิดว่า "ค่าไม่เปลี่ยน" แล้วปล่อยจอว่างไว้ */
static bool     gStatusDirty = true;

/* ---------------------------------------------------------------- ASCII gate
   ตัวกรองกลาง: byte ใดที่ไม่ใช่ ASCII พิมพ์ได้ (0x20-0x7E) จะกลายเป็น '?'
   ทำให้ต่อให้ชีตถูกแก้เป็นภาษาไทยในอนาคต จอก็จะขึ้น "????" ให้เห็นชัด
   แทนที่จะเป็นขยะที่อ่านไม่ออกและหาสาเหตุไม่เจอ */
static const size_t ASCII_BUF = 64;

static const char* asciiOnly(const char* s) {
  static char buf[ASCII_BUF];
  size_t n = 0;
  if (s) {
    for (; s[n] && n < ASCII_BUF - 1; n++) {
      uint8_t c = (uint8_t)s[n];
      buf[n] = (c >= 0x20 && c <= 0x7E) ? (char)c : '?';
    }
  }
  buf[n] = '\0';
  return buf;
}

/* จุดวาดข้อความจุดเดียวของทั้งโปรเจกต์ */
static void uiText(const char* s, int32_t x, int32_t y, uint8_t font,
                   uint16_t fg, uint16_t bg = C_BG, uint8_t datum = TL_DATUM) {
  tft.setTextDatum(datum);
  tft.setTextColor(fg, bg);
  tft.drawString(asciiOnly(s), x, y, font);
}

/* ฟิลด์ความกว้างคงที่: ล้างพื้นที่ก่อนวาด กันเศษข้อความเดิมที่ยาวกว่าค้างอยู่ */
static void uiField(const char* s, int32_t x, int32_t y, int32_t w, uint8_t font,
                    uint16_t fg, uint8_t datum = TL_DATUM) {
  tft.fillRect(x, y, w, tft.fontHeight(font), C_BG);
  int32_t tx = x;
  if (datum == TR_DATUM) tx = x + w;
  else if (datum == TC_DATUM) tx = x + w / 2;
  uiText(s, tx, y, font, fg, C_BG, datum);
}

/* ------------------------------------------------------------------ format */
/* 48250 -> "48,250" อ่านเลขบนจอได้เร็วกว่ามากเวลาเหลือบมอง */
static void fmtThousands(long v, char* out, size_t cap) {
  char raw[16];
  snprintf(raw, sizeof(raw), "%ld", v < 0 ? -v : v);
  int len = strlen(raw), o = 0;
  if (v < 0 && o < (int)cap - 1) out[o++] = '-';
  for (int i = 0; i < len && o < (int)cap - 1; i++) {
    if (i > 0 && (len - i) % 3 == 0) out[o++] = ',';
    out[o++] = raw[i];
  }
  out[o] = '\0';
}

static const char* WDAY[7]  = { "SUN","MON","TUE","WED","THU","FRI","SAT" };
static const char* MON12[12]= { "JAN","FEB","MAR","APR","MAY","JUN",
                                "JUL","AUG","SEP","OCT","NOV","DEC" };

/* เลขสัปดาห์แบบ ISO-8601 (สัปดาห์ที่มีวันพฤหัสอยู่ = สัปดาห์นั้นของปี) */
static int isoWeek(const struct tm& t) {
  int wday = (t.tm_wday + 6) % 7;            /* จันทร์ = 0 */
  int yday = t.tm_yday;                      /* 0-based    */
  int week = (yday - wday + 10) / 7;
  if (week < 1) return 52;                   /* ตกไปสัปดาห์สุดท้ายของปีก่อน */
  if (week > 52) {
    int jan1 = (t.tm_wday - t.tm_yday % 7 + 7) % 7;
    bool leap = ((t.tm_year + 1900) % 4 == 0 && (t.tm_year + 1900) % 100 != 0)
                || (t.tm_year + 1900) % 400 == 0;
    if (!(jan1 == 4 || (leap && jan1 == 3))) return 1;
  }
  return week;
}

/* ไล่สีรุ้งตามตำแหน่งตัวอักษร - ไม่หมุนสีตามเวลา เพื่อให้แต่ละหลักวาดใหม่
   เฉพาะตอนที่ตัวเลขเปลี่ยนจริง จอจะนิ่งไม่กะพริบ */
static uint16_t rainbow(uint8_t i, uint8_t n) {
  uint16_t h = (uint16_t)i * 1536 / (n ? n : 1);   /* 0-1535 = 6 ช่วงสี */
  uint8_t  seg = h / 256, f = h % 256;
  uint8_t  r = 0, g = 0, b = 0;
  switch (seg) {
    case 0: r = 255;     g = f;       b = 0;       break;
    case 1: r = 255 - f; g = 255;     b = 0;       break;
    case 2: r = 0;       g = 255;     b = f;       break;
    case 3: r = 0;       g = 255 - f; b = 255;     break;
    case 4: r = f;       g = 0;       b = 255;     break;
    default:r = 255;     g = 0;       b = 255 - f; break;
  }
  return tft.color565(r, g, b);
}

/* ================================================================== screens */

void uiBegin() {
  tft.init();
  tft.setRotation(SCR_ROTATION);
  tft.fillScreen(C_BG);
  tft.setTextWrap(false);
}

void uiBootScreen(const char* line) {
  tft.fillScreen(C_BG);
  uiText("MyCW RAINBOW CLOCK", SCR_W / 2, 70, 4, C_ACCENT, C_BG, TC_DATUM);
  uiText("FW " FW_VERSION,     SCR_W / 2, 104, 2, C_DIM,   C_BG, TC_DATUM);
  uiText(line,                 SCR_W / 2, 150, 2, C_TEXT,  C_BG, TC_DATUM);
}

/* หน้าสอนตั้ง WiFi จากมือถือ - ภาษาอังกฤษล้วนตามกฎ ข้อความสั้นและเป็นขั้นตอน
   เพราะพนักงานหน้าร้านเป็นคนทำตาม ไม่ใช่ช่าง */
void uiWifiSetupScreen(const char* webPin) {
  tft.fillScreen(C_BG);
  uiText("WIFI SETUP", SCR_W / 2, 6, 4, C_ACCENT, C_BG, TC_DATUM);
  tft.drawFastHLine(0, 36, SCR_W, C_LINE);

  uiText("1. On your phone join this WiFi", 8,  46, 2, C_TEXT);
  uiText("SSID : " AP_SSID,                 24, 68, 2, C_OK);
  uiText("PASS : " AP_PASS,                 24, 88, 2, C_OK);

  uiText("2. The setup page opens by itself", 8, 116, 2, C_TEXT);
  uiText("If not open  http://192.168.4.1",   24, 138, 2, C_DIM);

  uiText("3. Tap Configure WiFi, pick your", 8, 166, 2, C_TEXT);
  uiText("network, type password, Save",     24, 188, 2, C_TEXT);

  if (webPin && webPin[0]) {
    char b[40];
    snprintf(b, sizeof(b), "WEB PIN %s", webPin);
    uiText(b, SCR_W / 2, 216, 2, C_WARN, C_BG, TC_DATUM);
  }
  gFullRedraw = true;
}

void uiHuePairScreen(int secondsLeft, bool failed) {
  tft.fillScreen(C_BG);
  uiText("PHILIPS HUE PAIRING", SCR_W / 2, 10, 4, C_ACCENT, C_BG, TC_DATUM);
  tft.drawFastHLine(0, 44, SCR_W, C_LINE);

  if (failed) {
    uiText("PAIRING FAILED",            SCR_W / 2, 90,  4, C_PENDING, C_BG, TC_DATUM);
    uiText("Check bridge IP and retry", SCR_W / 2, 130, 2, C_DIM,     C_BG, TC_DATUM);
  } else {
    uiText("Press the round button on the",  SCR_W / 2, 70,  2, C_TEXT, C_BG, TC_DATUM);
    uiText("Hue Bridge now",                 SCR_W / 2, 92,  2, C_TEXT, C_BG, TC_DATUM);
    char b[16];
    snprintf(b, sizeof(b), "%d s", secondsLeft);
    uiText(b, SCR_W / 2, 130, 7, C_WARN, C_BG, TC_DATUM);
  }
  gFullRedraw = true;
}

void uiToast(const char* msg, uint16_t colour, uint16_t ms) {
  tft.fillRect(0, SCR_H - 24, SCR_W, 24, C_BG);
  uiText(msg, SCR_W / 2, SCR_H - 22, 2, colour, C_BG, TC_DATUM);
  gToastUntil  = millis() + ms;
  gStatusDirty = true;
}

/* =================================================================== pages */

/* วาดใหม่เฉพาะช่องที่ค่าเปลี่ยนจริง ถ้าวาดทุกวินาทีตามจังหวะนาฬิกา
   แถบล่างจะกะพริบให้เห็นตลอดเวลา */
static void drawStatusBar(const struct tm& t, bool full) {
  (void)t;
  const int y = SCR_H - 22;
  if (millis() < gToastUntil) return;         /* toast ทับอยู่ อย่าวาดทับกลับ */

  static int  lastRssi = 999;
  static bool lastWifi = false, lastNtp = false;
  static bool lastPaired = false, lastOn = false, lastHueOk = true;

  if (gStatusDirty) { full = true; gStatusDirty = false; }

  char b[48];

  if (full || uid.wifiOk != lastWifi || uid.rssi != lastRssi) {
    lastWifi = uid.wifiOk; lastRssi = uid.rssi;
    if (uid.wifiOk) snprintf(b, sizeof(b), "WIFI %d dBm", uid.rssi);
    else            snprintf(b, sizeof(b), "WIFI OFFLINE");
    uiField(b, 6, y, 130, 2, uid.wifiOk ? C_DIM : C_PENDING);
  }

  if (full || uid.ntpOk != lastNtp) {
    lastNtp = uid.ntpOk;
    uiField(uid.ntpOk ? "NTP OK" : "NTP --", 140, y, 70, 2,
            uid.ntpOk ? C_DIM : C_WARN);
  }

  if (full || uid.huePaired != lastPaired || uid.hueOn != lastOn
           || uid.hueOk != lastHueOk) {
    lastPaired = uid.huePaired; lastOn = uid.hueOn; lastHueOk = uid.hueOk;
    const char* lg = !uid.huePaired ? "HUE --"
                   : !uid.hueOk     ? "HUE ERROR"
                   : uid.hueOn      ? "LIGHTS ON" : "LIGHTS OFF";
    uiField(lg, 210, y, SCR_W - 216, 2,
            !uid.huePaired ? C_DIM : !uid.hueOk ? C_PENDING
                          : uid.hueOn ? C_OK : C_DIM, TR_DATUM);
  }
}

/* ------------------------------------------------------------- PAGE  CLOCK */
static void pageClock(const struct tm& t, bool full) {
  static char lastTime[9] = "";
  static int  lastMin = -1, lastDay = -1;
  static long lastTotal = -1;
  static int  cellX[8], cellW[8];

  if (full) {
    tft.fillScreen(C_BG);
    lastTime[0] = '\0'; lastMin = -1; lastDay = -1; lastTotal = -1;

    /* วัดความกว้างจริงของแต่ละหลักด้วยฟอนต์ 7 แล้วจำตำแหน่งไว้
       เพื่อให้รอบถัดไปวาดใหม่เฉพาะหลักที่เปลี่ยน */
    const char* sample = "00:00:00";
    int totalW = tft.textWidth(sample, 7);
    int x = (SCR_W - totalW) / 2;
    for (int i = 0; i < 8; i++) {
      char c[2] = { sample[i], '\0' };
      cellW[i] = tft.textWidth(c, 7);
      cellX[i] = x;
      x += cellW[i];
    }
  }

  /* --- แถวบน: วันที่ + เลขสัปดาห์ --- */
  if (full || t.tm_mday != lastDay) {
    lastDay = t.tm_mday;
    char b[32];
    snprintf(b, sizeof(b), "%s %02d %s %04d",
             WDAY[t.tm_wday % 7], t.tm_mday, MON12[t.tm_mon % 12], t.tm_year + 1900);
    uiField(b, 6, 6, 220, 2, C_TEXT);
    snprintf(b, sizeof(b), "W%02d", isoWeek(t));
    uiField(b, SCR_W - 66, 6, 60, 2, C_ACCENT, TR_DATUM);
  }

  /* --- เวลาตัวใหญ่ไล่สีรุ้ง: วาดใหม่เฉพาะหลักที่เปลี่ยน --- */
  char now[9];
  snprintf(now, sizeof(now), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  const int ty = 34;
  for (int i = 0; i < 8; i++) {
    if (!full && lastTime[0] && now[i] == lastTime[i]) continue;
    tft.fillRect(cellX[i], ty, cellW[i], tft.fontHeight(7), C_BG);
    char c[2] = { now[i], '\0' };
    uiText(c, cellX[i], ty, 7, rainbow(i, 8));
  }
  strcpy(lastTime, now);

  /* --- ยอดขายรวมวันนี้ --- */
  if (full || uid.total != lastTotal || t.tm_min != lastMin) {
    lastTotal = uid.total; lastMin = t.tm_min;
    char b[40], n[20];
    if (!uid.salesEver) {
      uiField("SALES NOT CONFIGURED", 0, 100, SCR_W, 4, C_DIM, TC_DATUM);
    } else {
      fmtThousands(uid.total, n, sizeof(n));
      snprintf(b, sizeof(b), "TODAY %s THB", n);
      uiField(b, 0, 100, SCR_W, 4, uid.salesStale ? C_WARN : C_TEXT, TC_DATUM);
    }

    /* บรรทัดเตือนสาขาที่ยังไม่ส่งยอด - ข้อมูลที่ผู้จัดการต้องเห็นเร็วที่สุด */
    int pending = 0;
    for (uint8_t i = 0; i < uid.brCount; i++) if (!uid.br[i].done) pending++;
    if (uid.salesEver && pending > 0) {
      snprintf(b, sizeof(b), "%d BRANCH%s PENDING", pending, pending > 1 ? "ES" : "");
      uiField(b, 0, 140, SCR_W, 2, C_PENDING, TC_DATUM);
    } else if (uid.salesEver) {
      uiField("ALL BRANCHES REPORTED", 0, 140, SCR_W, 2, C_OK, TC_DATUM);
    } else {
      uiField("", 0, 140, SCR_W, 2, C_DIM, TC_DATUM);
    }
  }

  if (full) tft.drawFastHLine(0, SCR_H - 30, SCR_W, C_LINE);
  drawStatusBar(t, full);
}

/* ------------------------------------------------------------- PAGE  SALES */
static void pageSales(const struct tm& t, bool full) {
  const int COL_X[2] = { 6, 166 };
  const int COL_W    = 148;
  const int ROW_Y[4] = { 40, 68, 96, 124 };

  if (full) {
    tft.fillScreen(C_BG);
    uiText("SALES TODAY", 6, 6, 4, C_ACCENT);
    tft.drawFastHLine(0, 34, SCR_W, C_LINE);
    tft.drawFastHLine(0, 152, SCR_W, C_LINE);
  }

  /* เวลาที่ดึงข้อมูลสำเร็จครั้งล่าสุด - ถ้าดึงไม่ผ่านจะขึ้น STALE สีเหลือง
     และคงตัวเลขเดิมไว้ ไม่โชว์ 0 เพราะจะเข้าใจผิดว่าวันนี้ขายไม่ได้ */
  {
    char b[32];
    if (!uid.salesEver) {
      strcpy(b, "NO DATA");
    } else {
      time_t at = (time_t)uid.salesAtEpoch;
      struct tm st; gmtime_r(&at, &st);
      snprintf(b, sizeof(b), "%s%02d:%02d",
               uid.salesStale ? "STALE " : "", st.tm_hour, st.tm_min);
    }
    uiField(b, SCR_W - 140, 12, 134, 2,
            !uid.salesEver ? C_DIM : uid.salesStale ? C_WARN : C_DIM, TR_DATUM);
  }

  /* 8 สาขา สองคอลัมน์ สี่แถว - พอดีหน้าเดียว ไม่ต้องเลื่อน */
  for (uint8_t i = 0; i < SALES_MAX_BRANCHES; i++) {
    int col = i / 4, row = i % 4;
    int x = COL_X[col], y = ROW_Y[row];

    tft.fillRect(x, y, COL_W, 20, C_BG);
    if (i >= uid.brCount) continue;

    uiText(uid.br[i].name, x, y, 2, C_TEXT);

    char n[20];
    fmtThousands(uid.br[i].rev, n, sizeof(n));
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(uid.br[i].done ? C_TEXT : C_DIM, C_BG);
    tft.drawString(asciiOnly(n), x + COL_W - 16, y, 2);

    /* จุดท้ายแถว: เขียว = ส่งรายงานแล้ว, แดง = PENDING */
    tft.fillCircle(x + COL_W - 5, y + 8, 4, uid.br[i].done ? C_OK : C_PENDING);
  }

  /* สรุปท้ายจอ */
  {
    char b[48], n[20];
    fmtThousands(uid.total, n, sizeof(n));
    snprintf(b, sizeof(b), "TOTAL %s THB", n);
    uiField(b, 6, 160, SCR_W - 12, 4, C_TEXT);

    char n2[20];
    fmtThousands(uid.mtd, n2, sizeof(n2));
    snprintf(b, sizeof(b), "MTD %s   MACHINES %d", n2, uid.machines);
    uiField(b, 6, 196, SCR_W - 12, 2, C_DIM);
  }

  drawStatusBar(t, full);
}

/* ------------------------------------------------------------ PAGE  LIGHTS */
static void pageLights(const struct tm& t, bool full) {
  if (full) {
    tft.fillScreen(C_BG);
    uiText("PHILIPS HUE", 6, 6, 4, C_ACCENT);
    tft.drawFastHLine(0, 34, SCR_W, C_LINE);
    uiText("STATUS",     6, 48,  2, C_DIM);
    uiText("BRIGHTNESS", 6, 96,  2, C_DIM);
    uiText("SCENE",      6, 150, 2, C_DIM);
    uiText("BRIDGE",     6, 178, 2, C_DIM);
  }

  if (!uid.huePaired) {
    uiField("NOT PAIRED", 110, 44, SCR_W - 116, 4, C_DIM);
    uiField("", 6, 116, SCR_W - 12, 2, C_DIM);
    uiField("Open http://" DEV_NAME ".local to pair", 6, 150, SCR_W - 12, 2, C_DIM);
    uiField("", 110, 178, SCR_W - 116, 2, C_DIM);
    drawStatusBar(t, full);
    return;
  }

  uiField(uid.hueOn ? "LIGHTS ON" : "LIGHTS OFF", 110, 44, SCR_W - 116, 4,
          uid.hueOn ? C_OK : C_DIM);

  /* แถบความสว่าง: Hue ใช้สเกล 0-254 แปลงเป็น % ให้คนอ่าน */
  {
    const int bx = 6, by = 118, bw = SCR_W - 80, bh = 14;
    int pct  = (int)((uid.hueBri * 100L + 127) / 254);
    int fill = (int)((long)bw * uid.hueBri / 254);
    tft.drawRect(bx, by, bw, bh, C_LINE);
    tft.fillRect(bx + 1, by + 1, bw - 2, bh - 2, C_BG);
    if (fill > 2) tft.fillRect(bx + 1, by + 1, fill - 2, bh - 2, C_ACCENT);
    char b[8];
    snprintf(b, sizeof(b), "%d%%", pct);
    uiField(b, SCR_W - 68, by - 2, 62, 2, C_TEXT, TR_DATUM);
  }

  {
    char b[40];
    const char* sid = (uid.hueScene == 'A') ? cfg.hue.sceneA.c_str()
                                            : cfg.hue.sceneB.c_str();
    if (sid && sid[0]) snprintf(b, sizeof(b), "%c (set)", uid.hueScene);
    else               snprintf(b, sizeof(b), "%c (none)", uid.hueScene);
    uiField(b, 110, 150, SCR_W - 116, 2, C_TEXT);

    snprintf(b, sizeof(b), "%s  API v%u", cfg.hue.ip.c_str(), uid.hueApi);
    uiField(b, 110, 178, SCR_W - 116, 2, C_DIM);
  }

  drawStatusBar(t, full);
}

/* ------------------------------------------------------------ PAGE  SYSTEM */
static void pageSystem(const struct tm& t, bool full) {
  if (full) {
    tft.fillScreen(C_BG);
    uiText("SYSTEM", 6, 6, 4, C_ACCENT);
    tft.drawFastHLine(0, 34, SCR_W, C_LINE);
    uiText("FW",     6, 44,  2, C_DIM);
    uiText("IP",     6, 68,  2, C_DIM);
    uiText("HOST",   6, 92,  2, C_DIM);
    uiText("UPTIME", 6, 116, 2, C_DIM);
    uiText("HEAP",   6, 140, 2, C_DIM);
    uiText("HUE",    6, 164, 2, C_DIM);
    uiText("SALES",  6, 188, 2, C_DIM);
  }

  char b[48];
  uiField(FW_VERSION, 100, 44, SCR_W - 106, 2, C_TEXT);
  uiField(uid.ipStr,  100, 68, SCR_W - 106, 2, C_TEXT);
  uiField("http://" DEV_NAME ".local", 100, 92, SCR_W - 106, 2, C_TEXT);

  uint32_t s = millis() / 1000;
  snprintf(b, sizeof(b), "%lud %02lu:%02lu:%02lu",
           (unsigned long)(s / 86400), (unsigned long)((s % 86400) / 3600),
           (unsigned long)((s % 3600) / 60), (unsigned long)(s % 60));
  uiField(b, 100, 116, SCR_W - 106, 2, C_TEXT);

  /* heap ต้องนิ่ง ถ้าเห็นค่าไหลลงเรื่อย ๆ แปลว่ามี leak (มักมาจาก TLS) */
  uint32_t heap = ESP.getFreeHeap();
  fmtThousands((long)heap, b, sizeof(b));
  strncat(b, " B", sizeof(b) - strlen(b) - 1);
  uiField(b, 100, 140, SCR_W - 106, 2, heap < TLS_MIN_FREE_HEAP ? C_WARN : C_TEXT);

  if (!uid.huePaired) snprintf(b, sizeof(b), "NOT PAIRED");
  else snprintf(b, sizeof(b), "v%u  %s", uid.hueApi, uid.hueOk ? "OK" : "ERROR");
  uiField(b, 100, 164, SCR_W - 106, 2,
          !uid.huePaired ? C_DIM : uid.hueOk ? C_OK : C_PENDING);

  if (!uid.salesEver) {
    snprintf(b, sizeof(b), "NOT CONFIGURED");
    uiField(b, 100, 188, SCR_W - 106, 2, C_DIM);
  } else {
    time_t at = (time_t)uid.salesAtEpoch;
    struct tm st; gmtime_r(&at, &st);
    /* ใช้ int32_t เพื่อให้การลบแบบ wrap-around ของ millis() ออกมาเป็นลบจริง
       ไม่ใช่เลขบวกมหาศาล */
    int32_t left = (int32_t)(uid.salesNextMs - millis()) / 1000;
    if (left < 0) left = 0;
    snprintf(b, sizeof(b), "%s %02d:%02d  next %lds",
             uid.salesStale ? "STALE" : "OK", st.tm_hour, st.tm_min, (long)left);
    uiField(b, 100, 188, SCR_W - 106, 2, uid.salesStale ? C_WARN : C_OK);
  }

  drawStatusBar(t, full);
}

/* ================================================================== router */

void uiSetPage(uint8_t page) {
  if (page >= PAGE_COUNT) page = PAGE_CLOCK;
  gPage = page;
  gFullRedraw = true;
  gDirty = true;
}

uint8_t uiPage()      { return gPage; }
void    uiNextPage()  { uiSetPage((gPage + 1) % PAGE_COUNT); }
void    uiMarkDirty() { gDirty = true; }

void uiTick(time_t localNow) {
  static uint32_t lastMs = 0;
  static time_t   lastSec = 0;

  /* เดินทั้งตามวินาทีที่เปลี่ยนและตามนาฬิกาเครื่องทุก 500ms
     ต้องมีเงื่อนไข 500ms ด้วย ไม่งั้นช่วงที่ NTP ยังไม่ซิงก์ localNow จะนิ่ง
     แล้วหน้า CLOCK จะไม่ถูกวาดใหม่เลย (toast/แถบสถานะค้างบนจอ)
     การวาดซ้ำไม่เปลืองเพราะทุกหน้าใช้ dirty-region เทียบค่าเก่าก่อนวาดอยู่แล้ว */
  uint32_t nowMs = millis();
  bool tickDue = (localNow != lastSec) || (nowMs - lastMs >= 500);
  if (!gFullRedraw && !gDirty && !tickDue) return;

  lastMs  = nowMs;
  lastSec = localNow;

  if (gToastUntil && nowMs > gToastUntil) {   /* toast หมดอายุ -> คืนแถบสถานะ */
    gToastUntil  = 0;
    gStatusDirty = true;
    gDirty       = true;
  }

  struct tm t;
  gmtime_r(&localNow, &t);                    /* localNow เป็น epoch ที่บวก tz แล้ว */

  bool full = gFullRedraw;
  gFullRedraw = false;
  gDirty = false;

  switch (gPage) {
    case PAGE_SALES:  pageSales(t, full);  break;
    case PAGE_LIGHTS: pageLights(t, full); break;
    case PAGE_SYSTEM: pageSystem(t, full); break;
    default:          pageClock(t, full);  break;
  }
}
