#define BLYNK_TEMPLATE_ID "TMPL39IHNO8eS"
#define BLYNK_TEMPLATE_NAME "SMART MEDICINE DISPENSARY"
#define BLYNK_AUTH_TOKEN "9IZnV6T6HxYj6A4wMb6dM-aoUAo0J8eU"

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

// PINS
#define buzzer 27
#define ir1 34
#define ir2 35

// 3 TIMERS
int medHour[3]   = {10, 12, 18};
int medMinute[3] = {0,  0,  0};

bool triggered[3] = {false, false, false};

// ALERT SYSTEM
bool alertActive = false;
unsigned long alertStart = 0;

String lastStatus = "";

// ================= STATUS =================
void updateStatus(String s) {
  if (s != lastStatus) {
    Blynk.virtualWrite(V3, s);
    Serial.println("Status: " + s);
    lastStatus = s;
  }
}

// ================= SERVO =================
void dispenseMedicine() {
  myServo.write(90);
  delay(1200);
  myServo.write(0);
}

// ================= LCD =================
void showHome(DateTime now) {
  char t[9];
  sprintf(t, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  lcd.setCursor(0, 0);
  lcd.print("Time:");
  lcd.print(t);
  lcd.print("   ");

  lcd.setCursor(0, 1);
  lcd.print("Waiting...     ");
}

// ================= BLYNK =================

// TIMER 1
BLYNK_WRITE(V0) {
  long t = param.asLong();
  medHour[0] = hour(t);
  medMinute[0] = minute(t);
}

// TIMER 2
BLYNK_WRITE(V4) {
  long t = param.asLong();
  medHour[1] = hour(t);
  medMinute[1] = minute(t);
}

// TIMER 3
BLYNK_WRITE(V5) {
  long t = param.asLong();
  medHour[2] = hour(t);
  medMinute[2] = minute(t);
}

// MANUAL DISPENSE
BLYNK_WRITE(V1) {
  if (param.asInt() == 1) {
    dispenseMedicine();
    updateStatus("Manual Dispense");
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  if (!rtc.begin()) {
    Serial.println("RTC NOT FOUND");
    while (1);
  }

  // 🔥 FIX: Ensure RTC has time
  if (!rtc.isrunning()) {
    Serial.println("Setting RTC time...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  myServo.attach(13);

  pinMode(buzzer, OUTPUT);
  pinMode(ir1, INPUT);
  pinMode(ir2, INPUT);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  lcd.setCursor(0, 0);
  lcd.print("Smart Med Box");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);
  lcd.clear();

  updateStatus("System Ready");
}

// ================= LOOP =================
void loop() {
  Blynk.run();

  DateTime now = rtc.now();

  // 🟢 NORMAL MODE
  if (!alertActive) {
    showHome(now);
    updateStatus("Waiting...");
  }

  // 🔥 CHECK ALL TIMERS
  for (int i = 0; i < 3; i++) {
    if (now.hour() == medHour[i] &&
        now.minute() == medMinute[i] &&
        !triggered[i] &&
        !alertActive) {

      triggered[i] = true;
      alertActive = true;
      alertStart = millis();

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Take Medicine!");

      updateStatus("Take Medicine!");

      dispenseMedicine();
    }
  }

  // 🔔 ALERT MODE
  if (alertActive) {

    // 👀 SENSOR CHECK
    if (digitalRead(ir1) == 0 && digitalRead(ir2) == 0) {

      digitalWrite(buzzer, LOW);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Medicine Taken");

      updateStatus("Medicine Taken");

      delay(1500);
      lcd.clear();

      alertActive = false;
    }

    // ⏰ MISSED DOSE
    else if (millis() - alertStart > 30000) {

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Missed Dose!");

      updateStatus("Missed Dose");

      delay(1500);
      lcd.clear();

      alertActive = false;
    }

    // 🔊 BUZZER (NON-BLOCKING)
    else {
      static unsigned long lastBeep = 0;

      if (millis() - lastBeep > 400) {
        digitalWrite(buzzer, !digitalRead(buzzer));
        lastBeep = millis();
      }
    }
  }

  // 🔄 RESET TRIGGERS
  for (int i = 0; i < 3; i++) {
    if (now.minute() != medMinute[i]) {
      triggered[i] = false;
    }
  }
}