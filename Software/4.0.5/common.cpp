/**
 * @file common.cpp
 * @brief Core functionality shared across all RAM testing modules
 *
 * This file implements common functions and data structures used by all RAM testing
 * modules (16-pin, 18-pin, 20-pin). It provides:
 *
 * - Global RAM type definitions and timing specifications
 * - LED control and error/success indication system
 * - OLED display management (when enabled)
 * - Ground short detection
 * - DRAM initialization routines
 * - Self-test functionality for hardware verification
 * - ADC utilities for voltage monitoring
 *
 * LED Blink Pattern System:
 * ------------------------
 * Success patterns use GREEN blinks (test passed) followed by ORANGE blinks (chip type):
 *   Format: [GREEN blinks] pause [ORANGE blinks] pause → repeats
 *   Example: 1 GREEN, 2 ORANGE = 41256 (256Kx1, 16-pin) tested OK
 *
 * Error patterns use RED blinks (error type) followed by ORANGE blinks (error details):
 *   Fast RED:           Configuration error (invalid DIP switches)
 *   Slow RED:           No RAM detected
 *   1 RED, n ORANGE:    Address line error (n = address line 0-15)
 *   2 RED, 1-6 ORANGE:  Pattern/random test error (1-5 = pattern 0-4, 6 = random)
 *   2 RED, 7 ORANGE:    Retention error (data not held during refresh)
 *   3 RED, n ORANGE:    Ground short detected (n = pin number)
 *
 * RAM Type Definitions:
 * --------------------
 * Each RAM type has specific timing parameters stored in ramTypes[] array:
 *   - name: PROGMEM string with chip designation
 *   - retentionMS: Maximum refresh interval in milliseconds
 *   - delayRows: Number of rows to delay during retention testing
 *   - rows/cols: Address space dimensions
 *   - flags: Packed boolean flags (staticColumn, nibbleMode, smallType)
 *   - delays[6]: Pattern-specific retention delays in 20μs units
 *   - writeTime: Write cycle time in 20μs units
 *
 * Self-Test Mode:
 * --------------
 * Activated when all DIP switches are ON at startup. Tests:
 *   1. External pulldown resistors on DIP switch pins
 *   2. Pin-to-pin shorts detection across all socket pins
 *   3. Continuity test with jumper wire from pin 20 to all other pins
 *   4. Visual feedback via OLED display and bicolor LED
 *
 * Hardware Connections:
 * --------------------
 *   LED_RED_PIN (13):   PB5 - Red LED (also Arduino built-in LED)
 *   LED_GREEN_PIN (12): PB4 - Green LED
 *   OLED (optional):    SCL on pin 13, SDA on pin 12 (I2C at 400kHz)
 *
 * @note OLED uses same pins as LEDs but operates on I2C protocol
 * @note Self-test functions require OLED to be enabled
 */

#include "common.h"
#include <string.h>
#include <stdio.h>

//=======================================================================================
// DISPLAY INITIALIZATION
//=======================================================================================

/**
 * OLED Display Object
 *
 * Software I2C implementation using bit-banging on LED pins:
 *   - clock: Pin 13 (LED_RED_PIN/PB5)
 *   - data:  Pin 12 (LED_GREEN_PIN/PB4)
 *   - Bus speed: 400kHz for faster screen updates
 *
 * Note: Only available when OLED is defined in common.h
 * OLED_2PAGE selects _2 suffix (256 byte buffer, 4 passes) vs _1 (128 byte, 8 passes)
 */
#ifdef OLED
#ifdef OLED_2PAGE
U8G2_SSD1306_128X64_NONAME_2_SW_I2C display(U8G2_R0, /*clock=*/13, /*data=*/12, /*reset=*/U8X8_PIN_NONE);
#else
U8G2_SSD1306_128X64_NONAME_1_SW_I2C display(U8G2_R0, /*clock=*/13, /*data=*/12, /*reset=*/U8X8_PIN_NONE);
#endif
#endif

//=======================================================================================
// GLOBAL VARIABLE DEFINITIONS
//=======================================================================================

/**
 * Detected RAM Type Index
 *
 * Index into ramTypes[] array identifying the detected chip.
 * Values correspond to RamType enum (T_4164, T_41256, etc.)
 * -1 indicates no RAM type detected yet (initial state)
 */
int type = -1;

/**
 * DIP Switch Configuration Mode
 *
 * Combines three DIP switch states to determine pin configuration:
 *   Pin 19 (A5) contributes Mode_20Pin (4) if HIGH
 *   Pin  3 (D3) contributes Mode_18Pin (2) if HIGH
 *   Pin  2 (D2) contributes Mode_16Pin (1) if HIGH
 *
 * Valid modes:
 *   Mode_16Pin (2): Tests 16-pin DRAMs (4164, 41256, 41257, 4816, 4532-L/H)
 *   Mode_18Pin (3): Tests 18-pin DRAMs (4416, 4464, 411000)
 *   Mode_20Pin (5): Tests 20-pin DRAMs (514256, 514258, 514400, 514402, 4116, 4027)
 *
 * Invalid modes (0,1,4,6,7) trigger ConfigFail()
 */
uint8_t Mode = 0;

//=======================================================================================
// TEST PATTERN DEFINITIONS
//=======================================================================================

/**
 * Standard Test Patterns
 *
 * Six patterns tested in sequence to verify different failure modes:
 *   [0] 0x00 - All zeros (stuck-at-0 test)
 *   [1] 0xFF - All ones (stuck-at-1 test)
 *   [2] 0xAA - Alternating bits 10101010 (column short test)
 *   [3] 0x55 - Alternating bits 01010101 (column short inverted)
 *   [4] 0xAA - Pseudo-random base pattern (address decoder test)
 *   [5] 0x55 - Pseudo-random inverted (retention test with full coverage)
 *
 * For patterns 4-5, actual data comes from randomTable[] lookup
 */
const uint8_t pattern[] = { 0x00, 0xff, 0xaa, 0x55, 0xaa, 0x55 };

//=======================================================================================
// RAM TYPE NAME STRINGS (PROGMEM)
//=======================================================================================

/**
 * RAM Type Name Strings
 *
 * Stored in flash memory (PROGMEM) to conserve RAM.
 * Saves approximately 370 bytes compared to RAM storage.
 * Referenced by ramTypes[].name pointers.
 */
const char ramType_4164[] PROGMEM = "4164 64Kx1";
const char ramType_41256[] PROGMEM = "41256 256Kx1";
const char ramType_41257[] PROGMEM = "41257 256Kx1-NM";
const char ramType_4416[] PROGMEM = "4416 16Kx4";
const char ramType_4464[] PROGMEM = "4464 64Kx4";
const char ramType_514256[] PROGMEM = "514256 256Kx4";
const char ramType_514258[] PROGMEM = "514258 256Kx4-SC";
const char ramType_514400[] PROGMEM = "514400 1Mx4";
const char ramType_514402[] PROGMEM = "514402 1Mx4-SC";
const char ramType_411000[] PROGMEM = "411000 1Mx1";
const char ramType_4116[] PROGMEM = "4116 16Kx1";
const char ramType_4816[] PROGMEM = "4816 16Kx1";
const char ramType_4027[] PROGMEM = "4027 4Kx1";
const char ramType_4532L[] PROGMEM = "4532/3732-L 32Kx1";
const char ramType_4532H[] PROGMEM = "4532/3732-H 32Kx1";

