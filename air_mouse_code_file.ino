#include <Wire.h>
#include <HijelHID_BLEMouse.h>
#include <MPU6050_light.h>
#include <math.h>

#define SDA_PIN 21
#define SCL_PIN 22

// Touch controls
#define LEFT_TOUCH_PIN 32
#define RIGHT_TOUCH_PIN 33

// Touch threshold factor
#define TOUCH_THRESHOLD_FACTOR 0.70f

MPU6050 mpu(Wire);
HijelBLEMouse bleMouse("ESP32 Air Mouse", "ESP32", 100, 3);

uint16_t leftThreshold = 0;
uint16_t rightThreshold = 0;

bool previousLeft = false;
bool previousRight = false;

unsigned long previousMicros = 0;


const unsigned long debounceDelay = 40;

const float gyroDeadzone = 2.0;
const float filterAlpha = 0.2;
const float sensitivity = 0.25;

float smoothX = 0;
float smoothY = 0;

int lastLeftState = HIGH;
int stableLeftState = HIGH;
int lastRightState = HIGH;
int stableRightState = HIGH;

unsigned long lastLeftTime = 0;
unsigned long lastRightTime = 0;


// TOUCH CALIBRATION
// Do not touch either pin during calibration.
// ==================================================

void calibrateTouch() {

  const int samples = 50;

  uint32_t sumLeft = 0;
  uint32_t sumRight = 0;

  Serial.println("Calibrating touch controls...");

  for (int i = 0; i < samples; i++) {

    sumLeft += touchRead(LEFT_TOUCH_PIN);
    sumRight += touchRead(RIGHT_TOUCH_PIN);

    delay(20);
  }

  float baselineLeft = (float)sumLeft / samples;
  float baselineRight = (float)sumRight / samples;

  leftThreshold =
      (uint16_t)(baselineLeft * TOUCH_THRESHOLD_FACTOR);

  rightThreshold =
      (uint16_t)(baselineRight * TOUCH_THRESHOLD_FACTOR);

  Serial.print("Left baseline: ");
  Serial.println(baselineLeft);

  Serial.print("Right baseline: ");
  Serial.println(baselineRight);

  Serial.print("Left threshold: ");
  Serial.println(leftThreshold);

  Serial.print("Right threshold: ");
  Serial.println(rightThreshold);
}

// ==================================================
// HANDLE TOUCH CLICKS
// GPIO32 = left click
// GPIO33 = right click
// ==================================================

void handleTouch() {

  uint16_t leftValue = touchRead(LEFT_TOUCH_PIN);
  uint16_t rightValue = touchRead(RIGHT_TOUCH_PIN);

  bool leftNow = leftValue < leftThreshold;
  bool rightNow = rightValue < rightThreshold;

  if (!bleMouse.isPaired()) {

    previousLeft = false;
    previousRight = false;

    return;
  }

  // Left button
  if (leftNow != previousLeft) {
    bleMouse.setButton(MouseButton::Left, leftNow);
    previousLeft = leftNow;
  }

  // Right button
  if (rightNow != previousRight) {
    bleMouse.setButton(MouseButton::Right, rightNow);
    previousRight = rightNow;
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  byte status = mpu.begin();
  if (status != 0) {
    Serial.print("MPU6050 Error: ");
    Serial.println(status);
    while (true) {
      delay(100);
    }
  }

  Serial.println("MPU6050 Found!");
  Serial.println("Keep the device still for gyro calibration.");
  mpu.calcGyroOffsets();
  bleMouse.begin();
  Serial.println("Air Mouse Ready");
  calibrateTouch();
}


void loop() {
  if (!bleMouse.isConnected()) {
    delay(10);
    return;
  }
  
  // Update gyroscope
  mpu.update();

  float gyroX = mpu.getGyroX();
  float gyroY = mpu.getGyroY();

  if (fabs(gyroX) < gyroDeadzone) gyroX = 0;
  if (fabs(gyroY) < gyroDeadzone) gyroY = 0;

  // FIXED: Added the missing minus signs for the low-pass filter
  smoothX = smoothX * (1.0 - filterAlpha) + gyroX * filterAlpha;
  smoothY = smoothY * (1.0 - filterAlpha) + gyroY * filterAlpha;

  // Apply deadzone AFTER filtering to reduce cursor drift
  if (fabs(smoothX) < 3.0) smoothX = 0;
  if (fabs(smoothY) < 3.0) smoothY = 0;

  // Convert gyro motion to cursor movement
  int moveX = (int)(sensitivity * smoothY);
  int moveY = (int)(-sensitivity * smoothX);

  // Send only non-zero movements
  if (moveX != 0 || moveY != 0) {
    bleMouse.move(moveX, moveY);
  }
   handleTouch();
  delay(5);
}