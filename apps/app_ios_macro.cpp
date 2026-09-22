#include "app_ios_macro.h"

// Macro to safely abort and return early
#define MDELAY(x) if(macroDelay(x)) { ble.releaseAll(); resetIdle(); return; }

void AppIosMacro_Key3() {
          // firstname + lastname + 2-3 random digits, all lowercase
          int digits = random(2, 4);
          String suffix = "";
          for (int i = 0; i < digits; i++) suffix += String(random(0, 10));
          String user = firstName() + lastName() + suffix;
          drawAction(">> typing username..");
          MDELAY(random(600, 1200));   // Short hesitation before typing username
          
          // ── Shift-Breaker: Defeat OS Sticky Shift ──
          humanTap(KEY_SPACE);
          humanTap(KEY_BACKSPACE);
          MDELAY(random(50, 150));

          humanType(user.c_str());
          MDELAY(random(800, 1500));   // Natural pause to check username before Enter
          humanTap(KEY_RETURN);
          MDELAY(random(150, 300));
          beepDone();
          drawAction((">> done: " + user).c_str());
          MDELAY(1200);
        }
void AppIosMacro_Key1() {
          // ── Matrix Key: first name + Enter, then last name + Enter ──
          String fn = firstName();
          String ln = lastName();
          drawAction(">> typing first name");
          MDELAY(random(800, 1500));   // Initial hesitation before starting
          
          // Press tab to select the first name box before typing
          humanTap(KEY_TAB);
          MDELAY(random(300, 600));
          
          // ── Shift-Breaker: Defeat OS Sticky Shift ──
          humanTap(KEY_SPACE);
          humanTap(KEY_BACKSPACE);
          MDELAY(random(50, 150));

          humanType(fn.c_str());
          MDELAY(random(200, 500));    // Quick natural pause before hitting Tab
          humanTap(KEY_TAB);
          MDELAY(random(300, 600));    // Very short pause to verify focus jumped
          drawAction(">> typing last name.");
          humanType(ln.c_str());
          MDELAY(random(800, 1600));   // Look over the form before hitting Submit
          humanTap(KEY_RETURN);
          MDELAY(random(150, 300));
          beepDone();
        }
void AppIosMacro_Key0() {
          newProfile();
          drawAction(">> new profile!"); // updates both displays
          beepNewProfile();
          MDELAY(1200);
        }
void AppIosMacro_Key2() {
          // ── Matrix Key: Form filling macro — human-paced ──
          drawAction(">> running macro...");

        // ── MONTH DROPDOWN ───────────────────────────────────────
        // Tab to Month, pause while eyes land on field
        MDELAY(random(300, 600));   // Quick flow from previous field
        humanTap(KEY_TAB);
        MDELAY(random(600, 1400));

        // Open dropdown, wait for options to appear
        humanTap(KEY_RETURN);
        MDELAY(random(400, 800));

        // Navigate: January is pre-selected at position 0.
        // DOWN×0 = Jan, DOWN×1 = Feb, ..., DOWN×11 = Dec
        int month = random(0, 12);
        for (int i = 0; i < month; i++) {
          humanTap(KEY_DOWN);
          MDELAY(random(90, 210));
        }
        MDELAY(random(300, 600));   // hover before confirming

        // Confirm month selection
        humanTap(KEY_RETURN);
        MDELAY(random(600, 1200));  // human glances at result, then moves on

        // ── DAY FIELD ────────────────────────────────────────────
        MDELAY(random(300, 600));   // Quick flow to next field
        humanTap(KEY_TAB);
        MDELAY(random(400, 800));   // eyes jump to Day field

        int day = random(1, 29);   // 1–28 safe across all months
        humanType(String(day).c_str());
        MDELAY(random(500, 1000));  // human reads back short day number

        // ── YEAR FIELD ───────────────────────────────────────────
        MDELAY(random(300, 600));   // Quick flow to next field
        humanTap(KEY_TAB);
        MDELAY(random(400, 700));   // eyes move to Year

        int year = random(1991, 2007);  // 1991–2006, ages 20–35 in 2026
        humanType(String(year).c_str());
        MDELAY(random(600, 1200));  // verify 4-digit year before leaving field

        // ── GENDER DROPDOWN ──────────────────────────────────────
        MDELAY(random(300, 600));   // Quick flow to next field
        humanTap(KEY_TAB);
        MDELAY(random(700, 1600));  // eyes travel to Gender, brief hesitation

        // Open dropdown
        humanTap(KEY_RETURN);
        MDELAY(random(400, 800));   // dropdown opens, scan options

        // Female=0, Male=1, Rather not say=2, Custom=3
        // DOWN×0 = Female (stay), DOWN×1 = Male, DOWN×2 = Rather not say
        int gender = random(0, 3);
        for (int i = 0; i < gender; i++) {
          humanTap(KEY_DOWN);
          MDELAY(random(120, 280));
        }
        MDELAY(random(300, 700));   // brief hover on chosen option

        humanTap(KEY_RETURN);
        MDELAY(random(800, 1800));  // done, settle before next action

        beepDone();
        drawAction(">> macro done!");
          MDELAY(1200);
        }
