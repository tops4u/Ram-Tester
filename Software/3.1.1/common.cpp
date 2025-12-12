// common.c - Implementation der gemeinsamen Functionen
//=======================================================================================

#include "common.h"
#include <string.h>
#include <stdio.h>

// Display-Objekt (nur wenn OLED aktiviert)
#ifdef OLED
U8X8_SSD1306_128X64_NONAME_SW_I2C display(/*clock=*/13, /*data=*/12, /*reset=*/U8X8_PIN_NONE);
#endif


#ifdef OLED
// Return left column to center 'n' characters on the current u8x8 display
inline uint8_t oledCenterCol(size_t n) {
  uint8_t cols = display.getCols();  // e.g. 16 on 128x64
  if (n > cols) n = cols;
  return (cols - n) / 2;
}

// Draw centered text; trims to visible width if too long
void oledDrawCentered(uint8_t row, const char *s) {
  uint8_t cols = display.getCols();
  size_t n = strlen(s);
  if (n <= cols) {
    display.drawString(oledCenterCol(n), row, s);
  } else {
    char buf[32];  // enough for common 128-wide (16 cols) & a bit more
    if (cols >= sizeof(buf)) cols = sizeof(buf) - 1;
    memcpy(buf, s, cols);
    buf[cols] = '\0';
    display.drawString(0, row, buf);  // too long: left-align trimmed
  }
}

// Overload for Arduino String
void oledDrawCentered(uint8_t row, const String &s) {
  oledDrawCentered(row, s.c_str());
}

// Convenience: centered line with a number appended (e.g. "Line=A7")
void oledDrawCenteredKV(uint8_t row, const char *key, int val) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%s%d", key, val);
  oledDrawCentered(row, buf);
}
#endif

//=======================================================================================
// GLOBALE VARIABLEN DEFINITIONEN
//=======================================================================================

// Detected RAM Type
int type = -1;
uint8_t Mode = 0;
uint8_t red = 13;    // PB5
uint8_t green = 12;  // PB4
// Test Patterns


// Test Patterns
const uint8_t pattern[] = { 0x00, 0xff, 0xaa, 0x55, 0xaa, 0x55 };

// The following RAM Types are currently identified. This table also contains retention timings.
// Structure: Name, Retention mS, DelayRows, rows, cols, IOBits, staticColumn, nibbleMode, smallType, delays[6], writeTime 
struct RAM_Definition ramTypes[] = {
  { "4164 64Kx1", 2, 1, 256, 256, 1, false, false, true, { 1069, 100, 100, 100, 100, 100 }, 986 },
  { "41256 256Kx1", 4, 1, 512, 512, 1, false, false, false, { 2167, 245, 245, 245, 245, 245 }, 1794 },
  { "41257 256K-NM", 4, 2, 512, 512, 1, false, true, false, { 2167, 245, 245, 245, 245, 245 }, 1794 },
  { "4416 16Kx4", 4, 4, 256, 64, 4, false, false, true, { 591, 591, 591, 591, 213, 213 }, 401 },
  { "4464 64Kx4", 4, 1, 256, 256, 4, false, false, false, { 2433, 950, 950, 950, 950, 950 }, 1540 },
  { "514256 256Kx4", 4, 2, 512, 512, 4, false, false, true, { 1370, 1363, 636, 636, 636, 636 }, 620 },
  { "514258 256K-SC", 4, 2, 512, 512, 4, true, false, true, { 1374, 1374, 604, 604, 604, 604 }, 620 },
  { "514400 1Mx4", 16, 5, 1024, 1024, 4, false, false, false, { 1957, 1957, 1957, 1957, 1957, 505 }, 1222 },
  { "514402 1Mx4-SC", 16, 5, 1024, 1024, 4, true, false, false, { 1936, 1955, 1962, 1962, 1951, 440 }, 1220 },
  { "411000 1Mx1", 8, 1, 1024, 1024, 1, false, false, false, { 4650, 1453, 1453, 1453, 1453, 1453 }, 3312 },
  { "4116 16Kx1", 2, 2, 128, 128, 1, false, false, false, { 520, 530, 46, 46, 46, 46 }, 472 },
  { "4816 16Kx1", 2, 2, 128, 128, 1, false, false, false, { 542, 542, 36, 36, 36, 36 }, 480 },
  { "4027 4Kx1", 2, 2, 64, 64, 1, false, false, false, {0,0,0,0,0,0}, 0}
};

