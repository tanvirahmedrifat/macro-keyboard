#include "app_ios_macro.h"

// Flag to block mouse during macro execution (BUG-4 FIX)
static bool macroRunning = false;

#define MDELAY(x) if(macroDelay(x)) { ble.releaseAll(); resetIdle(); return; }

// ── HUMAN HELPERS (used only by Keys 1-4) ────────────────────────────────────

// Defeat iOS Sticky-Shift: space + immediate backspace
static void shiftBreaker() {
  humanTap(KEY_SPACE);
  humanTap(KEY_BACKSPACE);
  MDELAY(gaussRandom(60, 25));
}

// ════════════════════════════════════════════════════════════════════════════
//  HUMAN-SPEED KEYS  (Keys 1, 2, 3, 4)
//  Uses humanType() + humanTap() + Gaussian delays — mimics real typing cadence
// ════════════════════════════════════════════════════════════════════════════

// ── KEY 3: Random Username  [HUMAN SPEED] ────────────────────────────────────
void AppIosMacro_Key3() {
  int digits = random(2, 4);
  String suffix = "";
  for (int i = 0; i < digits; i++) suffix += String(random(0, 10));
  String user = firstName() + lastName() + suffix;
  drawAction(">> typing username..");
  MDELAY(gaussRandom(900, 200));   // cognitive hesitation before starting

  shiftBreaker();
  humanType(user.c_str());
  MDELAY(gaussRandom(1100, 250));  // re-read username before confirming
  humanTap(KEY_RETURN);
  MDELAY(gaussRandom(200, 60));
  beepDone();
  drawAction((">> done: " + user).c_str());
  MDELAY(1200);
}

// ── KEY 1: First Name + Last Name  [HUMAN SPEED] ─────────────────────────────
void AppIosMacro_Key1() {
  String fn = firstName();
  String ln = lastName();
  drawAction(">> typing first name");
  MDELAY(gaussRandom(1100, 250)); // initial cognitive delay: reading the form

  humanTap(KEY_TAB);
  MDELAY(gaussRandom(400, 100));  // eyes travel to first name field

  shiftBreaker();
  humanType(fn.c_str());
  MDELAY(gaussRandom(350, 80));   // glance at typed name before tabbing

  humanTap(KEY_TAB);
  MDELAY(gaussRandom(450, 120));  // eyes shift to last name field
  drawAction(">> typing last name.");

  shiftBreaker();                 // also needed: second field may auto-capitalize
  humanType(ln.c_str());
  MDELAY(gaussRandom(1200, 250)); // re-read full name before submitting

  humanTap(KEY_RETURN);
  MDELAY(gaussRandom(220, 60));
  beepDone();
}

// ── KEY 2: Birthdate + Gender Form  [HUMAN SPEED] ────────────────────────────
void AppIosMacro_Key2() {
  drawAction(">> running macro...");

  // MONTH DROPDOWN
  MDELAY(gaussRandom(450, 100));
  humanTap(KEY_TAB);
  MDELAY(gaussRandom(900, 200));  // eyes settle on Month label

  humanTap(KEY_RETURN);           // open dropdown
  MDELAY(gaussRandom(550, 120));  // dropdown renders, eyes scan

  int month = random(0, 12);
  for (int i = 0; i < month; i++) {
    humanTap(KEY_DOWN);
    MDELAY(gaussRandom(140, 40)); // Gaussian arrow cadence (not flat uniform)
  }
  MDELAY(gaussRandom(450, 100));  // hover: confirm hovered month visually

  humanTap(KEY_RETURN);
  MDELAY(gaussRandom(900, 200));  // read-back glance before moving on

  // DAY FIELD
  MDELAY(gaussRandom(350, 80));
  humanTap(KEY_TAB);
  MDELAY(gaussRandom(550, 130));  // eyes travel to Day field

  int day = random(1, 29);        // 1-28 safe across all months
  humanType(String(day).c_str());
  MDELAY(gaussRandom(700, 150));  // re-read day

  // YEAR FIELD
  MDELAY(gaussRandom(350, 80));
  humanTap(KEY_TAB);
  MDELAY(gaussRandom(550, 130));  // eyes travel to Year field

  int year = random(1991, 2007);  // ages 20-35 in 2026
  humanType(String(year).c_str());
  MDELAY(gaussRandom(950, 200));  // verify 4-digit year carefully

  // GENDER DROPDOWN
  MDELAY(gaussRandom(400, 100));
  humanTap(KEY_TAB);
  MDELAY(gaussRandom(1100, 250)); // longer deliberation on gender field

  humanTap(KEY_RETURN);
  MDELAY(gaussRandom(600, 130));  // dropdown opens

  int gender = random(0, 3);      // Female=0, Male=1, Rather not say=2
  for (int i = 0; i < gender; i++) {
    humanTap(KEY_DOWN);
    MDELAY(gaussRandom(160, 50)); // reading each option
  }
  MDELAY(gaussRandom(500, 120));  // hover: confirm choice visually

  humanTap(KEY_RETURN);
  MDELAY(gaussRandom(1200, 250)); // settle: re-read the form section

  beepDone();
  drawAction(">> macro done!");
  MDELAY(1200);
}

