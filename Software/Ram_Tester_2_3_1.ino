// RAM Tester Program for RAM Tester PCB
// ======================================
//
// Author: Andreas Hoffmann
// Version: 2.3.1.fix
// Date: 07.06.2025
//
// This software is published under GPL 3.0. Respect the license terms.
// Project hosted at: https://github.com/tops4u/Ram-Tester/
//
// Note: The code contains duplication and is not designed for elegance or efficiency.
// The goal was to make it work quickly.
//
// Error LED Codes:
// - Long Green - Long Red - Steady Green : Test mode active
// - Continuous Red Blinking: Configuration error (e.g., DIP switches). Can also occur due to RAM defects.
// - 1 Red & n Green: Address decoder error. Green flashes indicate the failing address line (no green flash for A0).
// - 2 Red & n Green: RAM test error. Green flashes indicate which test pattern failed.
// - 3 Red & n Green: Row crosstalk or data retention (refresh) error. Green flashes indicate the failed test pattern.
// - 4 Red & n Green: Ground short detected on a pin. Green flashes indicate the pin number (of the ZIF Socket != ZIP).
// - Long Green/Short Red: Test passed for a smaller DRAM size in the current configuration.
// - Long Green/Short Off: Test passed for a larger DRAM size in the current configuration.
// - Long Green/Short Orange/Short Red: Test passed for a smaller DRAM size in the current configuration and it is a Static Column Type
//
// Assumptions:
// - The DRAM supports Page Mode for reading and writing.
// - DRAMs with a 4-bit data bus are tested column by column using these patterns: `0b0000`, `0b1111`, `0b1010`, and `0b0101`.
// - The program does not test RAM speed (access times) but Data retention Times
// - This Software does not test voltage levels of the output signals
// - This Software checks address decoding and crosstalk by using on run with pseudo randomized data.
//
// Version History:
// - 1.0: Initial implementation for 20-pin DIP/ZIP, supporting 256x4 DRAM (e.g., MSM514256C).
// - 1.1: Added auto-detection for 1M or 256k x4 DRAM.
// - 1.2: Support for 256kx1 DRAM (e.g., 41256).
// - 1.21: Added column address line checks for 41256/4164, ensuring all address lines, buffers, and column decoders work.
// - 1.22: Added checks for 4164/41256 DRAMs.
// - 1.23: Added row address checking for 4164/41256, complementing column checks from version 1.21.
// - 1.3: Full row and column tests for pins, buffers, and decoders on 514256 and 441000 DRAMs.
// - 1.4: Support for 4416/4464 added. Only 4416 tested as 4464 test chips were unavailable.
// - 2.0pre1: Introduced row crosstalk and refresh time checks (2ms for 4164, 4ms for 41256, 8ms for 20-pin DRAM types).
// - 2.0pre: Refresh tests for 4416/4464 not yet included. Enabled ground short tests and cleaned 20-pin code section.
// - 2.0: Fixed bugs for 4464 and adjusted refresh timing. To-do:
//         - Handle corner cases during crosstalk tests.
//         - Consider reverse-order testing (start with the last row).
// - 2.1: Added a test mode for installation checks after soldering. Test mode instructions available on GitHub.
//         To exit test mode: set all DIP switches to ON, reset, set DIP switches to OFF, and reset again.
// - 2.1.1: Fixed minor bugs in test patterns and I/O configuration for 18-pin RAM.
// - 2.1.2: Bug fix for 18Pin Verions (error in address line to physical port decoding)
// - 2.2.0a:Adding Static Column Tests for 514258
// - 2.2.0b:Added Random Bit Test for 20pin Types - this slightly prolonges Testing / Timing adjustment for FP-RAM pending
// - 2.3.0a:Major rework on Retention Testing. Introduced RAM Types for timing. Added OLED support.
//          Speed optimization in the Code to keep longer test times of pseudo random data at bay.
//          Minor Bugfix 16Bit had one col/row overrun - buggy but no negativ side effects
//          OLED Tests and final implementation missing
// - 2.3.0.pre: All functional targets for 2.3.0 are met. OLED Tested and working
// - 2.3.0.pre2: Checked and updated all Retention Timings. Fixed some minor Typos. Added 514402 Static Column 1Mx4 RAM
// - 2.3.0 : Option to deactivate OLED in Code (remove the line: #define OLED)
//          Code Version output in DIP Switch Error Screen.
// - 2.3.1 : Improved Address & Decoder Checks
//           RAM Inserted? Display
// - 2.3.1.fix Timing issue with 20Pin address tests
//
// Disclaimer:
// This project is for hobbyist use. There are no guarantees regarding its fitness for a specific purpose
// or its error-free operation. Use it at your own risk.

#include <EEPROM.h>
#include <avr/pgmspace.h>
#include <Arduino.h>

// ---- REMOVE THE FOLLOWING LINE TO REMOVE DISPLAY SUPPORT AND SPEED UP THE TESTER ------
#define OLED
// ---------------------------------------------------------------------------------------

#define version "2.3.1"

#ifdef OLED
#include <U8x8lib.h>
// You may use other OLED Displays if the are 128x64 or bigger or you adapt the code
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/13, /* data=*/12, /* reset=*/U8X8_PIN_NONE);  // SD1306 Tested, SD1315 should be compatible
#endif

// An additional delay of 62.5ns may be required for compatibility. (16MHz clock = 1 cycle = 62.5ns).
#define NOP __asm__ __volatile__("nop\n\t")

#define Mode_16Pin 2
#define Mode_18Pin 4
#define Mode_20Pin 5
#define EOL 254
#define NC 255
#define ON HIGH
#define OFF LOW
#define TESTING 0x00
#define LED_FLAG 0x01

struct RAM_Definition {
  char name[17];         // Name for the Display
  uint8_t mSRetention;   // This Testers assumptions of the Retention Time in ms
  uint8_t delayRows;     // How many rows are skipped before reading back and checking retention time
  uint16_t rows;         // How many rows does this type have
  uint16_t columns;      // How many columns does this type have
  uint8_t iOBits;        // How many Bits can this type I/O at the same time
  boolean staticColumn;  // Is this a Static Column Type
  boolean nibbleMode;    // Is this a nibble Mode Type --> Not yet tested
  boolean smallType;     // Is this the small type for this amount of pins
  uint16_t delays[6];    // List of specific delay times for retention testing
  uint16_t writeTime;    // Write Time to check last rows during retention testing
};

// The following RAM Types are currently identified
struct RAM_Definition ramTypes[] = {
  { "   4164 64Kx1   ", 2, 1, 256, 256, 1, false, false, true, { 1000, 0, 0, 0, 0, 0 }, 885 },
  { "  41256 256Kx1  ", 4, 1, 512, 512, 1, false, false, false, { 2104, 0, 0, 0, 0, 0 }, 1765 },  // There is a slight overshoot in retention testing (+2.5% of min time)
  { "41257  256Kx1-NM", 4, 2, 512, 512, 1, false, true, false, { 2104, 0, 0, 0, 0, 0 }, 1765 },   // Never tested yet so we treat it as 41256
  { "   4416  16Kx4  ", 4, 4, 256, 64, 4, false, false, true, { 542, 542, 548, 543, 113, 113 }, 455 },
  { "   4464  64Kx4  ", 4, 1, 256, 256, 4, false, false, false, { 2274, 603, 603, 603, 603, 603 }, 1700 },
  { " 514256  256Kx4 ", 4, 2, 512, 512, 4, false, false, true, { 1225, 1225, 295, 295, 295, 295 }, 757 },
  { "514258 256Kx4-SC", 4, 2, 512, 512, 4, true, false, true, { 1229, 1223, 425, 425, 425, 425 }, 757 },
  { "  514400  1Mx4  ", 16, 4, 1024, 1024, 4, false, false, false, { 2486, 2485, 2485, 2485, 641, 641 }, 1480 },
  { " 514402 1Mx4-SC ", 16, 5, 1024, 1024, 4, true, false, false, { 1687, 1687, 1687, 1591, 1782, 94 }, 1500 }
};

// Type Numbers for the RAM Definitions
#define T_4164 0
#define T_41256 1
#define T_41257 2  // Not tested yet. Should work in "normal" Fast Page Mode. Nibble Mode may be added in a later version
#define T_4416 3
#define T_4464 4
#define T_514256 5
#define T_514258 6
#define T_514400 7
#define T_514402 8

// The Testpatterns
const uint8_t pattern[] = { 0x00, 0xff, 0xaa, 0x55, 0xaa, 0x55 };  // Equals to 0b00000000, 0b11111111, 0b10101010, 0b01010101