void AppIosMacro_Key5() {
          // ── Matrix Key: Save to Notes Macro ──
          drawAction(">> notes macro");
  
          // Home screen (reset iOS state)
          ble.tap(KEY_H, KEY_MOD_LGUI);
          MDELAY(600);
  
          // Spotlight
          ble.tap(KEY_SPACE, KEY_MOD_LGUI);
          MDELAY(700);
  
          // Search for Notes app
          ble.print("Notes");
          MDELAY(1000); // Let search results populate fully
  
          // Open Notes
          ble.tap(KEY_RETURN, 0);
          MDELAY(2500); // ← Wait for Notes to fully load
  
          // Paste clipboard (e.g. username copied before)
          ble.tap(KEY_V, KEY_MOD_LGUI, 100, 50);
          MDELAY(1200); // Wait for paste animation + privacy popup
  
          // New line
          ble.tap(KEY_RETURN, 0);
          MDELAY(500);
  
          // Show password on both displays before typing
          drawPasswordTyping();
          MDELAY(300);
  
          // ── Shift-Breaker: Defeat OS Sticky Shift ──
          ble.tap(KEY_SPACE, 0);
          ble.tap(KEY_BACKSPACE, 0);
          MDELAY(50);

          // Type password (fast, inside private Notes)
          ble.print(pwd);
          MDELAY(300);
  
          // Confirm password line
          ble.tap(KEY_RETURN, 0);
          MDELAY(400);
  
          // If NTP time is available, stamp the entry with current date+time
          if (ntpSynced) {
            struct tm t;
            if (getLocalTime(&t)) {
              char stamp[32];
              strftime(stamp, sizeof(stamp), "[%Y-%m-%d %I:%M:%S%p]", &t); // 12-hr
              drawAction(">> stamping time");
              ble.print(stamp);
              MDELAY(200);
              ble.tap(KEY_RETURN, 0);
              MDELAY(300);
            }
          }
  
          // Return to home screen
          ble.tap(KEY_H, KEY_MOD_LGUI);
          MDELAY(400);
  
          beepDone();
          drawAction(">> notes done!");
          d2HoldPwd = true; // Hold password on D2
          MDELAY(900);
        }
void AppIosMacro_Key4() {
          // ── Matrix Key: human-speed password + Enter ──
          drawAction(">> typing pwd..");
        MDELAY(random(600, 1200));  // pre-type hesitation
        drawPasswordTyping();       // show full password on both screens
        
        // ── Shift-Breaker: Defeat OS Sticky Shift ──
        humanTap(KEY_SPACE);
        humanTap(KEY_BACKSPACE);
        MDELAY(random(50, 150));

        humanType(pwd);             // type at human speed while it’s visible
        MDELAY(random(800, 1400));  // natural re-read pause before Enter
        humanTap(KEY_RETURN);
        MDELAY(random(150, 300));
        beepDone();
        drawAction(">> pwd done!");
          d2HoldPwd = true; // Hold password on D2
          MDELAY(900);
        }
