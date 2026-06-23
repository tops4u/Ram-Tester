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
 *   2 RED, 1/2 ORANGE:  Checkerboard test error (1 = pass 0 up, 2 = pass 1 down)
 *   2 RED, 7 ORANGE:    Random pattern / retention error (patterns 4-5)
 *   2 RED, 8 ORANGE:    CBR refresh-counter fault (loop-mode CBR test)
 *   3 RED, n ORANGE:    Ground short detected (n = pin number)
 *   3 RED, 0 ORANGE:    Address line short (two address lines shorted)
 *   4 RED, n ORANGE:    SRAM (2114) functional fault (n = sub-test 1-7)
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
#include <avr/eeprom.h>
#include <avr/wdt.h>

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
 * Buffer mode is fixed to _2 (256 byte page buffer, 4 passes per full refresh).
 */
#ifdef OLED
U8G2_SSD1306_128X64_NONAME_2_SW_I2C display(U8G2_R0, /*clock=*/13, /*data=*/12, /*reset=*/U8X8_PIN_NONE);
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
const __FlashStringHelper *typeSuffix = NULL;
extern const __FlashStringHelper *g_speedSuffix;  // 2114 speed-class postfix (-20/-30/-45)
uint8_t halfGoodBlink = 0;                        // 0=normal, 1=4532(1G-5O), 2=3732(1G-6O), 3=ambig(5O+6O)

/**
 * DIP Switch Configuration Mode
 *
 * Combines three DIP switch states to determine pin configuration:
 *   Pin 19 (A5) contributes Mode_20Pin (5) if HIGH
 *   Pin  3 (D3) contributes Mode_18Pin (4) if HIGH
 *   Pin  2 (D2) contributes Mode_16Pin (2) if HIGH
 *
 * Valid modes:
 *   Mode_16Pin (2): Tests 16-pin DRAMs (4164, 41256, 41257, 4816, TMS4532, MSM3732)
 *   Mode_18Pin (4): Tests 18-pin DRAMs (4416, 4464, 411000, 2114)
 *   Mode_20Pin (5): Tests 20-pin DRAMs (514256, 514258, 514400, 514402, 4116, 4027)
 *
 * All three switches ON (11) -> EEPROM config page; other values trigger ConfigFail()
 */
uint8_t Mode = 0;

/**
 * Runtime configuration byte (EEPROM-backed, default 0xFF = factory)
 *
 * Initialised to 0xFF so a missing loadConfig() call falls back to factory
 * defaults rather than an arbitrary state.
 */
uint8_t g_config = 0xFF;

//=======================================================================================
// TEST PATTERN DEFINITIONS
//=======================================================================================

// Test data is generated, not table-driven:
//   - Passes 0-1: checkerboard ((row ^ col) & 1), up/down + inverted (per pin module)
//   - Patterns 4-5: pseudo-random from randomTable[] via mix8(), normal + inverted
// (The former constant pattern[] table for 0x00/0xFF/0xAA/0x55 is no longer used.)

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
const char ramType_4164[] PROGMEM = "4164 64Kx1 4ms";
const char ramType_4164_2ms[] PROGMEM = "4164 64Kx1 2ms";
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
const char ramType_2114[] PROGMEM = "2114 1Kx4";

// PROGMEM suffix strings for half-good chip display.
// Shared " 32K" suffix is appended in printTestOK() to save flash.
// Definitive (2 quadrants in a row/col pair):
const char qs_4532_4[] PROGMEM = "4532-4";
const char qs_4532_3[] PROGMEM = "4532-3";
const char qs_3732_H[] PROGMEM = "3732-H";
const char qs_3732_L[] PROGMEM = "3732-L";
// Ambiguous (single quadrant — could be either type):
const char qs_Q1[] PROGMEM = "3732-H/4532-4";
const char qs_Q2[] PROGMEM = "3732-L/4532-4";
const char qs_Q3[] PROGMEM = "3732-H/4532-3";
const char qs_Q4[] PROGMEM = "3732-L/4532-3";

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
 *   - IMPORTANT: the effective aging = these delays PLUS the real execution time
 *     of the writeRow/checkRow loops (the implicit part dominates where delays[5]
 *     is 0). Any speed change in those loops requires re-calibrating this table.
 *
 * Special Cases:
 *   - 4164: Uses 4ms refresh timing (accommodates both 2ms and 4ms variants)
 *   - TMS4532: RAS-split half-good 4164 (7-bit RAS, subtype NL3/NL4)
 *   - MSM3732: CAS-split half-good 4164 (7-bit CAS, subtype -L/-H)
 *   - 41257: Nibble mode requires special handling in test routines
 *   - 514258/514402: Static column mode enables faster column access
 *   - 4116/4027: Tested via 20-pin adapter with voltage conversion
 */
