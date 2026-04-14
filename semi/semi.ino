#define BLYNK_TEMPLATE_ID   "TMPL39IHNO8eS"
#define BLYNK_TEMPLATE_NAME "SMART MEDICINE DISPENSARY"
#define BLYNK_AUTH_TOKEN    "9IZnV6T6HxYj6A4wMb6dM-aoUAo0J8eU"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <ESP32Servo.h>
#include <TimeLib.h>
#include <time.h>          // for NTP sync

char ssid[] = "Asmit's F55";
char pass[] = "12345678";

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;
Servo myServo;

// ── PINS ──────────────────────────────────────────────────
#define buzzer 27
#define ir1    34
#define ir2    35

// ── 3 DOSE TIMERS ─────────────────────────────────────────
int medHour[3]   = {10, 12, 18};
int medMinute[3] = { 0,  0,  0};
bool triggered[3] = {false, false, false};

// ── ALERT SYSTEM ──────────────────────────────────────────
bool alertActive   = false;
unsigned long alertStart = 0;
#define ALERT_TIMEOUT_MS  30000   // 30 s to take or miss

// ── DISPENSED STATE (for camera handoff) ──────────────────
bool awaitingCamConfirm = false;   // camera python is processing
unsigned long dispensedAt = 0;
#define CAM_TIMEOUT_MS 20000       // 20 s camera window

// ── STATUS ────────────────────────────────────────────────
String lastStatus = "";

void updateStatus(String s) {
  if (s != lastStatus) {
    Blynk.virtualWrite(V3, s);
    Serial.println("Status: " + s);
    lastStatus = s;
  }
}

// ── PUSH NOTIFICATION via V10 ─────────────────────────────
// V10 is a string widget the PWA polls every 2 s
// We write the notification message so the web app shows it
void sendPushEvent(String msg) {
  Blynk.virtualWrite(V10, msg);
  Serial.println("Push: " + msg);
}

// ── SERVO ─────────────────────────────────────────────────
void dispenseMedicine() {
  myServo.write(90);
  delay(1200);
  myServo.write(0);
}

// ── LCD helpers ───────────────────────────────────────────
void lcdShow(String line1, String line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));
  if (line2.length()) {
    lcd.setCursor(0, 1);
    lcd.print(line2.substring(0, 16));
  }
}

void showHome(DateTime now) {
  char t[9];
  sprintf(t, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  lcd.setCursor(0, 0);
  lcd.print("Time:");
  lcd.print(t);
  lcd.print("  ");
  lcd.setCursor(0, 1);
  lcd.print("Waiting...     ");
}

// ── NTP -> RTC SYNC ───────────────────────────────────────
// IST = UTC + 5h30m = 19800 seconds offset
#define IST_OFFSET 19800

void syncRTCfromNTP() {
  Serial.println("[NTP] Syncing time from internet...");
  lcdShow("Syncing time", "Please wait...");

  configTime(IST_OFFSET, 0, "pool.ntp.org", "time.google.com");

  struct tm t;
  int retry = 0;
  while (!getLocalTime(&t) && retry < 20) {
    delay(500);
    retry++;
    Serial.print(".");
  }
  Serial.println();

  if (retry < 20) {
    // Write NTP time into RTC
    rtc.adjust(DateTime(
      t.tm_year + 1900,
      t.tm_mon  + 1,
      t.tm_mday,
      t.tm_hour,
      t.tm_min,
      t.tm_sec
    ));
    char buf[32];
    sprintf(buf, "RTC set %02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    Serial.println(buf);
    lcdShow("Time synced!", buf + 8);  // show HH:MM:SS part
    delay(1200);
  } else {
    Serial.println("[NTP] FAILED - using existing RTC time");
    lcdShow("NTP failed", "Using RTC time");
    delay(1500);
  }
}

// ── TIMER CALLBACKS ───────────────────────────────────────
// Web app sends a full "HH:MM" string on a SINGLE pin.
// V0 = Morning,  V4 = Afternoon,  V5 = Night

void parseTime(const char* s, int& hr, int& mn) {
  // Accepts "08:30" or plain int like "8" (legacy fallback)
  char buf[6];
  strncpy(buf, s, 5); buf[5] = '\0';
  char* colon = strchr(buf, ':');
  if (colon) {
    *colon = '\0';
    hr = atoi(buf);
    mn = atoi(colon + 1);
  } else {
    hr = atoi(buf);   // bare hour, minute stays unchanged
  }
}

BLYNK_WRITE(V0) {
  parseTime(param.asStr(), medHour[0], medMinute[0]);
  Serial.printf("[TIMER] Morning   -> %02d:%02d\n", medHour[0], medMinute[0]);
}

BLYNK_WRITE(V4) {
  parseTime(param.asStr(), medHour[1], medMinute[1]);
  Serial.printf("[TIMER] Afternoon -> %02d:%02d\n", medHour[1], medMinute[1]);
}

BLYNK_WRITE(V5) {
  parseTime(param.asStr(), medHour[2], medMinute[2]);
  Serial.printf("[TIMER] Night     -> %02d:%02d\n", medHour[2], medMinute[2]);
}

// Manual dispense
BLYNK_WRITE(V1) {
  if (param.asInt() == 1) {
    dispenseMedicine();
    updateStatus("Manual Dispense");
    // Signal camera to start watching
    Blynk.virtualWrite(V2, 1);
    awaitingCamConfirm = true;
    dispensedAt = millis();
    sendPushEvent("Medicine dispensed manually!");
  }
}

// Camera result written by detect.py → V9
// "1" = taken confirmed by cam, "0" = not detected
BLYNK_WRITE(V9) {
  int result = param.asInt();
  if (awaitingCamConfirm) {        // only act if we're still waiting
    awaitingCamConfirm = false;
    Blynk.virtualWrite(V2, 0);     // tell detect.py to close camera
    digitalWrite(buzzer, LOW);     // stop buzzer

    if (result == 1) {
      lcdShow("Medicine Taken", "Cam Confirmed");
      updateStatus("Medicine Taken");
      sendPushEvent("Medicine taken - camera confirmed!");
    } else {
      lcdShow("Missed Dose!", "Cam: Not Taken");
      updateStatus("Missed Dose");
      sendPushEvent("MISSED! Medicine not taken - check patient");
    }
    delay(1500);
    lcd.clear();
    alertActive = false;
  }
}

// ── RECONNECT: pull saved timer values from Blynk cloud ──
BLYNK_CONNECTED() {
  Serial.println("[BLYNK] Connected - syncing V0/V4/V5...");
  Blynk.syncVirtual(V0, V4, V5);  // only the 3 timer pins
}

// ── SETUP ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  if (!rtc.begin()) {
    Serial.println("RTC NOT FOUND");
    while (1);
  }

  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  myServo.attach(13);

  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);
  pinMode(ir1, INPUT);
  pinMode(ir2, INPUT);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // ── Sync RTC from NTP (WiFi is up after Blynk.begin) ────
  syncRTCfromNTP();

  lcdShow("Smart Med Box", "Starting...");
  delay(1500);
  lcd.clear();

  // Reset camera flag on boot
  Blynk.virtualWrite(V2, 0);
  Blynk.virtualWrite(V10, "");

  updateStatus("System Ready");
}

