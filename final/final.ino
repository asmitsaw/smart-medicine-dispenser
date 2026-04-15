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
#include <time.h>

char ssid[] = "Asmit's F55";
char pass[] = "12345678";

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;
Servo myServo;

// ── PINS ─────────────────────────────────────────────
#define buzzer 27
#define ir1    34
#define ir2    35
#define buttonPin 25

// ── BUTTON STATE (FIXED) ─────────────────────────────
bool lastButtonState = HIGH;
unsigned long lastPress = 0;

// ── TIMERS ───────────────────────────────────────────
int medHour[3]   = {10, 12, 18};
int medMinute[3] = { 0,  0,  0};
bool triggered[3] = {false, false, false};

// ── ALERT SYSTEM ─────────────────────────────────────
bool alertActive   = false;
unsigned long alertStart = 0;
#define ALERT_TIMEOUT_MS 30000

// ── CAMERA STATE ─────────────────────────────────────
bool awaitingCamConfirm = false;
unsigned long dispensedAt = 0;
#define CAM_TIMEOUT_MS 20000

// ── STATUS ───────────────────────────────────────────
String lastStatus = "";

void updateStatus(String s) {
  if (s != lastStatus) {
    Blynk.virtualWrite(V3, s);
    Serial.println("Status: " + s);
    lastStatus = s;
  }
}

void sendPushEvent(String msg) {
  Blynk.virtualWrite(V10, msg);
  Serial.println("Push: " + msg);
}

// ── SERVO ────────────────────────────────────────────
void dispenseMedicine() {
  myServo.write(90);
  delay(1200);
  myServo.write(0);
}