// LED blink pattern structure
typedef struct {
  uint8_t green_blinks;   // Primary success indicator
  uint8_t orange_blinks;  // Secondary type indicator
} LedPattern;
// Success patterns - GREEN first (success), ORANGE second (type)
const LedPattern ledPatterns[] = {
  { 1, 1 },  // T_4164    - 64Kx1        16-pin
  { 1, 2 },  // T_41256   - 256Kx1       16-pin
  { 1, 3 },  // T_41257   - 256K-NM      16-pin Nibble
  { 2, 1 },  // T_4416    - 16Kx4        18-pin
  { 2, 2 },  // T_4464    - 64Kx4        18-pin
  { 3, 1 },  // T_514256  - 256Kx4       20-pin
  { 3, 3 },  // T_514258  - 256K-SC      20-pin Static Column
  { 3, 2 },  // T_514400  - 1Mx4         20-pin
  { 3, 4 },  // T_514402  - 1M-SC        20-pin Static Column
  { 2, 3 },  // T_411000  - 1Mx1         18-pin
  { 4, 1 },  // T_4116    - 16Kx1 Ada    20-pin via adapter
  { 1, 4 },  // T_4816    - 16Kx1        16-pin
  { 2, 4 }  // T_4027    - 4Kx1         16-Pin
};


// Test Data Table for the Pseudo-Random test
uint8_t randomTable[256] = {
  0xB, 0xC, 0x4, 0xC, 0xF, 0x1, 0xE, 0xF, 0xC, 0xA, 0x4, 0x0, 0x9, 0x8, 0xC, 0xA,
  0xD, 0xE, 0x0, 0xE, 0xA, 0xF, 0xD, 0x6, 0xE, 0x9, 0xC, 0xB, 0xC, 0x7, 0x1, 0xC,
  0xE, 0x8, 0x1, 0xE, 0x3, 0xF, 0xC, 0x5, 0x7, 0x9, 0x3, 0x4, 0x4, 0xD, 0xE, 0x1,
  0x5, 0x6, 0x0, 0x3, 0xC, 0x3, 0x8, 0x0, 0xA, 0x8, 0x2, 0x7, 0x7, 0x3, 0x2, 0x1,
  0xB, 0x7, 0x3, 0x4, 0xC, 0x9, 0x5, 0x2, 0x6, 0xF, 0x3, 0x6, 0x2, 0x4, 0x7, 0xC,
  0x9, 0x3, 0x0, 0x0, 0xB, 0x5, 0x5, 0x2, 0x8, 0xA, 0x6, 0xD, 0xC, 0xE, 0x5, 0x3,
  0x4, 0x8, 0x7, 0xE, 0xB, 0x6, 0xD, 0xB, 0x8, 0x1, 0x5, 0x8, 0x0, 0x3, 0xC, 0xD,
  0x0, 0x0, 0xE, 0x7, 0x1, 0x9, 0xB, 0x7, 0xD, 0x4, 0xE, 0x0, 0x6, 0x8, 0xC, 0xA,
  0xA, 0xC, 0xF, 0x8, 0xD, 0x7, 0xD, 0x7, 0x4, 0xF, 0xE, 0x2, 0x4, 0x3, 0xE, 0x3,
  0x5, 0xE, 0x6, 0x0, 0xC, 0x1, 0x4, 0x9, 0xF, 0x0, 0xF, 0xA, 0x3, 0xD, 0x8, 0x0,
  0x7, 0x0, 0xE, 0x7, 0xC, 0x5, 0x2, 0x2, 0x3, 0xD, 0xE, 0x6, 0x4, 0x4, 0x6, 0x2,
  0xA, 0x3, 0xB, 0x5, 0x3, 0xB, 0x5, 0x5, 0x2, 0x9, 0x0, 0x5, 0xE, 0xE, 0xC, 0xA,
  0x7, 0x8, 0x0, 0xD, 0x7, 0x5, 0x7, 0x2, 0xF, 0x6, 0x6, 0x4, 0x2, 0x1, 0x6, 0x2,
  0xF, 0x8, 0x2, 0xA, 0x0, 0x1, 0x2, 0x0, 0x8, 0x8, 0xB, 0x6, 0x3, 0x6, 0x3, 0x4,
  0x3, 0x3, 0xF, 0x1, 0x8, 0x7, 0x7, 0xF, 0x9, 0x6, 0x5, 0xE, 0xF, 0xD, 0x1, 0xF,
  0xD, 0x0, 0x7, 0x2, 0xA, 0x4, 0xC, 0xE, 0x3, 0xE, 0x8, 0x7, 0xE, 0x2, 0xB, 0x5
};