//=======================================================================================
// RAM TYPE DEFINITIONS WITH TIMING PARAMETERS
//=======================================================================================

/**
 * RAM Type Definition Table
 *
 * Complete specifications for all supported DRAM types.
 * Each entry contains timing and structural parameters:
 *
 * @field name          PROGMEM string pointer to chip designation
 * @field retentionMS   Maximum refresh interval (2-16ms depending on chip)
 * @field delayRows     Number of rows to delay during retention test
 * @field rows          Number of row addresses (128-1024)
 * @field cols          Number of column addresses (64-1024)
 * @field flags         Packed boolean flags (bit 0: staticColumn, bit 1: nibbleMode, bit 2: smallType)
 * @field delays[6]     Pattern-specific retention delays in 20μs units (multiply by 20 for actual μs)
 * @field writeTime     Write cycle time in 20μs units (multiply by 20 for actual μs)
 *
 * Retention Timing Notes:
 *   - delays[0-1]: Stuck-at patterns, longer delays allowed
 *   - delays[2-4]: Alternating/pseudo-random patterns
 *   - delays[5]:   Final retention test with inverted random data
 *   - Timing empirically determined to maximize test coverage without false failures
 *   - Values stored in 20μs units to save flash (uint8_t instead of uint16_t)
 *
 * Special Cases:
 *   - 4164: Uses 4ms refresh timing (accommodates both 2ms and 4ms variants)
 *   - 4532-L/H: Half-functional 4164 variants (32K instead of 64K)
 *   - 41257: Nibble mode requires special handling in test routines
 *   - 514258/514402: Static column mode enables faster column access
 *   - 4116/4027: Tested via 20-pin adapter with voltage conversion
 */
struct RAM_Definition ramTypes[] = {
  // name, retMS, delayRows, rows, cols, flags, delays[6] (×20μs), writeTime (×20μs)
  { ramType_4164, 4, 2, 256, 256, RAM_FLAG_SMALL_TYPE, { 62, 61, 20, 20, 20, 20 }, 39 },
  { ramType_41256, 4, 1, 512, 512, 0, { 125, 41, 41, 41, 41, 41 }, 75 },
  { ramType_41257, 4, 0, 512, 512, RAM_FLAG_NIBBLE_MODE, { 0, 0, 0, 0, 0, 0 }, 0 },
  { ramType_4416, 4, 4, 256, 64, RAM_FLAG_SMALL_TYPE, { 30, 30, 30, 30, 11, 11 }, 21 },
  { ramType_4464, 4, 1, 256, 256, 0, { 122, 48, 48, 48, 48, 48 }, 77 },
  { ramType_514256, 4, 2, 512, 512, RAM_FLAG_SMALL_TYPE, { 69, 68, 27, 27, 27, 27 }, 31 },
  { ramType_514258, 4, 2, 512, 512, RAM_FLAG_STATIC_COLUMN | RAM_FLAG_SMALL_TYPE, { 69, 68, 27, 27, 27, 27 }, 31 },
  { ramType_514400, 16, 5, 1024, 1024, 0, { 98, 98, 98, 98, 98, 16 }, 62 },
  { ramType_514402, 16, 5, 1024, 1024, RAM_FLAG_STATIC_COLUMN, { 99, 98, 98, 98, 98, 14 }, 62 },
  { ramType_411000, 8, 1, 1024, 1024, 0, { 244, 135, 135, 135, 135, 135 }, 255 },
  { ramType_4116, 2, 2, 128, 128, 0, { 30, 30, 6, 6, 6, 6 }, 24 },
  { ramType_4816, 2, 2, 128, 128, 0, { 30, 30, 7, 7, 7, 7 }, 24 },
  { ramType_4027, 2, 2, 64, 64, 0, { 40, 40, 27, 27, 27, 27 }, 12 },
  { ramType_4532L, 2, 1, 128, 256, RAM_FLAG_SMALL_TYPE, { 54, 5, 5, 5, 5, 5 }, 50 },
  { ramType_4532H, 2, 1, 128, 256, RAM_FLAG_SMALL_TYPE, { 54, 5, 5, 5, 5, 5 }, 50 }
};

//=======================================================================================
// LED BLINK PATTERN DEFINITIONS
//=======================================================================================

/**
 * LED Blink Pattern Structure
 *
 * Defines success indication pattern for each RAM type.
 * Pattern consists of two components:
 *   - Green blinks: Primary success indicator (1-4 blinks)
 *   - Orange blinks: Secondary type identifier (1-6 blinks)
 */
typedef struct {
  uint8_t green_blinks;   ///< Number of green blinks (indicates test passed)
  uint8_t orange_blinks;  ///< Number of orange blinks (indicates specific chip type)
} LedPattern;

/**
 * Success Blink Pattern Table
 *
 * Maps each RamType enum to its unique LED pattern.
 * Index corresponds to RamType enum values (T_4164=0, T_41256=1, etc.)
 *
 * Pattern Organization:
 *   16-pin chips: 1 green + variable orange (1-6)
 *   18-pin chips: 2 green + variable orange (1-3)
 *   20-pin chips: 3 green + variable orange (1-4)
 *   Adapter chips: 4 green + variable orange (1-2)
 *
 * Complete Pattern List (see main .ino file header for full reference):
 *   1G-1O: 4164    | 1G-2O: 41256   | 1G-3O: 41257   | 1G-4O: 4816
 *   1G-5O: 4532-L  | 1G-6O: 4532-H  | 2G-1O: 4416    | 2G-2O: 4464
 *   2G-3O: 411000  | 3G-1O: 514256  | 3G-2O: 514400  | 3G-3O: 514258
 *   3G-4O: 514402  | 4G-1O: 4116    | 4G-2O: 4027
 */
const LedPattern ledPatterns[] = {
  { 1, 1 },  // T_4164    - 64Kx1        16-pin
  { 1, 2 },  // T_41256   - 256Kx1       16-pin
  { 1, 3 },  // T_41257   - 256K-NM      16-pin Nibble Mode
  { 2, 1 },  // T_4416    - 16Kx4        18-pin
  { 2, 2 },  // T_4464    - 64Kx4        18-pin
  { 3, 1 },  // T_514256  - 256Kx4       20-pin
  { 3, 3 },  // T_514258  - 256K-SC      20-pin Static Column
  { 3, 2 },  // T_514400  - 1Mx4         20-pin
  { 3, 4 },  // T_514402  - 1M-SC        20-pin Static Column
  { 2, 3 },  // T_411000  - 1Mx1         18-pin
  { 4, 1 },  // T_4116    - 16Kx1        20-pin via adapter
  { 1, 4 },  // T_4816    - 16Kx1        16-pin
  { 4, 2 },  // T_4027    - 4Kx1         20-pin via adapter
  { 1, 5 },  // T_4532_L  - 32Kx1 Low    16-pin (half-good 4164)
  { 1, 6 }   // T_4532_H  - 32Kx1 High   16-pin (half-good 4164)
};