void AppIosMacro_Key7() {
          // ── Matrix Key: Random USA Time Zone Macro ──
          drawAction(">> time zone..");
        
          // Command + H (Home Screen to reset state)
          ble.tap(KEY_H, KEY_MOD_LGUI);
          MDELAY(400);
          
          // Command + Space (Spotlight)
          ble.tap(KEY_SPACE, KEY_MOD_LGUI);
          MDELAY(600);
          
          // Select All (Command + A)
          ble.tap(KEY_A, KEY_MOD_LGUI);
          MDELAY(600);
          
          // Backspace
          ble.tap(KEY_BACKSPACE, 0);
          MDELAY(600);
          
          // Type "Date & Time"
          ble.print("Date & Time");
          MDELAY(1200); // Wait a bit longer for Spotlight to catch up
          
          // Return key
          ble.tap(KEY_RETURN, 0);
          MDELAY(3500); // Wait 3.5s for app to open
          
          // 4 second cue to physical tap on time zone field (4 gentle beeps)
          for (int i = 0; i < 4; i++) {
            buzzNote(1047, 100); 
            MDELAY(900);
          }
          
          // Grouping cities by Time Zone guarantees a time shift, 
          // while completely randomizing the specific city within that zone!
          const char* tz_east[] = {"New York", "Philadelphia", "Atlanta", "Detroit", "Boston", "Miami", "Charlotte", "Baltimore"};
          const char* tz_cent[] = {"Chicago", "New Orleans", "Austin", "St. Louis", "Dallas", "Houston", "Omaha", "Memphis"};
          const char* tz_mnt[]  = {"Denver", "Salt Lake City", "Albuquerque", "Boise"};
          const char* tz_pac[]  = {"Los Angeles", "Seattle", "San Francisco", "San Diego", "Las Vegas", "Portland", "San Jose"};
          const char* tz_ak[]   = {"Anchorage", "Juneau", "Sitka", "Nome"};
          const char* tz_hi[]   = {"Honolulu", "Adak"};
          const char* tz_phx[]  = {"Phoenix"};
          
          const char** all_tz[] = {tz_east, tz_cent, tz_mnt, tz_pac, tz_ak, tz_hi, tz_phx};
          int tz_counts[] = {8, 8, 4, 7, 4, 2, 1};
          
          // Pick a random time zone, then a random city within it
          int tIdx = random(0, 7);
          int cIdx = random(0, tz_counts[tIdx]);
          ble.print(all_tz[tIdx][cIdx]);
          MDELAY(1500); // Wait for iOS to filter the exact city
          
          // 3 second beep to give the user time to physically tap the city
          drawAction(">> tap city!");
          buzzNote(1500, 3000); 
          MDELAY(200);
          
          // Command + H (Home Screen)
          ble.tap(KEY_H, KEY_MOD_LGUI);
          MDELAY(400);
        }
void AppIosMacro_Key6() {
          // ── Matrix Key: Clear History Spotlight Macro ──
          drawAction(">> searching..");
        
        // Command + H (Home Screen to reset state)
        ble.tap(KEY_H, KEY_MOD_LGUI);
        MDELAY(400); // Wait for home screen
        
        // Command + Space (Spotlight)
        ble.tap(KEY_SPACE, KEY_MOD_LGUI);
        MDELAY(600); // Wait for spotlight to appear
        
        // Type "clear history"
        ble.print("clear history");
        MDELAY(800); // Wait for search results
        
        // Return key
        ble.tap(KEY_RETURN, 0);
        MDELAY(3500); // Wait 3.5s for app to open
        
        // Tab twice
        ble.tap(KEY_TAB, 0);
        MDELAY(300);
        ble.tap(KEY_TAB, 0);
        MDELAY(300);
        beepDone();
        drawAction(">> macro done!");
          MDELAY(800);
        }

void AppIosMacro_HandleMatrix(char mKey) {
    switch (mKey) {
        case '1':
            AppIosMacro_Key1(); // First name + last name
            break;
        case '2':
            AppIosMacro_Key2(); // Birthdate/gender form
            break;
        case '0':
            AppIosMacro_Key0(); // Generate new profile
            break;
        case '3':
            AppIosMacro_Key3(); // Random username
            break;
        case '4':
            AppIosMacro_Key4(); // Type password
            break;
        case '5':
            AppIosMacro_Key5(); // Save to Notes
            break;
        case '6':
            AppIosMacro_Key6(); // Clear History
            break;
        case '7':
            AppIosMacro_Key7(); // Random USA Time Zone
            break;
        default:
            break;
    }
}