// Flip lower nibble of random data every second run to have full coverage
void randomizeData(void) {
  for (uint16_t i = 0; i < 256; i++) {
    randomTable[i] = (randomTable[i] & 0x0F) ^ 0x0F;
  }
}

void adc_init(void) {
  ADMUX = (1 << REFS0);
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t adc_read(uint8_t channel) {
  ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC))
    ;
  return ADC;
}

float adc_to_voltage(uint16_t adc_value) {
  return (float)adc_value * ADC_VREF / ADC_RESOLUTION;
}

/**
 * All RAM Chips require 8 RAS-only Refresh Cycles (ROR) for proper initialization
 * Performs the mandatory initialization sequence as specified in DRAM datasheets
 * @param RASPin Arduino pin number connected to RAS signal
 * @param CASPin Arduino pin number connected to CAS signal
 */
void initRAM(int RASPin, int CASPin) {
  delayMicroseconds(250);
  pinMode(RASPin, OUTPUT);
  pinMode(CASPin, OUTPUT);
  // RAS is an Active LOW Signal
  digitalWrite(RASPin, HIGH);
  // For some DRAM CAS NEEDS to be low during Init!
  digitalWrite(CASPin, HIGH);
  for (int i = 0; i < 8; i++) {
    digitalWrite(RASPin, LOW);
    digitalWrite(RASPin, HIGH);
  }
}

void writeRAMType(void){
#ifdef OLED
  display.drawString(0, 4, "Detected:");
  display.drawString(0, 6, ramTypes[type].name);
  display.drawString(0, 2, "Checking...");
#endif
}

/**
 * Check for ground shorts on all pins based on the current mode
 * Routes to appropriate pin mapping based on detected RAM package type
 */
void checkGNDShort() {
  if (Mode == Mode_20Pin)
    checkGNDShort4Port(CPU_20PORTB, CPU_20PORTC, CPU_20PORTD);
  else if (Mode == Mode_18Pin)
    checkGNDShort4Port(CPU_18PORTB, CPU_18PORTC, CPU_18PORTD);
  else
    checkGNDShort4Port(CPU_16PORTB, CPU_16PORTC, CPU_16PORTD);
}

/**
 * Check for shorts to GND on all ports when inputs have pullups enabled
 * Tests each pin to ensure no shorts to ground that would prevent proper operation
 * @param portb Pin mapping array for PORTB
 * @param portc Pin mapping array for PORTC  
 * @param portd Pin mapping array for PORTD
 */
void checkGNDShort4Port(const int *portb, const int *portc, const int *portd) {
  for (int i = 0; i <= 7; i++) {
    int8_t mask = 1 << i;
    if (portb[i] != EOL && portb[i] != NC && ((PINB & mask) == 0)) {
      error(portb[i], 4);
    }
    if (portc[i] != EOL && portc[i] != NC && ((PINC & mask) == 0)) {
      error(portc[i], 4);
    }
    if (portd[i] != EOL && portd[i] != NC && ((PIND & mask) == 0)) {
      error(portd[i], 4);
    }
  }
}

/**
 * Set bicolor LED to specified color
 */
void setLED(LedColor color) {
  switch (color) {
    case LED_OFF:
      digitalWrite(red, OFF);
      digitalWrite(green, OFF);
      break;
    case LED_RED:
      digitalWrite(red, ON);
      digitalWrite(green, OFF);
      break;
    case LED_GREEN:
      digitalWrite(red, OFF);
      digitalWrite(green, ON);
      break;
    case LED_ORANGE:
      digitalWrite(red, ON);
      digitalWrite(green, ON);
      break;
  }
}

/**
 * Blink LED in specified color for count times
 */
void blinkLED_color(LedColor color, uint8_t count, uint16_t on_ms, uint16_t off_ms) {
  for (uint8_t i = 0; i < count; i++) {
    setLED(color);
    delay(on_ms);
    setLED(LED_OFF);
    if (i < count - 1) {
      delay(off_ms);
    }
  }
}