//=======================================================================================
// PSEUDO-RANDOM TEST DATA TABLE
//=======================================================================================

/**
 * Pseudo-Random Test Data Table
 *
 * 256-byte RAM array generated at startup for patterns 4-5 testing.
 * Generator uses compact LFSR algorithm - saves 188 bytes vs pre-populated table.
 */
uint8_t randomTable[256];

//=======================================================================================
// UTILITY FUNCTIONS
//=======================================================================================

/**
 * Generate pseudo-random test data table at startup
 *
 * Populates randomTable[] with 256 pseudo-random 4-bit values using LFSR algorithm.
 * Saves 188 bytes flash compared to storing pre-calculated table.
 *
 * @note Must be called during setup() before any RAM testing begins
 */
void generateRandomTable() {
  for (uint16_t i = 0; i < 256; i++) {
    uint16_t lfsr = 0xACE1u ^ (i * 0x3D);
    for (uint8_t j = 0; j < 8; j++) {
      lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
    }
    randomTable[i] = (lfsr ^ (lfsr >> 8)) & 0x0F;
  }
}

/**
 * Invert lower nibble of random data table for pattern 5
 *
 * Flips all 4 bits in each table entry to create inverted pattern.
 * Called once per test cycle to alternate between normal and inverted random data.
 * Ensures complete bit coverage - every cell tested with both 0 and 1 in all bit positions.
 *
 * Operation: randomTable[i] = (randomTable[i] & 0x0F) ^ 0x0F
 *   - Masks upper nibble (preserve, though always 0)
 *   - XOR lower nibble with 0x0F to invert all 4 bits
 *   - Example: 0x0B (1011) becomes 0x04 (0100)
 */
void invertRandomTable(void) {
  for (uint16_t i = 0; i < 256; i++) {
    randomTable[i] = (randomTable[i] & 0x0F) ^ 0x0F;
  }
}

//=======================================================================================
// ADC FUNCTIONS (for voltage monitoring and hardware verification)
//=======================================================================================

/**
 * Initialize ADC with AVcc reference and 128 prescaler
 *
 * Configures 10-bit ADC for voltage measurements:
 *   - Reference: AVcc (typically 5V on Arduino Uno)
 *   - Prescaler: 128 (16MHz / 128 = 125kHz ADC clock)
 *   - Resolution: 10-bit (0-1023 representing 0-5V)
 *
 * ADC is used in self-test mode to verify power supply voltages
 * and pulldown resistor functionality.
 */
void adc_init(void) {
  ADMUX = (1 << REFS0);                                      // AVcc reference
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);  // Enable ADC, prescaler 128
}

/**
 * Read 10-bit ADC value from specified channel
 *
 * Performs single ADC conversion and returns result.
 * Blocking function - waits for conversion to complete.
 *
 * @param channel ADC channel number (0-7 corresponding to A0-A7)
 * @return 10-bit ADC reading (0-1023 representing 0-Vref)
 */
uint16_t adc_read(uint8_t channel) {
  ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);  // Select channel, preserve reference
  ADCSRA |= (1 << ADSC);                      // Start conversion
  while (ADCSRA & (1 << ADSC))                // Wait for completion
    ;
  return ADC;  // Return 10-bit result
}

/**
 * Convert 10-bit ADC reading to voltage
 *
 * Converts raw ADC value to actual voltage using reference voltage.
 * Assumes AVcc reference (typically 5.0V on Arduino Uno).
 *
 * @param adc_value 10-bit ADC reading (0-1023)
 * @return Voltage in volts (0.0 - 5.0V with AVcc reference)
 */
float adc_to_voltage(uint16_t adc_value) {
  return (float)adc_value * ADC_VREF / ADC_RESOLUTION;
}

//=======================================================================================
// DRAM INITIALIZATION
//=======================================================================================

/**
 * Initialize DRAM with 8 RAS-only refresh cycles
 *
 * Performs mandatory DRAM initialization sequence as specified in all DRAM datasheets.
 * RAS-only refresh (ROR) cycles are required after power-up to:
 *   - Initialize internal refresh counters
 *   - Stabilize sense amplifiers
 *   - Prepare memory array for normal operation
 *
 * Sequence:
 *   1. Configure RAS and CAS pins as outputs
 *   2. Set both signals HIGH (inactive)
 *   3. Perform 8 RAS cycles (pulse RAS LOW then HIGH)
 *   4. Leave RAS and CAS HIGH (inactive)
 *
 * Timing:
 *   - 250μs initial delay allows power supply to stabilize
 *   - RAS pulses use digitalWrite (approximately 4-5μs per transition)
 *   - No minimum delay between RAS pulses required for initialization
 *
 * @param RASPin Arduino pin number connected to RAS signal
 * @param CASPin Arduino pin number connected to CAS signal
 *
 * @note Must be called before any read/write operations
 * @note Called once during setup() for each pin configuration mode
 */
void initRAM(uint8_t RASPin, uint8_t CASPin) {
  delayMicroseconds(100);      // Power stabilization delay
  digitalWrite(RASPin, HIGH);  // RAS inactive (active LOW)
  digitalWrite(CASPin, HIGH);  // CAS inactive (active LOW)
  pinMode(RASPin, OUTPUT);
  pinMode(CASPin, OUTPUT);
  // Perform 8 RAS-only refresh cycles
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(RASPin, LOW);   // Activate RAS
    digitalWrite(RASPin, HIGH);  // Deactivate RAS
  }
}

//=======================================================================================
// OLED DISPLAY FUNCTIONS
//=======================================================================================

/**
 * Display detected RAM type name on OLED screen
 *
 * Shows standard test screen with:
 *   - "RAM-TESTER" header centered at top
 *   - "Detected:" label
 *   - Chip name (from parameter)
 *   - "Checking..." status message
 *
 * Chip name can be either:
 *   - PROGMEM string from ramTypes[].name (e.g., after type detection)
 *   - Custom F() string (e.g., "4164/4532?" during ambiguous detection)
 *
 * Uses page buffer mode (U8G2 suffix _1) to minimize RAM usage.
 * Display updates only when OLED is defined in common.h.
 *
 * @param chipName PROGMEM string (via __FlashStringHelper) with chip designation
 *
 * Example usage:
 *   writeRAMType(F("4164/4532?"));  // During ambiguous detection
 *   writeRAMType((__FlashStringHelper*)ramTypes[type].name);  // After confirmed type
 */
