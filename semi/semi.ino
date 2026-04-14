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

char ssid[] = "WHITE_DEVIL";
char pass[] = "sunita@1974";

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;
Servo myServo;

#define buzzer 27
#define ir1 34
#define ir2 35

int medHour = 10;
int medMinute = 4;

bool alreadyTriggered = false;
String lastStatus = "";

// 🔥 STATUS
void updateStatus(String s) {
  if (s != lastStatus) {
    Blynk.virtualWrite(V3, s);
    Serial.println("Status: " + s);
    lastStatus = s;
  }
}

// 🔥 LCD HOME
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

// 🔥 SERVO
void dispenseMedicine() {
  myServo.write(90);
  delay(2000);
  myServo.write(0);
}

// 📱 TIME
BLYNK_WRITE(V0) {
  long t = param.asLong();
  medHour = hour(t);
  medMinute = minute(t);
}

// 📱 BUTTON
BLYNK_WRITE(V1) {
  if (param.asInt()) dispenseMedicine();
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

  lcd.print("Smart Med Box");
  delay(2000);
  lcd.clear();

  updateStatus("System Ready");
}

void loop() {
  Blynk.run();

  DateTime now = rtc.now();
  showHome(now);

  updateStatus("Waiting...");

  if (now.hour() == medHour && now.minute() == medMinute && !alreadyTriggered) {

    alreadyTriggered = true;

    lcd.clear();
    lcd.print("Take Medicine!");
    updateStatus("Take Medicine!");

    dispenseMedicine();

    unsigned long start = millis();

    while (millis() - start < 30000) {
      Blynk.run();

      if (digitalRead(ir1) == 0 && digitalRead(ir2) == 0) {

        digitalWrite(buzzer, LOW);

        lcd.clear();
        lcd.print("Medicine Taken");
        updateStatus("Medicine Taken");

        delay(2000);
        break;
      }

      digitalWrite(buzzer, HIGH);
      delay(200);
      digitalWrite(buzzer, LOW);
      delay(200);
    }

    if (millis() - start >= 30000) {
      lcd.clear();
      lcd.print("Missed Dose!");
      updateStatus("Missed Dose");
      delay(2000);
    }
  }

  if (now.minute() != medMinute) alreadyTriggered = false;

  delay(500);
}