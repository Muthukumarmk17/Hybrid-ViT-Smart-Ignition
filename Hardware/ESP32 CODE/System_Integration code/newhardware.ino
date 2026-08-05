#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// === OLED Settings ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === Pin Definitions ===
const int mq3Pin = 34;       // MQ-3 Analog input
const int buzzerPin = 26;    // Buzzer output
const int ledPin = 25;       // LED output
const int motorPin = 27;     // Motor relay control

// === Alcohol Threshold ===
const int threshold = 2000;

// === Variables ===
bool helmetDetected = false;
bool alcoholSafe = false;
bool motorRunning = false;
bool shutdownInitiated = false;
unsigned long shutdownStartTime = 0;
const unsigned long shutdownDelay = 60 * 1000;  // 1 minute
unsigned long lastBeepTime = 0;

void setup() {
  Serial.begin(115200);

  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW);

  // OLED Initialization
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10, 20);
  display.println("SMART HELMET");
  display.display();
  delay(1500);
}

void loop() {
  // --- Receive Helmet Detection Data from Python ---
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    if (data.startsWith("HELMET:")) {
      helmetDetected = (data.substring(7).toInt() == 1);
      Serial.print("Helmet status: ");
      Serial.println(helmetDetected ? "Detected" : "Not Detected");
    }
  }

  // --- Read Alcohol Sensor ---
  int sensorValue = analogRead(mq3Pin);
  alcoholSafe = (sensorValue < threshold);

  // --- Display Base Info ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.print("Helmet: ");
  display.println(helmetDetected ? "Yes" : "No");

  display.setCursor(0, 12);
  display.print("Alcohol: ");
  display.println(alcoholSafe ? "Safe" : "Detected");

  unsigned long now = millis();

  // === SAFE CONDITION ===
  if (helmetDetected && alcoholSafe) {
    digitalWrite(motorPin, HIGH);
    digitalWrite(ledPin, HIGH);
    digitalWrite(buzzerPin, LOW);

    if (shutdownInitiated) {
      shutdownInitiated = false;
      Serial.println("✅ Safe again — shutdown cancelled.");
      display.setCursor(0, 35);
      display.println("Safe again!");
      display.setCursor(0, 47);
      display.println("Motor continues");
      display.display();
      delay(1000);
    }

    motorRunning = true;
    display.setCursor(0, 35);
    display.println("Motor: ON (Safe)");
  }

  // === UNSAFE CONDITION WHILE MOTOR RUNNING ===
  else if (motorRunning) {
    if (!shutdownInitiated) {
      shutdownStartTime = now;
      shutdownInitiated = true;
      Serial.println("⚠ Unsafe! Motor will off in 1 minute.");
      digitalWrite(buzzerPin, HIGH);
      display.setCursor(0, 35);
      display.println("⚠ Unsafe!");
      display.setCursor(0, 47);
      display.println("Motor off in 60s");
      display.display();
      delay(1000);
      digitalWrite(buzzerPin, LOW);
    } 
    else {
      unsigned long elapsed = now - shutdownStartTime;
      unsigned long remaining = (shutdownDelay - elapsed) / 1000;

      // --- Countdown Display ---
      if (remaining > 0) {
        display.setCursor(0, 35);
        display.print("Unsafe! Off in ");
        display.print(remaining);
        display.println("s");
      }

      // --- Buzzer Beep Every 5 Seconds ---
      if (remaining % 5 == 0 && now - lastBeepTime > 1000) {
        digitalWrite(buzzerPin, HIGH);
        delay(200);
        digitalWrite(buzzerPin, LOW);
        lastBeepTime = now;
      }

      // --- Cancel Shutdown if Safe Again ---
      if (helmetDetected && alcoholSafe) {
        shutdownInitiated = false;
        Serial.println("✅ Recovered during countdown, motor continues.");
        display.setCursor(0, 35);
        display.println("Safe again!");
        display.setCursor(0, 47);
        display.println("Motor continues");
        display.display();
        delay(1000);
        motorRunning = true;
      }

      // --- Shutdown After 1 Minute ---
      if (elapsed >= shutdownDelay) {
        digitalWrite(motorPin, LOW);
        digitalWrite(ledPin, LOW);
        digitalWrite(buzzerPin, LOW);
        motorRunning = false;
        shutdownInitiated = false;
        Serial.println("⛔ Motor OFF (Unsafe expired)");
        display.setCursor(0, 35);
        display.println("Motor OFF (Unsafe)");
      }
    }
  }

  // === IDLE / MOTOR OFF ===
  else {
    digitalWrite(motorPin, LOW);
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, LOW);
    display.setCursor(0, 35);
    display.println("Motor: OFF");
  }

  display.display();
  delay(300);
}