void writeRAMType(const __FlashStringHelper* chipName) {
#ifdef OLED
  OLED_BEGIN()
    display.setFont(M_Font);
    display.setCursor(30, 16);
    display.print(F("RAM-TESTER"));
    display.setCursor(4, 31);
    display.print(F("Detected:"));
    display.setCursor(4, 46);
    display.print(chipName);
    display.setCursor(4, 61);
    display.print(F("Checking..."));
  OLED_END();
#endif
}

//=======================================================================================
// GROUND SHORT DETECTION
//=======================================================================================

/**
 * Check for ground shorts on all pins based on current mode
 *
 * Routes to pin-specific ground short checker based on detected RAM package type.
 * Called during setup() after pullups are enabled but before testing begins.
 *
 * Each pin mode uses different port mappings defined in respective header files:
 *   - Mode_20Pin: Uses CPU_20PORTB/C/D arrays from 20Pin.h
 *   - Mode_18Pin: Uses CPU_18PORTB/C/D arrays from 18Pin.h
 *   - Mode_16Pin: Uses CPU_16PORTB/C/D arrays from 16Pin.h
 *
 * @note Calls error() with code 4 (ground short) if any pin reads LOW with pullup enabled
 * @note Never returns if short detected - enters infinite error blink pattern
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
 * Check for shorts to ground on all ports with pullups enabled
 *
 * Tests each pin across three AVR ports (PORTB, PORTC, PORTD) to ensure
 * no shorts to ground that would prevent proper RAM testing.
 *
 * Test Method:
 *   1. Pullup resistors already enabled on all pins (done in setup())
 *   2. Read each pin's state via PINx register
 *   3. If pin reads LOW with pullup enabled → shorted to ground
 *   4. Report error with physical RAM socket pin number
 *
 * Pin Mapping Arrays:
 *   Each array maps AVR port bit positions [0-7] to RAM socket pins [1-20]
 *   Special values:
 *     EOL (-1): End of list, no more pins to check
 *     NC  (-2): Not connected, skip this bit position
 *
 * Error Handling:
 *   If short detected, calls error(pin_number, 4) which:
 *     - Shows error on OLED (pin number and error type)
 *     - Blinks LED in pattern: 3 RED, n ORANGE (n = pin number)
 *     - Never returns (infinite loop)
 *
 * @param portb Pin mapping array for PORTB (8 elements, bit 0-7 to pin numbers)
 * @param portc Pin mapping array for PORTC (8 elements, bit 0-7 to pin numbers)
 * @param portd Pin mapping array for PORTD (8 elements, bit 0-7 to pin numbers)
 *
 * @note Arrays must be defined in respective header files (16Pin.h, 18Pin.h, 20Pin.h)
 * @note Function never returns if short detected
 */
void checkGNDShort4Port(const uint8_t *portb, const uint8_t *portc, const uint8_t *portd) {
  for (uint8_t i = 0; i <= 7; i++) {
    uint8_t mask = 1 << i;  // Bit mask for current position

    // Check PORTB bit i
    if (portb[i] != EOL && portb[i] != NC && ((PINB & mask) == 0)) {
      error(portb[i], 4);  // Ground short on pin portb[i]
    }

    // Check PORTC bit i
    if (portc[i] != EOL && portc[i] != NC && ((PINC & mask) == 0)) {
      error(portc[i], 4);  // Ground short on pin portc[i]
    }

    // Check PORTD bit i
    if (portd[i] != EOL && portd[i] != NC && ((PIND & mask) == 0)) {
      error(portd[i], 4);  // Ground short on pin portd[i]
    }
  }
}

//=======================================================================================
// LED CONTROL FUNCTIONS
//=======================================================================================

/**
 * Set bicolor LED to specified color
 *
 * Controls two-color LED (red/green) to create four visual states.
 * LED is common cathode type with separate red and green anodes.
 *
 * Color Combinations:
 *   LED_OFF:    Both LEDs off (black)
 *   LED_RED:    Red on, green off (red)
 *   LED_GREEN:  Red off, green on (green)
 *   LED_ORANGE: Both on (appears orange/yellow due to color mixing)
 *
 * Pin Assignments:
 *   LED_RED_PIN (13):   PB5 - Red LED anode
 *   LED_GREEN_PIN (12): PB4 - Green LED anode
 *
 * @param color Desired LED color from LedColor enum
 *
 * @note Orange is created by turning on both red and green simultaneously
 * @note ON/OFF constants defined in common.h (HIGH/LOW or 1/0)
 */
void setLED(LedColor color) {
  switch (color) {
    case LED_OFF:
      CBI(PORTB, 5); CBI(PORTB, 4); break;
    case LED_RED:
      SBI(PORTB, 5); CBI(PORTB, 4); break;
    case LED_GREEN:
      CBI(PORTB, 5); SBI(PORTB, 4); break;
    case LED_ORANGE:
      SBI(PORTB, 5); SBI(PORTB, 4); break;
  }
}

/**
 * Blink LED in specified color with custom timing
 *
 * Creates repeating blink sequence with specified color, count, and timing.
 * Used to build complex error and success patterns.
 *
 * Timing Behavior:
 *   - Each blink: LED on for on_ms, then off for off_ms
 *   - Last blink: LED on for on_ms, then off (no final off_ms delay)
 *   - Caller responsible for inter-group delays
 *
 * @param color LED color to blink (LED_RED, LED_GREEN, LED_ORANGE)
 * @param count Number of blinks to perform (typically 1-7)
 * @param on_ms Duration LED stays on per blink (milliseconds)
 * @param off_ms Duration LED stays off between blinks (milliseconds)
 *
 * Example:
 *   blinkLED_color(LED_RED, 2, 100, 100);  // 2 red blinks, 100ms on/off
 *   delay(500);                             // Pause before next group
 *   blinkLED_color(LED_ORANGE, 5, 100, 100);  // 5 orange blinks
 */
void blinkLED_color(LedColor color, uint8_t count, uint16_t on_ms, uint16_t off_ms) {
  for (uint8_t i = 0; i < count; i++) {
    setLED(color);
    delay(on_ms);
    setLED(LED_OFF);
    if (i < count - 1) {  // Skip delay after last blink
      delay(off_ms);
    }
  }
}

/**
 * Prepare LED pins for indication of test results or errors
 *
 * Resets all I/O pins to safe state and configures LED pins for output.
 * Called before entering error() or testOK() to ensure clean LED operation.
 *
 * Reset Sequence:
 *   1. Re-enable interrupts (sei) - may have been disabled during testing
 *   2. Set all pins LOW to discharge any residual charge
 *   3. Configure all pins as inputs (except VCC pins which remain as-is)
 *   4. Configure LED pins as outputs
 *   5. Turn off both LEDs
 *
 * Port Configuration Details:
 *   PORTB: Clear all except upper 3 bits (preserve Arduino core usage)
 *   PORTC: Clear lower 6 bits, preserve upper 2 (ADC reference)
 *   PORTD: Set 0x1c then clear (handles special pins PD2-PD4)
 *   DDRx:  Configure all as inputs except LED pins
 *
 * @note Must be called before error() or testOK() LED patterns
 * @note Leaves LED pins in OUTPUT mode with both LEDs OFF
 */
