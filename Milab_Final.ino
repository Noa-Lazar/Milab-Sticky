#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

// ======= SERVO & BUTTON CONFIG =======
const int buttonPin = 32;
const int servoPin = 23;
Servo myservo;

bool servoBusy = false;
bool waitingForServo = false;
unsigned long pressTime = 0;
const unsigned long delayTime = 20000; // 20 seconds
const unsigned long preServoDelay = 100; // 0.1 seconds before servo moves

// ======= LED STRIP CONFIG =======
#define LED_PIN    15
#define LED_COUNT  120
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

  Serial.println("Setup complete. Press the button to activate.");
}

void loop() {
  unsigned long now = millis();
  bool currentButtonState = digitalRead(buttonPin) == HIGH;

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

  // === After 0.1 seconds, move the servo ===
  if (waitingForServo && (now - pressTime >= preServoDelay)) {
    Serial.println("0.1 seconds passed - Moving Servo down");
    myservo.write(0);  // Move down
    waitingForServo = false;
    servoBusy = true;
    pressTime = now; // reuse for servo return timing
  }

  // === Move servo back up after total delay, unless max presses reached ===
  if (servoBusy && (now - pressTime >= delayTime)) {
    if (pressCount < maxPresses) {
      Serial.println("20 seconds passed - Moving Servo up");
      myservo.write(70);  // Move up
      servoBusy = false;   // Ready for next press
    } else {
      Serial.println("Servo will stay down after 6 presses.");
      servoBusy = false;
    }
  }

  // === LED Effect Section ===
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
