// RAM Tester Diagnostic Sketch
// Tests all pins, resistors, and OLED display
// Target: ATMEGA328 custom PCB

#include <Arduino.h>
#include <U8x8lib.h>

//=======================================================================================
// PIN DEFINITIONS
//=======================================================================================

// LED Pins
#define LED_RED_PIN 13    // PB5
#define LED_GREEN_PIN 12  // PB4

// 20-Pin Socket Mapping (based on 20Pin.h)
// Port B Pins: PB0(CAS), PB1(RAS), PB2(OE), PB3(WE), PB4(A8)
// Port C Pins: PC0(IO0), PC1(IO1), PC2(IO2), PC3(IO3), PC4(A9)
// Port D Pins: PD0(A0), PD1(A1), PD2(A2), PD3(A3), PD4(A4), PD5(A5), PD6(A6), PD7(A7)

// Pins to test (all 20-pin socket pins except GND)
const uint8_t TEST_PINS_B[] = { 8, 9, 10, 11, 12 };        // PB0-PB4 (5 pins)
const uint8_t TEST_PINS_C[] = { 14, 15, 16, 17, 18, 19 };  // PC0-PC4 (6 pins)
const uint8_t TEST_PINS_D[] = { 0, 1, 2, 3, 4, 5, 6, 7 };  // PD0-PD7 (8 pins)
boolean check[20] = { false };
// Resistor test pins (ZIF Socket pins 8, 9, 10)
// Based on common.cpp checkGNDShort implementation
const uint8_t RESISTOR_TEST_PINS[] = { 2, 3, 19 };  // Arduino pin numbers

//=======================================================================================
// OLED DISPLAY
//=======================================================================================

U8X8_SSD1306_128X64_NONAME_SW_I2C display(
  /* clock=*/13,
  /* data=*/12,
  /* reset=*/U8X8_PIN_NONE);

//=======================================================================================
// LED CONTROL
//=======================================================================================

enum LedState {
  LED_OFF,
  LED_RED,
  LED_GREEN,
  LED_BLINK
};

void setLED(LedState state) {
  switch (state) {
    case LED_OFF:
      digitalWrite(LED_RED_PIN, LOW);
      digitalWrite(LED_GREEN_PIN, LOW);
      break;
    case LED_RED:
      digitalWrite(LED_RED_PIN, HIGH);
      digitalWrite(LED_GREEN_PIN, LOW);
      break;
    case LED_GREEN:
      digitalWrite(LED_RED_PIN, LOW);
      digitalWrite(LED_GREEN_PIN, HIGH);
      break;
    case LED_BLINK:
      // Handled in loop
      break;
  }
}

void blinkLED(uint16_t duration_ms) {
  unsigned long start = millis();
  while (millis() - start < duration_ms) {
    digitalWrite(LED_RED_PIN, HIGH);
    digitalWrite(LED_GREEN_PIN, LOW);
    delay(200);
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(LED_GREEN_PIN, HIGH);
    delay(300);
  }
}

//=======================================================================================
// SETUP
//=======================================================================================

void setup() {
  // Initialize LED pins
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  // Start with GREEN LED on (RED will be tested)
  setLED(LED_GREEN);
  if (!testResistors()) {
    blinkLED(10000);  // Blink for 10 seconds
    while (1)
      ;  // Halt
  }
  checkShortPins();
  // Setup all test pins as INPUT_PULLUP
  setupTestPins();
  // Check for initial shorts
  if (!checkAllPinsHigh()) {
    blinkLED(5000);
    while (1)
      ;
  }
}

//=======================================================================================
// RESISTOR TEST
//=======================================================================================

bool testResistors() {
  bool allPassed = true;

  for (uint8_t i = 0; i < 3; i++) {
    uint8_t pin = RESISTOR_TEST_PINS[i];

    // Set pin to OUTPUT HIGH
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    delayMicroseconds(10);

    // Switch to INPUT (no pullup) and read immediately
    pinMode(pin, INPUT);
    delay(500);  // 20Pin has a capacitor which needs to deplete long enough

    // If external pulldown resistor is working, pin should read LOW
    bool pinState = digitalRead(pin);

    if (pinState == HIGH) {
      allPassed = false;
    }

    // Return to INPUT_PULLUP for normal operation
    pinMode(pin, INPUT_PULLUP);
  }

  return allPassed;
}