/**
 * Prepare LED pins for indication of test results or errors
 * Configures all pins as inputs except VCC pins and LED, resets all outputs
 */
void setupLED() {
  sei();
  // Set all Pin LOW and configure all Pins as Input except the Vcc Pins and the LED
  PORTB = 0x00;
  PORTC &= 0xf0;
  PORTD = 0x1c;
  DDRB = 0x00;
  DDRC &= 0xc0;
  DDRD = 0x00;
  PORTD = 0x00;
  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);
  digitalWrite(red, OFF);
  digitalWrite(green, OFF);
}

// void u32_to_hex6(uint32_t val, char *buf) {
//   for (int8_t i = 5; i >= 0; i--) {
//     uint8_t nibble = val & 0xF;
//     buf[i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
//     val >>= 4;
//   }
//   buf[6] = '\0';
// }

/**
 * Indicate errors via LED pattern
 * Red LED flashes indicate error type, green LED flashes indicate error details
 * @param code Error detail code (varies by error type)
 * @param error Error type: 0=no RAM, 1=address error, 2=RAM fault, 3=retention error, 4=ground short
 */
void error(uint8_t code, uint8_t error, int16_t row, int16_t col) {
#ifdef OLED
  display.clearDisplay();
  display.setFont(u8x8_font_open_iconic_check_4x4);

  switch (error) {
    case 0:  // No RAM
      display.setFont(u8x8_font_open_iconic_embedded_4x4);
      display.drawString(6, 0, "G");
      display.setFont(u8x8_font_7x14B_1x2_r);
      oledDrawCentered(5, "RAM Inserted?");
      break;

    case 1:  // Address error
      display.drawString(6, 0, "B");
      display.setFont(u8x8_font_7x14B_1x2_r);
      if (code < 16) {
        oledDrawCentered(4, "Row address");
        display.drawString(4, 6, "Line=A");
        display.drawString(10, 6, String(code).c_str());
      } else if (code <= 32) {
        oledDrawCentered(4, "Col address");
        display.drawString(4, 6, "Line=A");
        display.drawString(10, 6, String(code >> 4).c_str());
      } else {
        oledDrawCentered(6, "Decoder Err");
      }
      break;

    case 2:  // RAM faulty (pattern or random test)
    case 3:  // Retention error - merged with case 2
      display.drawString(6, 0, "B");
      display.setFont(u8x8_font_7x14B_1x2_r);
      oledDrawCentered(4, "Ram Faulty!");
      display.drawString(3, 6, "Pattern ");
      display.drawString(11, 6, String(code).c_str());

      break;

    case 4:  // GND short
      display.setFont(u8x8_font_open_iconic_embedded_4x4);
      display.drawString(6, 0, "C");
      display.setFont(u8x8_font_7x14B_1x2_r);
      display.drawString(2, 5, "GND Short P=");
      display.drawString(14, 5, String(code).c_str());
      break;
  }
#endif

  setupLED();

  // LED patterns using RED and ORANGE only
  switch (error) {
    case 0:  // No RAM - slow red blink
      while (true) {
        setLED(LED_RED);
        delay(SLOW_BLINK_MS);
        setLED(LED_OFF);
        delay(SLOW_BLINK_MS);
      }
      break;

    case 1:  // Address error - 1 red, n orange
      while (true) {
        blinkLED_color(LED_RED, 1, BLINK_ON_MS, BLINK_OFF_MS);
        delay(INTER_BLINK_MS);
        if (code > 0 && code <= 20) {
          blinkLED_color(LED_ORANGE, code, BLINK_ON_MS, BLINK_OFF_MS);
        }
        delay(ERROR_PAUSE_MS);
      }
      break;

    case 2:  // RAM faulty - pattern or random test
    case 3:  // Retention error - merged handling
      while (true) {
        blinkLED_color(LED_RED, 2, BLINK_ON_MS, BLINK_OFF_MS);
        delay(INTER_BLINK_MS);

        uint8_t orange_count = 0;
        if (error == 3) {
          orange_count = 7;  // Retention = 7 orange
        } else if (code <= 4) {
          orange_count = code + 1;  // Pattern 0-4 = 1-5 orange
        } else {
          orange_count = 6;  // Random = 6 orange
        }

        if (orange_count > 0) {
          blinkLED_color(LED_ORANGE, orange_count, BLINK_ON_MS, BLINK_OFF_MS);
        }
        delay(ERROR_PAUSE_MS);
      }
      break;

    case 4:  // Ground short - 3 red, n orange
      while (true) {
        blinkLED_color(LED_RED, 3, BLINK_ON_MS, BLINK_OFF_MS);
        delay(INTER_BLINK_MS);
        if (code > 0 && code <= 20) {
          blinkLED_color(LED_ORANGE, code, BLINK_ON_MS, BLINK_OFF_MS);
        }
        delay(ERROR_PAUSE_MS);
      }
      break;
  }
}