struct RAM_Definition ramTypes[] = {
  // name, retMS, delayRows, rows, cols, flags, delays[6] (×20μs), writeTime (×20μs)
  { ramType_4164, 4, 2, 256, 256, RAM_FLAG_SMALL_TYPE, { 48, 46, 0, 0, 0, 0 }, 96 },
  // 41256 row-0 aging asymmetry (ACCEPTED): row 0's pre-check window contains no
  // interleaved checkRow yet (the delayRows pipeline is still filling), so its
  // effective age differs slightly from steady-state rows; delays[0] compensates
  // approximately. One row of 512 — not worth a special-cased pipeline.
  { ramType_41256, 4, 1, 512, 512, 0, { 94, 1, 1, 1, 1, 1 }, 75 },
  // 41257: nibble retention uses a DEDICATED refresh-split tail
  // (retentionTailNibble_16Pin in 16Pin.cpp), NOT the shared retentionTail. The
  // nibble row cycle is ~4.5 ms (≈ tREF); the old shared pipeline aged each cell
  // ~9 ms (~2.25× tREF) and risked false-failing in-spec parts. A RAS-only refresh
  // of the just-written row now splits that into two ~4.5 ms (≈ tREF) windows.
  // delays[]/writeTime are UNUSED for this type (the check/write runtime IS the
  // aging); delayRows is effectively 1. The values below are kept only as
  // documentation — the nibble tail never reads them.
  { ramType_41257, 4, 1, 512, 512, RAM_FLAG_NIBBLE_MODE, { 0, 0, 0, 0, 0, 0 }, 4 },
  { ramType_4416, 4, 2, 256, 64, RAM_FLAG_SMALL_TYPE, { 52, 52, 16, 16, 16, 16 }, 38 },
  // 4464: retuned for the every-column inline-snapshot tRAS recycle (5.0.5) — the
  // faster row raised the implicit aging share; explicit delays compensate (~4.0 ms).
  { ramType_4464, 4, 1, 256, 256, 0, { 96, 0, 0, 0, 0, 0 }, 94 },
  // 514256 writeTime 30: matches the measured FPM row write (~580-620 us); the old 36
  // (720 us) over-aged the last-row drain rows relative to the pipeline rows.
  { ramType_514256, 8, 4, 512, 512, RAM_FLAG_SMALL_TYPE, { 62, 62, 62, 62, 30, 30 }, 30 },
  { ramType_514258, 8, 4, 512, 512, RAM_FLAG_STATIC_COLUMN | RAM_FLAG_SMALL_TYPE, { 61, 61, 61, 61, 30, 30 }, 31 },
  { ramType_514400, 16, 5, 1024, 1024, 0, { 94, 94, 94, 94, 94, 31 }, 60 },
  { ramType_514402, 16, 5, 1024, 1024, RAM_FLAG_STATIC_COLUMN, { 95, 95, 95, 95, 95, 30 }, 64 },
  // 411000: effective aging lands at ~112% of the 8 ms spec — intentional margin
  // (vintage parts; tuning to exactly 100% would false-fail borderline-good chips).
  { ramType_411000, 8, 1, 1024, 1024, 0, { 239, 0, 0, 0, 0, 0 }, 229 },
  { ramType_4116, 2, 2, 128, 128, 0, { 13, 13, 1, 1, 1, 1 }, 24 },
  { ramType_4816, 2, 2, 128, 128, 0, { 23, 23, 1, 1, 1, 1 }, 24 },
  { ramType_4027, 2, 4, 64, 64, 0, { 16, 16, 16, 16, 0, 0 }, 15 },
  { ramType_4164_2ms, 2, 1, 256, 256, RAM_FLAG_SMALL_TYPE, { 0, 0, 0, 0, 0, 0 }, 37 },  // 4164 with 2ms retention — timing values need calibration
  { ramType_2114, 0, 0, 0, 0, 0, { 0 }, 0 }                                             // 2114 SRAM: only .name used; other fields unused
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
/**
 * Success Blink Pattern Table (packed: high nibble = green, low nibble = orange)
 *
 * Maps each RamType enum to its unique LED pattern.
 *
 * Pattern Organization:
 *   16-pin chips: 1 green + variable orange (1-6)
 *   18-pin chips: 2 green + variable orange (1-3)
 *   20-pin chips: 3 green + variable orange (1-4)
 *   Adapter chips: 4 green + variable orange (1-2)
 *
 * Half-good chips (via halfGoodBlink, not in this table):
 *   1G-5O: TMS4532 | 1G-6O: MSM3732 | 1G-5O+1G-6O: ambiguous
 */
const uint8_t ledPatterns[] PROGMEM = {
  0x11,  // T_4164    - 64Kx1        16-pin
  0x12,  // T_41256   - 256Kx1       16-pin
  0x13,  // T_41257   - 256K-NM      16-pin Nibble Mode
  0x21,  // T_4416    - 16Kx4        18-pin
  0x22,  // T_4464    - 64Kx4        18-pin
  0x31,  // T_514256  - 256Kx4       20-pin
  0x33,  // T_514258  - 256K-SC      20-pin Static Column
  0x32,  // T_514400  - 1Mx4         20-pin
  0x34,  // T_514402  - 1M-SC        20-pin Static Column
  0x23,  // T_411000  - 1Mx1         18-pin
  0x41,  // T_4116    - 16Kx1        20-pin via adapter
  0x14,  // T_4816    - 16Kx1        16-pin
  0x42,  // T_4027    - 4Kx1         20-pin via adapter
  0x11,  // T_4164_2MS - 4164 with 2ms retention (same LED as 4164)
  0x24   // T_2114
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
  // uint8_t counter: the 255+1==0 wrap-around ends the 256-entry loop with no
  // 16-bit compare. randomTable[] therefore holds only 4-bit values (& 0x0F).
  uint8_t i = 0;
  do {
    uint16_t lfsr = 0xACE1u ^ (i * 0x3D);
    for (uint8_t j = 0; j < 8; j++) {
      lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
    }
    randomTable[i] = (lfsr ^ (lfsr >> 8)) & 0x0F;
  } while (++i != 0);
}

/**
 * Invert lower nibble of random data table for pattern 5
 *
 * Flips all 4 bits in each table entry to create inverted pattern.
 * Called once per test cycle to alternate between normal and inverted random data.
 * Ensures complete bit coverage - every cell tested with both 0 and 1 in all bit positions.
 *
 * Operation: randomTable[i] ^= 0x0F
 *   - Entries are already 4-bit (generateRandomTable masks & 0x0F), so the former
 *     & 0x0F before the XOR was redundant -> plain XOR inverts all 4 bits.
 *   - Example: 0x0B (1011) becomes 0x04 (0100)
 *   - uint8_t counter: the 255+1==0 wrap-around ends the 256-entry loop.
 */
void invertRandomTable(void) {
  uint8_t i = 0;
  do {
    randomTable[i] ^= 0x0F;
  } while (++i != 0);
}

/**
 * Shared retention-aging tail for the random patterns 4-5 (see common.h).
 *
 * NOTE: the effective aging of a row is this explicit delay PLUS the real
 * execution time of the following row writes/checks (the implicit part
 * dominates for several types whose delays[5] is 0). The delays[] values in
 * ramTypes[] are therefore calibrated against the actual code timing — any
 * speed change in the writeRow / checkRow paths requires re-calibration.
 */
void retentionTail(uint16_t row, uint16_t last_row, uint8_t patNr,
                   void (*check)(uint16_t row, uint8_t patNr)) {
  uint8_t delRows = ramTypes[type].delayRows;
  if (row == last_row) {
    // Last row: drain all still-pending rows (oldest first). The oldest already
    // accumulated last_row's writeTime implicitly; each later check simulates
    // the missing row-write explicitly.
    for (int8_t x = delRows; x >= 0; x--) {
      if (x < delRows)
        delayMicroseconds(ramTypes[type].writeTime * 20);
      delayMicroseconds(ramTypes[type].delays[5] * 20);
      check(row - x, patNr);
    }
  } else if (row >= delRows) {
    // Normal row: verify the row that has been aging for delayRows row-writes.
    delayMicroseconds(ramTypes[type].delays[5] * 20);
    check(row - delRows, patNr);
  } else {
    // Early rows: nothing pending yet, just age.
    delayMicroseconds(ramTypes[type].delays[row] * 20);
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
  ADMUX = (1 << REFS0);                                               // AVcc reference
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
// ---- Compact direct-port replacements for digitalWrite/pinMode/digitalRead ----
// ATmega328P Arduino pin numbering: D0-D7=PORTD, D8-D13=PORTB, A0-A5=PORTC.
// These replace the Arduino API (and its PROGMEM pin tables, ~445 B total) in the
// non-timing-critical setup/self-test code. Same (pin, value/mode) signatures so
// call sites are a 1:1 swap. NOT interrupt-atomic — fine here (no ISR races these
// ports). mode uses the Arduino constants INPUT(0)/OUTPUT(1)/INPUT_PULLUP(2).
static volatile uint8_t *p_out(uint8_t p) {
  return p < 8 ? &PORTD : (p < 14 ? &PORTB : &PORTC);
}
static volatile uint8_t *p_dir(uint8_t p) {
  return p < 8 ? &DDRD : (p < 14 ? &DDRB : &DDRC);
}
static volatile uint8_t *p_in(uint8_t p) {
  return p < 8 ? &PIND : (p < 14 ? &PINB : &PINC);
}
static uint8_t p_bit(uint8_t p) {
  return p < 8 ? p : (p < 14 ? p - 8 : p - 14);
}

static void pWrite(uint8_t p, uint8_t v) {
  uint8_t m = 1 << p_bit(p);
  if (v) *p_out(p) |= m;
  else *p_out(p) &= ~m;
}
static void pMode(uint8_t p, uint8_t mode) {
  uint8_t m = 1 << p_bit(p);
  if (mode == OUTPUT) {
    *p_dir(p) |= m;
  } else {
    *p_dir(p) &= ~m;                           // input
    if (mode == INPUT_PULLUP) *p_out(p) |= m;  // enable pull-up
    else *p_out(p) &= ~m;                      // plain input (no pull-up)
  }
}
static uint8_t pRead(uint8_t p) {
  return (*p_in(p) & (1 << p_bit(p))) ? 1 : 0;
}

void initRAM(uint8_t RASPin, uint8_t CASPin) {
  // Power stabilization already happened during Arduino boot / setup() — no
  // additional delays needed here. The 8 RAS-only refresh cycles below satisfy
  // the DRAM init requirement.
  pWrite(RASPin, HIGH);  // RAS inactive (active LOW)
  pWrite(CASPin, HIGH);  // CAS inactive (active LOW)
  pMode(RASPin, OUTPUT);
  pMode(CASPin, OUTPUT);
  // Perform 8 RAS-only refresh cycles
  for (uint8_t i = 0; i < 8; i++) {
    pWrite(RASPin, LOW);   // Activate RAS
    pWrite(RASPin, HIGH);  // Deactivate RAS
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
 * Uses page buffer mode (U8G2 suffix _2, 256-byte buffer) to minimize RAM usage.
 * Display updates only when OLED is defined in common.h.
 *
 * @param chipName PROGMEM string (via __FlashStringHelper) with chip designation
 *
 * Example usage:
 *   writeRAMType(F("4164/4532?"));  // During ambiguous detection
 *   writeRAMType((__FlashStringHelper*)ramTypes[type].name);  // After confirmed type
 */
void writeRAMType(const __FlashStringHelper *chipName) {
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

// ---- GND-short signal labels (5.0.6) -------------------------------------------------
// Per pinout group: AVR port bit -> signal-name descriptor, parallel to the CPU_xxPORT[]
// pin-number arrays. On a ground short the OLED shows "GND Short <signal>" (e.g. "GND
// Short RAS", "GND Short A5", "GND Short Dout") instead of a bare socket pin number; the
// LED still blinks 3 red + <pin-number> orange (unchanged). Descriptor 0 = no known
// signal -> falls back to the numeric "Short Pin n". Address/data labels are generated
// ("A"+n, "IO"+n) so only the fixed mnemonics below cost string flash.
// NOTE: derived from the signal maps in 16Pin.h/18Pin.h/20Pin.h crossed with CPU_xxPORT[]
// — verify against the board/adapter schematic before trusting.
#define GL_NONE 0
#define GL_RAS  1
#define GL_CAS  2
#define GL_WE   3
#define GL_OE   4
#define GL_DIN  5
#define GL_DOUT 6
#define GL_CS   7
#define GL_A(n)  (0x10 | (n))  // address line An (0..9)
#define GL_IO(n) (0x20 | (n))  // data line IOn (1..4)

// 16-pin (4164/41256/41257/4816/3732/4532)
static const uint8_t LBL_16B[8] PROGMEM = { GL_A(6), GL_RAS,  GL_A(3),  GL_WE,    0,       0,       0,       0       };
static const uint8_t LBL_16C[8] PROGMEM = { GL_A(8), GL_DIN,  GL_DOUT,  GL_CAS,   GL_A(0), 0,       0,       0       };
static const uint8_t LBL_16D[8] PROGMEM = { GL_A(2), GL_A(1), 0,        0,        0,       0,       GL_A(7), GL_A(5) };
// 18-pin: intentionally NOT labelled. At GND-check time the sub-pinout is unknown
// (4416/4464 vs 411000-Alt vs 2114-SRAM all differ), so a label would be wrong for two of
// the three. 18-pin therefore falls back to the numeric "Short Pin n" via this zero map.
static const uint8_t LBL_NONE8[8] PROGMEM = { 0, 0, 0, 0, 0, 0, 0, 0 };
// 20-pin (514256/514258/514400/514402)
static const uint8_t LBL_20B[8] PROGMEM = { GL_CAS,  GL_RAS,  GL_OE,    GL_WE,    GL_A(8), 0,       0,       0       };
static const uint8_t LBL_20C[8] PROGMEM = { GL_IO(1),GL_IO(2),GL_IO(3), GL_IO(4), GL_A(9), 0,       0,       0       };
static const uint8_t LBL_20D[8] PROGMEM = { GL_A(0), GL_A(1), GL_A(2),  GL_A(3),  GL_A(4), GL_A(5), GL_A(6), GL_A(7) };
// 4116/4027 via adapter (same 20-pin socket, different pinout; no OE/A8/A9; the 4027 uses
// A6/PD6 also as /CS but we label it A6).
static const uint8_t LBL_41B[8] PROGMEM = { GL_CAS,  GL_RAS,  0,        GL_WE,    0,       0,       0,       0       };
static const uint8_t LBL_41C[8] PROGMEM = { GL_DOUT, GL_DIN,  0,        0,        0,       0,       0,       0       };
static const uint8_t LBL_41D[8] PROGMEM = { GL_A(0), GL_A(1), GL_A(2),  GL_A(3),  GL_A(4), GL_A(5), GL_A(6), 0       };

// Rendered signal name for the most recent GND short ("" = unknown -> show pin number).
char g_gndLabel[6] = { 0 };

bool test_4116(void);  // 20Pin.cpp — ADC-based adapter presence check (for label choice)

// Render a descriptor byte into out[] ("" if GL_NONE/unknown).
static void buildGndLabel(uint8_t d, char *out) {
  if ((d & 0xF0) == 0x10) { out[0] = 'A'; out[1] = '0' + (d & 0x0F); out[2] = 0; return; }
  if ((d & 0xF0) == 0x20) { out[0] = 'I'; out[1] = 'O'; out[2] = '0' + (d & 0x0F); out[3] = 0; return; }
  const char *s;
  switch (d) {
    case GL_RAS:  s = PSTR("RAS");  break;
    case GL_CAS:  s = PSTR("CAS");  break;
    case GL_WE:   s = PSTR("WE");   break;
    case GL_OE:   s = PSTR("OE");   break;
    case GL_DIN:  s = PSTR("Din");  break;
    case GL_DOUT: s = PSTR("Dout"); break;
    case GL_CS:   s = PSTR("CS");   break;
    default:      out[0] = 0; return;
  }
  strcpy_P(out, s);
}

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
  if (Mode == Mode_20Pin) {
    // 4116/4027 adapter sits in the 20-pin socket but has a different pinout. Detect it
    // (cheap ADC check) so the labels match, then restore the pull-ups test_4116() dropped.
    bool adapter = test_4116();
    PORTB |= 0b00111111;
    PORTC |= 0b00111111;
    PORTD = 0xFF;
    if (adapter)
      checkGNDShort4Port(CPU_20PORTB, CPU_20PORTC, CPU_20PORTD, LBL_41B, LBL_41C, LBL_41D);
    else
      checkGNDShort4Port(CPU_20PORTB, CPU_20PORTC, CPU_20PORTD, LBL_20B, LBL_20C, LBL_20D);
  } else if (Mode == Mode_18Pin) {
    checkGNDShort4Port(CPU_18PORTB, CPU_18PORTC, CPU_18PORTD, LBL_NONE8, LBL_NONE8, LBL_NONE8);
  } else {
    checkGNDShort4Port(CPU_16PORTB, CPU_16PORTC, CPU_16PORTD, LBL_16B, LBL_16C, LBL_16D);
  }
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
void checkGNDShort4Port(const uint8_t *portb, const uint8_t *portc, const uint8_t *portd,
                        const uint8_t *lblb, const uint8_t *lblc, const uint8_t *lbld) {
  for (uint8_t i = 0; i <= 7; i++) {
    uint8_t mask = 1 << i;  // Bit mask for current position

    // Check PORTB bit i
    if (portb[i] != EOL && portb[i] != NC && ((PINB & mask) == 0)) {
      buildGndLabel(pgm_read_byte(&lblb[i]), g_gndLabel);
      error(portb[i], 4);  // Ground short on pin portb[i]
    }

    // Check PORTC bit i
    if (portc[i] != EOL && portc[i] != NC && ((PINC & mask) == 0)) {
      buildGndLabel(pgm_read_byte(&lblc[i]), g_gndLabel);
      error(portc[i], 4);  // Ground short on pin portc[i]
    }

    // Check PORTD bit i
    if (portd[i] != EOL && portd[i] != NC && ((PIND & mask) == 0)) {
      buildGndLabel(pgm_read_byte(&lbld[i]), g_gndLabel);
      error(portd[i], 4);  // Ground short on pin portd[i]
    }
  }
}

//=======================================================================================
// ADDRESS PIN SHORT DETECTION
//=======================================================================================


void checkAddressShorts(uint8_t mB, uint8_t mC, uint8_t mD) {
  DDRB &= ~mB;
  DDRC &= ~mC;
  DDRD &= ~mD;
  PORTB |= mB;
  PORTC |= mC;
  PORTD |= mD;
  delayMicroseconds(5);
  for (uint8_t p = 0; p < 3; p++) {
    volatile uint8_t *base;
    uint8_t mask;
    if (p == 0) {
      base = &PIND;
      mask = mD;
    } else if (p == 1) {
      base = &PINB;
      mask = mB;
    } else {
      base = &PINC;
      mask = mC;
    }
    for (uint8_t bm = 1; bm; bm <<= 1) {
      if (!(mask & bm)) continue;
      *(base + 1) |= bm;
      *(base + 2) &= ~bm;
      delayMicroseconds(2);
      uint8_t eB = mB, eC = mC, eD = mD;
      if (p == 0) eD &= ~bm;
      else if (p == 1) eB &= ~bm;
      else eC &= ~bm;
      if ((PINB & eB) != eB || (PINC & eC) != eC || (PIND & eD) != eD)
        error(0, 6);
      *(base + 1) &= ~bm;
      *(base + 2) |= bm;
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
      CBI(PORTB, 5);
      CBI(PORTB, 4);
      break;
    case LED_RED:
      SBI(PORTB, 5);
      CBI(PORTB, 4);
      break;
    case LED_GREEN:
      CBI(PORTB, 5);
      SBI(PORTB, 4);
      break;
    case LED_ORANGE:
      SBI(PORTB, 5);
      SBI(PORTB, 4);
      break;
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
  while (count--) {
    setLED(color);
    delay(on_ms);
    setLED(LED_OFF);
    if (count) delay(off_ms);  // skip the off-delay after the last blink
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
  DDRC &= 0xc0;  // Preserve upper 2 bits
  DDRD = 0x00;
  PORTD = 0x00;  // Clear PORTD after DDR change

  // Direct port: PB5=RED, PB4=GREEN as outputs, both off
  DDRB |= ((1 << 5) | (1 << 4));
  PORTB &= ~((1 << 5) | (1 << 4));

  // 4116/4027 adapter (20-pin mode): park RAS/CAS/OE/WE (PB0-3) at a DEFINED HIGH idle
  // via the internal pull-up instead of leaving them floating. NMOS clock inputs must not
  // float — a 4027 otherwise drifts into a partially-selected state and latches/hangs,
  // notably across the post-test result display and the next reset. A pull-up (not a driven
  // output) keeps the level defined while limiting the current to ~0.25 mA, so even a chip
  // pin shorted to GND stays well under the I/O Imax. Harmless for the robust CMOS 514xxx.
  if (Mode == Mode_20Pin) {
    DDRB &= ~0x0F;   // PB0-3 = input (not hard-driven)
    PORTB |= 0x0F;   // internal pull-ups -> RAS/CAS/OE/WE held weakly HIGH (deasserted)
  }
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
 *   Type 2 - Pattern Test Failure (checkerboard passes 0-1):
 *     OLED:  "Error Checkerboard"
 *     LED:   2 RED, 1/2 ORANGE (1 = pass 0 up, 2 = pass 1 down)
 *     Cause: Memory cell / coupling failure during checkerboard testing
 *
 *   Type 3 - Retention / Random Pattern Error (patterns 4-5):
 *     OLED:  "Error RandomData"
 *     LED:   2 RED, 7 ORANGE
 *     Cause: Data not retained over the aging interval, or cell/decoder fault
 *
 *   Type 4 - Ground Short:
 *     OLED:  "Short Pin n"
 *     LED:   3 RED, n ORANGE (n = physical pin number)
 *     Cause: Socket pin shorted to ground
 *
 *   Type 5 - CBR Refresh-Counter Fault (loop-mode CBR test):
 *     OLED:  "CBR Timer fault"
 *     LED:   2 RED, 8 ORANGE
 *     Cause: Chip cannot hold data across its internal refresh interval
 *
 *   Type 6 - Address Line Short (two address lines shorted together):
 *     OLED:  "Short Addressline!"
 *     LED:   3 RED, 0 ORANGE (no orange — distinct from ground short)
 *     Cause: Two address lines shorted to each other
 *
 *   Type 7 - SRAM Functional Fault (2114, chip present but failed its test):
 *     OLED:  "SRAM Error"  (the sub-test number is intentionally not shown)
 *     LED:   4 RED, n ORANGE  (n = sub-test: 1/2 = _CS/_WE control, 3-7 = March C- M1-M5)
 *     Cause: A detected 2114 SRAM failed a control-signal or memory-cell test
 *            (distinct from Type 0 "no RAM", since the chip *was* detected)
 *
 * LED Timing Constants (from common.h):
 *   BLINK_ON_MS:     500ms - Standard blink on duration
 *   BLINK_OFF_MS:    500ms - Standard blink off duration
 *   INTER_BLINK_MS:  300ms - Delay between RED and ORANGE groups
 *   ERROR_PAUSE_MS:  1500ms - Delay before repeating pattern
 *   SLOW_BLINK_MS:   1000ms - Slow blink for "no RAM" error
 *   FAST_BLINK_MS:   200ms - Fast blink for config error (ConfigFail)
 *
 * @param code Error detail code (interpretation depends on error type)
 * @param error Error type (0-7 as described above)
 *
 * @note Function never returns - enters infinite LED blink loop
 * @note OLED display only updates when OLED is defined in common.h
 * @note After displaying error, calls setupLED() to prepare for blinking
 */
#ifdef OLED
// Render a standard error screen: centered icon at (50,33) + message at (x,54),
// optionally followed by a numeric value (val < 0 = none). Shared by error() to
// avoid repeating the firstPage/nextPage block for every error type.
static void errScreen(const uint8_t *icoFont, char ico, uint8_t x,
                      const __FlashStringHelper *msg, int16_t val) {
  OLED_BEGIN()
  display.setFont(icoFont);
  display.setCursor(50, 33);
  display.print(ico);
  display.setFont(M_Font);
  display.setCursor(x, 54);
  display.print(msg);
  if (val >= 0) display.print(val);
  OLED_END();
}
#endif

void error(uint8_t code, uint8_t error) {
#ifdef OLED
  switch (error) {
    case 0:  // No RAM detected in socket
      errScreen(custom_embedded_icons, 'G', 4, F("Defect or no RAM!"), -1);
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
        display.print(code);  // Row address line A0-A15
      } else if (code <= 32) {
        display.print(code - 16);  // Col address line A0-A15
      } else {
        display.print(F("?"));  // Decoder error
      }
      OLED_END();
      break;

    case 6:  // Address line short (two address lines shorted together)
      errScreen(custom_embedded_icons, 'C', 3, F("Short Addressline!"), -1);
      break;

    case 2:  // RAM faulty (pattern test failure)
    case 3:  // Retention error (merged display with case 2)
      // code: 0-1 = checkerboard passes, 4-6 = random patterns (16-pin reports
      // patNr+1). Plain-language text instead of the former "Failed Pattern n".
      if (code <= 1)
        errScreen(custom_check_icons, 'B', 1, F("Error Checkerboard"), -1);
      else
        errScreen(custom_check_icons, 'B', 8, F("Error RandomData"), -1);
      break;

    case 4:  // Ground short detected
      if (g_gndLabel[0]) {
        // Labelled: "GND Short <signal>" centred (M_Font ~7 px/char).
        OLED_BEGIN()
        display.setFont(custom_embedded_icons);
        display.setCursor(50, 33);
        display.print('C');
        display.setFont(M_Font);
        int x = 64 - (int)(10 + strlen(g_gndLabel)) * 7 / 2;
        if (x < 0) x = 0;
        display.setCursor(x, 54);
        display.print(F("GND Short "));
        display.print(g_gndLabel);
        OLED_END();
      } else {
        errScreen(custom_embedded_icons, 'C', 24, F("Short Pin "), code);  // code = pin number
      }
      break;

    case 5:  // Refresh timeout - chip cannot hold data at max refresh interval
      errScreen(custom_check_icons, 'B', 11, F("CBR Timer fault"), -1);
      break;

    case 7:  // SRAM (2114) functional fault on a PRESENT chip. The sub-test (code) is NOT
             // shown on the OLED (a bare number means nothing to the user) — it is still
             // encoded on the LED as the orange count. "SRAM Error" centered: 10*7=70px.
      errScreen(custom_check_icons, 'B', 29, F("SRAM Error"), -1);
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
    case 6:  // Address line short - 3 red, no orange (distinct from ground short)
      redBlinks = 3;
      orangeCount = 0;
      break;
    case 5:  // Refresh timeout - 2 red, 8 orange
      redBlinks = 2;
      orangeCount = 8;
      break;
    case 7:  // SRAM (2114) functional fault - 4 red, sub-test orange
      redBlinks = 4;
      orangeCount = (code > 0 && code <= 20) ? code : 0;
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
 * Valid DIP switch configurations (exactly one switch HIGH):
 *   - Mode_16Pin (2), Mode_18Pin (4), or Mode_20Pin (5)
 *   (All three HIGH = 11 -> enters the EEPROM config page, handled before this.)
 *
 * Invalid configurations triggering this error:
 *   - Mode 0: No switches set (all LOW)
 *   - Mode 6: 16+18 both set (2+4)
 *   - Mode 7: 16+20 both set (2+5)
 *   - Mode 9: 18+20 both set (4+5)
 *   (Values 1/3/8/10 are arithmetically unreachable from the three constants.)
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

// Loop / stress-test run counter (1-based, displayed as "RUN: n").
// Capped at LOOP_RUN_LIMIT to keep the on-screen number 7 digits or less
// (M_Font at x=70..127 holds ~8 chars; 1,000,000 fits comfortably).
// When the cap is reached testOK() stops returning and halts in the success
// blink, so the user can leave the rig running overnight without overflow.
#define LOOP_RUN_LIMIT 1000000UL
// Global (not static): the pin modules read it to gate the ~1-minute CBR refresh-time
// test to loop mode, every 10th run (see refreshTimeTest_* call sites).
uint32_t s_runCount = 1;

void printTestOK(const __FlashStringHelper *s) {
  uint8_t len;
  if (CFG_32K_ACTIVE && typeSuffix) {
    len = strlen_P((PGM_P)typeSuffix) + 4;  // +" 32K"
  } else {
    len = strlen_P((PGM_P)s);
    if (g_speedSuffix) len += strlen_P((PGM_P)g_speedSuffix);  // include 2114 "-20/-30/-45"
  }
  int pos = 64 - (len * 7 / 2);  // Center text (integer math, avoids float lib)
  OLED_BEGIN()
  display.setFont(custom_check_icons);
  if (!CFG_LOOP_ACTIVE)
    display.setCursor(50, 33);
  else
    display.setCursor(25, 33);
  display.print(F("A"));  // Checkmark icon
  display.setFont(M_Font);
  if (CFG_LOOP_ACTIVE) {
    display.setCursor(70, 15);
    display.print(F("Run Nr."));
    display.setCursor(70, 30);
    display.print(s_runCount);
  }
  if (CFG_32K_ACTIVE && typeSuffix) {
    // Half-good chip: inverted bar (white background, black text) for visual emphasis
    display.drawBox(0, 42, 128, 16);  // Full-width white bar
    display.setDrawColor(0);          // Black text on white bar
  }
  display.setCursor(pos, 54);
  if (CFG_32K_ACTIVE && typeSuffix) {
    display.print(typeSuffix);
    display.print(F(" 32K"));
  } else {
    display.print(s);
    if (g_speedSuffix) display.print(g_speedSuffix);  // 2114 speed class (-20/-30/-45)
  }
  display.setDrawColor(1);  // Restore normal draw color
  OLED_END();
}

#ifdef OLED
/**
 * Fast loop-mode optimisation: refresh ONLY the two tile rows holding the RUN
 * counter digits (y=16..31, OLED tile rows 2-3) instead of cycling through all
 * four pages with firstPage/nextPage. Sends ~128 bytes via the (patched) u8g2
 * software-I2C path instead of ~1024 bytes per full page-buffer flush.
 *
 * Combined with the local u8g2 speedup patch (SBI/CBI in src/U8g2/U8x8lib.cpp
 * which lifts SCK from ~22.7 kHz to ~600-800 kHz) this brings the per-iteration
 * counter refresh down to roughly 1-2 ms.
 *
 * Safe because the cross-iteration quadrant guard in 16Pin.cpp locks in
 * typeSuffix / halfGoodBlink / chip name after iter 1 — any new failure
 * aborts the loop via error() before reaching this code path.
 *
 * Buffer layout in OLED_2PAGE mode (256 bytes total) after setBufferCurrTileRow(2):
 *   bytes   0..127  = OLED tile row 2 (16 tile-cols × 8 bytes/tile)
 *   bytes 128..255  = OLED tile row 3
 * Each tile column is 8 bytes; we send cols 8..14 (the counter region).
 */
static void updateRunCounterOLED() {
  OLED_PREP();  // set open-drain idle (INPUT+pull-up); END_TRANSFER restores PB4/PB5 to OUTPUT after
  display.setBufferCurrTileRow(2);
  display.clearBuffer();
  display.setFont(M_Font);
  display.setDrawColor(1);
  display.setCursor(70, 30);  // baseline; glyph fits y=17..30 → within the buffer
  display.print(s_runCount);

  // Push only the affected tiles directly to the OLED via u8x8 — skips the
  // rest of the screen entirely. The patched u8g2 SW-I2C now runs at full speed.
  uint8_t *buf = display.getBufferPtr();
  u8x8_t *x8 = display.getU8x8();
  u8x8_DrawTile(x8, 8, 2, 7, buf + 8 * 8);        //  64 = first buffer row, col 8
  u8x8_DrawTile(x8, 8, 3, 7, buf + 128 + 8 * 8);  // 192 = second buffer row, col 8
}

/**
 * Fast per-second update of the CBR countdown: same 2-tile render as the RUN counter,
 * shows "CBR:<seconds-left>" in the value region (the printTestOK frame stays).
 */
static void updateCBRCountdownOLED(uint8_t sec) {
  OLED_PREP();
  display.setBufferCurrTileRow(2);
  display.clearBuffer();
  display.setFont(M_Font);
  display.setDrawColor(1);
  display.setCursor(70, 30);
  display.print(F("CBR:"));
  display.print(sec);
  uint8_t *buf = display.getBufferPtr();
  u8x8_t *x8 = display.getU8x8();
  u8x8_DrawTile(x8, 8, 2, 7, buf + 8 * 8);
  u8x8_DrawTile(x8, 8, 3, 7, buf + 128 + 8 * 8);
}
#endif

// Shared CBR refresh phase for the loop-mode refresh-COUNTER test (called from the
// checkerboard pass of each CBR-capable pin class via a function pointer). Refreshes the
// array for ~60 s using ONLY the supplied CAS-before-RAS cycle (the chip's internal
// counter picks the rows; external addresses are don't-care, so driving PB4/PB5 for the
// OLED/LED here is harmless). Turns the LED off and shows an "R:<sec>" countdown on the
// OLED (1x/second). millis() bounds it to ~60 s regardless of the per-chip CBR cycle
// time. Interrupts are enabled for millis()/OLED and disabled again on exit, so a caller
// inside a cli() region (the 16/18-pin passes) is restored.
// Prepare the CBR-test screen BEFORE the pass's write phase. The full-screen render
// takes 25-60 ms with ZERO refresh — done here (array not yet written) it is harmless;
// rendered at phase start (as before 5.0.5) it let the freshly written array decay
// far past the 8/16 ms retention spec -> false "CBR Timer fault" on weak chips.
// Each pass_X(0, true) caller invokes this right before the pass.
void cbrScreenPrep(void) {
  setLED(LED_OFF);  // red LED off during the long refresh (else it looks like an error)
#ifdef OLED
  // STANDARD success/loop screen (checkmark + centered chip name); the per-second
  // countdown overlays the value region. testOK() restores the RUN number afterwards.
  printTestOK((const __FlashStringHelper *)ramTypes[type].name);
  updateCBRCountdownOLED(60);
  setLED(LED_OFF);  // printTestOK / update drove PB4/PB5 -> LEDs off again
#endif
}

void cbrRefreshPhase(void (*cbr)()) {
  sei();
  setLED(LED_OFF);  // screen itself was already prepared by cbrScreenPrep (pre-write)
  uint32_t t0 = millis();
  uint32_t tEnd = t0 + 60000UL;
#ifdef OLED
  uint32_t nextTick = t0 + 1000UL;  // division-free 1 Hz threshold (was a 32-bit
  uint8_t sec = 60;                 // divide PER ITERATION -> ~60 us/CBR cadence)
#endif
  for (;;) {
    uint32_t now = millis();
    if ((int32_t)(now - tEnd) >= 0) break;
#ifdef OLED
    if ((int32_t)(now - nextTick) >= 0) {
      nextTick += 1000UL;
      updateCBRCountdownOLED(--sec);
      setLED(LED_OFF);  // the OLED update drove PB4/PB5 -> turn the LEDs back off
      // Catch-up burst: the ~2-3 ms OLED render ran with ZERO refresh (~190 missed
      // 15.625-us refresh slots) — repay them immediately, then resume the cadence.
      for (uint16_t i = 256; i; i--) cbr();
    }
#endif
    cbr();
    delayMicroseconds(8);  // steady cadence ~10-12 us/CBR, inside the 15.625 us budget
  }
  cli();
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
 *   2G-1O: 4416    | 2G-2O: 4464    | 2G-3O: 411000
 *   3G-1O: 514256  | 3G-2O: 514400  | 3G-3O: 514258  | 3G-4O: 514402
 *   4G-1O: 4116    | 4G-2O: 4027
 *   Half-good (halfGoodBlink): 1G-5O: TMS4532 | 1G-6O: MSM3732 | both: ambig
 *
 * Timing Constants:
 *   BLINK_ON_MS:     500ms - Blink on duration
 *   BLINK_OFF_MS:    500ms - Blink off duration
 *   INTER_BLINK_MS:  300ms - Delay between GREEN and ORANGE groups
 *   PATTERN_PAUSE_MS: 2000ms - Delay before repeating full pattern
 *
 * Error Handling:
 *   If type is invalid (< 0 or > T_4164_2MS), falls back to steady green blink
 *   (1 second on, 1 second off) to indicate unknown but successful test.
 *
 * @note Function never returns - enters infinite LED blink loop
 * @note Global variable 'type' must be set to valid RamType enum value
 * @note OLED display only updates when OLED is defined in common.h
 * @note Called by test functions after all patterns pass successfully
 */
void testOK(void) {
#ifdef OLED
  // Display strategy:
  //   - First call (s_runCount == 1): full screen render via printTestOK.
  //   - Subsequent loop iterations: only the counter region needs updating,
  //     so use the fast partial refresh — saves ~75% of OLED I2C time per
  //     iteration. The rest of the screen content is guaranteed unchanged
  //     by the cross-iteration quadrant guard.
  if (s_runCount > 1) {
    updateRunCounterOLED();
  } else {
    printTestOK((const __FlashStringHelper *)ramTypes[type].name);
  }
#endif
  // Loop / stress mode: keep iterating until LOOP_RUN_LIMIT is reached, then
  // fall through to the steady success-blink so the rig halts gracefully with
  // the final RUN counter still on screen. NOTE: deliberately OUTSIDE the
  // #ifdef OLED block — loop mode must work in non-OLED builds too.
  if (CFG_LOOP_ACTIVE && s_runCount < LOOP_RUN_LIMIT) {
    s_runCount++;
    return;
  }

  setupLED();  // Prepare LED pins for success pattern

  // Validate detected type
  if (type < 0 || type > T_4164_2MS) {
    // Unknown type - fallback to steady green blink
    while (true) {
      setLED(LED_GREEN);
      delay(1000);
      setLED(LED_OFF);
      delay(1000);
    }
  }

  uint8_t lp = pgm_read_byte(&ledPatterns[type]);
  uint8_t g1 = lp >> 4;
  uint8_t o1 = lp & 0x0F;
  // halfGoodBlink overrides (runtime): 1=4532(1G-5O), 2=3732(1G-6O), 3=ambig(1G-5O then 1G-6O).
  // halfGoodBlink is only ever set non-zero by the 16-pin quadrant analysis when
  // CFG_32K_ACTIVE was on, so the plain `if (halfGoodBlink)` guard is the runtime gate.
  // (Was previously wrapped in `#ifdef ENABLE_32K`, a macro that is never defined — which
  //  silently dropped these LED codes on every build, so half-good chips blinked as a plain
  //  4164 (1G-1O) even though the OLED showed the 32K suffix.)
  uint8_t o2 = 0;
  if (halfGoodBlink) {
    g1 = 1;
    o1 = (halfGoodBlink & 1) ? 5 : 6;  // 1→5(4532), 2→6(3732), 3→5(ambig 1st)
    if (halfGoodBlink == 3) o2 = 6;    // ambiguous: append 1G-6O as 2nd pattern
  }

  // Main success pattern loop
  while (true) {
    blinkLED_color(LED_GREEN, g1, BLINK_ON_MS, BLINK_OFF_MS);
    delay(INTER_BLINK_MS);
    blinkLED_color(LED_ORANGE, o1, BLINK_ON_MS, BLINK_OFF_MS);
    if (o2) {
      // Ambiguous half-good: play second pattern (1G-6O = could also be 3732)
      delay(PATTERN_PAUSE_MS);
      blinkLED_color(LED_GREEN, 1, BLINK_ON_MS, BLINK_OFF_MS);
      delay(INTER_BLINK_MS);
      blinkLED_color(LED_ORANGE, o2, BLINK_ON_MS, BLINK_OFF_MS);
    }
    delay(PATTERN_PAUSE_MS);
  }
}

// Render compressed QR code: each set bit in github_QR_packed[] becomes a 2x2 box
static void drawQR(uint8_t ox, uint8_t oy) {
  for (uint8_t r = 0; r < 29; r++) {
    for (uint8_t c = 0; c < 29; c++) {
      uint8_t b = pgm_read_byte(&github_QR_packed[r * 4 + (c >> 3)]);
      if (b & (1 << (c & 7)))
        display.drawBox(ox + (c << 1), oy + (r << 1), 2, 2);
    }
  }
}

void printQRandVersion(const __FlashStringHelper *s) {
  OLED_BEGIN()
  drawQR(67, 3);
  display.setFont(custom_embedded_icons);
  display.setCursor(18, 34);
  display.print(F("H"));
  display.setFont(M_Font);
  // Center the splash text in the left half of the screen (x=0..63),
  // i.e. between the left edge and the QR code (starts at x=67).
  // M_Font width = 7 px per character. Negative results clamped to 0.
  int txtPos = (64 - (int)strlen_P((PGM_P)s) * 7) / 2;
  if (txtPos < 0) txtPos = 0;
  display.setCursor(txtPos, 52);
  display.print(s);
  display.setFont(S_Font);
  if (CFG_32K_ACTIVE) {
    // 32K Check enable: inverted bar (white background, black text) for Version to indicate Flag
    display.drawBox(0, 56, 64, 8);  // Half-width white bar
    display.setDrawColor(0);        // Black text on white bar
    display.setCursor(0, 63);
  } else {
    display.setCursor(7, 63);
  }
  display.print(F("Ver.:"));
  display.print(F(VERSION_STR));
  if (CFG_32K_ACTIVE) {
    display.print(F(" 32"));
    display.setDrawColor(1);
  }
  OLED_END();
}


//=======================================================================================
// RUNTIME CONFIGURATION (EEPROM-backed)
//=======================================================================================

/**
 * Load runtime configuration byte from EEPROM address 0
 *
 * Fresh / unprogrammed EEPROM reads 0xFF — this yields the factory defaults
 * (32K=ON, Loop=OFF, Dim=OFF) by virtue of the bit polarities defined in
 * common.h. No magic byte is required.
 */
void loadConfig(void) {
  g_config = eeprom_read_byte((const uint8_t *)0);
}

#ifdef OLED
/**
 * Read the three DIP switches as a packed 3-bit value
 *
 * @return bit 0 = DIP1 (A5/pin 19), bit 1 = DIP2 (D3), bit 2 = DIP3 (D2)
 */
static uint8_t readDIPs(void) {
  uint8_t r = 0;                     // direct port reads (pins 19=A5/PC5, 3=PD3, 2=PD2)
  if (PINC & (1 << PC5)) r |= 0x01;  // A5 -> DIP1
  if (PIND & (1 << PD3)) r |= 0x02;  // D3 -> DIP2
  if (PIND & (1 << PD2)) r |= 0x04;  // D2 -> DIP3
  return r;
}

/**
 * Render one row of the config page
 *
 * Prints "DIPx: label state" with the ON/OFF text right-padded to a fixed
 * column position. Caller has already selected M_Font and set draw colour.
 *
 * @param y       Baseline Y coordinate
 * @param dipNum  '1', '2' or '3'
 * @param label   Setting label (PROGMEM string, max ~10 chars after "DIPx: ")
 * @param on      true → print "ON", false → print "OFF"
 */
static void renderCfgLine(uint8_t y, char dipNum, const __FlashStringHelper *label, bool on) {
  display.setCursor(0, y);
  display.print(F("DIP"));
  display.print(dipNum);
  display.print(F(": "));
  display.print(label);
  display.setCursor(7 * 15, y);  // fixed column for state (px 112)
  display.print(on ? F("ON") : F("OFF"));
}

/**
 * Render the full config page reflecting the CURRENT g_config in RAM
 *
 * Shows the *settings* (what is stored in EEPROM / mirrored in g_config),
 * not the raw DIP positions. The DIPs act as toggle inputs (edge-triggered).
 *
 * Layout (M_Font 7x14, 18 chars per 128 px line):
 *   y=14: DIP1: 32K Ena.    ON/OFF
 *   y=28: DIP2: Loop        ON/OFF
 *   y=42: DIP3: Disp.Dim    ON/OFF
 *   y=62: footer hint
 */
static void renderConfigPage(void) {
  OLED_BEGIN()
  display.setFont(M_Font);
  display.setDrawColor(1);
  renderCfgLine(14, '1', F("32K Ena."), CFG_32K_ACTIVE);
  renderCfgLine(28, '2', F("Loop"), CFG_LOOP_ACTIVE);
  renderCfgLine(42, '3', F("Disp.Dim"), CFG_DIM_ACTIVE);
  // Centered footer: "Toggle -> RESET" is 15 chars × 7 px = 105 px → (128-105)/2 = 11
  display.setCursor(11, 62);
  display.print(F("Toggle -> RESET"));
  OLED_END();
}
#endif  // OLED

/**
 * Enter the EEPROM configuration page (never returns)
 *
 * Triggered when all three DIPs are HIGH at boot (Mode == 11). The DIPs act
 * as edge-triggered toggles for the three settings stored in EEPROM:
 *
 *   DIP1 edge -> toggle 32K_ENABLED   bit in g_config
 *   DIP2 edge -> toggle LOOP_DISABLED bit in g_config (inverted: Loop ON = bit clear)
 *   DIP3 edge -> toggle DIM_DISABLED  bit in g_config (inverted: Dim  ON = bit clear)
 *
 * Behaviour:
 *   1. loadConfig() has already mirrored the EEPROM byte into g_config.
 *   2. The entry DIP state (all ON, by trigger definition) is remembered as
 *      the reference state.
 *   3. The OLED displays the *current settings* (not DIP positions).
 *   4. On every change of any DIP bit versus the reference state, the
 *      corresponding setting is toggled, written to EEPROM, the display is
 *      refreshed and the reference state is updated to the new DIP state.
 *      A toggle is therefore edge-triggered: moving a DIP off-and-back-on
 *      toggles the setting twice (net no change).
 *   5. Display brightness is updated live whenever the dim setting changes.
 *   6. Exit only via hardware RESET — the function loops forever.
 */
void enterConfigMode(void) {
#ifdef OLED
  // ---------- ENTRY SPLASH (same pattern as selfCheck) ----------
  // QR code + "Setup" + version string, accompanied by a 2.5 s
  // police-light effect so the user can confirm they intentionally
  // entered configuration mode before the live settings page appears.
  pMode(LED_RED_PIN, OUTPUT);
  pMode(LED_GREEN_PIN, OUTPUT);
  printQRandVersion(F("Setup"));
  for (uint8_t i = 0; i < 10; i++) {
    setLED((i & 1) ? LED_GREEN : LED_RED);
    delay(250);
  }
  setLED(LED_OFF);

  // g_config has already been initialised by loadConfig() in setup().
  // Reference = current DIP state at the moment we leave the splash.
  // (Not necessarily "all on" any more — the user may have already
  // started flipping switches during the splash delay.)
  uint8_t refDIPs = readDIPs();

  // Initial render reflects the stored settings (not the DIPs).
  renderConfigPage();

  while (true) {
    delay(50);  // simple debounce + idle pacing
    uint8_t dips = readDIPs();
    uint8_t changed = dips ^ refDIPs;
    if (!changed) continue;

    // Toggle each setting whose DIP bit changed since the last reference.
    if (changed & 0x01) g_config ^= CFG_32K_ENABLED;    // DIP1 -> 32K
    if (changed & 0x02) g_config ^= CFG_LOOP_DISABLED;  // DIP2 -> Loop
    if (changed & 0x04) {
      g_config ^= CFG_DIM_DISABLED;  // DIP3 -> Dim
      // Apply new brightness immediately for visual feedback.
      OLED_PREP();  // setContrast is an I2C transfer; ensure SDA/SCK are driven
      display.setContrast(CFG_DIM_ACTIVE ? 64 : 255);
    }

    // Persist new state and adopt the current DIP positions as new reference.
    eeprom_update_byte((uint8_t *)0, g_config);
    refDIPs = dips;
    renderConfigPage();
  }
#else
  // No OLED -> just halt; settings cannot be configured without display.
  setupLED();
  while (true) {
    setLED(LED_RED);
    delay(500);
    setLED(LED_OFF);
    delay(500);
  }
#endif
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
// The former TEST_PINS_B/C/D arrays were removed: their union is simply socket
// pins 0-19 except 13 (LED), which setupTestPins now iterates directly.

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
void selfCheckError(const __FlashStringHelper *text) {
  OLED_BEGIN()
  display.setFont(custom_check_icons);
  display.setCursor(50, 33);
  display.print(F("B"));  // Error icon
  display.setFont(M_Font);
  display.setCursor(10, 48);
  display.print(F("Self Test Fail!"));
  int pos = 64 - (strlen_P((PGM_P)text) * 7 / 2);  // Center text (integer math)
  display.setCursor(pos, 64);
  display.print(text);
  OLED_END();

  // Configure LED pins as OUTPUT for error indication
  DDRB |= ((1 << 5) | (1 << 4));
  setLED(LED_RED);
  while (1)
    ;
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
    pMode(pin, OUTPUT);
    pWrite(pin, HIGH);
    delayMicroseconds(10);

    // Switch to INPUT and wait for pulldown to discharge
    pMode(pin, INPUT);
    delay(500);  // Long delay for capacitor discharge

    // Check if pin pulled LOW by resistor
    bool pinState = pRead(pin);
    if (pinState == HIGH) {
      allPassed = false;  // Resistor missing or broken
    }

    // Restore INPUT_PULLUP for normal operation
    pMode(pin, INPUT_PULLUP);
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

  pWrite(19, HIGH);  // Power source for jumper test
  delay(100);

  // Enable pullups on all socket pins except 13 (LED) — the union of the former
  // per-port pin arrays is just 0-19 without 13.
  for (uint8_t i = 0; i < 20; i++) {
    if (i != 13) pMode(i, INPUT_PULLUP);
  }
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
    pWrite(19, HIGH);  // Ensure power
    if (i == 13)       // Skip LED pin
      continue;

    // Drive pin i LOW
    pMode(i, OUTPUT);
    pWrite(i, LOW);
    delay(200);

    // Test all other pins for shorts to pin i
    for (int j = 0; j < 20; j++) {
      if ((i == 13) || (i == j))  // Skip LED pin and self
        continue;

      // Try to drive pin j HIGH
      pMode(j, OUTPUT);
      pWrite(j, HIGH);
      pMode(j, INPUT_PULLUP);

      // Check if pin i is pulling pin j LOW (short detected)
      if (pRead(j) != true) {
        selfCheckError(F("Shorts ZIF/ZIP"));
        while (1)
          ;  // Halt - short detected
      }

      // Reset pin j for next test
      pMode(j, OUTPUT);
      pWrite(j, LOW);
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
 * LED Feedback:
 *   - While testing (not all pins done): steady ORANGE
 *     (green lit by the PB4 pull-up + red driven on).
 *   - On each successful contact: brief GREEN blink (red drops out for 200 ms).
 *   - When all pins are done: steady green (set by selfCheck after this returns).
 *
 * @note Pin 13 automatically marked as checked (LED pin, not testable)
 * @note Function blocks until all pins successfully tested
 * @note check[] array must be initialized to false before calling
 */
void testPins() {
  // Mark pin 13 as checked (LED pin, not testable)
  pinCheckBits |= (1UL << 13);

  // "Busy / not all pins done" indicator = ORANGE. The green LED is already lit by the
  // PB4 pull-up (the LED is FET-driven, so the pull-up fully switches it on) while PB4
  // stays INPUT_PULLUP and therefore still readable for the pin-12 continuity check. We
  // only add RED (PB5, the test-excluded pin) on top to make the colour orange.
  pMode(LED_RED_PIN, OUTPUT);
  pWrite(LED_RED_PIN, HIGH);

  // Target: all pins 0-19 checked (0xFFFFF = bits 0-19 set)
  const uint32_t allPinsMask = 0xFFFFFUL;

  while (pinCheckBits != allPinsMask) {
    // Check each pin for continuity
    for (uint8_t i = 0; i <= 19; i++) {
      if (i == 13) continue;  // Skip LED pin

      // Skip if already tested
      if (pinCheckBits & (1UL << i)) continue;

      // When jumper bridges pin 20 to pin i, pin i reads LOW
      if (pRead(i) == LOW) {
        pinCheckBits |= (1UL << i);  // Mark pin as tested

        // Visual feedback: GREEN blink (orange -> green -> orange). Green stays lit via
        // the PB4 pull-up the whole time; we only drop RED briefly to leave pure green,
        // then restore RED to return to the orange "still testing" state. PB4 is never
        // driven as an output, so pin 12 stays readable for its own continuity check.
        pWrite(LED_RED_PIN, LOW);   // red off -> pure green flash
        delay(200);
        pWrite(LED_RED_PIN, HIGH);  // red on again -> orange
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
// Two-line status screen (lines at y=25 and y=42), assumes M_Font already set.
// Shared by selfCheck's progress screens to avoid repeating the page-render block.
static void twoLineScreen(uint8_t x1, const __FlashStringHelper *l1,
                          uint8_t x2, const __FlashStringHelper *l2) {
  OLED_BEGIN()
  display.setCursor(x1, 25);
  display.print(l1);
  display.setCursor(x2, 42);
  display.print(l2);
  OLED_END();
}

void selfCheck(void) {
  // Configure LED pins early for police light effect
  pMode(LED_RED_PIN, OUTPUT);
  pMode(LED_GREEN_PIN, OUTPUT);

  // Splash screen with version info and QR code
  printQRandVersion(F("Self Test"));

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
    selfCheckError(F("Resistors"));
    while (1)
      ;  // Halt on failure
  }
  twoLineScreen(22, F("Resistors OK"), 12, F("Checking Shorts"));

  // Test 2: Pin-to-pin shorts
  checkShortPins();  // Never returns if short found

  // Test 3: Continuity test with jumper wire
  twoLineScreen(12, F("No Shorts found"), 12, F("Wire Pin20->all"));

  setupTestPins();
  testPins();

  // Success - all tests passed
  display.firstPage();
  printTestOK(F("Self Test OK"));

  // Reset all pins and configure LEDs for success indication
  setupLED();
  setLED(LED_GREEN);
  while (1)
    ;
}