void setupLED() {
  sei();  // Re-enable interrupts

  // Clear all pin outputs
  PORTB = 0x00;
  PORTC &= 0xf0;  // Preserve upper nibble
  PORTD = 0x1c;   // Special handling for PORTD

  // Configure all as inputs
  DDRB = 0x00;
  DDRC &= 0xc0;   // Preserve upper 2 bits
  DDRD = 0x00;
  PORTD = 0x00;   // Clear PORTD after DDR change

  // Direct port: PB5=RED, PB4=GREEN as outputs, both off
  DDRB |= ((1 << 5) | (1 << 4));
  PORTB &= ~((1 << 5) | (1 << 4));
}

//=======================================================================================
// ERROR HANDLING
//=======================================================================================

/**
 * Indicate test failure via OLED and LED blink pattern
 *
 * Displays error information on OLED screen and enters infinite LED blink loop.
 * Never returns - user must reset tester to run another test.
 *
 * Error Types and Patterns:
 *
 *   Type 0 - No RAM Detected:
 *     OLED:  "RAM Inserted?" with icon
 *     LED:   Slow red blink (SLOW_BLINK_MS on/off)
 *     Cause: No chip in socket or chip not making contact
 *
 *   Type 1 - Address Line Error:
 *     OLED:  "Row/Col address Line=An" (n = 0-15 for row, 16-31 for col)
 *            or "Decoder Err" if code > 32
 *     LED:   1 RED, n ORANGE (n = code, typically 0-15)
 *     Cause: Address line stuck or disconnected
 *
 *   Type 2 - Pattern Test Failure:
 *     OLED:  "Ram Faulty! Pattern: n"
 *     LED:   2 RED, 1-6 ORANGE (patterns 0-4 = 1-5 orange, random = 6 orange)
 *     Cause: Memory cell failure during pattern testing
 *
 *   Type 3 - Retention Error:
 *     OLED:  "Ram Faulty! Pattern: n"
 *     LED:   2 RED, 7 ORANGE
 *     Cause: Data not retained over refresh interval
 *
 *   Type 4 - Ground Short:
 *     OLED:  "GND Short Pin Nr=n"
 *     LED:   3 RED, n ORANGE (n = physical pin number)
 *     Cause: Socket pin shorted to ground
 *
 * LED Timing Constants (from common.h):
 *   BLINK_ON_MS:     100ms - Standard blink on duration
 *   BLINK_OFF_MS:    100ms - Standard blink off duration
 *   INTER_BLINK_MS:  400ms - Delay between RED and ORANGE groups
 *   ERROR_PAUSE_MS:  1000ms - Delay before repeating pattern
 *   SLOW_BLINK_MS:   500ms - Slow blink for "no RAM" error
 *   FAST_BLINK_MS:   50ms - Fast blink for config error (ConfigFail)
 *
 * @param code Error detail code (interpretation depends on error type)
 * @param error Error type (0-4 as described above)
 * @param row Optional row address where error occurred (for future use)
 * @param col Optional column address where error occurred (for future use)
 *
 * @note Function never returns - enters infinite LED blink loop
 * @note OLED display only updates when OLED is defined in common.h
 * @note After displaying error, calls setupLED() to prepare for blinking
 */
void error(uint8_t code, uint8_t error, int16_t row, int16_t col) {
#ifdef OLED
  switch (error) {
    case 0:  // No RAM detected in socket
      OLED_BEGIN()
        display.setFont(custom_embedded_icons);
        display.setCursor(50, 33);
        display.print(F("G"));  // "No chip" icon
        display.setFont(M_Font);
        display.setCursor(4, 54);
        display.print(F("Defect or no RAM!"));
      OLED_END();
      break;

    case 1:  // Address line error
      OLED_BEGIN()
        display.setFont(custom_check_icons);
        display.setCursor(50, 33);
        display.print(F("B"));
        display.setFont(M_Font);
        display.setCursor(15, 54);
        display.print(F("Addressline A"));
        if (code < 16) {
          display.print(code);  // Row
        } else if (code <= 32) {
          display.print(code >> 4);  // Col
        } else {
          display.print(F("?"));  // Decoder
        }
      OLED_END();
      break;

    case 2:  // RAM faulty (pattern test failure)
    case 3:  // Retention error (merged display with case 2)
      OLED_BEGIN()
        display.setFont(custom_check_icons);
        display.setCursor(50, 33);
        display.print(F("B"));
        display.setFont(M_Font);
        display.setCursor(8, 54);
        display.print(F("Failed Pattern "));
        display.print(code);  // Pattern 0-5
      OLED_END();
      break;

    case 4:  // Ground short detected
      OLED_BEGIN()
        display.setFont(custom_embedded_icons);
        display.setCursor(50, 33);
        display.print(F("C"));
        display.setFont(M_Font);
        display.setCursor(24, 54);
        display.print(F("Short Pin "));
        display.print(code);  // Pin number
      OLED_END();
      break;

    case 5:  // Refresh timeout - chip cannot hold data at max refresh interval
      OLED_BEGIN()
        display.setFont(custom_check_icons);
        display.setCursor(50, 33);
        display.print(F("B"));
        display.setFont(M_Font);
        display.setCursor(15, 54);
        display.print(F("Refresh Timer"));
      OLED_END();
      break;
  }
#endif

  setupLED();  // Prepare LED pins for error indication

  // Unified error blink pattern
  // slowMode: continuous red blink (no RAM detected)
  // Otherwise: redBlinks RED, then orangeCount ORANGE, repeat
  uint8_t redBlinks = 0;
  uint8_t orangeCount = 0;
  bool slowMode = false;

  switch (error) {
    case 0:  // No RAM - slow red blink
      slowMode = true;
      break;
    case 1:  // Address error - 1 red, code orange
      redBlinks = 1;
      orangeCount = (code > 0 && code <= 20) ? code : 0;
      break;
    case 2:  // Pattern error - 2 red, pattern-based orange
      redBlinks = 2;
      orangeCount = (code <= 4) ? code + 1 : 6;
      break;
    case 3:  // Retention error - 2 red, 7 orange
      redBlinks = 2;
      orangeCount = 7;
      break;
    case 4:  // Ground short - 3 red, pin orange
      redBlinks = 3;
      orangeCount = (code > 0 && code <= 20) ? code : 0;
      break;
    case 5:  // Refresh timeout - 2 red, 8 orange
      redBlinks = 2;
      orangeCount = 8;
      break;
  }

  // Unified infinite blink loop
  while (true) {
    if (slowMode) {
      setLED(LED_RED);
      delay(SLOW_BLINK_MS);
      setLED(LED_OFF);
      delay(SLOW_BLINK_MS);
    } else {
      blinkLED_color(LED_RED, redBlinks, BLINK_ON_MS, BLINK_OFF_MS);
      delay(INTER_BLINK_MS);
      if (orangeCount > 0) {
        blinkLED_color(LED_ORANGE, orangeCount, BLINK_ON_MS, BLINK_OFF_MS);
      }
      delay(ERROR_PAUSE_MS);
    }
  }
}

