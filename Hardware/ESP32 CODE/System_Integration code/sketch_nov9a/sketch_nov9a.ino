#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// === OLED Settings ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === Pins ===
const int mq3Pin = 34;       // Analog pin for MQ-3
const int buzzerPin = 26;    // Buzzer
const int ledPin = 25;       // Indicator LED
const int motorPin = 27;     // Motor Relay control

// === Alcohol Detection Threshold ===
const int threshold = 2000;

// === Variables ===
bool helmetDetected = false;
bool alcoholSafe = false;
unsigned long motorOnTime = 0;
bool motorRunning = false;
bool shutdownInitiated = false;
unsigned long shutdownStartTime = 0;
const unsigned long shutdownDelay = 3 * 60 * 1000;  // 3 minutes

void setup() {
  Serial.begin(115200);

  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW); // motor off initially

  // === Initialize OLED ===
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 init failed");
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10, 20);
  display.println("SMART HELMET");
  display.display();
  delay(2000);
}

void loop() {
  // === Check Serial for Helmet Data ===
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    data.trim();

    if (data.startsWith("HELMET:")) {
      helmetDetected = (data.substring(7).toInt() == 1);
      Serial.print("Helmet status updated: ");
      Serial.println(helmetDetected ? "Detected" : "Not Detected");
    }
  }

  // === Read MQ-3 Sensor ===
  int sensorValue = analogRead(mq3Pin);
  alcoholSafe = (sensorValue < threshold);

  // === Display and Control Logic ===
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 10);
  display.print("Helmet: ");
  display.println(helmetDetected ? "Yes" : "No");

  display.setCursor(0, 25);
  display.print("Alcohol: ");
  display.println(alcoholSafe ? "Safe" : "Detected");

  // === Motor Logic ===
  if (helmetDetected && alcoholSafe) {
    digitalWrite(motorPin, HIGH);
    digitalWrite(ledPin, HIGH);
    digitalWrite(buzzerPin, LOW);
    display.setCursor(0, 40);
    display.setTextSize(1);
    display.println("Motor: ON (Safe)");
    motorRunning = true;
    shutdownInitiated = false;
  } 
  else if (motorRunning) {
    // If unsafe condition detected while motor is running
    if (!shutdownInitiated) {
      shutdownStartTime = millis();
      shutdownInitiated = true;
      digitalWrite(buzzerPin, HIGH);
      display.setCursor(0, 40);
      display.setTextSize(1);
      display.println("Unsafe! Motor off in 3min");
    } 
    else {
      unsigned long elapsed = millis() - shutdownStartTime;
      if (elapsed >= shutdownDelay) {
        digitalWrite(motorPin, LOW);
        digitalWrite(ledPin, LOW);
        digitalWrite(buzzerPin, LOW);
        motorRunning = false;
        shutdownInitiated = false;
        display.setCursor(0, 40);
        display.setTextSize(1);
        display.println("Motor OFF (Unsafe)");
      }
    }
  } 
  else {
    // Normal case (not safe, motor off)
    digitalWrite(motorPin, LOW);
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, LOW);
    display.setCursor(0, 40);
    display.setTextSize(1);
    display.println("Motor: OFF");
  }

  display.display();
  delay(500);
}