// Randomize the access to the pseudo random table by using col & row
static inline __attribute__((always_inline)) uint8_t mix8(uint16_t col, uint16_t row) {
  /* one MUL instead of two */
  uint16_t v = col ^ (row * 251u);  // 251 = 0xFB

  /* fold high and low byte → eight useful bits */
  return (uint8_t)(v ^ (v >> 8));
}

// Mapping for 4164 (2ms Refresh Rate) / 41256/257 (4 ms Refresh Rate)
// A0 = PC4   RAS = PB1   t RAS->CAS = 150-200ns -> Max Pulsewidth 10'000ns
// A1 = PD1   CAS = PC3   t CAS->dOut= 75 -100ns -> Max Pulsewidth 10'000ns
// A2 = PD0   WE  = PB3
// A3 = PB2   Din = PC1
// A4 = PB4   Dout= PC2
// A5 = PD7
// A6 = PB0
// A7 = PD6
// A8 = PC0 (only 41256/257 Type, on 4164 this Pin is NC)
// The following are the Pin Mappings from Ports -> DIP Pinout for DIP 16 Candidates.
const int CPU_16PORTB[] = { 13, 4, 12, 3, EOL, EOL, EOL, EOL };  // Position 4 would be A8 but the LED is attached to PB4 as well
const int CPU_16PORTC[] = { 1, 2, 14, 15, 5, EOL, EOL, EOL };
const int CPU_16PORTD[] = { 6, 7, 8, NC, NC, NC, 9, 10 };
const int RAS_16PIN = 9;   // Digital Out 9 on Arduino Uno is used for RAS
const int CAS_16PIN = 17;  // Corresponds to Analog 3 or Digital 17
// Address Distribution for 16Pin Types. HW Circuitry is optimized for larger Models where Testing Speed is more relevant
#define CAS_LOW16 PORTC &= 0xf7
#define CAS_HIGH16 PORTC |= 0x08
#define RAS_LOW16 PORTB &= 0xfd
#define RAS_HIGH16 PORTB |= 0x02
#define WE_LOW16 PORTB &= 0xf7
#define WE_HIGH16 PORTB |= 0x08
// This is the older implementation and is just used for RAS address setting, that does not need the DATA Pin
#define SET_ADDR_PIN16(addr) \
  { \
    PORTB = ((PORTB & 0xea) | (addr & 0x0010) | ((addr & 0x0008) >> 1) | ((addr & 0x0040) >> 6)); \
    PORTC = ((PORTC & 0xe8) | ((addr & 0x0001) << 4) | ((addr & 0x0100) >> 8)); \
    PORTD = ((addr & 0x0080) >> 1) | ((addr & 0x0020) << 2) | ((addr & 0x0004) >> 2) | (addr & 0x0002); \
  }

// ─────────────────────────────────────────────────────────────────────────────
// Build the three 512-entry lookup tables directly in flash (PROGMEM)
// ─────────────────────────────────────────────────────────────────────────────
// Lookup Table generation Macros.
#define MAP_B(a) ((((a)&0x0010)) | (((a)&0x0008) >> 1) | (((a)&0x0040) >> 6))
#define MAP_C(a) ((((a)&0x0001) << 4) | (((a)&0x0100) >> 8)) /* PC1=Databit later */
#define MAP_D(a) ((((a)&0x0080) >> 1) | (((a)&0x0020) << 2) | (((a)&0x0004) >> 2) | ((a)&0x0002))

#define ROW16(f, base) \
  f(base + 0), f(base + 1), f(base + 2), f(base + 3), \
    f(base + 4), f(base + 5), f(base + 6), f(base + 7), \
    f(base + 8), f(base + 9), f(base + 10), f(base + 11), \
    f(base + 12), f(base + 13), f(base + 14), f(base + 15)

#define ROW256(f, base) \
  ROW16(f, base + 0), ROW16(f, base + 16), \
    ROW16(f, base + 32), ROW16(f, base + 48), \
    ROW16(f, base + 64), ROW16(f, base + 80), \
    ROW16(f, base + 96), ROW16(f, base + 112), \
    ROW16(f, base + 128), ROW16(f, base + 144), \
    ROW16(f, base + 160), ROW16(f, base + 176), \
    ROW16(f, base + 192), ROW16(f, base + 208), \
    ROW16(f, base + 224), ROW16(f, base + 240)

#define GEN512(f) ROW256(f, 0), ROW256(f, 256)

// Look up tables, generated during compile time by the above macros
static const uint8_t PROGMEM lutB[512] = { GEN512(MAP_B) };
static const uint8_t PROGMEM lutC[512] = { GEN512(MAP_C) };
static const uint8_t PROGMEM lutD[512] = { GEN512(MAP_D) };

static inline void setAddrData(uint16_t addr, uint8_t dataBit /*0|1*/) {
  PORTB = (PORTB & 0xEA) | pgm_read_byte(&lutB[addr]);
  PORTD = pgm_read_byte(&lutD[addr]);
  uint8_t pc = (PORTC & 0xE0) | pgm_read_byte(&lutC[addr]);
  if (dataBit & 1) pc |= _BV(PC1);  // PC1 = DIN
  PORTC = pc;
}
// Mapping for 4416 / 4464 - max Refresh 4ms
// They have both the same Pinout. Both have 8 Bit address range, however 4416 uses only A1-A6 for Column addresses (64)
// A0 = PB2   RAS = PC4
// A1 = PB4   CAS = PC2
// A2 = PD7   WE  = PB1
// A3 = PD6   OE  = PC0
// A4 = PD2   IO0 = PC1
// A5 = PD1   IO1 = PB3
// A6 = PD0   IO2 = PB0
// A7 = PD5   IO3 = PC3
// The following are the Pin Mappings from Ports -> DIP Pinout for DIP 18 Candidates.
const int CPU_18PORTB[] = { 15, 4, 14, 3, EOL, EOL, EOL, EOL };  // Position 4 would be A8 but the LED is attached to PB4 as well
const int CPU_18PORTC[] = { 1, 2, 16, 17, 5, EOL, EOL, EOL };
const int CPU_18PORTD[] = { 6, 7, 8, 9, NC, 10, 11, 12 };
const int RAS_18PIN = 18;  // Digital Out 18 / A4 on Arduino Uno is used for RAS
const int CAS_18PIN = 16;  // Corresponds to Analog 2 or Digital 16
#define CAS_LOW18 PORTC &= 0xfb
#define CAS_HIGH18 PORTC |= 0x04
#define RAS_LOW18 PORTC &= 0xef
#define RAS_HIGH18 PORTC |= 0x10
#define OE_LOW18 PORTC &= 0xfe
#define OE_HIGH18 PORTC |= 0x01
#define WE_LOW18 PORTB &= 0xfd
#define WE_HIGH18 PORTB |= 0x02
// Address Distribution for 18Pin Types
#define SET_ADDR_PIN18(addr) \
  { \
    PORTB = (PORTB & 0xeb) | ((addr & 0x01) << 2) | ((addr & 0x02) << 3); \
    PORTD = ((addr & 0x04) << 5) | ((addr & 0x08) << 3) | ((addr & 0x80) >> 2) | ((addr & 0x20) >> 4) | ((addr & 0x40) >> 6) | ((addr & 0x10) >> 2); \
  }

#define SET_DATA_PIN18(data) \
  { \
    PORTB = (PORTB & 0xf6) | ((data & 0x02) << 2) | ((data & 0x04) >> 2); \
    PORTC = (PORTC & 0xf5) | ((data & 0x01) << 1) | (data & 0x08); \
  }

#define GET_DATA_PIN18 (((PINC & 0x02) >> 1) + ((PINB & 0x08) >> 2) + ((PINB & 0x01) << 2) + (PINC & 0x08))

static inline uint8_t get_data_pin18() {
  uint8_t current_pinc = PINC;  // Lokale Variablen, werden jedes Mal neu geladen
  uint8_t current_pinb = PINB;

  return ((current_pinc & 0x02) >> 1) + ((current_pinb & 0x08) >> 2) + ((current_pinb & 0x01) << 2) + (current_pinc & 0x08);
}

