/* =========================================================================
   clock_action.gs - ส่วนที่ต้อง "เพิ่ม" เข้าไปใน Apps Script เดิม

   โปรเจกต์ปลายทาง : "webapp tsp meter"
   Spreadsheet     : TSP_Master_Database Ver.2

   *** นี่ไม่ใช่โปรเจกต์ใหม่ - เป็น patch ที่เพิ่ม case เดียวเข้า Code.gs เดิม ***
   ไม่แก้ case เดิมแม้แต่บรรทัดเดียว และเรียกใช้ helper เดิมทั้งหมด
   (meterAll_ / branches_ / isRevOk_ / hasFlag_ / today_ / monthStart_ / round2)
   ยอดที่นาฬิกาแสดงจึงตรงกับหน้า Dashboard และ AppSheet เสมอ
   เพราะมาจาก single source of truth เดียวกัน

   ------------------------------------------------------------------------
   วิธีติดตั้ง (ทำครั้งเดียว)
   ------------------------------------------------------------------------
   1) เปิดชีต Branches เพิ่มคอลัมน์ใหม่ "ท้ายตาราง" ชื่อ  Short_Name
      ใส่ชื่อย่อ "อักษรละตินเท่านั้น ยาวไม่เกิน 10 ตัว" ให้ 8 สาขา Active
      (จอ TFT แสดงภาษาไทยไม่ได้ ดู CLAUDE.md ข้อ 1)

        TPS-01  โกรเซอรี่เมือง              -> GOCER
        TPS-02  ไลม์ไลท์                    -> LIMELIGHT
        TPS-03  โรบินสันฉลอง                -> ROBIN CLG
        TPS-04  เซ็นทรัลเชิงทะเล            -> CTW
        TPS-05  จังซีลอน                    -> JUNGCEYLN
        TPS-06A เซ็นทรัลภูเก็ต (หุ้นส่วน)    -> CPK-A
        TPS-06B เซ็นทรัลภูเก็ต (พี่โต้ง)     -> CPK-B
        TPS-07  เซ็นทรัลป่าตอง              -> CPATONG

      การเพิ่มคอลัมน์ท้ายตารางไม่กระทบ AppSheet และไม่กระทบ readRows_()
      ซึ่งอ่านตามชื่อหัวคอลัมน์ ไม่ใช่ตามตำแหน่ง
      ถ้าไม่ใส่ ระบบจะ fallback ไปใช้ Machine_Prefix แล้ว Branch_ID ตามลำดับ

   2) วางโค้ดบล็อก B (ด้านล่างสุดของไฟล์นี้) ต่อท้าย Code.gs

   3) วางโค้ดบล็อก A เข้าไปใน switch ของ routeGet_()
      วางไว้ข้าง ๆ case 'dashboard' ได้เลย

   4) Project Settings -> Script Properties -> Add script property
        ชื่อ  : CLOCK_TOKEN
        ค่า   : สุ่มมาเอง 24-32 ตัวอักษร (อย่าใช้คำที่เดาได้)
      *** ห้ามเขียน token ลงในโค้ด และห้าม commit ขึ้น git ***

   5) Triggers -> Add Trigger
        function : warmClockCache
        event    : Time-driven -> Minutes timer -> Every 5 minutes
      เหตุผล: meterAll_() อ่าน Daily_Logs 3,300+ แถวทุกครั้ง call แรกอาจกิน
      5-10 วินาที ซึ่งนานเกินไปสำหรับนาฬิกาที่ต้องเดินวินาทีต่อเนื่อง
      trigger ตัวนี้อุ่น cache ไว้ล่วงหน้า นาฬิกาจึงได้คำตอบใน < 1 วินาที

   6) Deploy -> Manage deployments -> แก้ deployment เดิม -> New version
      (ใช้ URL /exec เดิม ไม่ต้องเปลี่ยนอะไรในนาฬิกา)

   7) ทดสอบบนเบราว์เซอร์:
        https://script.google.com/macros/s/XXXX/exec?action=clock&t=<CLOCK_TOKEN>
      ต้องได้ JSON 8 สาขา ชื่อเป็น ASCII ล้วน และยอดต้องตรงกับหน้า Dashboard
   ========================================================================= */


/* =========================================================================
   บล็อก A - วางเข้าไปใน switch(a) ของ routeGet_()
   =========================================================================

    case 'clock':{
      const tk = clockToken_();
      if(!tk || String(p.t||'') !== tk) return {ok:0,e:'auth'};
      return clockCached_();
    }

   ========================================================================= */