/**
 * Indicate DIP switch configuration error
 *
 * Called when DIP switch settings are invalid (no switches set or multiple switches set).
 * Displays error screen with QR code to documentation and enters infinite fast red blink.
 *
 * Valid DIP switch configurations:
 *   - Exactly one switch HIGH: Mode_16Pin (2), Mode_18Pin (3), or Mode_20Pin (5)
 *
 * Invalid configurations triggering this error:
 *   - Mode 0: No switches set (all LOW)
 *   - Mode 1: Invalid combination
 *   - Mode 4: Invalid combination
 *   - Mode 6: Two or more switches set
 *   - Mode 7: All three switches set
 *
 * LED Pattern:
 *   Fast red blink (FAST_BLINK_MS = 50ms on/off)
 *   Easily distinguished from other error patterns
 *
 * @note Function never returns - enters infinite blink loop
 * @note OLED displays version number and QR code to GitHub documentation
 * @note Called from setup() if Mode validation fails
 */
void ConfigFail() {
  setupLED();  // Prepare LED pins

  // Infinite fast red blink pattern
  while (true) {
    setLED(LED_RED);
    delay(FAST_BLINK_MS);
    setLED(LED_OFF);
    delay(FAST_BLINK_MS);
  }
}

//=======================================================================================
// SUCCESS INDICATION
//=======================================================================================


void printTestOK(String s) {
  int pos = 64 - (s.length()*3.5);
  OLED_BEGIN()
    display.setFont(M_Font);
    display.setCursor(pos, 54);
    display.print(s);
    display.setFont(custom_check_icons);
    display.setCursor(50, 33);
    display.print(F("A"));  // Checkmark icon
  OLED_END();
}

/**
 * Indicate test success via OLED and LED blink pattern
 *
 * Displays success screen on OLED and enters infinite LED blink pattern indicating
 * both test success (GREEN blinks) and specific RAM type (ORANGE blinks).
 * Never returns - user must reset tester to run another test.
 *
 * OLED Display:
 *   - Checkmark icon (custom font character 'A')
 *   - Detected chip name from ramTypes[type].name
 *   - Name centered horizontally based on string length
 *
 * LED Blink Pattern Format:
 *   [GREEN blinks] → INTER_BLINK_MS → [ORANGE blinks] → PATTERN_PAUSE_MS → repeat
 *
 * Pattern Mapping (from ledPatterns[] array):
 *   16-pin chips: 1 green + variable orange
 *   18-pin chips: 2 green + variable orange
 *   20-pin chips: 3 green + variable orange
 *   Adapter chips: 4 green + variable orange
 *
 * Complete Pattern Reference:
 *   1G-1O: 4164    | 1G-2O: 41256   | 1G-3O: 41257   | 1G-4O: 4816
 *   1G-5O: 4532-L  | 1G-6O: 4532-H  | 2G-1O: 4416    | 2G-2O: 4464
 *   2G-3O: 411000  | 3G-1O: 514256  | 3G-2O: 514400  | 3G-3O: 514258
 *   3G-4O: 514402  | 4G-1O: 4116    | 4G-2O: 4027
 *
 * Timing Constants:
 *   BLINK_ON_MS:     100ms - Blink on duration
 *   BLINK_OFF_MS:    100ms - Blink off duration
 *   INTER_BLINK_MS:  400ms - Delay between GREEN and ORANGE groups
 *   PATTERN_PAUSE_MS: 1000ms - Delay before repeating full pattern
 *
 * Error Handling:
 *   If type is invalid (< 0 or > T_4532_H), falls back to steady green blink
 *   (1 second on, 1 second off) to indicate unknown but successful test.
 *
 * @note Function never returns - enters infinite LED blink loop
 * @note Global variable 'type' must be set to valid RamType enum value
 * @note OLED display only updates when OLED is defined in common.h
 * @note Called by test functions after all patterns pass successfully
 */
void testOK() {
#ifdef OLED
  // Center chip name horizontally based on string length
  printTestOK((__FlashStringHelper*)ramTypes[type].name);
#endif

  setupLED();  // Prepare LED pins for success pattern

  // Validate detected type
  if (type < 0 || type > T_4532_H) {
    // Unknown type - fallback to steady green blink
    while (true) {
      setLED(LED_GREEN);
      delay(1000);
      setLED(LED_OFF);
      delay(1000);
    }
  }

  // Main success pattern loop
  while (true) {
    // GREEN blinks indicate TEST PASSED
    blinkLED_color(LED_GREEN, ledPatterns[type].green_blinks, BLINK_ON_MS, BLINK_OFF_MS);
    delay(INTER_BLINK_MS);

    // ORANGE blinks indicate SPECIFIC RAM TYPE
    blinkLED_color(LED_ORANGE, ledPatterns[type].orange_blinks, BLINK_ON_MS, BLINK_OFF_MS);
    delay(PATTERN_PAUSE_MS);
  }
}

void printQRandVersion(String s){
  OLED_BEGIN()
    display.drawXBMP(67, 3, 58, 58, github_QR);
    display.setFont(custom_embedded_icons);
    display.setCursor(18, 34);
    display.print(F("H"));
    display.setFont(M_Font);
    display.setCursor(0, 52);
    display.print(s);
    display.setFont(S_Font);
    display.setCursor(0, 63);
    display.print(F("Version:"));
    display.setCursor(40, 63);
    display.print(version);
  OLED_END();
}


//=======================================================================================
// SELF-TEST / HARDWARE VERIFICATION FUNCTIONS
//=======================================================================================

/**
 * Test Pin Arrays for Self-Check Mode
 *
 * Defines all socket pins to test during hardware self-check.
 * Covers all 20 pins of ZIF socket except GND (pin 10).
 *
 * Pin Assignments:
 *   PORTB (5 pins): PB0-PB4 → Arduino pins 8-12
 *   PORTC (6 pins): PC0-PC5 → Arduino pins 14-19 (A0-A5)
 *   PORTD (8 pins): PD0-PD7 → Arduino pins 0-7
 *
 * Total: 19 pins tested (20 socket pins - 1 GND pin)
 */