// ── KEY 4: Type Password  [HUMAN SPEED] ──────────────────────────────────────
void AppIosMacro_Key4() {
  drawAction(">> typing pwd..");
  MDELAY(gaussRandom(900, 200));  // hesitation: reading the password field
  drawPasswordTyping();

  shiftBreaker();
  humanType(pwd);                  // human-paced keystroke timing
  MDELAY(gaussRandom(1100, 250)); // re-read password before submitting
  humanTap(KEY_RETURN);
  MDELAY(gaussRandom(200, 60));
  beepDone();
  drawAction(">> pwd done!");
  d2HoldPwd = true;
  MDELAY(900);
}

// ════════════════════════════════════════════════════════════════════════════
//  FAST KEYS  (Keys 0, 5, 6, 7)
//  Uses ble.print() + ble.tap() + fixed MDELAY — maximum speed
// ════════════════════════════════════════════════════════════════════════════

// ── KEY 0: New Profile  [FAST] ────────────────────────────────────────────────
void AppIosMacro_Key0() {
  newProfile();
  drawAction(">> new profile!");
  beepNewProfile();
  MDELAY(1200);
}

// ── KEY 5: Save to Notes  [FAST] ─────────────────────────────────────────────
void AppIosMacro_Key5() {
  drawAction(">> notes macro");

  ble.tap(KEY_H, KEY_MOD_LGUI);
  MDELAY(600);

  ble.tap(KEY_SPACE, KEY_MOD_LGUI);
  MDELAY(700);

  ble.print("Notes");
  MDELAY(1000);

  ble.tap(KEY_RETURN, 0);
  MDELAY(2500);

  ble.tap(KEY_V, KEY_MOD_LGUI, 100, 50);
  MDELAY(1200);

  ble.tap(KEY_RETURN, 0);
  MDELAY(500);

  drawPasswordTyping();
  MDELAY(300);

  // Shift-Breaker still required to defeat iOS Sticky-Shift
  ble.tap(KEY_SPACE, 0);
  ble.tap(KEY_BACKSPACE, 0);
  MDELAY(50);

  ble.print(pwd);
  MDELAY(300);

  ble.tap(KEY_RETURN, 0);
  MDELAY(400);

  if (ntpSynced) {
    struct tm t;
    if (getLocalTime(&t)) {
      char stamp[32];
      strftime(stamp, sizeof(stamp), "[%Y-%m-%d %I:%M:%S%p]", &t);
      drawAction(">> stamping time");
      ble.print(stamp);
      MDELAY(200);
      ble.tap(KEY_RETURN, 0);
      MDELAY(300);
    }
  }

  ble.tap(KEY_H, KEY_MOD_LGUI);
  MDELAY(400);

  beepDone();
  drawAction(">> notes done!");
  d2HoldPwd = true;
  MDELAY(900);
}

// ── KEY 7: Random USA Time Zone  [FAST] ──────────────────────────────────────
void AppIosMacro_Key7() {
  drawAction(">> time zone..");

  ble.tap(KEY_H, KEY_MOD_LGUI);
  MDELAY(400);

  ble.tap(KEY_SPACE, KEY_MOD_LGUI);
  MDELAY(600);

  ble.tap(KEY_A, KEY_MOD_LGUI);
  MDELAY(600);
  ble.tap(KEY_BACKSPACE, 0);
  MDELAY(600);

  ble.print("Date & Time");
  MDELAY(1200);

  ble.tap(KEY_RETURN, 0);
  MDELAY(3500);

  // 4 audio cues for user to physically tap the time zone field
  for (int i = 0; i < 4; i++) {
    if (!silentMode) { buzzNote(1047, 100); }
    MDELAY(900);
  }

  const char* tz_east[] = {"New York", "Philadelphia", "Atlanta", "Detroit", "Boston", "Miami", "Charlotte", "Baltimore"};
  const char* tz_cent[] = {"Chicago", "New Orleans", "Austin", "St. Louis", "Dallas", "Houston", "Omaha", "Memphis"};
  const char* tz_mnt[]  = {"Denver", "Salt Lake City", "Albuquerque", "Boise"};
  const char* tz_pac[]  = {"Los Angeles", "Seattle", "San Francisco", "San Diego", "Las Vegas", "Portland", "San Jose"};
  const char* tz_ak[]   = {"Anchorage", "Juneau", "Sitka", "Nome"};
  const char* tz_hi[]   = {"Honolulu", "Adak"};
  const char* tz_phx[]  = {"Phoenix"};

  const char** all_tz[] = {tz_east, tz_cent, tz_mnt, tz_pac, tz_ak, tz_hi, tz_phx};
  int tz_counts[] = {8, 8, 4, 7, 4, 2, 1};

  int tIdx = random(0, 7);
  int cIdx = random(0, tz_counts[tIdx]);

  ble.print(all_tz[tIdx][cIdx]);
  MDELAY(1500);

  drawAction(">> tap city!");
  beepDone();
  MDELAY(3000);

  ble.tap(KEY_H, KEY_MOD_LGUI);
  MDELAY(400);
}

