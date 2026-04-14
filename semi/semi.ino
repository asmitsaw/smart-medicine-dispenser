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

// ── BLYNK CALLBACKS ───────────────────────────────────────

// Morning timer (hour+minute via V0/V6)
BLYNK_WRITE(V0) {
  long t = param.asLong();
  medHour[0]   = hour(t);
  medMinute[0] = minute(t);
}

// Afternoon timer (V4/V7)
BLYNK_WRITE(V4) {
  long t = param.asLong();
  medHour[1]   = hour(t);
  medMinute[1] = minute(t);
}

// Night timer (V5/V8)
BLYNK_WRITE(V5) {
  long t = param.asLong();
  medHour[2]   = hour(t);
  medMinute[2] = minute(t);
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
  if (awaitingCamConfirm) {
    awaitingCamConfirm = false;
    Blynk.virtualWrite(V2, 0);   // reset dispensed flag

    if (result == 1) {
      lcdShow("Medicine Taken", "Cam Confirmed");
      updateStatus("Medicine Taken");
      sendPushEvent("Medicine taken - camera confirmed!");
      digitalWrite(buzzer, LOW);
      delay(1500);
      lcd.clear();
      alertActive = false;
    } else {
      lcdShow("Missed Dose!", "Cam: Not Taken");
      updateStatus("Missed Dose");
      sendPushEvent("MISSED! Medicine not taken - check patient");
      delay(1500);
      lcd.clear();
      alertActive = false;
    }
  }
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

  lcdShow("Smart Med Box", "Starting...");
  delay(2000);
  lcd.clear();

  // Reset camera flag on boot
  Blynk.virtualWrite(V2, 0);
  Blynk.virtualWrite(V10, "");

  updateStatus("System Ready");
}

// ── LOOP ──────────────────────────────────────────────────
void loop() {
  Blynk.run();
  DateTime now = rtc.now();

  // ── NORMAL MODE ─────────────────────────────────────────
  if (!alertActive && !awaitingCamConfirm) {
    showHome(now);
    updateStatus("Waiting...");
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

    // ── IR SENSOR CHECK (physical pickup) ──────────────────
    // Both IR blocked = hand in dispenser = medicine picked up
    if (digitalRead(ir1) == LOW && digitalRead(ir2) == LOW) {
      // Physical pickup confirmed — let camera run for cam confirm
      // Update LCD but keep cam window open
      lcd.setCursor(0, 1);
      lcd.print("IR: Picked up  ");
    }

    // ── CAMERA TIMEOUT ─────────────────────────────────────
    // If camera python never responded within CAM_TIMEOUT_MS,
    // fall back to IR sensor result
    if (awaitingCamConfirm && (millis() - dispensedAt > CAM_TIMEOUT_MS)) {
      awaitingCamConfirm = false;
      Blynk.virtualWrite(V2, 0);

      // IR-based fallback
      if (digitalRead(ir1) == LOW && digitalRead(ir2) == LOW) {
        lcdShow("Medicine Taken", "IR Confirmed");
        updateStatus("Medicine Taken");
        sendPushEvent("Medicine taken - IR confirmed");
      } else {
        lcdShow("Missed Dose!", "No Detection");
        updateStatus("Missed Dose");
        sendPushEvent("MISSED! Medicine not taken - check patient");
      }
      digitalWrite(buzzer, LOW);
      delay(1500);
      lcd.clear();
      alertActive = false;
    }

    // ── OVERALL ALERT TIMEOUT (30 s) ───────────────────────
    else if (!awaitingCamConfirm && (millis() - alertStart > ALERT_TIMEOUT_MS)) {
      lcdShow("Missed Dose!");
      updateStatus("Missed Dose");
      sendPushEvent("MISSED! Medicine not taken");
      delay(1500);
      lcd.clear();
      alertActive = false;
    }

    // ── BUZZER (non-blocking) ─────────────────────────────
    else if (!awaitingCamConfirm == false) {
      // still alert, keep buzzing
      static unsigned long lastBeep = 0;
      if (millis() - lastBeep > 400) {
        digitalWrite(buzzer, !digitalRead(buzzer));
        lastBeep = millis();
      }
    }
  }

  // ── RESET TRIGGERS (minute changed) ──────────────────────
  for (int i = 0; i < 3; i++) {
    if (now.minute() != medMinute[i]) {
      triggered[i] = false;
    }
  }
}