const uint8_t TEST_PINS_B[] = { 8, 9, 10, 11, 12 };        // PB0-PB4 (5 pins)
const uint8_t TEST_PINS_C[] = { 14, 15, 16, 17, 18, 19 };  // PC0-PC5 (6 pins)
const uint8_t TEST_PINS_D[] = { 0, 1, 2, 3, 4, 5, 6, 7 };  // PD0-PD7 (8 pins)

/**
 * Pin Check Status Array
 *
 * Boolean array tracking which pins have been tested successfully.
 * Used by testPins() to verify continuity with jumper wire test.
 * check[13] set to true automatically (LED pin, not testable).
 */
// Bitfield for pin continuity check (bit N = pin N tested)
// Saves 16 bytes RAM vs boolean array
uint32_t pinCheckBits = 0;

/**
 * DIP Switch Pulldown Resistor Test Pins
 *
 * Pins with external pulldown resistors for DIP switch operation:
 *   Pin  2 (D2):  16-pin mode switch
 *   Pin  3 (D3):  18-pin mode switch
 *   Pin 19 (A5):  20-pin mode switch
 *
 * These resistors pull pins LOW when switch is OFF (floating).
 * testResistors() verifies resistors are functional.
 */
const uint8_t RESISTOR_TEST_PINS[] = { 2, 3, 19 };

/**
 * Display self-test error message on OLED
 *
 * Shows error screen with:
 *   - Error icon (cross mark)
 *   - "Self Test Fail!" header
 *   - Custom error text (centered)
 *
 * Called by self-test functions when hardware verification fails.
 * Typically followed by infinite loop (caller halts execution).
 *
 * @param text Descriptive error message (e.g., "Resistors", "Shorts ZIF/ZIP")
 */
void selfCheckError(String text) {
  OLED_BEGIN()
    display.setFont(custom_check_icons);
    display.setCursor(50, 33);
    display.print(F("B"));  // Error icon
    display.setFont(M_Font);
    display.setCursor(10, 48);
    display.print(F("Self Test Fail!"));
    int pos = 64 - (text.length()*3.5);  // Center text horizontally
    display.setCursor(pos, 64);
    display.print(text);
  OLED_END();

  // Configure LED pins as OUTPUT for error indication
  DDRB |= ((1 << 5) | (1 << 4));
  setLED(LED_RED);
  while(1);
}

/**
 * Test external pulldown resistors on DIP switch pins
 *
 * Verifies that external pulldown resistors are properly installed and functional.
 * These resistors (typically 10kΩ) pull DIP switch pins LOW when switch is OFF.
 *
 * Test Procedure (for each pin):
 *   1. Configure pin as OUTPUT and drive HIGH
 *   2. Wait 10μs for pin to charge
 *   3. Change to INPUT (no pullup)
 *   4. Wait 500ms for external pulldown to discharge pin
 *   5. Read pin state - should be LOW if resistor present
 *   6. If pin reads HIGH → resistor missing or broken
 *   7. Restore INPUT_PULLUP mode for normal operation
 *
 * Timing Note:
 *   500ms delay accounts for capacitor on 20-pin socket that must discharge
 *   through pulldown resistor before pin reads LOW reliably.
 *
 * @return true if all three resistors passed test, false if any failed
 *
 * @note Does not display error - caller (selfCheck) handles error display
 * @note Tests pins 2, 3, 19 (DIP switch inputs)
 */
bool testResistors() {
  bool allPassed = true;

  for (uint8_t i = 0; i < 3; i++) {
    uint8_t pin = RESISTOR_TEST_PINS[i];

    // Charge pin HIGH
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    delayMicroseconds(10);

    // Switch to INPUT and wait for pulldown to discharge
    pinMode(pin, INPUT);
    delay(500);  // Long delay for capacitor discharge

    // Check if pin pulled LOW by resistor
    bool pinState = digitalRead(pin);
    if (pinState == HIGH) {
      allPassed = false;  // Resistor missing or broken
    }

    // Restore INPUT_PULLUP for normal operation
    pinMode(pin, INPUT_PULLUP);
  }

  return allPassed;
}

/**
 * Configure all test pins with pullups for continuity testing
 *
 * Prepares socket pins for jumper wire continuity test.
 * Pin 19 (A5) driven HIGH to power jumper wire test.
 * All other pins set to INPUT_PULLUP to detect continuity.
 *
 * Test Setup:
 *   1. Pin 19 set HIGH (wire will be connected here)
 *   2. Wait 100ms for pin to stabilize
 *   3. All test pins set to INPUT_PULLUP
 *   4. User bridges pin 20 (socket) to all other pins with jumper
 *   5. testPins() monitors each pin for LOW state (continuity detected)
 *
 * @note Called after automatic tests complete
 * @note User must manually bridge pin 20 to all other pins in sequence
 */