// ── LOOP ──────────────────────────────────────────────────
static unsigned long lastBeep    = 0;  // global so buzzer is truly non-blocking
static unsigned long lastLcdDraw = 0;  // throttle LCD/status writes

void loop() {
  Blynk.run();
  DateTime now = rtc.now();

  // ── NORMAL MODE — throttled to 1 s ──────────────────────
  if (!alertActive && !awaitingCamConfirm) {
    if (millis() - lastLcdDraw > 1000) {
      showHome(now);
      updateStatus("Waiting...");
      lastLcdDraw = millis();
    }
  }

  // ── CHECK ALL DOSE TIMERS ────────────────────────────────
  for (int i = 0; i < 3; i++) {
    if (now.hour()   == medHour[i] &&
        now.minute() == medMinute[i] &&
        !triggered[i] && !alertActive) {

      triggered[i] = true;
      alertActive  = true;
      alertStart   = millis();

      lcdShow("Take Medicine!");
      updateStatus("Take Medicine!");
      sendPushEvent("Time to take your medicine!");

      dispenseMedicine();

      // Signal camera python to start watching
      Blynk.virtualWrite(V2, 1);
      awaitingCamConfirm = true;
      dispensedAt = millis();
    }
  }

  // ── ALERT MODE ──────────────────────────────────────────
  if (alertActive) {

    // 1️⃣ BUZZER — beeps for entire alert duration (non-blocking)
    if (millis() - lastBeep > 400) {
      digitalWrite(buzzer, !digitalRead(buzzer));
      lastBeep = millis();
    }

    // 2️⃣ IR SENSORS — FIRST PRIORITY
    // Both IR LOW = hand physically blocking dispenser = medicine picked up
    if (digitalRead(ir1) == LOW && digitalRead(ir2) == LOW) {
      digitalWrite(buzzer, LOW);         // stop buzzer immediately
      Blynk.virtualWrite(V2, 0);         // signal detect.py to close camera
      awaitingCamConfirm = false;

      lcdShow("Medicine Taken", "IR Confirmed");
      updateStatus("Medicine Taken");
      sendPushEvent("Medicine taken - IR confirmed!");
      delay(1500);
      lcd.clear();
      alertActive = false;
    }

    // 3️⃣ CAMERA TIMEOUT — detect.py didn't respond, fall back to IR snapshot
    else if (awaitingCamConfirm && (millis() - dispensedAt > CAM_TIMEOUT_MS)) {
      awaitingCamConfirm = false;
      Blynk.virtualWrite(V2, 0);
      digitalWrite(buzzer, LOW);

      if (digitalRead(ir1) == LOW && digitalRead(ir2) == LOW) {
        lcdShow("Medicine Taken", "IR Fallback");
        updateStatus("Medicine Taken");
        sendPushEvent("Medicine taken - IR confirmed");
      } else {
        lcdShow("Missed Dose!", "Timeout");
        updateStatus("Missed Dose");
        sendPushEvent("MISSED! Medicine not taken - check patient");
      }
      delay(1500);
      lcd.clear();
      alertActive = false;
    }

    // 4️⃣ OVERALL 30s TIMEOUT — everything else timed out
    else if (!awaitingCamConfirm && (millis() - alertStart > ALERT_TIMEOUT_MS)) {
      digitalWrite(buzzer, LOW);
      lcdShow("Missed Dose!");
      updateStatus("Missed Dose");
      sendPushEvent("MISSED! Medicine not taken");
      delay(1500);
      lcd.clear();
      alertActive = false;
    }

    // 5️⃣ Show camera status on LCD row 2 while waiting
    else if (awaitingCamConfirm) {
      lcd.setCursor(0, 1);
      lcd.print("Cam watching... ");
    }
  }

  // ── RESET TRIGGERS ───────────────────────────────────────
  // Re-arm only when the HH:MM has moved past the scheduled time
  for (int i = 0; i < 3; i++) {
    if (now.hour() != medHour[i] || now.minute() != medMinute[i]) {
      triggered[i] = false;
    }
  }
}