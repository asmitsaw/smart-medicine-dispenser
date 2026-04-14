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

// WiFi
char ssid[] = "WHITE_DEVIL";
char pass[] = "sunita@1974";

// LCD + RTC
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

// Servo
Servo myServo;

// Pins
#define buzzer 27
#define ir1 34
#define ir2 35

// Medicine time
int medHour = 10;
int medMinute = 4;

bool alreadyTriggered = false;

// 🔥 STATUS CONTROL
String lastStatus = "";

void updateStatus(String newStatus) {
  if (newStatus != lastStatus) {
    Blynk.virtualWrite(V3, newStatus);
    lastStatus = newStatus;
    Serial.println("Status: " + newStatus);
  }
}

// 📱 TIME FROM APP
BLYNK_WRITE(V0) {
  long t = param.asLong();

  medHour = hour(t);
  medMinute = minute(t);

  Serial.print("Time set: ");
  Serial.print(medHour);
  Serial.print(":");
  Serial.println(medMinute);
}

// 📱 MANUAL DISPENSE
BLYNK_WRITE(V1) {
  if (param.asInt() == 1) {
    dispenseMedicine();
  }
}

// 🔧 SERVO FUNCTION
void dispenseMedicine() {
  myServo.write(90);   // open
  delay(2000);

  myServo.write(0);    // close (reset)
  delay(500);
}
void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  rtc.begin();

  myServo.attach(13);

  pinMode(buzzer, OUTPUT);
  pinMode(ir1, INPUT);
  pinMode(ir2, INPUT);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  lcd.setCursor(0, 0);
  lcd.print("Smart Med Box");
  lcd.setCursor(0, 1);
  lcd.print("System Ready");
  delay(2000);
  lcd.clear();

  updateStatus("System Ready");
}

void loop() {
  Blynk.run();

  DateTime now = rtc.now();

  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  lcd.print(timeStr);
  lcd.print("   ");

  lcd.setCursor(0, 1);
  lcd.print("Waiting...     ");

  updateStatus("Waiting...");

  if (now.hour() == medHour && now.minute() == medMinute && !alreadyTriggered) {

    alreadyTriggered = true;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Take Medicine!");

    updateStatus("Take Medicine!");

    dispenseMedicine();

    unsigned long startTime = millis();

    while (millis() - startTime < 30000) {

      int s1 = digitalRead(ir1);
      int s2 = digitalRead(ir2);

      if (s1 == 0 && s2 == 0) {
        delay(200);

        if (digitalRead(ir1) == 0 && digitalRead(ir2) == 0) {

          digitalWrite(buzzer, LOW);

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Medicine Taken");

          updateStatus("Medicine Taken");

          delay(3000);
          lcd.clear();
          break;
        }
      }

      digitalWrite(buzzer, HIGH);
      delay(200);
      digitalWrite(buzzer, LOW);
      delay(200);

      lcd.setCursor(0, 1);
      lcd.print("Please Take Med");
    }

    if (millis() - startTime >= 30000) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Missed Dose!");

      updateStatus("Missed Dose");

      delay(3000);
      lcd.clear();
    }
  }

  if (now.minute() != medMinute) {
    alreadyTriggered = false;
  }

  delay(1000);
}