// Mapping for 514256 / 441000 - max Refresh 8ms
// A0 = PD0   RAS = PB1
// A1 = PD1   CAS = PB0
// A2 = PD2   WE  = PB3
// A3 = PD3   OE  = PB2
// A4 = PD4   IO0 = PC0
// A5 = PD5   IO1 = PC1
// A6 = PD6   IO2 = PC2
// A7 = PD7   IO3 = PC3
// A8 = PB4
// A9 = PC4 (only 1Mx4 Ram, on the 256x4 Chip this Pin is NC or even missing i.e. 19 Pin ZIP)
// The following are the Pin Mappings from Ports -> DIP Pinout for DIP 20 Candidates.
// --> If the ZIP Socket Adapter is used, the PIN Counting is different!
const int CPU_20PORTB[] = { 17, 4, 16, 3, EOL, EOL, EOL, EOL };  // Position 4 would be A8 but the LED is attached to PB4 as well
const int CPU_20PORTC[] = { 1, 2, 18, 19, 5, 10, EOL, EOL };
const int CPU_20PORTD[] = { 6, 7, 8, 9, 11, 12, 13, 14 };
const int RAS_20PIN = 9;  // Digital Out 9 on Arduino Uno is used for RAS
const int CAS_20PIN = 8;
#define CAS_LOW20 PORTB &= 0xfe
#define CAS_HIGH20 PORTB |= 0x01
#define RAS_LOW20 PORTB &= 0xfd
#define RAS_HIGH20 PORTB |= 0x02
#define OE_LOW20 PORTB &= 0xfb
#define OE_HIGH20 PORTB |= 0x04
#define WE_LOW20 PORTB &= 0xf7
#define WE_HIGH20 PORTB |= 0x08