// ── KEY 6: Clear History  [FAST] ─────────────────────────────────────────────
void AppIosMacro_Key6() {
  drawAction(">> searching..");

  ble.tap(KEY_H, KEY_MOD_LGUI);
  MDELAY(400);

  ble.tap(KEY_SPACE, KEY_MOD_LGUI);
  MDELAY(600);

  ble.print("clear history");
  MDELAY(800);

  ble.tap(KEY_RETURN, 0);
  MDELAY(3500);

  ble.tap(KEY_TAB, 0);
  MDELAY(300);
  ble.tap(KEY_TAB, 0);
  MDELAY(300);

  beepDone();
  drawAction(">> macro done!");
  MDELAY(800);
}

// ── MATRIX DISPATCH ───────────────────────────────────────────────────────────
void AppIosMacro_HandleMatrix(char mKey) {
    macroRunning = true;
    switch (mKey) {
        case '1': AppIosMacro_Key1(); break; // First name + last name  [HUMAN]
        case '2': AppIosMacro_Key2(); break; // Birthdate/gender form   [HUMAN]
        case '3': AppIosMacro_Key3(); break; // Random username         [HUMAN]
        case '4': AppIosMacro_Key4(); break; // Type password           [HUMAN]
        case '0': AppIosMacro_Key0(); break; // Generate new profile    [FAST]
        case '5': AppIosMacro_Key5(); break; // Save to Notes           [FAST]
        case '6': AppIosMacro_Key6(); break; // Clear History           [FAST]
        case '7': AppIosMacro_Key7(); break; // Random USA Time Zone    [FAST]
        default: break;
    }
    macroRunning = false;
}

// ── MOUSE MODE (used when no macro is running) ────────────────────────────────
static unsigned long lastMouseUpdate = 0;
static float mouseSpeed = 1.0;
static bool centerIsPressed = false;

void AppIosMacro_Update() {
    if (!ble.isConnected()) return;
    if (macroRunning) return;

    unsigned long now = millis();
    if (now - lastMouseUpdate < 20) return; // 50Hz
    lastMouseUpdate = now;

    bool up    = (digitalRead(PIN1) == LOW);
    bool down  = (digitalRead(PIN2) == LOW);
    bool left  = (digitalRead(PIN3) == LOW);
    bool right = (digitalRead(PIN4) == LOW);
    bool center= (digitalRead(PIN5) == LOW);

    if (up || down || left || right) {
        mouseSpeed += 0.4;
        if (mouseSpeed > 18.0) mouseSpeed = 18.0;
        int8_t dx = 0, dy = 0;
        if (up)    dy = -(int8_t)mouseSpeed;
        if (down)  dy =  (int8_t)mouseSpeed;
        if (left)  dx = -(int8_t)mouseSpeed;
        if (right) dx =  (int8_t)mouseSpeed;
        ble.mouseMove(dx, dy, 0);
        resetIdle();
    } else {
        mouseSpeed = 1.0;
    }

    if (center && !centerIsPressed) {
        centerIsPressed = true;
        ble.mousePress(1);
        resetIdle();
    } else if (!center && centerIsPressed) {
        centerIsPressed = false;
        ble.mouseRelease(1);
        resetIdle();
    }
}

void AppIosMacro_HandleEvent(LogicalEvent ev) {
    // Swallow directional/center events — handled as raw pins in AppIosMacro_Update
    if (ev == EV_UP_TAP    || ev == EV_UP_HOLD    ||
        ev == EV_DOWN_TAP  || ev == EV_DOWN_HOLD  ||
        ev == EV_LEFT_TAP  || ev == EV_LEFT_HOLD  ||
        ev == EV_RIGHT_TAP || ev == EV_RIGHT_HOLD ||
        ev == EV_CENTER_TAP || ev == EV_CENTER_HOLD || ev == EV_CENTER_HOLD_5S) {
        return;
    }
}