void setupTestPins() {
  // Reset bitfield for fresh test
  pinCheckBits = 0;

  digitalWrite(19, HIGH);  // Power source for jumper test
  delay(100);

  // Enable pullups on all test pins
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

/**
 * Check if all socket pins are reading HIGH with pullups enabled
 *
 * Verifies that no pins are stuck LOW or shorted to ground.
 * Called before continuity test to ensure clean starting state.
 *
 * @return true if all test pins read HIGH, false if any read LOW
 *
 * @note Does not test pin 13 (LED pin, not in test arrays)
 */
bool checkAllPinsHigh() {
  // Check PORTB pins
  for (uint8_t i = 0; i < sizeof TEST_PINS_B; i++) {
    if (digitalRead(TEST_PINS_B[i]) == LOW) {
      return false;
    }
  }

  // Check PORTD pins
  for (uint8_t i = 0; i < sizeof TEST_PINS_D; i++) {
    if (digitalRead(TEST_PINS_D[i]) == LOW) {
      return false;
    }
  }

  // Check PORTC pins
  for (uint8_t i = 0; i < sizeof TEST_PINS_C; i++) {
    if (digitalRead(TEST_PINS_C[i]) == LOW) {
      return false;
    }
  }

  return true;
}

/**
 * Test for shorts between all socket pins
 *
 * Exhaustive pin-to-pin short detection across all 20 socket pins.
 * Drives each pin LOW one at a time and verifies all other pins remain HIGH.
 *
 * Test Algorithm:
 *   For each pin i (0-19, skip 13):
 *     1. Set pin 19 HIGH (ensure power for test)
 *     2. Configure pin i as OUTPUT LOW
 *     3. Wait 200ms for signal to settle
 *     4. For each other pin j (0-19, skip 13 and i):
 *        a. Set pin j as OUTPUT HIGH
 *        b. Change to INPUT_PULLUP
 *        c. Read pin j - should be HIGH (not pulled down by pin i)
 *        d. If LOW → short between pin i and pin j
 *        e. Set pin j back to OUTPUT LOW for next iteration
 *
 * Visual Feedback:
 *   - During test: Red LED on
 *   - Test complete: Green LED on
 *   - Error detected: Calls selfCheckError() and halts
 *
 * @note Pin 13 skipped (used for LED, cannot test reliably)
 * @note Tests 19 × 18 = 342 pin pair combinations
 * @note Function never returns if short detected
 */
void checkShortPins(void) {
  for (int i = 0; i < 20; i++) {
    digitalWrite(19, HIGH);  // Ensure power
    if (i == 13)             // Skip LED pin
      continue;

    // Drive pin i LOW
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
    delay(200);

    // Test all other pins for shorts to pin i
    for (int j = 0; j < 20; j++) {
      if ((i == 13) || (i == j))  // Skip LED pin and self
        continue;

      // Try to drive pin j HIGH
      pinMode(j, OUTPUT);
      digitalWrite(j, HIGH);
      pinMode(j, INPUT_PULLUP);

      // Check if pin i is pulling pin j LOW (short detected)
      if (digitalRead(j) != true) {
        selfCheckError("Shorts ZIF/ZIP");
        while (1);  // Halt - short detected
      }

      // Reset pin j for next test
      pinMode(j, OUTPUT);
      digitalWrite(j, LOW);
    }
  }

  // Visual feedback: test complete
  setLED(LED_RED);
  delay(250);
  setLED(LED_GREEN);
}

/**
 * Interactive continuity test with jumper wire
 *
 * Guides user through manual continuity test of all socket pins.
 * User bridges pin 20 to each other pin in turn with jumper wire.
 * LED provides feedback when each pin is successfully tested.
 *
 * Test Procedure:
 *   1. User connects jumper wire from pin 20 to any untested pin
 *   2. When pin reads LOW (continuity detected):
 *      - Mark pin as checked
 *      - Flash green LED to indicate success
 *   3. Repeat until all pins checked (array check[] all true)
 *   4. Exit when all pins successfully tested
 *
 * Pin-by-Pin Feedback:
 *   On each successful contact:
 *     - Green LED turns off (via pinMode INPUT_PULLUP)
 *     - Red LED flashes briefly (200ms)
 *     - Red LED turns off
 *     - Green LED restored to INPUT_PULLUP
 *
 * @note Pin 13 automatically marked as checked (LED pin, not testable)
 * @note Function blocks until all pins successfully tested
 * @note check[] array must be initialized to false before calling
 */
void testPins() {
  // Mark pin 13 as checked (LED pin, not testable)
  pinCheckBits |= (1UL << 13);

  // Target: all pins 0-19 checked (0xFFFFF = bits 0-19 set)
  const uint32_t allPinsMask = 0xFFFFFUL;

  while (pinCheckBits != allPinsMask) {
    // Check each pin for continuity
    for (uint8_t i = 0; i <= 19; i++) {
      if (i == 13) continue;  // Skip LED pin

      // Skip if already tested
      if (pinCheckBits & (1UL << i)) continue;

      // When jumper bridges pin 20 to pin i, pin i reads LOW
      if (digitalRead(i) == LOW) {
        pinCheckBits |= (1UL << i);  // Mark pin as tested

        // Visual feedback: flash red LED
        pinMode(LED_GREEN_PIN, OUTPUT);
        digitalWrite(LED_GREEN_PIN, LOW);
        digitalWrite(LED_RED_PIN, HIGH);
        delay(200);
        digitalWrite(LED_RED_PIN, LOW);
        pinMode(LED_GREEN_PIN, INPUT_PULLUP);
      }
    }
  }
}

/**
 * Main self-test / hardware verification function
 *
 * Comprehensive hardware test sequence activated when all DIP switches are ON.
 * Tests PCB assembly, socket connections, and pulldown resistors.
 *
 * Test Sequence:
 *   1. Configure LED pins as outputs for visual feedback
 *   2. Display "Self Test" splash screen with version and QR code
 *   3. Police light effect: Alternate red/green LEDs during 2.5s display (10× 250ms cycles)
 *   4. Test external pulldown resistors (testResistors)
 *      - If fail: Display error with RED LED and halt
 *   5. Test for shorts between all socket pins (checkShortPins)
 *      - Exhaustive n×n pin pair testing
 *      - If fail: Display error with RED LED and halt
 *   6. Display instructions for jumper wire test
 *   7. Setup pins for continuity testing (setupTestPins)
 *   8. Run interactive jumper wire test (testPins)
 *      - User must bridge pin 20 to all other pins
 *      - LED feedback on each successful connection
 *   9. Display "Self Test OK" with checkmark icon
 *  10. Turn GREEN LED on and halt (infinite loop)
 *
 * Activation:
 *   Automatically triggered in main program when Mode indicates all switches ON.
 *   Typically: if (Mode == 7) selfCheck();
 *
 * User Instructions (shown on OLED):
 *   - "Jmp. Wire ZIF P20" - Get jumper wire ready for pin 20
 *   - "Bridge P20->all Pin" - Connect pin 20 to each other pin
 *
 * Visual Feedback:
 *   - Police light effect: Alternating red/green during initial 2.5s splash screen
 *   - Green LED steady on during automated tests (resistors, shorts)
 *   - Red LED flashes during short circuit testing
 *   - Green LED blinks when each pin successfully tested in jumper test
 *   - Success: Steady GREEN LED (infinite loop)
 *   - Failure: Steady RED LED (infinite loop)
 *
 * @note Requires OLED to be enabled - no visual feedback without display
 * @note Function halts (infinite loop) if resistor or short test fails
 * @note Returns normally only if all tests pass successfully
 */
void selfCheck(void) {
  // Configure LED pins early for police light effect
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);

  // Splash screen with version info and QR code
  printQRandVersion("Self Test");

  // Police light effect during 2500ms delay (10 cycles × 250ms = 2500ms)
  for (uint8_t i = 0; i < 10; i++) {
    // Alternate between red and green every 250ms
    if (i % 2 == 0) {
      setLED(LED_RED);
    } else {
      setLED(LED_GREEN);
    }
    delay(250);
  }

  setLED(LED_OFF);  // Turn off LED before starting tests
  display.setFont(M_Font);

  setLED(LED_GREEN);  // Green on during auto tests

  // Test 1: Pulldown resistors
  if (!testResistors()) {
    selfCheckError("Resistors");
    while (1);  // Halt on failure
  }
  OLED_BEGIN()
    display.setCursor(22,25);
    display.print(F("Resistors OK"));
    display.setCursor(12,42);
    display.print(F("Checking Shorts"));
  OLED_END();

  // Test 2: Pin-to-pin shorts
  checkShortPins();  // Never returns if short found

  // Test 3: Continuity test with jumper wire
  OLED_BEGIN()
    display.setCursor(12,25);
    display.print(F("No Shorts found"));
    display.setCursor(12,42);
    display.print(F("Wire Pin20->all"));
  OLED_END();

  setupTestPins();
  testPins();

  // Success - all tests passed
  display.firstPage();
  printTestOK("Self Test OK");

  // Reset all pins and configure LEDs for success indication
  setupLED();
  setLED(LED_GREEN);
  while (1);
}