//=======================================================================================
// PIN SETUP
//=======================================================================================

void setupTestPins() {
  digitalWrite(19, HIGH);
  delay(100);
  for (uint8_t i = 0; i < sizeof TEST_PINS_C; i++) {
    pinMode(TEST_PINS_C[i], INPUT_PULLUP);
  }
  for (uint8_t i = 0; i < sizeof TEST_PINS_B; i++) {
    pinMode(TEST_PINS_B[i], INPUT_PULLUP);
  }
  for (uint8_t i = 0; i < sizeof TEST_PINS_D; i++) {
    pinMode(TEST_PINS_D[i], INPUT_PULLUP);
  }
}

//=======================================================================================
// PIN CHECKING
//=======================================================================================

bool checkAllPinsHigh() {
  // Check Port B
  for (uint8_t i = 0; i < sizeof TEST_PINS_B; i++) {
    if (digitalRead(TEST_PINS_B[i]) == LOW) {
      return false;
    }
  }
  //Check Port D
  for (uint8_t i = 0; i < sizeof TEST_PINS_D; i++) {
    if (digitalRead(TEST_PINS_D[i]) == LOW) {
      return false;
    }
  }
  // Check Port C
  for (uint8_t i = 0; i < sizeof TEST_PINS_C; i++) {
    if (digitalRead(TEST_PINS_C[i]) == LOW) {
      return false;
    }
  }
  return true;
}

void checkShortPins(void) {
  for (int i = 0; i < 20; i++) {
    digitalWrite(19, HIGH);
    if (i == 13)
      continue;
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
    delay(200);
    for (int j = 0; j < 20; j++) {
      if ((i == 13) || (i == j))
        continue;
      pinMode(j, OUTPUT);
      digitalWrite(j, HIGH);
      pinMode(j, INPUT_PULLUP);
      if (digitalRead(j) != true) {
        for (int u = 0; u <= i; u++) {
          setLED(LED_RED);
          delay(250);
          setLED(LED_OFF);
          delay(500);
        }
        for (int u = 0; u <= j; u++) {
          setLED(LED_GREEN);
          delay(250);
          setLED(LED_OFF);
          delay(500);
        }
        while (1)
          ;
      }
      pinMode(j, OUTPUT);
      digitalWrite(j, LOW);
    }
  }
  setLED(LED_RED);
  delay(250);
  setLED(LED_GREEN);
}
//=======================================================================================
// PIN TESTING
//=======================================================================================

void testPins() {
  check[13] = true;  // This is used for the LED so no need to check it.
  while (true) {
    boolean allPass = true;
    for (int i = 0; i <= 19; i++) {
      if (i == 13) i++;
      if (digitalRead(i) == LOW) {
        check[i] = true;
        pinMode(LED_GREEN_PIN, OUTPUT);
        digitalWrite(LED_GREEN_PIN, LOW);
        digitalWrite(LED_RED_PIN, HIGH);
        delay(200);
        digitalWrite(LED_RED_PIN, LOW);
        pinMode(LED_GREEN_PIN, INPUT_PULLUP);
      }
      if (check[i] == false)
        allPass = false;
    }
    if (allPass == true)
      break;
  }
}

//=======================================================================================
// OLED TEST
//=======================================================================================

void testOLED() {
  // LED off during OLED test
  setLED(LED_OFF);

  // Endless loop for OLED testing
  while (true) {
    // Initialize display
    display.begin();
    display.setFont(u8x8_font_chroma48medium8_r);
    display.clear();
    delay(500);

    // Write "TEST" centered (128 pixels / 8 = 16 chars, "TEST" = 4 chars, center at (16-4)/2 = 6)
    display.drawString(6, 3, "TEST");
    delay(1000);
  }
}

//=======================================================================================
// MAIN LOOP
//=======================================================================================

void loop() {
  testPins();
  testOLED();
}