/**
 * Configuration error - fast red blinking
 */
void ConfigFail() {
  setupLED();
  while (true) {
    setLED(LED_RED);
    delay(FAST_BLINK_MS);
    setLED(LED_OFF);
    delay(FAST_BLINK_MS);
  }
}

/**
 * Test successful - GREEN/ORANGE pattern for identified RAM type
 */
void testOK() {
#ifdef OLED
  display.clearDisplay();
  display.setFont(u8x8_font_7x14B_1x2_r);
  oledDrawCentered(5, ramTypes[type].name);
  display.setFont(u8x8_font_open_iconic_check_4x4);
  display.drawString(6, 0, "A");
#endif

  setupLED();

  // Validate type
  if (type < 0 || type > T_4816) {
    // Unknown type - steady green
    while (true) {
      setLED(LED_GREEN);
      delay(1000);
      setLED(LED_OFF);
      delay(1000);
    }
  }

  // Main success pattern loop
  while (true) {
    // Green blinks first - indicates SUCCESS
    blinkLED_color(LED_GREEN, ledPatterns[type].green_blinks, BLINK_ON_MS, BLINK_OFF_MS);

    delay(INTER_BLINK_MS);

    // Orange blinks second - indicates specific type
    blinkLED_color(LED_ORANGE, ledPatterns[type].orange_blinks, BLINK_ON_MS, BLINK_OFF_MS);

    delay(PATTERN_PAUSE_MS);
  }
}

/**
 * Factory test mode for installation checks after PCB assembly and soldering
 * Tests LED functionality and checks all pin connections for shorts to ground
 * To exit: set all DIP switches to ON, short pin 1 to ground 5 times until green LED stays on
 * This is an initial test for soldering problems - Switch all DIP Switches to 0
 * The LED will be green for 1 sec and red for 1 sec to test LED function. If first the Red and then the Green lights up, write 0x00 to Position 0x01 of the EEPROM.
 * All Inputs will become PullUP
 * One by One short the Inputs to GND which checks connection to GND. If Green LED comes on one Pin Grounded was detected, RED if it was more than one
 * If Green does not light then this contact has a problem.
 * To Quit Test Mode forever set all DIP Switches to ON and Short Pin 1 to Ground for 5 Times until the Green LED is steady on. This indicates the EEPROM stored the Information.
 */

void buildTest() {
#ifdef OLED
  display.drawString(2, 2, "TEST MODE");
  display.drawString(0, 4, "All DIP to '1'");
  display.drawString(1, 6, "& Reset quit");
#endif
  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);
  digitalWrite(green, OFF);
  digitalWrite(red, OFF);
  digitalWrite(green, ON);
  delay(1000);
  digitalWrite(green, OFF);
  digitalWrite(red, ON);
  delay(1000);
  digitalWrite(red, OFF);
  int8_t counter = 0;
  bool pin1 = false;
  DDRB &= 0b11100000;
  DDRC &= 0b11000000;
  DDRD = 0x00;
  // If all DIP Switches are ON disable the Test Mode
  if ((digitalRead(19) && digitalRead(3) && digitalRead(2))) {
    EEPROM.update(TESTING, 0x00);
#ifdef OLED
    display.clearDisplay();
    display.drawString(1, 2, "TEST MODE");
    display.drawString(0, 4, "DEACTIVATED");
#endif
    while (true)
      ;
  }
  // Activate Pullups for Testing
  PORTB |= 0b00011111;
  PORTC |= 0b00111111;
  PORTD = 0xff;
  do {
    int c = 0;
    for (int i = 0; i <= 19; i++) {
      if (i == 13)
        continue;
      if (digitalRead(i) == false)
        c++;
    }
    if (c == 1)
      digitalWrite(red, ON);
    else
      digitalWrite(red, OFF);
  } while (true);
}