// ── LCD ──────────────────────────────────────────────
void lcdShow(String l1, String l2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(l1.substring(0, 16));
  if (l2.length()) {
    lcd.setCursor(0, 1);
    lcd.print(l2.substring(0, 16));
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

// ── NTP SYNC ─────────────────────────────────────────
#define IST_OFFSET 19800

void syncRTCfromNTP() {
  configTime(IST_OFFSET, 0, "pool.ntp.org", "time.google.com");

  struct tm t;
  int retry = 0;
  while (!getLocalTime(&t) && retry < 20) {
    delay(500);
    retry++;
  }

  if (retry < 20) {
    rtc.adjust(DateTime(
      t.tm_year + 1900,
      t.tm_mon + 1,
      t.tm_mday,
      t.tm_hour,
      t.tm_min,
      t.tm_sec
    ));
  }
}

// ── TIMER INPUT ──────────────────────────────────────
void unpackTime(long sec, int& hr, int& mn) {
  if (sec <= 0) return;
  hr = sec / 3600;
  mn = (sec % 3600) / 60;
}

BLYNK_WRITE(V0) { unpackTime(param.asLong(), medHour[0], medMinute[0]); }
BLYNK_WRITE(V4) { unpackTime(param.asLong(), medHour[1], medMinute[1]); }
BLYNK_WRITE(V5) { unpackTime(param.asLong(), medHour[2], medMinute[2]); }

// ── APP MANUAL BUTTON ────────────────────────────────
BLYNK_WRITE(V1) {
  if (param.asInt() == 1) {
    Blynk.virtualWrite(V1, 0); // Clear the '1' from Blynk Server so it doesn't trigger again on reset!
    dispenseMedicine();
    updateStatus("Manual Dispense");

    Blynk.virtualWrite(V2, 1);
    awaitingCamConfirm = true;
    dispensedAt = millis();
    
    // Activate buzzer alert so patient is notified
    alertActive = true;
    alertStart = millis();

    sendPushEvent("Medicine dispensed manually!");
  }
}

// ── CAMERA RESULT ────────────────────────────────────
BLYNK_WRITE(V9) {
  int result = param.asInt();
  if (awaitingCamConfirm) {
    awaitingCamConfirm = false;
    Blynk.virtualWrite(V2, 0);
    digitalWrite(buzzer, LOW);

    if (result == 1) {
      lcdShow("Medicine Taken", "Cam Confirmed");
      updateStatus("Medicine Taken");
    } else {
      lcdShow("Missed Dose!", "Cam: Not Taken");
      updateStatus("Missed Dose");
    }

    delay(1500);
    lcd.clear();
    alertActive = false;
  }
}

BLYNK_CONNECTED() {
  Blynk.syncVirtual(V0, V4, V5);
}

// ── SETUP ────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  pinMode(buttonPin, INPUT_PULLUP);
  lastButtonState = digitalRead(buttonPin); // Prevent false press edge upon boot!

  rtc.begin();
  myServo.attach(13);

  pinMode(buzzer, OUTPUT);
  pinMode(ir1, INPUT);
  pinMode(ir2, INPUT);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  syncRTCfromNTP();

  lcdShow("Smart Med Box", "Starting...");
  delay(1500);
  lcd.clear();

  Blynk.virtualWrite(V2, 0);
  updateStatus("System Ready");
}

// ── LOOP ─────────────────────────────────────────────
static unsigned long lastBeep = 0;
static unsigned long lastLcdDraw = 0;

void loop() {
  Blynk.run();
  DateTime now = rtc.now();

  // NORMAL MODE
  if (!alertActive && !awaitingCamConfirm) {
    if (millis() - lastLcdDraw > 1000) {
      showHome(now);
      updateStatus("Waiting...");
      lastLcdDraw = millis();
    }
  }

  // ── BUTTON (FIXED & SAFE) ─────────────────────────
  bool currentButtonState = digitalRead(buttonPin);

  if (lastButtonState == HIGH && currentButtonState == LOW &&
      !alertActive && !awaitingCamConfirm &&
      millis() - lastPress > 800) {

    lastPress = millis();

    lcdShow("Manual Dispense");

    dispenseMedicine();
    updateStatus("Manual Dispense");

    Blynk.virtualWrite(V2, 1);
    awaitingCamConfirm = true;
    dispensedAt = millis();

    alertActive = true;
    alertStart = millis();

    sendPushEvent("Medicine dispensed manually (button)!");
  }

  lastButtonState = currentButtonState;

  // ── TIMER CHECK ────────────────────────────────────
  for (int i = 0; i < 3; i++) {
    if (now.hour() == medHour[i] &&
        now.minute() == medMinute[i] &&
        !triggered[i] && !alertActive) {

      triggered[i] = true;
      alertActive = true;
      alertStart = millis();

      lcdShow("Take Medicine!");
      updateStatus("Take Medicine!");

      dispenseMedicine();

      Blynk.virtualWrite(V2, 1);
      awaitingCamConfirm = true;
      dispensedAt = millis();
    }
  }

  // ── TIMEOUT CHECK ─────────────────────────────────
  // If the Python script is closed, camera fails, or user ignores both IR and Cam,
  // the system will time out here and reset itself automatically.
  if ((awaitingCamConfirm && millis() - dispensedAt > CAM_TIMEOUT_MS) ||
      (alertActive && millis() - alertStart > ALERT_TIMEOUT_MS)) {
      
      awaitingCamConfirm = false;
      alertActive = false;
      digitalWrite(buzzer, LOW);
      Blynk.virtualWrite(V2, 0); // Disable python camera trigger
      
      lcdShow("System Timeout", "Missed Dose!");
      updateStatus("Missed Dose");
      delay(2000);
      lcd.clear();
  }

  // ── ALERT MODE ────────────────────────────────────
  if (alertActive) {

    if (millis() - lastBeep > 400) {
      digitalWrite(buzzer, !digitalRead(buzzer));
      lastBeep = millis();
    }

    if (digitalRead(ir1) == LOW && digitalRead(ir2) == LOW) {
      digitalWrite(buzzer, LOW);
      Blynk.virtualWrite(V2, 0);
      awaitingCamConfirm = false;

      lcdShow("Medicine Taken", "IR Confirmed");
      updateStatus("Medicine Taken");

      delay(1500);
      lcd.clear();
      alertActive = false;
    }
  }

  // RESET
  for (int i = 0; i < 3; i++) {
    if (now.hour() != medHour[i] || now.minute() != medMinute[i]) {
      triggered[i] = false;
    }
  }
}