/* =========================================================================
   บล็อก B - วางต่อท้าย Code.gs
   ========================================================================= */

const CLOCK_CACHE_KEY = 'MYCW_CLOCK_V1';
/* TTL ต้องยาวกว่ารอบของ trigger (5 นาที) ไม่งั้นจะมีช่วงที่ cache หมดอายุ
   แล้วนาฬิกาบังเอิญมาเรียกพอดี ต้องรอคำนวณสด 5-10 วินาที */
const CLOCK_CACHE_TTL = 600;

function clockToken_(){
  return PropertiesService.getScriptProperties().getProperty('CLOCK_TOKEN') || '';
}

/* ตัวกรอง ASCII ฝั่งเซิร์ฟเวอร์ (ชั้นที่ 2 ของกฎจอภาษาอังกฤษ)
   รับประกันว่าไม่มีอักษรไทยหลุดออก API ไปถึงจอ แม้ชีตจะถูกแก้ในอนาคต */
function ascii_(s){
  return String(s == null ? '' : s).replace(/[^\x20-\x7E]/g, '').trim().slice(0, 10);
}

/* branches_() เดิมคืนแค่ {id,name} (ชื่อไทย) จึงต้องอ่านชีตเองเพื่อเอา
   Short_Name / Machine_Prefix มาทำชื่อย่อ ASCII สำหรับจอ */
function clockBranches_(){
  return readRows_(TAB_BRANCH).rows
    .filter(b => String(b.Status).trim() === 'Active')
    .map(b => ({
      id: String(b.Branch_ID || '').trim(),
      nm: ascii_(b.Short_Name) || ascii_(b.Machine_Prefix) || ascii_(b.Branch_ID)
    }))
    .filter(b => b.id);
}

function clockPayload_(){
  const m  = meterAll_();
  const td = today_();
  const ms = monthStart_();
  const ok = r => isRevOk_(r);          /* ตัดเฉพาะแถวธง DUP เหมือน dashboard */

  const revToday = {}, reported = {}, machSet = {};
  let mtd = 0;

  m.forEach(r => {
    if(!r.date) return;
    if(r.date === td){
      if(ok(r)) revToday[r.bid] = (revToday[r.bid] || 0) + r.rev;
      /* "ส่งรายงานแล้ว" = มีแถวของวันนี้ที่ไม่ใช่รายการซ้ำ
         ใช้เกณฑ์เดียวกับฟิลด์ done ของ action 'dashboard' */
      if(!hasFlag_(r, 'DUP')){
        reported[r.bid] = true;
        machSet[r.bid + '|' + r.mach] = true;
      }
    }
    if(r.date >= ms && r.date <= td && ok(r)) mtd += r.rev;
  });

  let tot = 0;
  const b = clockBranches_().map(x => {
    const v = round2(revToday[x.id] || 0);
    tot += v;
    return [x.nm, Math.round(v), reported[x.id] ? 1 : 0];
  });

  /* payload ตั้งใจให้เล็กกว่า 700 ไบต์ เพราะ ESP8266 ต้อง parse ทั้งก้อนใน RAM
     ใช้ array แทน object และปัดเป็นจำนวนเต็ม (บนจอไม่แสดงสตางค์อยู่แล้ว) */
  return {
    ok : 1,
    d  : td,
    tot: Math.round(tot),
    mtd: Math.round(mtd),
    mc : Object.keys(machSet).length,
    b  : b
  };
}

function clockCached_(){
  const c   = CacheService.getScriptCache();
  const hit = c.get(CLOCK_CACHE_KEY);
  if(hit){
    try { return JSON.parse(hit); } catch(e){ /* cache เสีย -> คำนวณใหม่ */ }
  }
  const p = clockPayload_();
  c.put(CLOCK_CACHE_KEY, JSON.stringify(p), CLOCK_CACHE_TTL);
  return p;
}

/* ผูกกับ time-driven trigger ทุก 5 นาที (ขั้นตอนที่ 5 ด้านบน) */
function warmClockCache(){
  ensureTabs_();
  CacheService.getScriptCache()
    .put(CLOCK_CACHE_KEY, JSON.stringify(clockPayload_()), CLOCK_CACHE_TTL);
}

/* รันมือจากเมนู Run เพื่อตรวจว่า payload หน้าตาถูกต้องก่อน deploy */
function testClockPayload(){
  ensureTabs_();
  Logger.log(JSON.stringify(clockPayload_()));
}
