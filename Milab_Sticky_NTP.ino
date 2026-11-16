#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <WiFi.h>
#include "time.h"

// ======= NTP / LOCAL TIME CONFIG =======
const char* ssid = "RUNI-Wireless";
const char* password = "";
const char* ntpServer = "pool.ntp.org";
// Israel local time, including automatic daylight-saving adjustment.
const char* timeZone = "IST-2IDT,M3.4.4/26,M10.5.0";

// ======= SERVO & BUTTON CONFIG =======
const int buttonPin = 32;
const int servoPin = 23;
Servo myservo;

bool servoBusy = false;
bool waitingForServo = false;
unsigned long pressTime = 0;
const unsigned long preServoDelay = 100; // 0.1 seconds before servo moves

// Track the date on which the daily servo return has already happened.
int lastResetYear = -1;
int lastResetDay = -1;

// ======= LED STRIP CONFIG =======
#define LED_PIN 15
#define LED_COUNT 120
#define PIXELS_PER_PRESS 20
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

struct FlickerPixel {
  int pixelIndex;
  unsigned long startTime;
};

FlickerPixel activePixels[LED_COUNT];
int currentPixelCount = 0;

// ======= STATE TRACKING =======
bool prevButtonState = false;
int pressCount = 0;
const int maxPresses = 6;
bool celebrationDone = false;

void setup() {
  Serial.begin(115200);

  // Servo setup
  pinMode(buttonPin, INPUT_PULLDOWN);
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);
  myservo.attach(servoPin, 500, 2500);
  myservo.write(70); // Initial up position

  // LED setup
  strip.begin();
  strip.show();
  strip.setBrightness(150);

  // Connect to Wi-Fi and synchronize the ESP32 clock with NTP.
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");

  configTzTime(timeZone, ntpServer);

  // Wait for the first successful time synchronization.
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo, 1000)) {
    Serial.println("Waiting for NTP time synchronization...");
  }
  Serial.println(&timeinfo, "Local time: %Y-%m-%d %H:%M:%S");

  // Wi-Fi is no longer needed; the ESP32 system clock keeps running.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  Serial.println("Setup complete. Press the button to activate.");
}

void loop() {
  unsigned long now = millis();
  bool currentButtonState = digitalRead(buttonPin) == HIGH;

  // === Return the servo to its initial position once every day at 03:00 ===
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    if (timeinfo.tm_hour == 3 &&
        lastResetYear != timeinfo.tm_year &&
        lastResetDay != timeinfo.tm_yday) {
      Serial.println("03:00 - Moving Servo up");
      myservo.write(70); // Return servo to initial up position
      servoBusy = false; // Allow the next day's button press
      lastResetYear = timeinfo.tm_year;
      lastResetDay = timeinfo.tm_yday;
    }
  }

  // === Button press ===
  if (currentButtonState && !prevButtonState && !servoBusy && !waitingForServo) {
    if (pressCount >= maxPresses) {
      Serial.println("Max presses reached. Servo stays down.");
    } else {
      pressCount++;
      Serial.printf("Button pressed - Count: %d\n", pressCount);
      pressTime = now;
      waitingForServo = true;

      // Add breathing pixels unless it's the 6th press
      if (pressCount < maxPresses) {
        int nextPixel = currentPixelCount;
        for (int i = 0; i < PIXELS_PER_PRESS && currentPixelCount < LED_COUNT; i++) {
          activePixels[currentPixelCount++] = {nextPixel++, now};
          Serial.printf("Pixel %d started breathing\n", nextPixel - 1);
        }
      }
    }
  }
  prevButtonState = currentButtonState;

  // === After 0.1 seconds, move the servo down ===
  if (waitingForServo && (now - pressTime >= preServoDelay)) {
    Serial.println("0.1 seconds passed - Moving Servo down");
    myservo.write(0); // Move down
    waitingForServo = false;
    servoBusy = true; // Keep it down until the daily 03:00 reset
  }

  // === LED Effect Section (unchanged) ===
  if (pressCount == maxPresses && !celebrationDone) {
    playCelebration();
    celebrationDone = true;
  } else if (celebrationDone) {
    // Set all pixels to solid white
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, strip.Color(255, 255, 255));
    }
    strip.show();
    delay(50);
  } else {
    // Breathing animation for active pixels
    strip.clear();
    for (int i = 0; i < currentPixelCount; i++) {
      FlickerPixel &p = activePixels[i];
      unsigned long elapsed = now - p.startTime;
      if (elapsed < 10000) {
        float t = (elapsed % 2000) / 2000.0 * 2 * PI;
        float brightness = (sin(t - PI / 2) + 1.0) / 2.0;
        int whiteValue = (int)(brightness * 255);
        strip.setPixelColor(p.pixelIndex, strip.Color(whiteValue, whiteValue, whiteValue));
      } else if (elapsed < 20000) {
        strip.setPixelColor(p.pixelIndex, strip.Color(0, 0, 0));
      } else {
        strip.setPixelColor(p.pixelIndex, strip.Color(255, 255, 255));
      }
    }
    strip.show();
    delay(20);
  }
}

// ===== Celebration Mode =====
void playCelebration() {
  Serial.println("🎉 Playing celebration animation!");
  for (int j = 0; j < 40; j++) { // 40 flashes ~4 seconds
    for (int i = 0; i < LED_COUNT; i++) {
      int r = random(0, 256);
      int g = random(0, 256);
      int b = random(0, 256);
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
    delay(100);
  }
}
