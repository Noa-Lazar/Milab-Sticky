
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <WiFi.h>
#include "time.h"

// ======= NTP / LOCAL TIME CONFIG =======
const char* ssid = "RUNI-Wireless";
const char* password = "";
const char* ntpServer = "pool.ntp.org";
const char* timeZone = "IST-2IDT,M3.4.4/26,M10.5.0";

// ======= SERVO & BUTTON CONFIG =======
const int buttonPin = 32;
const int servoPin = 23;
Servo myservo;

bool servoBusy = false;
bool waitingForServo = false;
unsigned long pressTime = 0;
const unsigned long preServoDelay = 100;

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

// ======= TIMING =======
const unsigned long ledUpdateInterval = 20;
const unsigned long celebrationFrameInterval = 100;

unsigned long lastLedUpdate = 0;
unsigned long lastCelebrationFrame = 0;

bool celebrationActive = false;
int celebrationFrame = 0;
const int celebrationTotalFrames = 40;

// ======= NTP STATE =======
bool ntpConfigured = false;
bool timeIsSynced = false;

// ======= SETUP =======
void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT_PULLDOWN);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  myservo.setPeriodHertz(50);
  myservo.attach(servoPin, 500, 2500);
  myservo.write(70);

  strip.begin();
  strip.setBrightness(150);
  strip.show();

  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  Serial.println("Setup complete.");
}

// ======= MAIN LOOP =======
void loop() {
  unsigned long now = millis();

  // === NTP synchronization ===
  if (!ntpConfigured && WiFi.status() == WL_CONNECTED) {
    configTzTime(timeZone, ntpServer);
    ntpConfigured = true;
    Serial.println("WiFi connected. NTP synchronization started.");
  }

  struct tm timeinfo;
  bool haveTime = timeIsSynced && getLocalTime(&timeinfo, 0);

  if (!timeIsSynced && ntpConfigured &&
      getLocalTime(&timeinfo, 0)) {

    timeIsSynced = true;

    Serial.println(&timeinfo,
      "Local time: %Y-%m-%d %H:%M:%S");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi turned off.");
    haveTime = true;
  }

  // === Daily servo reset at 03:00 ===
  if (haveTime &&
      timeinfo.tm_hour == 3 &&
      (lastResetYear != timeinfo.tm_year ||
       lastResetDay != timeinfo.tm_yday)) {

    Serial.println("03:00 - Moving Servo up");

    myservo.write(70);
    servoBusy = false;

    lastResetYear = timeinfo.tm_year;
    lastResetDay = timeinfo.tm_yday;
  }

  // === Button press ===
  bool currentButtonState = digitalRead(buttonPin) == HIGH;

  if (currentButtonState && !prevButtonState &&
      !servoBusy && !waitingForServo) {

    if (pressCount >= maxPresses) {
      Serial.println("Max presses reached. Servo stays down.");
    } else {
      pressCount++;

      Serial.printf("Button pressed - Count: %d\n", pressCount);

      pressTime = now;
      waitingForServo = true;

      if (pressCount < maxPresses) {
        for (int i = 0;
             i < PIXELS_PER_PRESS && currentPixelCount < LED_COUNT;
             i++) {

          activePixels[currentPixelCount] = {
            currentPixelCount, now
          };

          currentPixelCount++;
        }
      }

      // Start non-blocking celebration on the 6th press
      if (pressCount == maxPresses && !celebrationDone) {
        celebrationActive = true;
        celebrationDone = true;
        celebrationFrame = 0;
        lastCelebrationFrame = now;
      }
    }
  }

  prevButtonState = currentButtonState;

  // === Move servo down after 100ms ===
  if (waitingForServo &&
      (unsigned long)(now - pressTime) >= preServoDelay) {

    Serial.println("Moving Servo down");

    myservo.write(0);
    waitingForServo = false;
    servoBusy = true;
  }

  // === Non-blocking celebration ===
  if (celebrationActive) {

    if ((unsigned long)(now - lastCelebrationFrame) >=
        celebrationFrameInterval) {

      lastCelebrationFrame = now;

      for (int i = 0; i < LED_COUNT; i++) {
        int r = random(0, 256);
        int g = random(0, 256);
        int b = random(0, 256);

        strip.setPixelColor(i, strip.Color(r, g, b));
      }

      strip.show();
      celebrationFrame++;

      if (celebrationFrame >= celebrationTotalFrames) {
        celebrationActive = false;
        Serial.println("Celebration finished.");
      }
    }

  } else if ((unsigned long)(now - lastLedUpdate) >=
             ledUpdateInterval) {

    lastLedUpdate = now;

    // === LED schedule: 21:00 - 06:00 ===
    bool nightLighting = haveTime &&
        (timeinfo.tm_hour >= 21 || timeinfo.tm_hour < 6);

    strip.clear();

    for (int i = 0; i < currentPixelCount; i++) {
      FlickerPixel &p = activePixels[i];

      unsigned long elapsed = now - p.startTime;

      if (elapsed < 10000) {
        // Breathing effect for 10 seconds
        float t = (elapsed % 2000) / 2000.0f * 2 * PI;
        float brightness = (sin(t - PI / 2) + 1.0f) / 2.0f;
        int whiteValue = (int)(brightness * 255);

        strip.setPixelColor(
          p.pixelIndex,
          strip.Color(whiteValue, whiteValue, whiteValue)
        );

      } else if (nightLighting) {
        // Stay on until 06:00
        strip.setPixelColor(
          p.pixelIndex,
          strip.Color(255, 255, 255)
        );
      }
    }

    strip.show();
  }
}
