#include "touch.h"

#ifdef USE_XPT2046
  #include <XPT2046_Touchscreen.h>
  #include <SPI.h>
  static XPT2046_Touchscreen ts(PIN_T_CS, PIN_T_IRQ);
#endif

/* คิว event เล็ก ๆ เพราะรอบเดียวอาจเกิดได้มากกว่า 1 เหตุการณ์
   (เช่น double-tap ของ B หมดเวลารอพอดีกับที่ A ถูกแตะ) */
static TouchEvent q[4];
static uint8_t    qHead = 0, qTail = 0;

static void push(TouchEvent e) {
  uint8_t n = (uint8_t)((qTail + 1) % 4);
  if (n == qHead) return;            /* คิวเต็ม - ทิ้ง event เก่าสุดไปเลย */
  q[qTail] = e;
  qTail = n;
}

static TouchEvent pop() {
  if (qHead == qTail) return EV_NONE;
  TouchEvent e = q[qHead];
  qHead = (uint8_t)((qHead + 1) % 4);
  return e;
}

/* ------------------------------------------------------- gesture per button */
struct Btn {
  bool     stable      = false;   /* สถานะหลัง debounce                    */
  bool     rawPrev     = false;
  uint32_t rawChangeAt = 0;
  uint32_t pressAt     = 0;
  bool     longFired   = false;   /* ยิง long ไปแล้ว -> ตอนปล่อยไม่ต้องยิง tap */
  bool     tapPending  = false;   /* รอดูว่าจะมี tap ที่สองไหม (ปุ่ม B)     */
  uint32_t tapAt       = 0;
};

static Btn bA, bB;

/* อัปเดต 1 ปุ่ม: raw = สถานะดิบตอนนี้ (true = กำลังแตะ) */
static void step(Btn& b, bool raw, uint32_t now, bool isB) {
  /* --- debounce --- */
  if (raw != b.rawPrev) { b.rawPrev = raw; b.rawChangeAt = now; }
  if (now - b.rawChangeAt >= TOUCH_DEBOUNCE_MS && b.stable != raw) {
    b.stable = raw;
    if (raw) {                                  /* ขอบขาขึ้น = เริ่มกด */
      b.pressAt   = now;
      b.longFired = false;
    } else {                                    /* ขอบขาลง = ปล่อย */
      uint32_t held = now - b.pressAt;
      if (!b.longFired && held <= TOUCH_TAP_MAX_MS) {
        if (!isB) {
          push(EV_A_TAP);                       /* ปุ่ม A ไม่มี double-tap */
        } else if (b.tapPending) {
          b.tapPending = false;
          push(EV_B_DOUBLE);
        } else {
          b.tapPending = true;                  /* รอดูว่าจะมีครั้งที่สอง */
          b.tapAt = now;
        }
      }
    }
  }

  /* --- long press: ยิงทันทีที่ถึงเกณฑ์ ไม่รอปล่อย ให้ผู้ใช้รู้สึกว่าติดมือ --- */
  if (b.stable && !b.longFired && (now - b.pressAt) >= TOUCH_LONG_MS) {
    b.longFired  = true;
    b.tapPending = false;
    push(isB ? EV_B_LONG : EV_A_LONG);
  }

  /* --- หมดเวลารอ tap ที่สอง -> สรุปว่าเป็น single tap --- */
  if (isB && b.tapPending && (now - b.tapAt) >= TOUCH_DBL_GAP_MS) {
    b.tapPending = false;
    push(EV_B_TAP);
  }
}

/* ------------------------------------------------------------------ public */
void touchBegin() {
#ifdef USE_XPT2046
  ts.begin();
  ts.setRotation(SCR_ROTATION);
#else
  /* TTP223 เป็น push-pull output จึงใช้ INPUT ธรรมดาได้
     ถ้าถอดโมดูลออกขาจะลอยและอาจเกิด event ผี - ดู README เรื่อง R pulldown 100k */
  pinMode(PIN_TOUCH_A, INPUT);
  pinMode(PIN_TOUCH_B, INPUT);
#endif
}

TouchEvent touchPoll() {
  uint32_t now = millis();

#ifdef USE_XPT2046
  bool rawA = false, rawB = false;
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    /* ค่าดิบของ XPT2046 อยู่ราว 200-3900 แบ่งครึ่งจอ: ซ้าย = A, ขวา = B */
    if (p.x < 2048) rawA = true; else rawB = true;
  }
#else
  bool rawA = digitalRead(PIN_TOUCH_A) == HIGH;   /* active HIGH (จั๊มเปอร์ B ไม่บัดกรี) */
  bool rawB = digitalRead(PIN_TOUCH_B) == HIGH;
#endif

  step(bA, rawA, now, false);
  step(bB, rawB, now, true);
  return pop();
}

bool touchHeldB() { return bB.stable; }