// Test Data Table for the Pseudo-Random test
const uint8_t nibbleArray[256] = {
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

// Detected RAM Type
int type = -1;
uint8_t Mode = 0;    // PinMode 2 = 16 Pin, 4 = 18 Pin, 5 = 20 Pin
uint8_t red = 13;    // PB5
uint8_t green = 12;  // PB4 -> Co Used with RAM Test Socket, see comments below!

void setup() {
#ifdef OLED
  // Init the OLED Disaplay
  u8x8.begin();
  u8x8.setBusClock(10000000);
  u8x8.setPowerSave(0);
  u8x8.setFont(u8x8_font_7x14B_1x2_f);
  u8x8.drawString(0, 0, "   RAM-TESTER");
#endif
  // Data Direction Register Port B, C & D - Preconfig as Input (Bit=0)
  DDRB &= 0b11100000;
  DDRC &= 0b11000000;
  DDRD = 0x00;
  // If TESTING is set enter "Factory Test" Mode
  if (EEPROM.read(TESTING) != 0) {
    buildTest();
  }
  // Wait for the Candidate to properly Startup
  if (digitalRead(19) == 1) {
    Mode += Mode_20Pin;
  }
  if (digitalRead(3) == 1) {
    Mode += Mode_18Pin;
  }
  if (digitalRead(2) == 1) {
    Mode += Mode_16Pin;
  }
  // Check if the DIP Switch is set for a valid Configuration.
  if (Mode < 2 || Mode > 5) {
#ifdef OLED
    u8x8.clearDisplay();
    u8x8.setFont(u8x8_font_open_iconic_embedded_4x4);
    u8x8.drawString(6, 0, "H");
    u8x8.setFont(u8x8_font_7x14B_1x2_r);
    u8x8.drawString(2, 5, "DIP Settings!");
    u8x8.setFont(u8x8_font_5x7_r);
    u8x8.drawString(2, 7, "Version:");
    u8x8.drawString(10, 7, version);
#endif
    ConfigFail();
  }
  // With a valid Config, activate the PullUps
  PORTB |= 0b00011111;
  PORTC |= 0b00111111;
  PORTD = 0xff;
  digitalWrite(13, ON);  // Switch the LED on PB5 on for the rest of the test as it will show as yellow not to confuse Users as steady green.
  // Settle State - PullUps my require some time.
  checkGNDShort();  // Check for Shorts towards GND. Shorts on Vcc can't be tested as it would need Pull-Downs.
  // Startup Delay as per Datasheets
  delayMicroseconds(200);
  if (Mode == Mode_20Pin) {
    initRAM(RAS_20PIN, CAS_20PIN);
    test20Pin();
  }
  if (Mode == Mode_18Pin) {
    initRAM(RAS_18PIN, CAS_18PIN);
    test18Pin();
  }
  if (Mode == Mode_16Pin) {
    initRAM(RAS_16PIN, CAS_16PIN);
    test16Pin();
  }
}

// This Sketch should never reach the Loop...
void loop() {
  ConfigFail();
}

// All RAM Chips require 8 RAS only Refresh Cycles (ROR) for proper initailization
void initRAM(int RASPin, int CASPin) {
  pinMode(RASPin, OUTPUT);
  pinMode(CASPin, OUTPUT);
  // RAS is an Active LOW Signal
  digitalWrite(RASPin, HIGH);
  // For some DRAM CAS !NEEDS! to be low during Init!
  digitalWrite(CASPin, HIGH);
  for (int i = 0; i < 8; i++) {
    digitalWrite(RASPin, LOW);
    digitalWrite(RASPin, HIGH);
  }
}

//=======================================================================================
// 16 - Pin DRAM Test Code
//=======================================================================================

void test16Pin() {
  // Configure I/O for this Chip Type
  DDRB = 0b00111111;
  PORTB = 0b00101010;
  DDRC = 0b00011011;
  PORTC = 0b00001000;
  DDRD = 0b11000011;
  PORTD = 0x00;
  Sense41256();
#ifdef OLED
  u8x8.drawString(0, 4, "Detected Type:");
  u8x8.drawString(0, 6, ramTypes[type].name);
  u8x8.drawString(0, 2, "Checking...    ");
  // Redo because otherwise u8x8 interferes with the Test
  DDRB = 0b00111111;
  PORTB = 0b00101010;
#endif
  checkAdressing_16Pin();
  for (uint8_t patNr = 0; patNr <= 4; patNr++)
    for (uint16_t row = 0; row < ramTypes[type].rows; row++) {  // Iterate over all ROWs
      write16PinRow(row, ramTypes[type].columns, patNr);
    }
  // Good Candidate.
  testOK();
}

// Prepare and execute ROW Access for 16 Pin Types
void rASHandling16Pin(uint16_t row) {
  RAS_HIGH16;
  // Row Address distribution Logic for 41256/64 16 Pin RAM - more complicated as the PCB circuit is optimized for 256x4 / 1Mx4 Types.
  SET_ADDR_PIN16(row);
  RAS_LOW16;
}

// Write and Read (&Check) Pattern from Cols
void write16PinRow(uint16_t row, uint16_t cols, uint8_t patNr) {
  // Prepare Write Cycle
  CAS_HIGH16;
  rASHandling16Pin(row);  // Set the Row
  WE_LOW16;
  uint8_t pat = pattern[patNr];
  cli();
  uint16_t col = cols;
  if (patNr < 4)
    do {
      // Column Address distribution logic for 41256/64 16 Pin RAM
      setAddrData(--col, pat);
      // Rotate the Pattern 1 Bit to the LEFT (c has not rotate so there is a trick with 2 Shift)
      pat = (pat << 1) | (pat >> 7);
      CAS_HIGH16;
    } while (col != 0);
  else
    do {
      // Column Address distribution logic for 41256/64 16 Pin RAM
      CAS_HIGH16;
      col--;
      setAddrData(col, nibbleArray[mix8(col, row)]);
    } while (col != 0);
  CAS_HIGH16;
  sei();
  // Prepare Read Cycle
  WE_HIGH16;
  // Read and check the Row we just wrote, otherwise Error 2
  if (patNr < 4) {
    rowCheck16Pin(cols, row, patNr, 2);
    return;
  }
  refreshRow16Pin(row);
  if (row == ramTypes[type].rows - 1) {  // Last Row writen, we have to check the last n Rows as well.
    // Retention testing the last rows, they will no longer be written only read back. Simulte the write time to get a correct retention time test.
    for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {
      rASHandling16Pin(row - x);
      rowCheck16Pin(cols, row - x, patNr, 3);
      delayMicroseconds(ramTypes[type].writeTime);  // Simulate writing even if it is no longer done for the last rows
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
    }
    return;
  }
  if (row >= ramTypes[type].delayRows) {
    rASHandling16Pin(row - ramTypes[type].delayRows);
    rowCheck16Pin(cols, row - ramTypes[type].delayRows, patNr, 3);
    return;
  }
  if (row < ramTypes[type].delayRows)
    delayMicroseconds(ramTypes[type].delays[row]);
  else
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
}

void refreshRow16Pin(uint16_t row) {
  rASHandling16Pin(row);  // Refresh this ROW
  RAS_HIGH16;
}

void rowCheck16Pin(uint16_t cols, uint16_t row, uint8_t patNr, uint8_t check) {
  uint8_t pat = pattern[patNr];
  // Iterate over the Columns and read & check Pattern
  cli();
  uint16_t col = cols;
  if (patNr < 4)
    //for (uint16_t col = 0; col < cols; col++) {
    do {
      setAddrData(--col, 0);
      //NOP;  // This RAM is later slow to propagate the Output, so take time to settle signals
      CAS_HIGH16;
      if (((PINC & 0x04) >> 2) != (pat & 0x01)) {
        error(patNr + 1, check);
      }  // Check if Pattern matches
      pat = (pat << 1) | (pat >> 7);
    } while (col != 0);
  else
    do {
      setAddrData(--col, 0);
      //NOP;
      CAS_HIGH16;
      if (((PINC & 0x04) >> 2) != (nibbleArray[mix8(col, row)] & 0x01)) {
        error(patNr + 1, check);
      }  // Check if Pattern matches
    } while (col != 0);
  sei();
  RAS_HIGH16;
}

//=======================================================================================
// Adressleitungs- und Decoder-Prüfung für 16-Pin DRAM (4164/41256)
//=======================================================================================

void checkAdressing_16Pin() {
  uint16_t max_rows = ramTypes[type].rows;
  uint16_t max_cols = ramTypes[type].columns;

  // Bestimme Anzahl Address-Bits für Row und Column
  uint8_t row_bits = 0, col_bits = 0;
  uint16_t temp = max_rows - 1;
  while (temp) {
    temp >>= 1;
    row_bits++;
  }
  temp = max_cols - 1;
  while (temp) {
    temp >>= 1;
    col_bits++;
  }

  //===========================================================================
  // PHASE 1: ROW ADDRESS LINES TESTING
  //===========================================================================

  // Configure I/O für 16-Pin
  DDRB = 0b00111111;
  PORTB = 0b00101010;
  DDRC = 0b00011011;
  PORTC = 0b00001000;
  DDRD = 0b11000011;
  PORTD = 0x00;

  CAS_HIGH16;
  RAS_HIGH16;
  WE_LOW16;

  // Write Phase - Walking-Bit Pattern für Row-Adressen
  for (uint8_t bit = 0; bit <= row_bits; bit++) {
    uint16_t test_addr = (bit == 0) ? 0 : (1 << (bit - 1));
    uint8_t test_data = (bit == 0) ? 0 : 1;

    SET_ADDR_PIN16(test_addr);

    RAS_LOW16;
    PORTC = (PORTC & 0xfd) | ((test_data & 0x01) << 1);  // Set Din
    WE_LOW16;
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
    WE_HIGH16;
    RAS_HIGH16;
  }

  // Read Phase - Verifikation Row-Adressen
  for (uint8_t bit = 0; bit <= row_bits; bit++) {
    uint16_t test_addr = (bit == 0) ? 0 : (1 << (bit - 1));
    uint8_t expected_data = (bit == 0) ? 0 : 1;

    SET_ADDR_PIN16(test_addr);

    RAS_LOW16;
    CAS_LOW16;
    NOP;
    NOP;

    if (((PINC & 0x04) >> 2) != expected_data) {
      error(bit, 1);  // Row address line error
    }

    CAS_HIGH16;
    RAS_HIGH16;
  }

  //===========================================================================
  // PHASE 2: COLUMN ADDRESS LINES TESTING
  //===========================================================================

  uint16_t test_row = max_rows >> 1;  // Mittlere Row verwenden

  SET_ADDR_PIN16(test_row);
  RAS_LOW16;
  WE_LOW16;

  // Write Phase - Column Address Walking-Bit
  for (uint8_t bit = 0; bit <= col_bits; bit++) {
    uint16_t test_col = (bit == 0) ? 0 : (1 << (bit - 1));
    uint8_t test_data = (bit & 1) ? 1 : 0;

    setAddrData(test_col, test_data);
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
  }

  WE_HIGH16;
  RAS_HIGH16;

  // Read Phase - Column Address Verifikation
  SET_ADDR_PIN16(test_row);
  RAS_LOW16;

  for (uint8_t bit = 0; bit <= col_bits; bit++) {
    uint16_t test_col = (bit == 0) ? 0 : (1 << (bit - 1));
    uint8_t expected_data = (bit & 1) ? 1 : 0;

    setAddrData(test_col, 0);  // Data bit auf 0 für Read
    CAS_LOW16;
    NOP;
    NOP;

    if (((PINC & 0x04) >> 2) != expected_data) {
      error(bit + 16, 1);  // Column address line error (16-31)
    }

    CAS_HIGH16;
  }

  RAS_HIGH16;

  //===========================================================================
  // PHASE 3: ADDRESS DECODER STRESS TEST mit Gray-Code
  //===========================================================================

  // Row Address Gray-Code Test
  WE_LOW16;

  for (uint16_t i = 0; i < min(max_rows, 256); i++) {
    uint16_t gray_addr = i ^ (i >> 1);                                   // Gray-Code conversion
    uint8_t test_data = (gray_addr & 0x01) ^ ((gray_addr & 0x02) >> 1);  // Mixed pattern

    if (gray_addr >= max_rows) continue;

    SET_ADDR_PIN16(gray_addr);
    RAS_LOW16;
    PORTC = (PORTC & 0xfd) | ((test_data & 0x01) << 1);  // Set Din
    WE_LOW16;
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
    WE_HIGH16;
    RAS_HIGH16;
  }

  // Gray-Code Verifikation
  for (uint16_t i = 0; i < min(max_rows, 256); i++) {
    uint16_t gray_addr = i ^ (i >> 1);
    uint8_t expected_data = (gray_addr & 0x01) ^ ((gray_addr & 0x02) >> 1);

    if (gray_addr >= max_rows) continue;

    SET_ADDR_PIN16(gray_addr);
    RAS_LOW16;
    CAS_LOW16;
    NOP;
    NOP;

    if (((PINC & 0x04) >> 2) != expected_data) {
      error(32 + (i & 0x1f), 1);  // Decoder error (32-63)
    }

    CAS_HIGH16;
    RAS_HIGH16;
  }

  //===========================================================================
  // PHASE 4: COLUMN DECODER GRAY-CODE TEST
  //===========================================================================

  // Column Address Gray-Code Test
  SET_ADDR_PIN16(test_row);
  RAS_LOW16;
  WE_LOW16;

  for (uint16_t i = 0; i < min(max_cols, 256); i++) {
    uint16_t gray_col = i ^ (i >> 1);
    uint8_t test_data = (gray_col & 0x01) ^ ((gray_col & 0x04) >> 2);

    if (gray_col >= max_cols) continue;

    setAddrData(gray_col, test_data);
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
  }

  WE_HIGH16;
  RAS_HIGH16;

  // Column Gray-Code Verifikation
  SET_ADDR_PIN16(test_row);
  RAS_LOW16;

  for (uint16_t i = 0; i < min(max_cols, 256); i++) {
    uint16_t gray_col = i ^ (i >> 1);
    uint8_t expected_data = (gray_col & 0x01) ^ ((gray_col & 0x04) >> 2);

    if (gray_col >= max_cols) continue;

    setAddrData(gray_col, 0);  // Data bit auf 0 für Read
    CAS_LOW16;
    NOP;
    NOP;

    if (((PINC & 0x04) >> 2) != expected_data) {
      error(64 + (i & 0x1f), 1);  // Column decoder error (64-95)
    }

    CAS_HIGH16;
  }

  RAS_HIGH16;
}

// Address Line Checks and sensing for 41256 or 4164
void Sense41256() {
  boolean big = true;
  CAS_HIGH16;
  // RAS Testing set Row 0 Col 0 and set Bit Low.
  rASHandling16Pin(0);
  PORTC &= 0xfd;
  WE_LOW16;
  CAS_LOW16;
  NOP;
  CAS_HIGH16;
  WE_HIGH16;
  for (uint8_t a = 0; a <= 8; a++) {
    uint16_t adr = (1 << a);
    rASHandling16Pin(adr);
    // Write Bit Col 0 High
    WE_LOW16;
    PORTC |= 0x02;
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
    WE_HIGH16;
    // Back to Row 0 then check if Bit at Col 0 is still 0
    rASHandling16Pin(0);
    CAS_LOW16;
    NOP;
    NOP;
    // Check for Dout = 0
    if (((PINC & 0x04) >> 2) != (0x0)) {
      // If A8 Line is set and it is a fail, this might be a 6464 Type
      if (a == 8)
        big = false;
      else
        error(1, 0);
    }
    CAS_HIGH16;
  }
  if (big)
    type = T_41256;
  else
    type = T_4164;
}

//=======================================================================================
// 18 - Pin DRAM Test Code
//=======================================================================================

void test18Pin() {
  // Configure I/O for this Chip Type
  DDRB = 0b00111111;
  PORTB = 0b00100010;
  DDRC = 0b00011111;
  PORTC = 0b00010101;
  DDRD = 0b11100111;
  sense4464();
#ifdef OLED
  u8x8.drawString(0, 4, "Detected Type:");
  u8x8.drawString(0, 6, ramTypes[type].name);
  u8x8.drawString(0, 2, "Checking...    ");
  // Redo because otherwise u8x8 interferes with the Test
  DDRB = 0b00111111;
  PORTB = 0b00100010;
#endif
  checkAdressing_18Pin();
  for (uint8_t pat = 0; pat < 5; pat++)
    for (uint16_t row = 0; row < ramTypes[type].rows; row++) {  // Iterate over all ROWs
      write18PinRow(row, pat, ramTypes[type].columns);
    }
  testOK();
}

void write18PinRow(uint8_t row, uint8_t patNr, uint16_t width) {
  uint16_t colAddr;  // Prepared Column Adress to safe Init Time. This is needed when A0 & A8 are not used for Col addressing.
  uint8_t init_shift = type == T_4416 ? 1 : 0;
  // Prepare Write Cycle
  rASHandling18Pin(row);
  WE_LOW18;
  configDOut18Pin();
  SET_DATA_PIN18(pattern[patNr]);
  cli();
  if (patNr < 4)
    for (uint16_t col = 0; col < width; col++) {
      colAddr = (col << init_shift);
      SET_ADDR_PIN18(colAddr);
      CAS_LOW18;
      NOP;
      CAS_HIGH18;
    }
  else
    for (uint16_t col = 0; col < width; col++) {
      SET_DATA_PIN18(nibbleArray[mix8(col, row)]);
      colAddr = (col << init_shift);
      SET_ADDR_PIN18(colAddr);
      CAS_LOW18;
      NOP;
      CAS_HIGH18;
    }
  sei();
  WE_HIGH18;
  // If we check 255 Columns the time for Write & Read(Check) exceeds the Refresh time. We need to add a Refresh in the Middle
  if (patNr < 4) {
    checkRow18Pin(width, row, patNr, init_shift, 2);
    return;
  }
  if (patNr == 4) {
    refreshRow18Pin(row);
    if (row == ramTypes[type].rows - 1) {  // Last Row writen, we have to check the last n Rows as well.
      // Retention testing the last rows, they will no longer be written only read back. Simulte the write time to get a correct retention time test.
      for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {
        rASHandling18Pin(row - x);
        checkRow18Pin(width, row - x, patNr, init_shift, 3);
        delayMicroseconds(ramTypes[type].writeTime);  // Simulate writing even if it is no longer done for the last rows
        delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
      }
      return;
    }
    if (row >= ramTypes[type].delayRows) {
      rASHandling18Pin(row - ramTypes[type].delayRows);
      checkRow18Pin(width, row - ramTypes[type].delayRows, patNr, init_shift, 3);
    }
    if (row < ramTypes[type].delayRows)
      delayMicroseconds(ramTypes[type].delays[row]);
    else
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
  }
}


void checkRow18Pin(uint16_t width, uint8_t row, uint8_t patNr, uint8_t init_shift, uint8_t errorNr) {
  configDIn18Pin();
  uint8_t pat = pattern[patNr] & 0x0f;
  OE_LOW18;
  cli();
  if (patNr < 4)
    for (uint16_t col = 0; col < width; col++) {
      SET_ADDR_PIN18(col << init_shift);
      CAS_LOW18;
      NOP;
      if ((GET_DATA_PIN18) != pat) {
        error(patNr, errorNr);
      }
      CAS_HIGH18;
    }
  else
    for (uint16_t col = 0; col < width; col++) {
      SET_ADDR_PIN18(col << init_shift);
      CAS_LOW18;
      CAS_HIGH18;
      if ((get_data_pin18()) != nibbleArray[mix8(col, row)]) {
        error(patNr, errorNr);
      }
    }
  sei();
  OE_HIGH18;
  RAS_HIGH18;
}

void configDOut18Pin() {
  DDRB |= 0x09;  // Configure D1 & D2 as Outputs
  DDRC |= 0x0a;  // Configure D0 & D3 as Outputs
}

void configDIn18Pin() {
  DDRB &= 0xf6;  // Config Data Lines for input
  DDRC &= 0xf5;
}

void refreshRow18Pin(uint8_t row) {
  rASHandling18Pin(row);
  NOP;
  NOP;
  RAS_HIGH18;
}

void rASHandling18Pin(uint8_t row) {
  RAS_HIGH18;
  SET_ADDR_PIN18(row);
  RAS_LOW18;
}

//=======================================================================================
// Adressleitungs- und Decoder-Prüfung für 18-Pin DRAM (4416/4464)
//=======================================================================================

void checkAdressing_18Pin() {
  return;
  uint16_t max_rows = ramTypes[type].rows;
  uint16_t max_cols = ramTypes[type].columns;
  uint8_t init_shift = (type == T_4416) ? 1 : 0;  // 4416 nutzt A0 nicht für Columns

  // Bestimme Anzahl Address-Bits für Row und Column
  uint8_t row_bits = 0, col_bits = 0;
  uint16_t temp = max_rows - 1;
  while (temp) {
    temp >>= 1;
    row_bits++;
  }
  temp = max_cols - 1;
  while (temp) {
    temp >>= 1;
    col_bits++;
  }

  //===========================================================================
  // PHASE 1: ROW ADDRESS LINES TESTING
  //===========================================================================

  // Configure I/O für 18-Pin
  DDRB |= 0x09;  // D1 & D2 als Output
  DDRC |= 0x0a;  // D0 & D3 als Output

  RAS_HIGH18;
  CAS_HIGH18;
  OE_HIGH18;
  WE_LOW18;

  uint8_t test_col = 0x04;
  for (uint8_t bit = 0; bit <= row_bits; bit++) {
    // Write
    uint8_t test_row = (bit == 0) ? 0 : (1 << (bit - 1));
    uint8_t test_data = (bit == 0) ? 0x0 : 0xF;
    WE_LOW18;
    // Write Operation
    SET_ADDR_PIN18(test_row);
    RAS_LOW18;
    SET_ADDR_PIN18(test_col);
    SET_DATA_PIN18(test_data);
    CAS_LOW18;
    NOP;
    CAS_HIGH18;
    NOP;
    RAS_HIGH18;
    NOP;
    WE_HIGH18;
    // SOFORT Read-back (keine Pause!)
    DDRB &= 0xf6;
    DDRC &= 0xf5;  // Input
    OE_LOW18;
    SET_ADDR_PIN18(test_row);
    RAS_LOW18;
    SET_ADDR_PIN18(test_col);
    CAS_LOW18;
    NOP;
    if (GET_DATA_PIN18 != test_data) {
      error(bit, 1);
    }
    CAS_HIGH18;
    RAS_HIGH18;
    OE_HIGH18;
    // Zurück zu Output für nächste Iteration
    DDRB |= 0x09;
    DDRC |= 0x0a;
  }

  //===========================================================================
  // PHASE 2: COLUMN ADDRESS LINES TESTING
  //===========================================================================

  uint8_t test_row = max_rows >> 1;  // Mittlere Row verwenden

  SET_ADDR_PIN18(test_row);
  RAS_LOW18;
  WE_LOW18;

  // Write Phase - Column Address Walking-Bit
  // KORREKTUR: 4416 verwendet nur A1-A6 für Columns (6 Bits), A0 und A7 werden NICHT verwendet!
  uint8_t start_bit = (type == T_4416) ? 1 : 0;       // 4416 startet bei Bit 1 (A1)
  uint8_t end_bit = (type == T_4416) ? 6 : col_bits;  // 4416 endet bei Bit 6 (A6)

  for (uint8_t bit = start_bit; bit <= end_bit; bit++) {
    uint8_t test_col = (1 << bit);  // Direkt das Bit setzen, nicht bit-1
    uint8_t test_data = (bit & 1) ? 0xA : 0x5;
    uint16_t colAddr = (test_col << init_shift);

    SET_ADDR_PIN18(colAddr);
    SET_DATA_PIN18(test_data);

    CAS_LOW18;
    NOP;
    CAS_HIGH18;
  }

  WE_HIGH18;
  RAS_HIGH18;

  // Read Phase - Column Address Verifikation
  DDRB &= 0xf6;
  DDRC &= 0xf5;

  SET_ADDR_PIN18(test_row);
  RAS_LOW18;
  OE_LOW18;

  for (uint8_t bit = start_bit; bit <= end_bit; bit++) {
    uint8_t test_col = (1 << bit);
    uint8_t expected_data = (bit & 1) ? 0xA : 0x5;
    uint16_t colAddr = (test_col << init_shift);

    SET_ADDR_PIN18(colAddr);

    CAS_LOW18;
    NOP;

    if (GET_DATA_PIN18 != expected_data) {
      error(bit + 16, 1);  // Column address line error (16-31)
    }

    CAS_HIGH18;
  }

  OE_HIGH18;
  RAS_HIGH18;

  //===========================================================================
  // PHASE 3: ADDRESS DECODER STRESS TEST mit Gray-Code
  //===========================================================================

  // Configure für Output
  DDRB |= 0x09;
  DDRC |= 0x0a;
  WE_LOW18;

  // Gray-Code Test für bessere Decoder-Abdeckung
  for (uint16_t i = 0; i < min(max_rows, 256); i++) {
    uint8_t gray_addr = (i ^ (i >> 1)) & 0xFF;     // Gray-Code conversion auf 8-Bit
    uint8_t test_data = (gray_addr & 0x0f) ^ 0x5;  // Mixed pattern

    if (gray_addr >= max_rows) continue;

    SET_ADDR_PIN18(gray_addr);
    SET_DATA_PIN18(test_data);

    RAS_LOW18;
    CAS_LOW18;
    NOP;
    CAS_HIGH18;
    RAS_HIGH18;
  }

  WE_HIGH18;

  // Gray-Code Verifikation
  DDRB &= 0xf6;
  DDRC &= 0xf5;
  OE_LOW18;

  for (uint16_t i = 0; i < min(max_rows, 256); i++) {
    uint8_t gray_addr = (i ^ (i >> 1)) & 0xFF;
    uint8_t expected_data = (gray_addr & 0x0f) ^ 0x5;

    if (gray_addr >= max_rows) continue;

    SET_ADDR_PIN18(gray_addr);

    RAS_LOW18;
    CAS_LOW18;
    NOP;

    if (GET_DATA_PIN18 != expected_data) {
      error(32 + (i & 0x1f), 1);  // Decoder error (32-63)
    }

    CAS_HIGH18;
    RAS_HIGH18;
  }

  OE_HIGH18;
}

void sense4464() {
  boolean big = true;
  rASHandling18Pin(0);  // Use Row 0 for Size Tests
  WE_LOW18;
  SET_DATA_PIN18(0x0);
  SET_ADDR_PIN18(0x00);
  CAS_LOW18;
  NOP;
  CAS_HIGH18;
  // 4416 CAS addressing does not Use A0 nor A7 we set A0 to check and Write 1111 to this Column
  // First test the CAS addressing
  for (uint8_t a = 0; a <= 7; a++) {
    uint8_t col = (1 << a);
    configDOut18Pin();
    WE_LOW18;
    SET_DATA_PIN18(0xf);
    SET_ADDR_PIN18(col);
    CAS_LOW18;
    NOP;
    CAS_HIGH18;
    SET_ADDR_PIN18(0x00);
    WE_HIGH18;
    configDIn18Pin();
    OE_LOW18;
    CAS_LOW18;
    NOP;
    NOP;
    if ((GET_DATA_PIN18 & 0xf) != (0x0)) {
      if (a == 0) {
        big = false;
        CAS_HIGH18;
        OE_HIGH18;
        WE_LOW18;
        configDOut18Pin();
        SET_DATA_PIN18(0x0);
        SET_ADDR_PIN18(0x01);
        CAS_LOW18;
      } else if ((a == 7) && (big == false))
        NOP;
      else {
        error(1, 0);
      }
    }
    PORTC |= 0x05;
  }
  if (big)
    type = T_4464;
  else
    type = T_4416;
}

//=======================================================================================
// 20 - Pin DRAM Test Code
//=======================================================================================

void test20Pin() {
  // Configure I/O for this Chip Type
  PORTB = 0b00111111;
  PORTC = 0b10000000;
  PORTD = 0x00;
  DDRB = 0b00011111;
  DDRC = 0b00011111;
  DDRD = 0xFF;
  senseRAM_20Pin();
  senseSCRAM();
#ifdef OLED
  u8x8.drawString(0, 4, "Detected Type:");
  u8x8.drawString(0, 6, ramTypes[type].name);
  u8x8.drawString(0, 2, "Checking...    ");
  // Redo because otherwise u8x8 interferes with the test
  PORTB = 0b00111111;
  DDRB = 0b00011111;
#endif
  checkAdressing_20Pin();
  for (uint8_t pat = 0; pat < 5; pat++) {                       // Check all 4Bit Patterns
    for (uint16_t row = 0; row < ramTypes[type].rows; row++) {  // Iterate over all ROWs
      write20PinRow(row, pat);
    }
  }
  // Good Candidate.
  testOK();
}

// Prepare and execute ROW Access for 20 Pin Types
void rASHandlingPin20(uint16_t row) {
  RAS_HIGH20;
  msbHandlingPin20(row >> 8);  // Preset ROW Adress
  PORTD = (uint8_t)(row & 0xff);
  RAS_LOW20;
}

// Prepare Controll Lines and perform Checks
void write20PinRow(uint16_t row, uint8_t pattern) {
  PORTB |= 0x0f;                   // Set all RAM Controll Lines to HIGH = Inactive
  cASHandlingPin20(row, pattern);  // Do the Test
  PORTB |= 0x0f;                   // Set all RAM Controll Lines to HIGH = Inactive
}

// 20 Pin RAM use A8 and probably A9 larger than 8 lanes of PORTD for the lower 8 address bits
void msbHandlingPin20(uint8_t address) {
  PORTB = (PORTB & 0xef) | ((address & 0x01) << 4);
  PORTC = (PORTC & 0xef) | ((address & 0x02) << 3);
}

// Write and Read (&Check) Pattern from Cols
void cASHandlingPin20(uint16_t row, uint8_t patNr) {
  rASHandlingPin20(row);  // Set the Row
  // Prepare Write Cycle
  PORTC &= 0xf0;                     // Set all Outputs to LOW
  DDRC |= 0x0f;                      // Configure IOs for Output
  PORTC |= (pattern[patNr] & 0x0f);  // Alternative Pattern Odd & Even so we can check for Crosstalk later.
  WE_LOW20;
  uint8_t col = 0;
  uint8_t msbCol = ramTypes[type].columns / 256;
  cli();
  // Write Data Loop
  for (uint8_t msb = 0; msb < msbCol; msb++) {
    msbHandlingPin20(msb);  // Set the MSB as needed
    // Regular Test patterns like 0000 / 1111 / 1010 / 0101
    if (patNr != 4) {
      do {
        PORTD = col;  // Set Col Adress
        CAS_LOW20;
        CAS_HIGH20;
      } while (++col != 0);
    } else {
      // Cache the upper IO Pins of PORTC for faster access - they will not change in the CAS Loop on 8-Bits, only Bit 9 may change in MSB Handling
      uint8_t io = (PORTC & 0xf0);
      // Write "Random" test data from the table
      do {
        if (patNr == 4) {
          PORTC = io | (nibbleArray[mix8(col, row)]);
        }
        PORTD = col;  // Set Col Adress
        CAS_LOW20;
        CAS_HIGH20;
      } while (++col != 0);
    }
  }
  sei();
  // Prepare Read Cycle
  WE_HIGH20;
  PORTC &= 0xf0;  // Clear all Outputs
  DDRC &= 0xf0;   // Configure IOs for Input
  // As long its not yet Random Data just check we get the same back that was just written
  if (patNr < 4) {
    checkRow20Pin(patNr, row, 2);
    return;
  }
  if (patNr == 4) {
    // Refresh the Row to have stable Timings and check refresh
    refreshRow20Pin(row);                  // Refresh the current row before leaving
    if (row == ramTypes[type].rows - 1) {  // Last Row writen, we have to check the last n Rows as well.
      // Retention testing the last rows, they will no longer be written only read back. Simulte the write time to get a correct retention time test.
      for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {
        rASHandlingPin20(row - x);
        checkRow20Pin(4, row - x, 3);
        delayMicroseconds(ramTypes[type].writeTime);  // Simulate writing even if it is no longer done for the last rows
        delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
      }
      return;
    }
    // Default Retention Testing
    if (row >= ramTypes[type].delayRows) {
      rASHandlingPin20(row - ramTypes[type].delayRows);
      checkRow20Pin(4, row - ramTypes[type].delayRows, 3);  // Check for the last random Pattern on this ROW
    }
    // Data retention Testing first rows. They need special treatment since they will be read back later. Timing is special at the beginning to match retention times
    if (row < ramTypes[type].delayRows)
      delayMicroseconds(ramTypes[type].delays[row]);
    else
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
  }
}

void refreshRow20Pin(uint16_t row) {
  CAS_HIGH20;
  rASHandlingPin20(row);
  RAS_HIGH20;
}

// Check one full row for normal FP-Mode RAM
void checkRow20Pin(uint8_t patNr, uint16_t row, uint8_t errNr) {
  uint8_t pat = pattern[patNr] & 0x0f;
  OE_LOW20;
  boolean scRAM = ramTypes[type].staticColumn;
  cli();
  uint8_t col = 0;
  uint8_t msbCol = ramTypes[type].columns / 256;
  for (uint8_t msb = 0; msb < msbCol; msb++) {
    msbHandlingPin20(msb);  // Set the MSB as needed
    // Distinguish between Static Column and FastPage Mode - Ugly code duplication but fastest code possible.
    if (scRAM == false) {
      do {
        PORTD = col;  // Set Col Adress
        CAS_LOW20;
        CAS_HIGH20;
        // If the Data matches the current pattern its ok
        if (pat == (PINC & 0x0f))
          continue;
        // Or if it matches the random data its also ok
        if ((PINC & 0x0f) == (nibbleArray[mix8(col, row)]))
          continue;
        // If both do not match, there is an error.
        error(patNr, errNr);
      } while (++col != 0);
    } else {
      // Same for static column RAM
      CAS_LOW20;
      do {
        PORTD = col;  // Set Col Adress
        if (pat == (PINC & 0x0f))
          continue;
        if ((PINC & 0x0f) == (nibbleArray[mix8(col, row)]))
          continue;
        error(patNr, errNr);
      } while (++col != 0);
    }
  }
  sei();
  CAS_HIGH20;
  OE_HIGH20;
}

//=======================================================================================
// UMFASSENDE Adressleitungs- und Decoder-Prüfung für 20-Pin DRAM
//=======================================================================================

void checkAdressing_20Pin() {
  register uint16_t max_rows = ramTypes[type].rows;
  register uint16_t max_cols = ramTypes[type].columns;

  // Bestimme Anzahl Address-Bits für Row und Column
  uint8_t row_bits = 0, col_bits = 0;
  uint16_t temp = max_rows - 1;
  while (temp) {
    temp >>= 1;
    row_bits++;
  }
  temp = max_cols - 1;
  while (temp) {
    temp >>= 1;
    col_bits++;
  }

  //===========================================================================
  // PHASE 1: ROW ADDRESS LINES TESTING
  //===========================================================================
  DDRC |= 0x0f;  // IOs als Output
  RAS_HIGH20;
  CAS_HIGH20;
  OE_HIGH20;
  WE_LOW20;

  // Write Phase - Walking-Bit Pattern für Row-Adressen
  for (uint8_t bit = 0; bit <= row_bits; bit++) {
    register uint16_t test_row = (bit == 0) ? 0 : (1 << (bit - 1));
    register uint8_t test_data = (bit == 0) ? 0x00 : 0x55;
    register uint8_t test_col = 0x00;  // Feste Column für Row-Test

    // Phase 1: Row-Adresse setzen und RAS aktivieren
    register uint8_t row_msb = test_row >> 8;
    register uint8_t row_lsb = test_row & 0xff;

    PORTB = (PORTB & 0xef) | ((row_msb & 0x01) << 4);
    PORTC = (PORTC & 0xe0) | ((row_msb & 0x02) << 3);
    PORTD = row_lsb;
    RAS_LOW20;

    // Phase 2: Column-Adresse setzen und CAS aktivieren
    register uint8_t col_msb = test_col >> 8;
    register uint8_t col_lsb = test_col & 0xff;

    PORTB = (PORTB & 0xef) | ((col_msb & 0x01) << 4);
    PORTC = (PORTC & 0xe0) | ((col_msb & 0x02) << 3) | (test_data & 0x0f);
    PORTD = col_lsb;

    CAS_LOW20;
    NOP;
    CAS_HIGH20;
    NOP;
    RAS_HIGH20;
  }

  WE_HIGH20;

  // Read Phase - Verifikation Row-Adressen
  DDRC &= 0xf0;  // IOs als Input
  PORTC &= 0xe0;
  OE_LOW20;

  for (uint8_t bit = 0; bit <= row_bits; bit++) {
    register uint16_t test_row = (bit == 0) ? 0 : (1 << (bit - 1));
    register uint8_t expected_data = (bit == 0) ? 0x00 : 0x55;
    register uint8_t test_col = 0x00;  // Dieselbe Column

    // Phase 1: Row-Adresse setzen
    register uint8_t row_msb = test_row >> 8;
    register uint8_t row_lsb = test_row & 0xff;

    PORTB = (PORTB & 0xef) | ((row_msb & 0x01) << 4);
    PORTC = (PORTC & 0xef) | ((row_msb & 0x02) << 3);
    PORTD = row_lsb;
    RAS_LOW20;

    // Phase 2: Column-Adresse setzen
    register uint8_t col_msb = test_col >> 8;
    register uint8_t col_lsb = test_col & 0xff;

    PORTB = (PORTB & 0xef) | ((col_msb & 0x01) << 4);
    PORTC = (PORTC & 0xef) | ((col_msb & 0x02) << 3);
    PORTD = col_lsb;

    CAS_LOW20;
    NOP;
    NOP;
    if ((expected_data & 0x0f) != (PINC & 0x0f)) {
      error(bit, 1);  // Row address line error
    }

    CAS_HIGH20;
    RAS_HIGH20;
  }

  OE_HIGH20;

  //===========================================================================
  // PHASE 2: COLUMN ADDRESS LINES TESTING
  //===========================================================================

  DDRC |= 0x0f;
  register uint16_t test_row = max_rows >> 1;  // Mittlere Row verwenden

  rASHandlingPin20(test_row);
  WE_LOW20;

  // Write Phase - Column Address Walking-Bit
  cli();
  for (uint8_t bit = 0; bit <= col_bits; bit++) {
    register uint16_t test_col = (bit == 0) ? 0 : (1 << (bit - 1));
    register uint8_t test_data = (bit & 1) ? 0xA : 0x5;

    register uint8_t col_msb = test_col >> 8;
    register uint8_t col_lsb = test_col & 0xff;

    PORTB = (PORTB & 0xef) | ((col_msb & 0x01) << 4);
    PORTC = (PORTC & 0xe0) | ((col_msb & 0x02) << 3) | (test_data & 0x0f);
    PORTD = col_lsb;

    CAS_LOW20;
    NOP;
    CAS_HIGH20;
  }
  sei();

  WE_HIGH20;
  RAS_HIGH20;

  // Read Phase - Column Address Verifikation
  DDRC &= 0xf0;
  rASHandlingPin20(test_row);
  OE_LOW20;

  cli();
  for (uint8_t bit = 0; bit <= col_bits; bit++) {
    register uint16_t test_col = (bit == 0) ? 0 : (1 << (bit - 1));
    register uint8_t expected_data = (bit & 1) ? 0xA : 0x5;

    register uint8_t col_msb = test_col >> 8;
    register uint8_t col_lsb = test_col & 0xff;

    PORTB = (PORTB & 0xef) | ((col_msb & 0x01) << 4);
    PORTC = (PORTC & 0xef) | ((col_msb & 0x02) << 3);
    PORTD = col_lsb;

    CAS_LOW20;
    NOP;
    NOP;
    if ((PINC & 0x0f) != (expected_data & 0x0f)) {
      sei();
      error(bit + 16, 1);  // Column address line error (16-31)
    }

    CAS_HIGH20;
  }
  sei();

  OE_HIGH20;
  RAS_HIGH20;

  //===========================================================================
  // PHASE 3: ADDRESS DECODER STRESS TEST mit Gray-Code
  //===========================================================================

  DDRC |= 0x0f;
  WE_LOW20;

  // Gray-Code Test für bessere Decoder-Abdeckung
  cli();
  for (uint16_t i = 0; i < min(max_rows, 256); i++) {
    register uint16_t gray_addr = i ^ (i >> 1);              // Gray-Code conversion
    register uint8_t test_data = (gray_addr & 0x0f) ^ 0x05;  // Mixed pattern

    if (gray_addr >= max_rows) continue;

    PORTB = (PORTB & 0xef) | ((gray_addr & 0x0100) >> 4);
    PORTC = (PORTC & 0xe0) | ((gray_addr & 0x0200) >> 6) | (test_data & 0x0f);
    PORTD = gray_addr & 0xff;

    RAS_LOW20;
    NOP;
    CAS_LOW20;
    NOP;
    CAS_HIGH20;
    NOP;
    RAS_HIGH20;
  }
  sei();
  NOP;
  WE_HIGH20;

  // Gray-Code Verifikation
  DDRC &= 0xf0;
  OE_LOW20;

  cli();
  for (uint16_t i = 0; i < min(max_rows, 256); i++) {
    register uint16_t gray_addr = i ^ (i >> 1);
    register uint8_t expected_data = (gray_addr & 0x0f) ^ 0x05;

    if (gray_addr >= max_rows) continue;

    PORTB = (PORTB & 0xef) | ((gray_addr & 0x0100) >> 4);
    PORTC = (PORTC & 0xef) | ((gray_addr & 0x0200) >> 6);
    PORTD = gray_addr & 0xff;

    RAS_LOW20;
    CAS_LOW20;
    NOP;
    NOP;
    if ((PINC & 0x0f) != (expected_data & 0x0f)) {
      sei();
      error(32 + (i & 0x1f), 1);  // Decoder error (32-63)
    }

    CAS_HIGH20;
    RAS_HIGH20;
  }
  sei();

  OE_HIGH20;
}


// The following Routine checks if A9 Pin is used - which is the case for 1Mx4 DRAM in 20Pin Mode
void senseRAM_20Pin() {
  boolean big = true;
  DDRC |= 0x0f;  // Configure IOs for Output
  // Schreibe 0x5 in Row 0 Col 0 als Referenz-Pattern
  RAS_HIGH20;
  PORTC = (PORTC & 0xe0) | 0x05;  // A9 to LOW, Data = 0x5
  rASHandlingPin20(0);
  WE_LOW20;
  CAS_LOW20;
  CAS_HIGH20;
  // Row address line, buffer and decoder test
  for (uint8_t a = 0; a <= 9; a++) {
    uint16_t adr = (1 << a);
    // Schreibe 0xA in Row (2^a), Col 0
    rASHandlingPin20(adr);
    msbHandlingPin20(0);            // Column MSB = 0
    PORTD = 0x00;                   // Column LSB = 0
    DDRC |= 0x0f;                   // Configure IOs for Output
    PORTC = (PORTC & 0xe0) | 0x0A;  // Data = 0xA
    WE_LOW20;
    CAS_LOW20;
    CAS_HIGH20;
    WE_HIGH20;
    // Lese Row 0, Col 0 zurück
    rASHandlingPin20(0);
    PORTD = 0x00;   // Set Col Address
    PORTB &= 0xef;  // Clear address Bit 8
    PORTC &= 0xe0;  // A9 to LOW - Outputs low
    DDRC &= 0xf0;   // Configure IOs for Input
    OE_LOW20;
    CAS_LOW20;
    CAS_HIGH20;
    uint8_t read_data = PINC & 0x0f;
    if (read_data != 0x5) {  // Sollte 0x5 bleiben, außer bei Aliasing
      if (a == 9) {
        big = false;  // A9 Aliasing → 256Kx4
      } else
        error(1, 0);
    }
    OE_HIGH20;
  }
  if (big)
    type = T_514400;
  else
    type = T_514256;
}

void senseSCRAM() {
  PORTD = 0x00;   // Set Row and Col address to 0
  PORTB &= 0xef;  // Clear address Bit 8
  PORTC &= 0xe0;  // Set all Outputs and A9 to LOW
  DDRC |= 0x0f;   // IOs as Outputs
  rASHandlingPin20(0);
  WE_LOW20;
  for (uint8_t col = 0; col <= 16; col++) {
    PORTC = ((PORTC & 0xf0) | (col & 0x0f));
    PORTD = col;  // Set Col Adress
    CAS_LOW20;
    NOP;
    CAS_HIGH20;
  }
  WE_HIGH20;
  // Read Back from CS Mode Write
  rASHandlingPin20(0);
  DDRC &= 0xf0;  // Configure IOs for Input
  OE_LOW20;
  CAS_LOW20;
  for (uint8_t col = 0; col <= 16; col++) {
    PORTD = col;  // Set Col Adress
    NOP;
    NOP;
    if ((col & 0x0f) != (PINC & 0x0f)) {  // Read the Data at this address
      CAS_HIGH20;
      OE_HIGH20;
      RAS_HIGH20;
      return;
    }
  }
  CAS_HIGH20;
  OE_HIGH20;
  RAS_HIGH20;
  if (type == T_514400)
    type = T_514402;
  else
    type = T_514258;
}

//=======================================================================================
// GENERIC CODE
//=======================================================================================

void checkGNDShort() {
  if (Mode == Mode_20Pin)
    checkGNDShort4Port(CPU_20PORTB, CPU_20PORTC, CPU_20PORTD);
  else if (Mode == Mode_18Pin)
    checkGNDShort4Port(CPU_18PORTB, CPU_18PORTC, CPU_18PORTD);
  else
    checkGNDShort4Port(CPU_16PORTB, CPU_16PORTC, CPU_16PORTD);
}

// Check for Shorts to GND, when Inputs are Pullup
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

// Prepare LED for inidcation of Results or Errors
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

// Indicate Errors. Red LED for Error Type, and green for additional Error Info.
void error(uint8_t code, uint8_t error) {
#ifdef OLED
  u8x8.clearDisplay();
  switch (error) {
    case 0:
      u8x8.setFont(u8x8_font_open_iconic_embedded_4x4);
      u8x8.drawString(6, 0, "G");
      u8x8.setFont(u8x8_font_7x14B_1x2_r);
      u8x8.drawString(1, 5, "RAM Inserted?");
      break;
    case 1:
      {
        u8x8.setFont(u8x8_font_open_iconic_check_4x4);
        u8x8.drawString(6, 0, "B");
        u8x8.setFont(u8x8_font_7x14B_1x2_r);
        if (code < 16) {
          u8x8.drawString(2, 4, "Row address");
          u8x8.drawString(4, 6, "Line=A");
          u8x8.drawString(10, 6, String(code).c_str());
        } else if (code < 32) {
          u8x8.drawString(0, 4, "Column address");
          u8x8.drawString(4, 6, "Line=A");
          u8x8.drawString(10, 6, String(code >> 4).c_str());
        } else if (code < 63) {
          u8x8.drawString(1, 5, "Decoder Error");
        }
        break;
      }
    case 2:
      {
        u8x8.setFont(u8x8_font_open_iconic_check_4x4);
        u8x8.drawString(6, 0, "B");
        u8x8.setFont(u8x8_font_7x14B_1x2_r);
        u8x8.drawString(0, 5, "  RAM Faulty!");
        break;
      }
    case 3:
      {
        u8x8.setFont(u8x8_font_open_iconic_check_4x4);
        u8x8.drawString(6, 0, "B");
        u8x8.setFont(u8x8_font_7x14B_1x2_r);
        u8x8.drawString(0, 5, "Retention Error");
        break;
      }
    case 4:
      {
        u8x8.setFont(u8x8_font_open_iconic_embedded_4x4);
        u8x8.drawString(6, 0, "C");
        u8x8.setFont(u8x8_font_7x14B_1x2_r);
        u8x8.drawString(0, 5, "GND Short. P=");
        u8x8.drawString(13, 5, String(code).c_str());
        break;
      }
  }
#endif
  setupLED();
  while (true) {
    for (int i = 0; i < error; i++) {
      digitalWrite(red, ON);
      delay(500);
      digitalWrite(red, OFF);
      delay(500);
    }
    for (int i = 0; i < code; i++) {
      digitalWrite(green, ON);
      delay(150);
      digitalWrite(green, OFF);
      delay(150);
    }
    delay(1000);
  }
}

// GREEN - OFF Flashlight - Indicate a successfull test
void testOK() {
#ifdef OLED
  u8x8.clearDisplay();
  u8x8.setFont(u8x8_font_7x14B_1x2_r);
  u8x8.drawString(0, 5, ramTypes[type].name);
  u8x8.setFont(u8x8_font_open_iconic_check_4x4);
  u8x8.drawString(6, 0, "A");
#endif
  setupLED();
  while (true) {
    digitalWrite(red, OFF);
    digitalWrite(green, ON);
    delay(850);
    if (ramTypes[type].staticColumn) {
      digitalWrite(red, ON);
      delay(250);
    }
    digitalWrite(green, OFF);
    if (ramTypes[type].smallType)
      digitalWrite(red, ON);
    else
      digitalWrite(red, OFF);
    delay(250);
  }
}

// Indicate a Problem with the DipSwitch Config (Continuous Red Blink)
void ConfigFail() {
  setupLED();
  while (true) {
    digitalWrite(red, ON);
    delay(250);
    digitalWrite(red, OFF);
    delay(250);
  }
}

// This is the initial Test for soldering Problems
// Switch all DIP Switches to 0
// The LED will be green for 1 sec and red for 1 sec to test LED function. If first the Red and then the Green lites up, write 0x00 to Position 0x01 of the EEPROM.
// All Inputs will become PullUP
// One by One short the Inputs to GND which checks connection to GND. If Green LED comes on one Pin Grounded was detected, RED if it was more than one
// If Green does not lite then this contact has a problem.
// To Quit Test Mode forever set all DIP Switches to ON and Short Pin 1 to Ground for 5 Times until the Green LED is steady on. This indicates the EEPROM stored the Information.
void buildTest() {
#ifdef OLED
  u8x8.drawString(3, 2, "TEST MODE");
  u8x8.drawString(0, 4, "All DIP to '1'");
  u8x8.drawString(0, 6, "& Reset to quit");
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
    u8x8.clearDisplay();
    u8x8.drawString(0, 2, "   TEST MODE");
    u8x8.drawString(0, 4, "  DEACTIVATED");
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
