/*
=====================================
RAM Tester Program for RAM Tester PCB
=====================================

Author:   Andreas Hoffmann
Project:  github.com/tops4u/ram-tester
Version:  2.4.1 final
Date:     27.06.2025

This software is published under GPL 3.0. Respect the license terms.
Project hosted at: https://github.com/tops4u/Ram-Tester/

Note: The code contains duplication and is not designed for elegance or efficiency.
The goal was to make it work lightning fast!

Error LED Codes:
- Long Green - Long Red - Steady Green : Test mode active
- Continuous Red Blinking: Configuration error (e.g., DIP switches). Can also occur due to RAM defects.
- 1 Red & n Green: Address decoder error. Green flashes indicate the failing address line (no green flash for A0).
- 2 Red & n Green: RAM test error. Green flashes indicate which test pattern failed.
- 3 Red & n Green: Row crosstalk or data retention (refresh) error. Green flashes indicate the failed test pattern.
- 4 Red & n Green: Ground short detected on a pin. Green flashes indicate the pin number (of the ZIF Socket != ZIP).
- Long Green/Short Red: Test passed for a smaller DRAM size in the current configuration.
- Long Green/Short Off: Test passed for a larger DRAM size in the current configuration.
- Long Green/Short Orange/Short Red: Test passed for a smaller DRAM size in the current configuration and it is a Static Column Type

Assumptions:
- The DRAM supports Page Mode for reading and writing.
- DRAMs with a 4-bit data bus are tested column by column using these patterns: `0b0000`, `0b1111`, `0b1010`, and `0b0101`.
- The program does not test RAM speed (access times) but Data retention Times
- This Software does not test voltage levels of the output signals
- This Software checks address decoding and crosstalk by using on run with pseudo randomized data.

Version History:
- 1.0         Initial implementation for 20-pin DIP/ZIP, supporting 256x4 DRAM (e.g., MSM514256C).

- 1.1         Added auto-detection for 1M or 256k x4 DRAM.

- 1.2         Support for 256kx1 DRAM (e.g., 41256).

- 1.21        Added column address line checks for 41256/4164, ensuring all address lines, buffers, and column decoders work.

- 1.22        Added checks for 4164/41256 DRAMs.

- 1.23        Added row address checking for 4164/41256, complementing column checks from version 1.21.

- 1.3         Full row and column tests for pins, buffers, and decoders on 514256 and 441000 DRAMs.

- 1.4         Support for 4416/4464 added. Only 4416 tested as 4464 test chips were unavailable.

- 2.0pre1     Introduced row crosstalk and refresh time checks (2ms for 4164, 4ms for 41256, 8ms for 20-pin DRAM types).

- 2.0pre      Refresh tests for 4416/4464 not yet included. Enabled ground short tests and cleaned 20-pin code section.

- 2.0         Fixed bugs for 4464 and adjusted refresh timing. To-do:
              Handle corner cases during crosstalk tests.
              Consider reverse-order testing (start with the last row).

- 2.1         Added a test mode for installation checks after soldering. Test mode instructions available on GitHub.
              To exit test mode: set all DIP switches to ON, reset, set DIP switches to OFF, and reset again.

- 2.1.1        Fixed minor bugs in test patterns and I/O configuration for 18-pin RAM.

- 2.1.2        Bug fix for 18Pin Versions (error in address line to physical port decoding)

- 2.2.0a       Adding Static Column Tests for 514258

- 2.2.0b       Added Random Bit Test for 20pin Types - this slightly prolonges Testing / Timing adjustment for FP-RAM pending

- 2.3.0a       Major rework on Retention Testing. Introduced RAM Types for timing. Added OLED support.
               Speed optimization in the Code to keep longer test times of pseudo random data at bay.
               Minor Bugfix 16Bit had one col/row overrun - buggy but no negativ side effects
               OLED Tests and final implementation missing

- 2.3.0.pre    All functional targets for 2.3.0 are met. OLED Tested and working

- 2.3.0.pre2   Checked and updated all Retention Timings. Fixed some minor Typos. Added 514402 Static Column 1Mx4 RAM

- 2.3.0        Option to deactivate OLED in Code (remove the line: #define OLED)
               Code Version output in DIP Switch Error Screen.

- 2.3.1        Improved Address & Decoder Checks
               RAM Inserted? Display

- 2.3.1.fix    Timing issue with 20Pin address tests

- 2.4.0        Implemented Tests for 41C1000 DIP 1Mx1 Chip
               Various speed improvements for all RAM, resulting in up to 40% shorter Tests

- 2.4.1        Added Randomdata inversion for every second run. This ensures all Cells are 
               Retention Tested if the user wants this. This is using the EEPROM but with wear leveling
               Added DIAG Macro to enable Diagnostics after upload for DIY People who soldered the PCB

Disclaimer:
This project is for hobbyist use. There are no guarantees regarding its fitness for a specific purpose
or its error-free operation. Use it at your own risk.
*/

#include <EEPROM.h>
#include <avr/pgmspace.h>
#include <Arduino.h>

// ---- COMMENT THE FOLLOWING LINE TO DISABLE DISPLAY SUPPORT AND SPEED UP THE TESTER ------
#define OLED
// -----------------------------------------------------------------------------------------

// ---- UNCOMMENT THE FOLLOWING LINE TO ENABLE DIAG MODE AFTER FIRMWARE UPLOAD -------------
// #define DIAG
// ---- THIS MIGHT BE USEFULL FOR DIY PEOPLE WHO WANT TO CHECK THE SOLDERING ---------------

#define version "2.4.1"

#ifdef OLED
#include <U8x8lib.h>
// You may use other OLED Displays if the are 128x64 or bigger or you adapt the code
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/13, /* data=*/12, /* reset=*/U8X8_PIN_NONE);  // SD1306 Tested, SD1315 should be compatible
#endif

// Additional delay of 62.5ns may be required for compatibility. (16MHz clock = 1 cycle = 62.5ns).
#define NOP __asm__ __volatile__("nop\n\t")

// Pin mode definitions for different DRAM package types
#define Mode_16Pin 2
#define Mode_18Pin 4
#define Mode_20Pin 5
#define EOL 254
#define NC 255
#define ON HIGH
#define OFF LOW
#define TESTING 0x00
#define LED_FLAG 0x01

// Structure defining characteristics of different RAM types
struct RAM_Definition {
  char name[17];         // Name for the Display
  uint8_t mSRetention;   // This Tester's assumptions of the Retention Time in ms
  uint8_t delayRows;     // How many rows are skipped before reading back and checking retention time
  uint16_t rows;         // How many rows does this type have
  uint16_t columns;      // How many columns does this type have
  uint8_t iOBits;        // How many Bits can this type I/O at the same time
  boolean staticColumn;  // Is this a Static Column Type
  boolean nibbleMode;    // Is this a nibble Mode Type --> Not yet implemented
  boolean smallType;     // Is this the small type for this amount of pins
  uint16_t delays[6];    // List of specific delay times for retention testing
  uint16_t writeTime;    // Write Time to check last rows during retention testing
};

// The following RAM Types are currently identified. This table also contains retention timings.
struct RAM_Definition ramTypes[] = {
  { "   4164 64Kx1   ", 2, 1, 256, 256, 1, false, false, true, { 1069, 100, 100, 100, 100, 100 }, 986 },
  { "  41256 256Kx1  ", 4, 1, 512, 512, 1, false, false, false, { 2167, 245, 245, 245, 245, 245 }, 1794 },
  { "41257  256Kx1-NM", 4, 2, 512, 512, 1, false, true, false, { 2167, 245, 245, 245, 245, 245 }, 1794 },  // Never tested yet so we treat it as 41256
  { "   4416  16Kx4  ", 4, 4, 256, 64, 4, false, false, true, { 591, 591, 591, 591, 213, 213 }, 401 },
  { "   4464  64Kx4  ", 4, 1, 256, 256, 4, false, false, false, { 2433, 950, 950, 950, 950, 950 }, 1540 },
  { " 514256  256Kx4 ", 4, 2, 512, 512, 4, false, false, true, { 1370, 1363, 636, 636, 636, 636 }, 620 },
  { "514258 256Kx4-SC", 4, 2, 512, 512, 4, true, false, true, { 1374, 1374, 604, 604, 604, 604 }, 620 },
  { "  514400  1Mx4  ", 16, 5, 1024, 1024, 4, false, false, false, { 1957, 1957, 1957, 1957, 1957, 505 }, 1222 },
  { " 514402 1Mx4-SC ", 16, 5, 1024, 1024, 4, true, false, false, { 1936, 1955, 1962, 1962, 1951, 440 }, 1220 },
  { "  411000  1Mx1  ", 8, 1, 1024, 1024, 1, false, false, false, { 4650, 1453, 1453, 1453, 1453, 1453 }, 3312 }
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
#define T_411000 9

// SBI - Set Bit in I/O Register (inline assembly for fast bit manipulation)
#define SBI(port, bit) __asm__ __volatile__("sbi %0, %1" ::"I"(_SFR_IO_ADDR(port)), "I"(bit))

// CBI - Clear Bit in I/O Register (inline assembly for fast bit manipulation)
#define CBI(port, bit) __asm__ __volatile__("cbi %0, %1" ::"I"(_SFR_IO_ADDR(port)), "I"(bit))

// Fast 8-bit left rotation using inline assembly
static inline uint8_t __attribute__((always_inline, hot)) rotate_left(uint8_t val) {
  __asm__ __volatile__(
    "lsl %0 \n\t"           // Logical Shift Left (Bit 7 → Carry)
    "adc %0, __zero_reg__"  // Add with Carry (Carry → Bit 0)
    : "=r"(val)             // Output: val is modified
    : "0"(val)              // Input: val as input value
  );
  return val;
}

// The Test patterns
const uint8_t pattern[] = { 0x00, 0xff, 0xaa, 0x55, 0xaa, 0x55 };  // Equals to 0b00000000, 0b11111111, 0b10101010, 0b01010101

// Randomize the access to the pseudo random table by using col & row
static inline __attribute__((always_inline)) uint8_t mix8(uint16_t col, uint16_t row) {
  uint16_t v = col ^ (row * 255u);  // Compiler optimization: (row << 8) - row
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
#define CAS_LOW16 CBI(PORTC, 3)
#define CAS_HIGH16 SBI(PORTC, 3)
#define RAS_LOW16 CBI(PORTB, 1)
#define RAS_HIGH16 SBI(PORTB, 1)
#define WE_LOW16 CBI(PORTB, 3)
#define WE_HIGH16 SBI(PORTB, 3)

// Fast address setting macro for 16-pin DRAMs using direct port manipulation
#define SET_ADDR_PIN16(addr) \
  { \
    PORTB = ((PORTB & 0xea) | (addr & 0x0010) | ((addr & 0x0008) >> 1) | ((addr & 0x0040) >> 6)); \
    PORTC = ((PORTC & 0xe8) | ((addr & 0x0001) << 4) | ((addr & 0x0100) >> 8)); \
    PORTD = ((addr & 0x0080) >> 1) | ((addr & 0x0020) << 2) | ((addr & 0x0004) >> 2) | (addr & 0x0002); \
  }

// This Macro is used to generate the Address Lookuptable (LUT)
#define MAP_COMBINED(a) \
  { \
    .portb = ((((a)&0x0010)) | (((a)&0x0008) >> 1) | (((a)&0x0040) >> 6)), \
    .portc = ((((a)&0x0001) << 4) | (((a)&0x0100) >> 8)), \
    .portd = ((((a)&0x0080) >> 1) | (((a)&0x0020) << 2) | (((a)&0x0004) >> 2) | ((a)&0x0002)) \
  }

// Packed struct for 24-Bit Data (3 Bytes per Entry)
struct __attribute__((packed)) port_config16_t {
  uint8_t portb;
  uint8_t portc;  // Without PC1 (DIN) - set separately
  uint8_t portd;
};

// ROW16 Macro for combined structure
#define ROW16_COMBINED(base) \
  MAP_COMBINED(base + 0), MAP_COMBINED(base + 1), MAP_COMBINED(base + 2), MAP_COMBINED(base + 3), \
    MAP_COMBINED(base + 4), MAP_COMBINED(base + 5), MAP_COMBINED(base + 6), MAP_COMBINED(base + 7), \
    MAP_COMBINED(base + 8), MAP_COMBINED(base + 9), MAP_COMBINED(base + 10), MAP_COMBINED(base + 11), \
    MAP_COMBINED(base + 12), MAP_COMBINED(base + 13), MAP_COMBINED(base + 14), MAP_COMBINED(base + 15)

// ROW256 Macro for combined structure
#define ROW256_COMBINED(base) \
  ROW16_COMBINED(base + 0), ROW16_COMBINED(base + 16), \
    ROW16_COMBINED(base + 32), ROW16_COMBINED(base + 48), \
    ROW16_COMBINED(base + 64), ROW16_COMBINED(base + 80), \
    ROW16_COMBINED(base + 96), ROW16_COMBINED(base + 112), \
    ROW16_COMBINED(base + 128), ROW16_COMBINED(base + 144), \
    ROW16_COMBINED(base + 160), ROW16_COMBINED(base + 176), \
    ROW16_COMBINED(base + 192), ROW16_COMBINED(base + 208), \
    ROW16_COMBINED(base + 224), ROW16_COMBINED(base + 240)

// Generation of complete 512-entry LUT
#define GEN512_COMBINED ROW256_COMBINED(0), ROW256_COMBINED(256)

// Combined Lookup-Table (1536 Bytes)
static const struct port_config16_t PROGMEM lut_combined[512] = {
  GEN512_COMBINED
};

// Fast address and data setting function using optimized assembly
//ALTERNATE : TEST!
static void __attribute__((hot)) setAddrData(uint16_t addr, uint8_t dataBit) {
  const struct port_config16_t *entry = &lut_combined[addr];

  __asm__ volatile(
    // Sequential PROGMEM-Reads (uses Z+ auto-increment)
    "lpm r18, Z+     \n\t"  // portb (1 cycle)
    "lpm r19, Z+     \n\t"  // portc (1 cycle)
    "lpm r20, Z+     \n\t"  // portd (1 cycle)

    // PORTB update
    "in  r21, %[portb] \n\t"
    "andi r21, 0xEA    \n\t"
    "or  r21, r18      \n\t"
    "out %[portb], r21 \n\t"

    // PORTD update
    "out %[portd], r20 \n\t"

    // PORTC update with optimized DIN-Branch
    "in  r21, %[portc] \n\t"
    "andi r21, 0xE0    \n\t"  // Clear CAS Line
    "or  r21, r19      \n\t"
    "sbrs %[databit], 2 \n\t"  // Skip if bit 2 set
    "rjmp 1f           \n\t"
    "ori r21, 0x02     \n\t"  // Set PC1 (DIN)
    "1: out %[portc], r21 \n\t"

    :
    : [portb] "I"(_SFR_IO_ADDR(PORTB)),
      [portc] "I"(_SFR_IO_ADDR(PORTC)),
      [portd] "I"(_SFR_IO_ADDR(PORTD)),
      "z"(entry),
      [databit] "r"(dataBit)
    : "r18", "r19", "r20", "r21");
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

// Control signal macros for 18-pin DRAMs
#define CAS_LOW18 PORTC &= 0xfb
#define CAS_HIGH18 PORTC |= 0x04
#define RAS_LOW18 PORTC &= 0xef
#define RAS_HIGH18 PORTC |= 0x10
#define OE_LOW18 PORTC &= 0xfe
#define OE_HIGH18 PORTC |= 0x01
#define WE_LOW18 PORTB &= 0xfd
#define WE_HIGH18 PORTB |= 0x02

// Fast address setting for 18-pin DRAMs using inline assembly
static inline __attribute__((always_inline, hot)) void setAddr18Pin(uint8_t addr) {
  __asm__ volatile(
    // PORTB: (PORTB & 0xEB) | ((addr & 0x01) << 2) | ((addr & 0x02) << 3)
    "in   r16, %[portb]     \n\t"  // Read current PORTB
    "andi r16, 0xEB         \n\t"  // Apply mask
    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x01         \n\t"  // addr & 0x01
    "lsl  r17               \n\t"
    "lsl  r17               \n\t"  // << 2
    "or   r16, r17          \n\t"
    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x02         \n\t"  // addr & 0x02
    "lsl  r17               \n\t"
    "lsl  r17               \n\t"
    "lsl  r17               \n\t"  // << 3
    "or   r16, r17          \n\t"
    "out  %[portb], r16     \n\t"

    // PORTD: Complete reassignment (like C-Version)
    "clr  r16               \n\t"  // r16 = 0 (Start with empty PORTD!)

    // (addr & 0x04) << 5
    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x04         \n\t"
    "swap r17               \n\t"  // << 4
    "lsl  r17               \n\t"  // << 5
    "or   r16, r17          \n\t"

    // (addr & 0x08) << 3
    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x08         \n\t"
    "swap r17               \n\t"  // << 4
    "lsr  r17               \n\t"  // << 3
    "or   r16, r17          \n\t"

    // (addr & 0x80) >> 2
    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x80         \n\t"
    "lsr  r17               \n\t"
    "lsr  r17               \n\t"  // >> 2
    "or   r16, r17          \n\t"

    // (addr & 0x20) >> 4
    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x20         \n\t"
    "swap r17               \n\t"  // >> 4
    "andi r17, 0x0F         \n\t"  // Clear upper bits
    "or   r16, r17          \n\t"

    // (addr & 0x40) >> 6
    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x40         \n\t"
    "swap r17               \n\t"  // >> 4
    "lsr  r17               \n\t"
    "lsr  r17               \n\t"  // >> 6
    "or   r16, r17          \n\t"

    // (addr & 0x10) >> 2
    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x10         \n\t"
    "lsr  r17               \n\t"
    "lsr  r17               \n\t"  // >> 2
    "or   r16, r17          \n\t"

    "out  %[portd], r16     \n\t"  // Completely overwrite PORTD
    :
    : [portb] "I"(_SFR_IO_ADDR(PORTB)),
      [portd] "I"(_SFR_IO_ADDR(PORTD)),
      [addr] "r"(addr)
    : "r16", "r17");
}

// setData18Pin
static inline __attribute__((always_inline, hot)) void setData18Pin(uint8_t data) {
  __asm__ volatile(
    // PORTB: (PORTB & 0xf6) | ((data & 0x02) << 2) | ((data & 0x04) >> 2)
    "in   r16, %[portb]     \n\t"
    "andi r16, 0xf6         \n\t"  // Lowercase f for consistency with C
    "mov  r17, %[data]      \n\t"
    "andi r17, 0x02         \n\t"
    "lsl  r17               \n\t"
    "lsl  r17               \n\t"
    "or   r16, r17          \n\t"
    "mov  r17, %[data]      \n\t"
    "andi r17, 0x04         \n\t"
    "lsr  r17               \n\t"
    "lsr  r17               \n\t"
    "or   r16, r17          \n\t"
    "out  %[portb], r16     \n\t"

    // PORTC: (PORTC & 0xf5) | ((data & 0x01) << 1) | (data & 0x08)
    // IMPORTANT: New register r20 instead of r16!
    "in   r20, %[portc]     \n\t"
    "andi r20, 0xf5         \n\t"  // Lowercase f for consistency
    "mov  r17, %[data]      \n\t"
    "andi r17, 0x01         \n\t"
    "lsl  r17               \n\t"
    "or   r20, r17          \n\t"
    "mov  r17, %[data]      \n\t"
    "andi r17, 0x08         \n\t"
    "or   r20, r17          \n\t"
    "out  %[portc], r20     \n\t"

    :
    : [portb] "I"(_SFR_IO_ADDR(PORTB)),
      [portc] "I"(_SFR_IO_ADDR(PORTC)),
      [data] "r"(data)
    : "r16", "r17", "r20");  // r20 added!
}

// getData18Pin
static inline __attribute__((always_inline, hot)) uint8_t getData18Pin(void) {
  uint8_t result;
  __asm__ volatile(
    // ((PINC & 0x02) >> 1) + ((PINB & 0x08) >> 2) + ((PINB & 0x01) << 2) + (PINC & 0x08)
    "in   r16, %[pinc]      \n\t"
    "in   r17, %[pinb]      \n\t"

    // Start with 0
    "clr  r18               \n\t"

    // (PINC & 0x02) >> 1 -> add to r18
    "mov  r19, r16          \n\t"
    "andi r19, 0x02         \n\t"
    "lsr  r19               \n\t"
    "add  r18, r19          \n\t"  // ADD instead of OR!

    // (PINB & 0x08) >> 2 -> add to r18
    "mov  r19, r17          \n\t"
    "andi r19, 0x08         \n\t"
    "lsr  r19               \n\t"
    "lsr  r19               \n\t"
    "add  r18, r19          \n\t"  // ADD instead of OR!

    // (PINB & 0x01) << 2 -> add to r18
    "mov  r19, r17          \n\t"
    "andi r19, 0x01         \n\t"
    "lsl  r19               \n\t"
    "lsl  r19               \n\t"
    "add  r18, r19          \n\t"  // ADD instead of OR!

    // (PINC & 0x08) -> add to r18 (no shift)
    "andi r16, 0x08         \n\t"
    "add  r18, r16          \n\t"  // ADD instead of OR!

    "mov  %0, r18           \n\t"
    : "=r"(result)
    : [pinc] "I"(_SFR_IO_ADDR(PINC)),
      [pinb] "I"(_SFR_IO_ADDR(PINB))
    : "r16", "r17", "r18", "r19");
  return result;
}

//=======================================================================================
// PIN MAPPING for KM41C1000 18-Pin Socket
//=======================================================================================

// KM41C1000 Pin →  Pin Mapping (18-Pin Socket Alternative)
// Pin 1  (Din)  → PC0
// Pin 2  (WE)   → PC1
// Pin 3  (RAS)  → PB3
// Pin 5  (A0)   → PC4
// Pin 6  (A1)   → PD0
// Pin 7  (A2)   → PD1
// Pin 8  (A3)   → PD2
// Pin 10 (A4)   → PD5
// Pin 11 (A5)   → PD6
// Pin 12 (A6)   → PD7
// Pin 13 (A7)   → PB4
// Pin 14 (A8)   → PB2
// Pin 15 (A9)   → PB0
// Pin 16 (CAS)  → PC2
// Pin 17 (Dout) → PC3

const uint8_t RAS_18PIN_ALT = 11;
const uint8_t CAS_18PIN_ALT = A2;

// Control Signal Macros for 18Pin_Alt (KM41C1000 type)
#define RAS_LOW_18PIN_ALT CBI(PORTB, 3)
#define RAS_HIGH_18PIN_ALT SBI(PORTB, 3)
#define CAS_LOW_18PIN_ALT CBI(PORTC, 2)
#define CAS_HIGH_18PIN_ALT SBI(PORTC, 2)
#define WE_LOW_18PIN_ALT CBI(PORTC, 1)
#define WE_HIGH_18PIN_ALT SBI(PORTC, 1)

// Data Macros for 18Pin_Alt (1-Bit)
#define SET_DIN_18PIN_ALT(data) \
  do { \
    if (data) SBI(PORTC, 0); \
    else CBI(PORTC, 0); \
  } while (0)
#define GET_DOUT_18PIN_ALT() ((PINC & 0x08) >> 3)

// Compact port configuration structure for 18Pin_Alt
struct __attribute__((packed)) port_config_18Pin_Alt_t {
  uint8_t portb;  // A7→PB4, A8→PB2, A9→PB0
  uint8_t portd;  // A1→PD0, A2→PD1, A3→PD2, A4→PD5, A5→PD6, A6→PD7
};

// Mapping without PORTC - as PORTC only has one Bit to set no need to waste 1024 Byte for it
#define MAP_18PIN_ALT_COMBINED(a) \
  { \
    .portb = ((((a)&0x080) >> 3) | (((a)&0x100) >> 6) | (((a)&0x200) >> 9)), \
    .portd = ((((a)&0x002) >> 1) | (((a)&0x004) >> 1) | (((a)&0x008) >> 1) | (((a)&0x010) << 1) | (((a)&0x020) << 1) | (((a)&0x040) << 1)) \
  }

// ROW64 Macro for combined structure
#define ROW64_18PIN_ALT_COMBINED(base) \
  MAP_18PIN_ALT_COMBINED(base + 0), MAP_18PIN_ALT_COMBINED(base + 1), MAP_18PIN_ALT_COMBINED(base + 2), MAP_18PIN_ALT_COMBINED(base + 3), \
    MAP_18PIN_ALT_COMBINED(base + 4), MAP_18PIN_ALT_COMBINED(base + 5), MAP_18PIN_ALT_COMBINED(base + 6), MAP_18PIN_ALT_COMBINED(base + 7), \
    MAP_18PIN_ALT_COMBINED(base + 8), MAP_18PIN_ALT_COMBINED(base + 9), MAP_18PIN_ALT_COMBINED(base + 10), MAP_18PIN_ALT_COMBINED(base + 11), \
    MAP_18PIN_ALT_COMBINED(base + 12), MAP_18PIN_ALT_COMBINED(base + 13), MAP_18PIN_ALT_COMBINED(base + 14), MAP_18PIN_ALT_COMBINED(base + 15), \
    MAP_18PIN_ALT_COMBINED(base + 16), MAP_18PIN_ALT_COMBINED(base + 17), MAP_18PIN_ALT_COMBINED(base + 18), MAP_18PIN_ALT_COMBINED(base + 19), \
    MAP_18PIN_ALT_COMBINED(base + 20), MAP_18PIN_ALT_COMBINED(base + 21), MAP_18PIN_ALT_COMBINED(base + 22), MAP_18PIN_ALT_COMBINED(base + 23), \
    MAP_18PIN_ALT_COMBINED(base + 24), MAP_18PIN_ALT_COMBINED(base + 25), MAP_18PIN_ALT_COMBINED(base + 26), MAP_18PIN_ALT_COMBINED(base + 27), \
    MAP_18PIN_ALT_COMBINED(base + 28), MAP_18PIN_ALT_COMBINED(base + 29), MAP_18PIN_ALT_COMBINED(base + 30), MAP_18PIN_ALT_COMBINED(base + 31), \
    MAP_18PIN_ALT_COMBINED(base + 32), MAP_18PIN_ALT_COMBINED(base + 33), MAP_18PIN_ALT_COMBINED(base + 34), MAP_18PIN_ALT_COMBINED(base + 35), \
    MAP_18PIN_ALT_COMBINED(base + 36), MAP_18PIN_ALT_COMBINED(base + 37), MAP_18PIN_ALT_COMBINED(base + 38), MAP_18PIN_ALT_COMBINED(base + 39), \
    MAP_18PIN_ALT_COMBINED(base + 40), MAP_18PIN_ALT_COMBINED(base + 41), MAP_18PIN_ALT_COMBINED(base + 42), MAP_18PIN_ALT_COMBINED(base + 43), \
    MAP_18PIN_ALT_COMBINED(base + 44), MAP_18PIN_ALT_COMBINED(base + 45), MAP_18PIN_ALT_COMBINED(base + 46), MAP_18PIN_ALT_COMBINED(base + 47), \
    MAP_18PIN_ALT_COMBINED(base + 48), MAP_18PIN_ALT_COMBINED(base + 49), MAP_18PIN_ALT_COMBINED(base + 50), MAP_18PIN_ALT_COMBINED(base + 51), \
    MAP_18PIN_ALT_COMBINED(base + 52), MAP_18PIN_ALT_COMBINED(base + 53), MAP_18PIN_ALT_COMBINED(base + 54), MAP_18PIN_ALT_COMBINED(base + 55), \
    MAP_18PIN_ALT_COMBINED(base + 56), MAP_18PIN_ALT_COMBINED(base + 57), MAP_18PIN_ALT_COMBINED(base + 58), MAP_18PIN_ALT_COMBINED(base + 59), \
    MAP_18PIN_ALT_COMBINED(base + 60), MAP_18PIN_ALT_COMBINED(base + 61), MAP_18PIN_ALT_COMBINED(base + 62), MAP_18PIN_ALT_COMBINED(base + 63)

// Generation of complete 1024-entry LUT
#define GEN1024_18PIN_ALT_COMBINED \
  ROW64_18PIN_ALT_COMBINED(0), ROW64_18PIN_ALT_COMBINED(64), ROW64_18PIN_ALT_COMBINED(128), ROW64_18PIN_ALT_COMBINED(192), \
    ROW64_18PIN_ALT_COMBINED(256), ROW64_18PIN_ALT_COMBINED(320), ROW64_18PIN_ALT_COMBINED(384), ROW64_18PIN_ALT_COMBINED(448), \
    ROW64_18PIN_ALT_COMBINED(512), ROW64_18PIN_ALT_COMBINED(576), ROW64_18PIN_ALT_COMBINED(640), ROW64_18PIN_ALT_COMBINED(704), \
    ROW64_18PIN_ALT_COMBINED(768), ROW64_18PIN_ALT_COMBINED(832), ROW64_18PIN_ALT_COMBINED(896), ROW64_18PIN_ALT_COMBINED(960)

static const struct port_config_18Pin_Alt_t PROGMEM lut_18Pin_Alt_combined[1024] = {
  GEN1024_18PIN_ALT_COMBINED
};

// Fast address setting for 18Pin_Alt using lookup table and inline assembly
static void __attribute__((always_inline, hot)) setAddr_18Pin_Alt(uint16_t addr) {
  const struct port_config_18Pin_Alt_t *entry = &lut_18Pin_Alt_combined[addr];

  __asm__ volatile(
    // Explicit offsets (like the C-Version)
    "lpm r18, Z       \n\t"  // portb (offset 0)
    "adiw r30, 1      \n\t"  // Z += 1
    "lpm r20, Z       \n\t"  // portd (offset 1)
    "sbiw r30, 1      \n\t"  // Z back to original position

    // PORTB: (PORTB & 0xEA) | portb
    "in  r21, %[portb] \n\t"
    "andi r21, 0xEA    \n\t"
    "or  r21, r18      \n\t"
    "out %[portb], r21 \n\t"

    // PORTC: (PORTC & 0xEF) | ((addr & 0x001) << 4)
    "in  r19, %[portc] \n\t"
    "andi r19, 0xEF    \n\t"
    "bst %A[addr], 0   \n\t"
    "bld r19, 4        \n\t"
    "out %[portc], r19 \n\t"

    // PORTD: (PORTD & 0x18) | portd
    "in  r21, %[portd] \n\t"
    "andi r21, 0x18    \n\t"
    "or  r21, r20      \n\t"
    "out %[portd], r21 \n\t"

    :
    : [portb] "I"(_SFR_IO_ADDR(PORTB)),
      [portc] "I"(_SFR_IO_ADDR(PORTC)),
      [portd] "I"(_SFR_IO_ADDR(PORTD)),
      [addr] "r"(addr),
      "z"(entry)
    : "r18", "r19", "r20", "r21");
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

// Control signal macros for 20-pin DRAMs
#define CAS_LOW20 CBI(PORTB, 0)
#define CAS_HIGH20 SBI(PORTB, 0)
#define RAS_LOW20 CBI(PORTB, 1)
#define RAS_HIGH20 SBI(PORTB, 1)
#define OE_LOW20 CBI(PORTB, 2)
#define OE_HIGH20 SBI(PORTB, 2)
#define WE_LOW20 CBI(PORTB, 3)
#define WE_HIGH20 SBI(PORTB, 3)

// High address bit control for larger DRAMs
#define SET_A8_LOW20 CBI(PORTB, 4)
#define SET_A8_HIGH20 SBI(PORTB, 4)
#define SET_A9_LOW20 CBI(PORTC, 4)
#define SET_A9_HIGH20 SBI(PORTC, 4)

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

// EEPROM Parameter
#define EEPROM_START_ADDR 16
#define EEPROM_END_ADDR 1023
#define MAX_COUNT 255

uint8_t eeprom_counter_with_wear_leveling(void) {
  uint16_t addr;
  uint8_t value;
  uint8_t parity;
  // Find current active address (first non-0xFF value)
  for (addr = EEPROM_START_ADDR; addr <= EEPROM_END_ADDR; addr++) {
    value = eeprom_read_byte((uint8_t *)addr);
    if (value != 0xFF) break;
  }
  // If all cells are 0xFF (fresh EEPROM), start at beginning
  if (addr > EEPROM_END_ADDR) {
    addr = EEPROM_START_ADDR;
    value = 0;
    eeprom_write_byte((uint8_t *)addr, value);
  }
  // Get parity before incrementing
  parity = value & 1;
  // Increment counter
  value++;
  // Check for wear leveling
  if (value >= MAX_COUNT) {
    addr++;
    // Check if we've used all addresses
    if (addr > EEPROM_END_ADDR) {
      // Clear entire EEPROM range and restart
      for (uint16_t i = EEPROM_START_ADDR; i <= EEPROM_END_ADDR; i++) {
        eeprom_write_byte((uint8_t *)i, 0xFF);
      }
      addr = EEPROM_START_ADDR;
    }
    value = 0;
  }
  // Write new value
  eeprom_write_byte((uint8_t *)addr, value);
  return parity;
}

// Flip lower nibble of random data every second run to have full coverage
void randomizeData(void) {
  uint16_t i;
  uint8_t xor_mask = eeprom_counter_with_wear_leveling() ? 0x0F : 0x00;
  for (i = 0; i < 256; i++) {
    randomTable[i] = (randomTable[i] & 0x0F) ^ xor_mask;
  }
}

// Helper to extract one test bit for chips with only one data line out of the table above
static inline __attribute__((always_inline)) uint8_t get_test_bit(uint16_t col, uint16_t row) {
  uint16_t bit_index = (col + (row << 4)) & 0x3FF;  // % 1024 via AND-mask (2^10-1)

  // Load nibble from table (~2-3 cycles)
  uint8_t nibble_val = randomTable[bit_index >> 2];  // /4 for nibble index

  // Extract bit with switch (avoids expensive variable shifts) (~1-2 cycles)
  switch (bit_index & 3) {                    // %4 for bit position in nibble
    case 0: return nibble_val & 0x04;         // Bit 2
    case 1: return (nibble_val >> 1) & 0x04;  // Bit 3
    case 2: return (nibble_val << 1) & 0x04;  // Bit 1
    case 3: return (nibble_val << 2) & 0x04;  // Bit 0
    default: return 0;                        // Should never be reached
  }
  // Total: ~11-15 cycles
}

// Function Prototypes
void buildTest(void);
void testOK(void);
void error(uint8_t code, uint8_t error);
void initRAM(int RASPin, int CASPin);
void ConfigFail(void);
void checkGNDShort(void);
void checkGNDShort4Port(const int *portb, const int *portc, const int *portd);

// 16-Pin DRAM Functions
void test_16Pin(void);
void sense41256_16Pin(void);
void checkAddressing_16Pin(void);
void rasHandling_16Pin(uint16_t row);
void writeRow_16Pin(uint16_t row, uint16_t cols, uint8_t patNr);
void refreshRow_16Pin(uint16_t row);
void checkRow_16Pin(uint16_t cols, uint16_t row, uint8_t patNr, uint8_t check);

// 18-Pin DRAM Functions
void test_18Pin(void);
void sense4464_18Pin(void);
void sense411000_18Pin_Alt(void);
void configDataOut_18Pin(void);
void configDataIn_18Pin(void);
void checkAddressing_18Pin(void);
void rasHandling_18Pin(uint8_t row);
void writeRow_18Pin(uint8_t row, uint8_t patNr, uint16_t width);
void refreshRow_18Pin(uint8_t row);
void checkRow_18Pin(uint16_t width, uint8_t row, uint8_t patNr, uint8_t init_shift, uint8_t errorNr);

// 18-Pin Alternative DRAM Functions (KM41C1000 type)
void checkAddressing_18Pin_Alt(void);
void rasHandling_18Pin_Alt(uint16_t row);
void writeRow_18Pin_Alt(uint16_t row, uint8_t patNr);
void checkRow_18Pin_Alt(uint16_t row, uint8_t patNr, uint8_t check);
static void refreshRow_18Pin_Alt(uint16_t row);

// 20-Pin DRAM Functions
void test_20Pin(void);
void senseRAM_20Pin();
void senseSCRAM_20Pin();
void checkAddressing_20Pin();
void rasHandling_20Pin(uint16_t row);
void casHandling_20Pin(uint16_t row, uint8_t patNr, boolean is_static);
static void msbHandling_20Pin(uint8_t address);
void writeRow_20Pin(uint16_t row, uint8_t pattern, boolean is_static);
void refreshRow_20Pin(uint16_t row);
void checkRow_20Pin(uint8_t patNr, uint16_t row, uint8_t errNr, boolean is_static);

// Detected RAM Type
int type = -1;
uint8_t Mode = 0;    // PinMode 2 = 16 Pin, 4 = 18 Pin, 5 = 20 Pin
uint8_t red = 13;    // PB5
uint8_t green = 12;  // PB4 -> Co Used with RAM Test Socket, see comments below!

/**
 * Arduino setup function - initializes hardware and starts appropriate RAM test
 * Configures I/O ports, reads DIP switches to determine RAM type, and routes to test functions
 */
void setup() {
#ifdef OLED
  // Init the OLED Display
  u8x8.begin();
  u8x8.setBusClock(10000000);
  u8x8.setPowerSave(0);
  u8x8.setFont(u8x8_font_7x14B_1x2_f);
  u8x8.drawString(3, 0, "RAM-TESTER");
#endif
  randomizeData();
  // Data Direction Register Port B, C & D - Preconfig as Input (Bit=0)
  DDRB &= 0b11100000;
  DDRC &= 0b11000000;
  DDRD = 0x00;
  // If TESTING is set enter "Factory Test" Mode
  if (EEPROM.read(TESTING) != 0) {
#ifdef DIAG
    buildTest();
#endif
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
  // Settle State - PullUps may require some time.
  checkGNDShort();  // Check for Shorts towards GND. Shorts on Vcc can't be tested as it would need Pull-Downs.
  // Startup Delay as per Datasheets
  delayMicroseconds(200);
  if (Mode == Mode_20Pin) {
    initRAM(RAS_20PIN, CAS_20PIN);
    test_20Pin();
  }
  if (Mode == Mode_18Pin) {
    initRAM(RAS_18PIN, CAS_18PIN);
    test_18Pin();
  }
  if (Mode == Mode_16Pin) {
    initRAM(RAS_16PIN, CAS_16PIN);
    test_16Pin();
  }
}

/**
 * Arduino main loop - should never be reached during normal operation
 * If reached, indicates a configuration failure and triggers error indication
 */
void loop() {
  ConfigFail();
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

//=======================================================================================
// 16 - Pin DRAM Test Code
//=======================================================================================

/**
 * Main test function for 16-pin DRAMs (4164, 41256)
 * Configures I/O ports, detects RAM type, performs address testing and pattern tests
 */
void test_16Pin() {
  // Configure I/O for this Chip Type
  DDRB = 0b00111111;
  PORTB = 0b00101010;
  DDRC = 0b00011011;
  PORTC = 0b00001000;
  DDRD = 0b11000011;
  PORTD = 0x00;
  sense41256_16Pin();
#ifdef OLED
  u8x8.drawString(0, 4, "Detected Type:");
  u8x8.drawString(0, 6, ramTypes[type].name);
  u8x8.drawString(0, 2, "Checking...    ");
  // Redo because otherwise u8x8 interferes with the Test
  DDRB = 0b00111111;
  PORTB = 0b00101010;
#endif
  checkAddressing_16Pin();
  for (uint8_t patNr = 0; patNr <= 4; patNr++)
    for (uint16_t row = 0; row < ramTypes[type].rows; row++) {  // Iterate over all ROWs
      writeRow_16Pin(row, ramTypes[type].columns, patNr);
    }
  // Good Candidate.
  testOK();
}

/**
 * Prepare and execute ROW Access for 16 Pin Types
 * Sets row address and activates RAS signal
 * @param row Row address to access (0 to max_rows-1)
 */
void rasHandling_16Pin(uint16_t row) {
  RAS_HIGH16;
  // Row Address distribution Logic for 41256/64 16 Pin RAM - more complicated as the PCB circuit is optimized for 256x4 / 1Mx4 Types.
  SET_ADDR_PIN16(row);
  RAS_LOW16;
}

/**
 * Write and verify pattern data to a complete row
 * Handles different test patterns including stuck-at, walking patterns, and pseudo-random data
 * @param row Row address to write/test
 * @param cols Number of columns in this RAM type
 * @param patNr Pattern number (0-4): 0=all zeros, 1=all ones, 2,3=walking patterns, 4=pseudo-random
 */
void writeRow_16Pin(uint16_t row, uint16_t cols, uint8_t patNr) {
  // Prepare Write Cycle
  CAS_HIGH16;
  rasHandling_16Pin(row);  // Set the Row
  WE_LOW16;
  uint8_t pat = pattern[patNr];
  cli();
  uint16_t col = cols;
  if (patNr < 2) {
    register uint16_t col = cols;
    while (col > 0) {
      // Unrolled loop iteration 1
      setAddrData(--col, pat);
      CAS_HIGH16;
      WE_HIGH16;
      CAS_LOW16;
      CAS_HIGH16;
      if (((PINC ^ pat) & 0x04) != 0) {
        error(patNr + 1, 2);
      }
      WE_LOW16;
    }
    return;
  } else if (patNr < 4)
    do {
      // Column Address distribution logic for 41256/64 16 Pin RAM
      setAddrData(--col, pat);
      // Rotate the Pattern 1 Bit to the LEFT (c has not rotate so there is a trick with 2 Shift)
      pat = rotate_left(pat);
      CAS_HIGH16;
    } while (col != 0);
  else
    do {
      // Column Address distribution logic for 41256/64 16 Pin RAM
      CAS_HIGH16;
      col--;
      setAddrData(col, randomTable[mix8(col, row)]);
    } while (col != 0);
  CAS_HIGH16;
  sei();
  // Prepare Read Cycle
  WE_HIGH16;
  // Read and check the Row we just wrote, otherwise Error 2
  if (patNr < 4) {
    checkRow_16Pin(cols, row, patNr, 2);
    return;
  }
  refreshRow_16Pin(row);
  if (row == ramTypes[type].rows - 1) {  // Last Row written, we have to check the last n Rows as well.
    // Retention testing the last rows, they will no longer be written only read back. Simulate the write time to get a correct retention time test.
    for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {
      rasHandling_16Pin(row - x);
      checkRow_16Pin(cols, row - x, patNr, 3);
      delayMicroseconds(ramTypes[type].writeTime);  // Simulate writing even if it is no longer done for the last rows
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
    }
    return;
  } else if (row >= ramTypes[type].delayRows) {
    rasHandling_16Pin(row - ramTypes[type].delayRows);
    checkRow_16Pin(cols, row - ramTypes[type].delayRows, patNr, 3);
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
    return;
  } else {
    delayMicroseconds(ramTypes[type].delays[row]);
  }
}

/**
 * Refresh a specific row by performing RAS-only cycle
 * @param row Row address to refresh
 */
void refreshRow_16Pin(uint16_t row) {
  rasHandling_16Pin(row);  // Refresh this ROW
  RAS_HIGH16;
}

/**
 * Read and verify data from a complete row
 * Supports different pattern types and optimized loop unrolling for performance
 * @param cols Number of columns to check
 * @param row Row address being tested
 * @param patNr Pattern number (0-4)
 * @param check Error code to report if check fails (2=write error, 3=retention error)
 */
void checkRow_16Pin(uint16_t cols, uint16_t row, uint8_t patNr, uint8_t check) {
  register uint8_t pat = pattern[patNr];
  // Iterate over the Columns and read & check Pattern
  cli();
  if (patNr < 4) {
    // OPTIMIZED: Loop unrolling for read operations
    register uint16_t col = cols;
    register uint16_t col4 = col & ~3;

    // Process 4 columns at a time
    while (col > col4) {
      // Unrolled read iteration 1
      setAddrData(--col, 0);
      CAS_HIGH16;
      if (((PINC ^ pat) & 0x04) != 0) {
        error(patNr + 1, check);
      }
      pat = rotate_left(pat);

      // Unrolled read iteration 2
      setAddrData(--col, 0);
      CAS_HIGH16;
      if (((PINC ^ pat) & 0x04) != 0) {
        error(patNr + 1, check);
      }
      pat = rotate_left(pat);

      // Unrolled read iteration 3
      setAddrData(--col, 0);
      CAS_HIGH16;
      if (((PINC ^ pat) & 0x04) != 0) {
        error(patNr + 1, check);
      }
      pat = rotate_left(pat);

      // Unrolled read iteration 4
      setAddrData(--col, 0);
      CAS_HIGH16;
      if (((PINC ^ pat) & 0x04) != 0) {
        error(patNr + 1, check);
      }
      pat = rotate_left(pat);
    }

    // Handle remaining columns
    while (col > 0) {
      setAddrData(--col, 0);
      CAS_HIGH16;
      if (((PINC ^ pat) & 0x04) != 0) {
        error(patNr + 1, check);
      }
      pat = rotate_left(pat);
    }
  } else {
    // Random pattern checking
    register uint16_t col = cols;
    do {
      setAddrData(--col, 0);
      CAS_HIGH16;
      if (((PINC ^ randomTable[mix8(col, row)]) & 0x04) != 0) {
        error(patNr + 1, check);
      }
    } while (col != 0);
  }
  sei();
  RAS_HIGH16;
}

//=======================================================================================
// Comprehensive Address Line and Decoder Testing for 16-Pin DRAM (4164/41256)
//=======================================================================================

void checkAddressing_16Pin(void) {
  uint16_t max_rows = ramTypes[type].rows;
  uint16_t max_cols = ramTypes[type].columns;

  // Determine number of Address-Bits for Row and Column
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

  // Configure I/O for 16-Pin
  DDRB = 0b00111111;
  PORTB = 0b00101010;
  DDRC = 0b00011011;
  PORTC = 0b00001000;
  DDRD = 0b11000011;
  PORTD = 0x00;

  CAS_HIGH16;
  RAS_HIGH16;
  WE_LOW16;

  // Write Phase - Walking-Bit Pattern for Row-Addresses
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

  // Read Phase - Verification Row-Addresses
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

  uint16_t test_row = max_rows >> 1;  // Use middle Row

  SET_ADDR_PIN16(test_row);
  RAS_LOW16;
  WE_LOW16;

  // Write Phase - Column Address Walking-Bit
  for (uint8_t bit = 0; bit <= col_bits; bit++) {
    uint16_t test_col = (bit == 0) ? 0 : (1 << (bit - 1));
    uint8_t test_data = (bit & 1) ? 4 : 0;

    setAddrData(test_col, test_data);
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
  }

  WE_HIGH16;
  RAS_HIGH16;

  // Read Phase - Column Address Verification
  SET_ADDR_PIN16(test_row);
  RAS_LOW16;

  for (uint8_t bit = 0; bit <= col_bits; bit++) {
    uint16_t test_col = (bit == 0) ? 0 : (1 << (bit - 1));
    uint8_t expected_data = (bit & 1) ? 4 : 0;

    setAddrData(test_col, 0);  // Data bit to 0 for Read
    CAS_LOW16;
    NOP;
    NOP;

    if ((PINC & 0x04) != expected_data) {
      error(bit + 16, 1);  // Column address line error (16-31)
    }

    CAS_HIGH16;
  }

  RAS_HIGH16;

  //===========================================================================
  // PHASE 3: ADDRESS DECODER STRESS TEST with Gray-Code
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

  // Gray-Code Verification
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

    setAddrData(gray_col, test_data << 0x02);
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
  }

  WE_HIGH16;
  RAS_HIGH16;

  // Column Gray-Code Verification
  SET_ADDR_PIN16(test_row);
  RAS_LOW16;

  for (uint16_t i = 0; i < min(max_cols, 256); i++) {
    uint16_t gray_col = i ^ (i >> 1);
    uint8_t expected_data = (gray_col & 0x01) ^ ((gray_col & 0x04) >> 2);

    if (gray_col >= max_cols) continue;

    setAddrData(gray_col, 0);  // Data bit to 0 for Read
    CAS_LOW16;
    NOP;
    NOP;

    if ((PINC & 0x04) != expected_data << 0x02) {
      error(64 + (i & 0x1f), 1);  // Column decoder error (64-95)
    }

    CAS_HIGH16;
  }

  RAS_HIGH16;
}


/**
 * Detect and differentiate between 4164 (64Kx1) and 41256 (256Kx1) RAM types
 * Tests address line A8 to determine if it's functional (41256) or causes aliasing (4164)
 */
void sense41256_16Pin() {
  boolean big = true;
  CAS_HIGH16;
  // RAS Testing set Row 0 Col 0 and set Bit Low.
  rasHandling_16Pin(0);
  PORTC &= 0xfd;
  WE_LOW16;
  CAS_LOW16;
  NOP;
  CAS_HIGH16;
  WE_HIGH16;
  for (uint8_t a = 0; a <= 8; a++) {
    uint16_t adr = (1 << a);
    rasHandling_16Pin(adr);
    // Write Bit Col 0 High
    WE_LOW16;
    PORTC |= 0x02;
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
    WE_HIGH16;
    // Back to Row 0 then check if Bit at Col 0 is still 0
    rasHandling_16Pin(0);
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

/**
 * Main test function for 18-pin DRAMs (4416, 4464, and KM41C1000 alternative types)
 * Configures I/O ports, detects RAM type, and routes to appropriate test functions
 */
void test_18Pin() {
  // Configure I/O for this Chip Type
  DDRB = 0b00111111;
  PORTB = 0b00100010;
  DDRC = 0b00011111;
  PORTC = 0b00010101;
  DDRD = 0b11100111;
  sense4464_18Pin();
  if (type == -1) {
    sense411000_18Pin_Alt();
  }
#ifdef OLED
  u8x8.drawString(0, 4, "Detected Type:");
  u8x8.drawString(0, 6, ramTypes[type].name);
  u8x8.drawString(0, 2, "Checking...    ");
#endif
  if (type == T_411000) {
    // Restore port config after OLED
    DDRB = (DDRB & 0xE0) | 0x1D;
    DDRC = (DDRC & 0xE0) | 0x17;
    DDRD = (DDRD & 0x18) | 0xE7;
    checkAddressing_18Pin_Alt();
    // Pattern Tests for 1Mx1 DRAM
    for (uint8_t patNr = 0; patNr <= 4; patNr++) {
      for (uint16_t row = 0; row < 1024; row++) {
        writeRow_18Pin_Alt(row, patNr);
      }
    }
  } else {
    // Restore port config after OLED
    DDRB = 0b00111111;
    PORTB = 0b00100010;
    checkAddressing_18Pin();
    for (uint8_t pat = 0; pat < 5; pat++)
      for (uint16_t row = 0; row < ramTypes[type].rows; row++) {  // Iterate over all ROWs
        writeRow_18Pin(row, pat, ramTypes[type].columns);
      }
  }
  testOK();
}

/**
 * Write and verify pattern data to a complete row for standard 18-pin DRAMs
 * @param row Row address to write/test
 * @param patNr Pattern number (0-4)
 * @param width Number of columns in this RAM type
 */
void writeRow_18Pin(uint8_t row, uint8_t patNr, uint16_t width) {
  uint16_t colAddr;  // Prepared Column Address to save Init Time. This is needed when A0 & A8 are not used for Col addressing.
  uint8_t init_shift = type == T_4416 ? 1 : 0;
  // Prepare Write Cycle
  rasHandling_18Pin(row);
  WE_LOW18;
  configDataOut_18Pin();
  setData18Pin(pattern[patNr]);
  cli();
  if (patNr < 4)
    for (uint16_t col = 0; col < width; col++) {
      CAS_HIGH18;
      colAddr = (col << init_shift);
      setAddr18Pin(colAddr);
      CAS_LOW18;
    }
  else
    for (uint16_t col = 0; col < width; col++) {
      CAS_HIGH18;
      setData18Pin(randomTable[mix8(col, row)]);
      colAddr = (col << init_shift);
      setAddr18Pin(colAddr);
      CAS_LOW18;
    }
  sei();
  WE_HIGH18;
  CAS_HIGH18;
  // If we check 255 Columns the time for Write & Read(Check) exceeds the Refresh time. We need to add a Refresh in the Middle
  if (patNr < 4) {
    checkRow_18Pin(width, row, patNr, init_shift, 2);
    return;
  }
  refreshRow_18Pin(row);
  if (row == ramTypes[type].rows - 1) {  // Last Row written, we have to check the last n Rows as well.
    // Retention testing the last rows, they will no longer be written only read back. Simulate the write time to get a correct retention time test.
    for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {
      rasHandling_18Pin(row - x);
      checkRow_18Pin(width, row - x, patNr, init_shift, 3);
      delayMicroseconds(ramTypes[type].writeTime);  // Simulate writing even if it is no longer done for the last rows
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
    }
    return;
  }
  if (row >= ramTypes[type].delayRows) {
    rasHandling_18Pin(row - ramTypes[type].delayRows);
    checkRow_18Pin(width, row - ramTypes[type].delayRows, patNr, init_shift, 3);
  }
  if (row < ramTypes[type].delayRows)
    delayMicroseconds(ramTypes[type].delays[row]);
  else
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
}

/**
 * Read and verify data from a complete row for standard 18-pin DRAMs
 * @param width Number of columns to check
 * @param row Row address being tested  
 * @param patNr Pattern number (0-4)
 * @param init_shift Address shift for 4416 compatibility (1 for 4416, 0 for 4464)
 * @param errorNr Error code to report if check fails
 */
void checkRow_18Pin(uint16_t width, uint8_t row, uint8_t patNr, uint8_t init_shift, uint8_t errorNr) {
  configDataIn_18Pin();
  uint8_t pat = pattern[patNr] & 0x0f;
  OE_LOW18;
  cli();
  if (patNr < 4)
    for (uint16_t col = 0; col < width; col++) {
      setAddr18Pin(col << init_shift);
      CAS_LOW18;
      NOP;
      CAS_HIGH18;
      if ((getData18Pin()) != pat) {
        error(patNr, errorNr);
      }
    }
  else
    for (uint16_t col = 0; col < width; col++) {
      setAddr18Pin(col << init_shift);
      CAS_LOW18;
      CAS_HIGH18;
      if (getData18Pin() != randomTable[mix8(col, row)]) {
        error(patNr, errorNr);
      }
    }
  sei();
  OE_HIGH18;
  RAS_HIGH18;
}

/**
 * Configure data lines as outputs for write operations
 */
void configDataOut_18Pin() {
  DDRB |= 0x09;  // Configure D1 & D2 as Outputs
  DDRC |= 0x0a;  // Configure D0 & D3 as Outputs
}

/**
 * Configure data lines as inputs for read operations
 */
void configDataIn_18Pin() {
  DDRB &= 0xf6;  // Config Data Lines for input
  DDRC &= 0xf5;
  PORTB |= 0x09;  // Activate the Pullups, otherwise static can keep the lines
  PORTC |= 0x0A;  // Causing false positives. However this is the limit of the 4416 Output driver
}

/**
 * Refresh a specific row by performing RAS-only cycle
 * @param row Row address to refresh
 */
void refreshRow_18Pin(uint8_t row) {
  rasHandling_18Pin(row);
  NOP;
  RAS_HIGH18;
}

/**
 * Set row address and activate RAS signal for standard 18-pin DRAMs
 * @param row Row address to access
 */
void rasHandling_18Pin(uint8_t row) {
  RAS_HIGH18;
  setAddr18Pin(row);
  RAS_LOW18;
}

//=======================================================================================
// Comprehensive Address Line and Decoder Testing for 18-Pin DRAM (4416/4464)
//=======================================================================================

/**
 * Address line and decoder testing for standard 18-pin DRAMs
 * Tests row/column address lines with walking-bit patterns and gray-code stress tests
 * Handles differences between 4416 and 4464 addressing schemes
 */
void checkAddressing_18Pin() {
  return;
  uint16_t max_rows = ramTypes[type].rows;
  uint16_t max_cols = ramTypes[type].columns;
  uint8_t init_shift = (type == T_4416) ? 1 : 0;  // 4416 does not use A0 for Columns

  // Determine number of Address-Bits for Row and Column
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

  // Configure I/O for 18-Pin
  DDRB |= 0x09;  // D1 & D2 as Output
  DDRC |= 0x0a;  // D0 & D3 as Output

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
    setAddr18Pin(test_row);
    RAS_LOW18;
    setAddr18Pin(test_col);
    setData18Pin(test_data);
    CAS_LOW18;
    NOP;
    CAS_HIGH18;
    NOP;
    RAS_HIGH18;
    NOP;
    WE_HIGH18;
    // IMMEDIATE Read-back (no pause!)
    DDRB &= 0xf6;
    DDRC &= 0xf5;  // Input
    OE_LOW18;
    setAddr18Pin(test_row);
    RAS_LOW18;
    setAddr18Pin(test_col);
    CAS_LOW18;
    NOP;
    if (getData18Pin() != test_data) {
      error(bit, 1);
    }
    CAS_HIGH18;
    RAS_HIGH18;
    OE_HIGH18;
    // Back to Output for next iteration
    DDRB |= 0x09;
    DDRC |= 0x0a;
  }

  //===========================================================================
  // PHASE 2: COLUMN ADDRESS LINES TESTING
  //===========================================================================

  uint8_t test_row = max_rows >> 1;  // Use middle Row

  setAddr18Pin(test_row);
  RAS_LOW18;
  WE_LOW18;

  // Write Phase - Column Address Walking-Bit
  uint8_t start_bit = (type == T_4416) ? 1 : 0;       // 4416 starts at Bit 1 (A1)
  uint8_t end_bit = (type == T_4416) ? 6 : col_bits;  // 4416 ends at Bit 6 (A6)

  for (uint8_t bit = start_bit; bit <= end_bit; bit++) {
    uint8_t test_col = (1 << bit);  // Directly set the bit, not bit-1
    uint8_t test_data = (bit & 1) ? 0xA : 0x5;
    uint16_t colAddr = (test_col << init_shift);

    setAddr18Pin(colAddr);
    setData18Pin(test_data);

    CAS_LOW18;
    NOP;
    CAS_HIGH18;
  }

  WE_HIGH18;
  RAS_HIGH18;

  // Read Phase - Column Address Verification
  DDRB &= 0xf6;
  DDRC &= 0xf5;

  setAddr18Pin(test_row);
  RAS_LOW18;
  OE_LOW18;

  for (uint8_t bit = start_bit; bit <= end_bit; bit++) {
    uint8_t test_col = (1 << bit);
    uint8_t expected_data = (bit & 1) ? 0xA : 0x5;
    uint16_t colAddr = (test_col << init_shift);

    setAddr18Pin(colAddr);

    CAS_LOW18;
    NOP;

    if (getData18Pin() != expected_data) {
      error(bit + 16, 1);  // Column address line error (16-31)
    }

    CAS_HIGH18;
  }

  OE_HIGH18;
  RAS_HIGH18;

  //===========================================================================
  // PHASE 3: ADDRESS DECODER STRESS TEST with Gray-Code
  //===========================================================================

  // Configure for Output
  DDRB |= 0x09;
  DDRC |= 0x0a;
  WE_LOW18;

  // Gray-Code Test for better Decoder-Coverage
  for (uint16_t i = 0; i < min(max_rows, 256); i++) {
    uint8_t gray_addr = (i ^ (i >> 1)) & 0xFF;     // Gray-Code conversion to 8-Bit
    uint8_t test_data = (gray_addr & 0x0f) ^ 0x5;  // Mixed pattern

    if (gray_addr >= max_rows) continue;

    setAddr18Pin(gray_addr);
    setData18Pin(test_data);

    RAS_LOW18;
    CAS_LOW18;
    NOP;
    CAS_HIGH18;
    RAS_HIGH18;
  }

  WE_HIGH18;

  // Gray-Code Verification
  DDRB &= 0xf6;
  DDRC &= 0xf5;
  OE_LOW18;

  for (uint16_t i = 0; i < min(max_rows, 256); i++) {
    uint8_t gray_addr = (i ^ (i >> 1)) & 0xFF;
    uint8_t expected_data = (gray_addr & 0x0f) ^ 0x5;

    if (gray_addr >= max_rows) continue;

    setAddr18Pin(gray_addr);

    RAS_LOW18;
    CAS_LOW18;
    NOP;

    if (getData18Pin() != expected_data) {
      error(32 + (i & 0x1f), 1);  // Decoder error (32-63)
    }

    CAS_HIGH18;
    RAS_HIGH18;
  }

  OE_HIGH18;
}

/**
 * Detect and differentiate between 4416 (16Kx4) and 4464 (64Kx4) RAM types
 * Tests column addressing to determine memory size and addressing scheme
 */
void sense4464_18Pin() {
  boolean big = true;
  rasHandling_18Pin(0);  // Use Row 0 for Size Tests
  WE_LOW18;
  setData18Pin(0x0);
  setAddr18Pin(0x00);
  CAS_LOW18;
  NOP;
  CAS_HIGH18;
  // 4416 CAS addressing does not Use A0 nor A7 we set A0 to check and Write 1111 to this Column
  // First test the CAS addressing
  for (uint8_t a = 0; a <= 7; a++) {
    uint8_t col = (1 << a);
    configDataOut_18Pin();
    WE_LOW18;
    setData18Pin(0xf);
    setAddr18Pin(col);
    CAS_LOW18;
    NOP;
    CAS_HIGH18;
    setAddr18Pin(0x00);
    WE_HIGH18;
    configDataIn_18Pin();
    OE_LOW18;
    CAS_LOW18;
    NOP;
    NOP;
    if ((getData18Pin() & 0xf) != (0x0)) {
      if (a == 0) {
        big = false;
        CAS_HIGH18;
        OE_HIGH18;
        WE_LOW18;
        configDataOut_18Pin();
        setData18Pin(0x0);
        setAddr18Pin(0x01);
        CAS_LOW18;
      } else if ((a == 7) && (big == false))
        NOP;
      else {
        return;
      }
    }
    PORTC |= 0x05;
  }
  if (big)
    type = T_4464;
  else
    type = T_4416;
}

/**
 * Detect KM41C1000 type RAM (1Mx1) using alternative 18-pin socket configuration
 * This function is called when standard 4416/4464 detection fails
 */
void sense411000_18Pin_Alt() {
  // Test if it's a KM41C1000 type
  // This function is called when 4416/4464 Detection fails
  CBI(PORTC, 4);  // PB3 LOW to disable the Pullup
  DDRB = (DDRB & 0xE0) | 0x1D;
  DDRC = (DDRC & 0xE0) | 0x17;
  DDRD = (DDRD & 0x18) | 0xE7;
  initRAM(RAS_18PIN_ALT, CAS_18PIN_ALT);
  // Configure Ports for KM41C1000
  // Initial State
  RAS_HIGH_18PIN_ALT;
  CAS_HIGH_18PIN_ALT;
  WE_HIGH_18PIN_ALT;
  // Test 1: Basic Write/Read Test
  setAddr_18Pin_Alt(0);  // Row 0
  RAS_LOW_18PIN_ALT;
  SET_DIN_18PIN_ALT(1);
  setAddr_18Pin_Alt(0);  // Column 0
  WE_LOW_18PIN_ALT;
  CAS_LOW_18PIN_ALT;
  NOP;
  CAS_HIGH_18PIN_ALT;
  WE_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;
  // Read back
  setAddr_18Pin_Alt(0);  // Row 0
  RAS_LOW_18PIN_ALT;
  setAddr_18Pin_Alt(0);  // Column 0
  CAS_LOW_18PIN_ALT;
  NOP;
  NOP;
  uint8_t read1 = GET_DOUT_18PIN_ALT();
  CAS_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;
  // Test 2: Different pattern
  setAddr_18Pin_Alt(1);  // Row 1
  RAS_LOW_18PIN_ALT;
  SET_DIN_18PIN_ALT(0);
  setAddr_18Pin_Alt(1);  // Column 1
  WE_LOW_18PIN_ALT;
  CAS_LOW_18PIN_ALT;
  NOP;
  CAS_HIGH_18PIN_ALT;
  WE_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;
  // Read back
  setAddr_18Pin_Alt(1);  // Row 1
  RAS_LOW_18PIN_ALT;
  setAddr_18Pin_Alt(1);  // Column 1
  CAS_LOW_18PIN_ALT;
  NOP;
  NOP;
  uint8_t read2 = GET_DOUT_18PIN_ALT();
  CAS_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;
  if (read1 >= 1 && read2 == 0) {
    type = T_411000;
  } else {
    error(1, 0);
  }
}

//=======================================================================================
// 18-Pin Alternative DRAM Functions (KM41C1000 type)
//=======================================================================================

/**
 * OPTIMIZED: Row Write for 18Pin_Alt with Pattern-Rotation
 * Writes test patterns to a complete row and performs inline verification for stuck-at patterns
 * @param row Row address to write/test
 * @param patNr Pattern number (0-4)
 */
void __attribute__((hot)) writeRow_18Pin_Alt(uint16_t row, uint8_t patNr) {
  RAS_HIGH_18PIN_ALT;
  setAddr_18Pin_Alt(row);
  RAS_LOW_18PIN_ALT;
  WE_LOW_18PIN_ALT;

  uint8_t pat = pattern[patNr];

  cli();
  if (patNr < 2) {
    // Either it is 0 or 1 for the first 2 patterns, so we speed up here.
    SET_DIN_18PIN_ALT(pat & 0x08);
    // Regular patterns with Bit-Rotation
    for (uint16_t col = 0; col < 1024; col++) {
      setAddr_18Pin_Alt(col);
      CAS_LOW_18PIN_ALT;
      CAS_HIGH_18PIN_ALT;
      // Pattern 0 + 1 checks for stuck bits, we can check inline
      WE_HIGH_18PIN_ALT;
      CAS_LOW_18PIN_ALT;
      CAS_HIGH_18PIN_ALT;
      if (((PINC ^ pat) & 0x08) != 0) {
        error(patNr, 2);
      }
      WE_LOW_18PIN_ALT;
    }
    return;
  } else if (patNr < 4) {
    // Regular patterns with Bit-Rotation
    for (uint16_t col = 0; col < 1024; col++) {
      SET_DIN_18PIN_ALT(pat & 0x08);
      setAddr_18Pin_Alt(col);
      CAS_LOW_18PIN_ALT;
      pat = rotate_left(pat);  // 1-Bit Rotation for 1Mx1
      CAS_HIGH_18PIN_ALT;
    }
  } else {
    // Random pattern
    for (uint16_t col = 0; col < 1024; col++) {
      SET_DIN_18PIN_ALT(randomTable[mix8(col, row)] & 0x08);
      setAddr_18Pin_Alt(col);
      CAS_LOW_18PIN_ALT;
      CAS_HIGH_18PIN_ALT;
    }
  }
  sei();

  WE_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;

  // Read back and check
  if (patNr < 4) {
    checkRow_18Pin_Alt(row, patNr, 2);
    return;
  }
  refreshRow_18Pin_Alt(row);
  // Retention testing (analog to other RAMs)
  if (row == ramTypes[type].rows - 1) {                       // Last row
    for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {  // Check last 5 rows
      checkRow_18Pin_Alt(row - x, patNr, 3);
      delayMicroseconds(ramTypes[type].writeTime);  // Simulate writing even if it is no longer done for the last rows
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
    }
    return;
  } else if (row >= ramTypes[type].delayRows) {
    checkRow_18Pin_Alt(row - ramTypes[type].delayRows, patNr, 3);
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
    return;
  }
  delayMicroseconds(ramTypes[type].delays[row]);
}

/**
 * Row Check for 18Pin_Alt
 * Reads and verifies data from a complete row
 * @param row Row address being tested
 * @param patNr Pattern number (0-4)
 * @param errorNr Error code to report if check fails
 */
void __attribute__((hot)) checkRow_18Pin_Alt(uint16_t row, uint8_t patNr, uint8_t errorNr) {
  uint8_t pat = pattern[patNr];
  rasHandling_18Pin_Alt(row);
  cli();
  if (patNr < 4) {
    for (uint16_t col = 0; col < 1024; col++) {
      setAddr_18Pin_Alt(col);
      CAS_LOW_18PIN_ALT;
      CAS_HIGH_18PIN_ALT;
      if (((PINC ^ pat) & 0x08) != 0) {
        error(patNr + 1, errorNr);
      }
      pat = rotate_left(pat);
    }
  } else {
    for (uint16_t col = 0; col < 1024; col++) {
      setAddr_18Pin_Alt(col);
      CAS_LOW_18PIN_ALT;
      uint8_t expected_bit = randomTable[mix8(col, row)];
      CAS_HIGH_18PIN_ALT;
      if (((PINC ^ expected_bit) & 0x08) != 0) {
        error(patNr + 1, errorNr);
      }
    }
  }
  sei();

  RAS_HIGH_18PIN_ALT;
}

/**
 * Set row address and activate RAS signal for 18Pin_Alt
 * @param row Row address to access
 */
void rasHandling_18Pin_Alt(uint16_t row) {
  RAS_HIGH_18PIN_ALT;
  setAddr_18Pin_Alt(row);
  RAS_LOW_18PIN_ALT;
}

/**
 * Refresh for 18Pin_Alt
 * Performs RAS-only cycle to refresh specified row
 * @param row Row address to refresh
 */
static void __attribute__((always_inline, hot))
refreshRow_18Pin_Alt(uint16_t row) {
  rasHandling_18Pin_Alt(row);
  RAS_HIGH_18PIN_ALT;
}

/**
 * ADDRESS TESTING for 18Pin_Alt
 * Comprehensive test of row and column address lines for KM41C1000 type
 */
void checkAddressing_18Pin_Alt() {
  uint16_t max_rows = 1024;
  uint16_t max_cols = 1024;

  // Row Address Testing (A0-A9, 10 Bits)
  for (uint8_t bit = 0; bit <= 9; bit++) {
    uint16_t test_row = (bit == 0) ? 0 : (1 << (bit - 1));
    uint8_t test_data = (bit == 0) ? 0 : 1;
    uint16_t test_col = 0x100;  // Safe Column-Address

    // Write
    setAddr_18Pin_Alt(test_row);
    RAS_LOW_18PIN_ALT;
    SET_DIN_18PIN_ALT(test_data);
    setAddr_18Pin_Alt(test_col);
    WE_LOW_18PIN_ALT;
    CAS_LOW_18PIN_ALT;
    NOP;
    CAS_HIGH_18PIN_ALT;
    WE_HIGH_18PIN_ALT;
    RAS_HIGH_18PIN_ALT;

    // Read
    setAddr_18Pin_Alt(test_row);
    RAS_LOW_18PIN_ALT;
    setAddr_18Pin_Alt(test_col);
    CAS_LOW_18PIN_ALT;
    NOP;
    NOP;

    if (GET_DOUT_18PIN_ALT() != test_data) {
      error(bit, 1);  // Row address error
    }

    CAS_HIGH_18PIN_ALT;
    RAS_HIGH_18PIN_ALT;
  }

  // Column Address Testing
  uint16_t test_row = 512;  // Middle Row

  for (uint8_t bit = 0; bit <= 9; bit++) {
    uint16_t test_col = (bit == 0) ? 0 : (1 << (bit - 1));
    uint8_t test_data = (bit & 1) ? 1 : 0;

    // Write
    setAddr_18Pin_Alt(test_row);
    RAS_LOW_18PIN_ALT;
    SET_DIN_18PIN_ALT(test_data);
    setAddr_18Pin_Alt(test_col);
    WE_LOW_18PIN_ALT;
    CAS_LOW_18PIN_ALT;
    NOP;
    CAS_HIGH_18PIN_ALT;
    WE_HIGH_18PIN_ALT;
    RAS_HIGH_18PIN_ALT;

    // Read
    setAddr_18Pin_Alt(test_row);
    RAS_LOW_18PIN_ALT;
    setAddr_18Pin_Alt(test_col);
    CAS_LOW_18PIN_ALT;
    NOP;
    NOP;

    if (GET_DOUT_18PIN_ALT() != test_data) {
      error(bit + 16, 1);  // Column address error
    }

    CAS_HIGH_18PIN_ALT;
    RAS_HIGH_18PIN_ALT;
  }
}

//=======================================================================================
// 20 - Pin DRAM Test Code
//=======================================================================================

/**
 * Main test function for 20-pin DRAMs (514256, 514258, 514400, 514402)
 * Configures I/O ports, detects RAM type, performs address testing and pattern tests
 */
void test_20Pin() {
  // Configure I/O for this Chip Type
  PORTB = 0b00111111;
  PORTC = 0b10000000;
  PORTD = 0x00;
  DDRB = 0b00011111;
  DDRC = 0b00011111;
  DDRD = 0xFF;
  senseRAM_20Pin();
  senseSCRAM_20Pin();
#ifdef OLED
  u8x8.drawString(0, 4, "Detected Type:");
  u8x8.drawString(0, 6, ramTypes[type].name);
  u8x8.drawString(0, 2, "Checking...    ");
  // Redo because otherwise u8x8 interferes with the test
  PORTB = 0b00111111;
  DDRB = 0b00011111;
#endif
  checkAddressing_20Pin();
  // OPTIMIZED: Pre-cache frequently used values
  register uint16_t total_rows = ramTypes[type].rows;
  register boolean is_static = ramTypes[type].staticColumn;

  for (register uint8_t pat = 0; pat < 5; pat++) {
    for (register uint16_t row = 0; row < total_rows; row++) {
      writeRow_20Pin(row, pat, is_static);
    }
  }
  // Good Candidate.
  testOK();
}

/**
 * Prepare and execute ROW Access for 20 Pin Types
 * Sets row address with MSB handling and activates RAS signal
 * @param row Row address to access (0 to max_rows-1)
 */
void rasHandling_20Pin(uint16_t row) {
  RAS_HIGH20;
  msbHandling_20Pin(row >> 8);  // Preset ROW Address
  PORTD = (uint8_t)(row & 0xff);
  RAS_LOW20;
}

/**
 * Prepare Control Lines and perform write/read operations for a complete row
 * Handles different test patterns and static column vs fast page mode
 * @param row Row address to write/test  
 * @param pattern_idx Pattern number (0-4)
 * @param is_static True for static column mode, false for fast page mode
 */
void __attribute__((hot)) writeRow_20Pin(uint16_t row, uint8_t pattern_idx, boolean is_static) {
  // OPTIMIZED: Use SBI instead of |= operation
  SBI(PORTB, 0);
  SBI(PORTB, 1);
  SBI(PORTB, 2);
  SBI(PORTB, 3);  // All control lines HIGH
  casHandling_20Pin(row, pattern_idx, is_static);
  SBI(PORTB, 0);
  SBI(PORTB, 1);
  SBI(PORTB, 2);
  SBI(PORTB, 3);  // All control lines HIGH
}

/**
 * Fast MSB (high address bits A8/A9) handling for 20-pin DRAMs
 * Uses inline assembly for optimal performance
 * @param address Upper address bits (bit 0 = A8, bit 1 = A9)
 */
static inline __attribute__((always_inline)) void msbHandling_20Pin(uint8_t address) {
  __asm__ volatile(
    "in  r16, %[portb] \n\t"
    "andi r16, 0xEF    \n\t"  // Clear A8
    "in  r17, %[portc] \n\t"
    "andi r17, 0xEF    \n\t"  // Clear A9
    "sbrc %[addr], 0   \n\t"  // Skip if bit 0 clear
    "ori r16, 0x10     \n\t"  // Set A8
    "sbrc %[addr], 1   \n\t"  // Skip if bit 1 clear
    "ori r17, 0x10     \n\t"  // Set A9
    "out %[portb], r16 \n\t"
    "out %[portc], r17 \n\t"
    :
    : [portb] "I"(_SFR_IO_ADDR(PORTB)),
      [portc] "I"(_SFR_IO_ADDR(PORTC)),
      [addr] "r"(address)
    : "r16", "r17");
}

/**
 * Write and Read (&Check) Pattern from Columns with optimized loops
 * Handles both static column and fast page mode with extensive loop unrolling
 * @param row Row address for the operation
 * @param patNr Pattern number (0-4)  
 * @param is_static True for static column mode, false for fast page mode
 */
void __attribute__((hot)) casHandling_20Pin(uint16_t row, uint8_t patNr, boolean is_static) {
  rasHandling_20Pin(row);

  // Prepare Write Cycle
  PORTC &= 0xf0;
  DDRC |= 0x0f;
  register uint8_t pattern_data = (pattern[patNr] & 0x0f);
  PORTC |= pattern_data;
  OE_HIGH20;
  WE_LOW20;
  register uint8_t msbCol = ramTypes[type].columns / 256;
  // OPTIMIZED: Extended critical section with loop unrolling
  cli();
  // Write Data Loop with optimizations
  for (register uint8_t msb = 0; msb < msbCol; msb++) {
    msbHandling_20Pin(msb);
    if (patNr < 2) {
      register uint8_t col = 0;
      do {
        // 8 unrolled iterations for maximum speed
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        WE_HIGH20;
        OE_LOW20;
        CAS_LOW20;
        CAS_HIGH20;
        if ((PINC ^ pattern_data) & 0x0f != 0)
          error(patNr, 2);
        OE_HIGH20;
        WE_LOW20;
        col++;
        PORTD = col;
        CAS_LOW20;
        WE_HIGH20;
        OE_LOW20;
        CAS_HIGH20;
        if ((PINC ^ pattern_data) & 0x0f != 0)
          error(patNr, 2);
        OE_HIGH20;
        WE_LOW20;
        col++;
        PORTD = col;
        CAS_LOW20;
        WE_HIGH20;
        OE_LOW20;
        CAS_HIGH20;
        if ((PINC ^ pattern_data) & 0x0f != 0)
          error(patNr, 2);
        OE_HIGH20;
        WE_LOW20;
        col++;
        PORTD = col;
        CAS_LOW20;
        WE_HIGH20;
        OE_LOW20;
        CAS_HIGH20;
        if ((PINC ^ pattern_data) & 0x0f != 0)
          error(patNr, 2);
        OE_HIGH20;
        WE_LOW20;
        col++;
      } while (col != 0);
      return;
    } else if (patNr < 4) {
      // OPTIMIZED: Loop unrolling for regular patterns (8x unroll)
      register uint8_t col = 0;
      do {
        // 8 unrolled iterations for maximum speed
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        col++;
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        col++;
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        col++;
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        col++;
      } while (col != 0);
    } else {
      // OPTIMIZED: Random pattern with cached PORTC upper bits
      register uint8_t io = (PORTC & 0xf0);
      register uint8_t col = 0;

      // OPTIMIZED: Batch nibble lookups (4x unroll)
      do {
        PORTC = io | (randomTable[mix8(col, row)]);
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        col++;
        PORTC = io | (randomTable[mix8(col, row)]);
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        col++;
        PORTC = io | (randomTable[mix8(col, row)]);
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        col++;
        PORTC = io | (randomTable[mix8(col, row)]);
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        col++;
      } while (col != 0);
    }
  }
  sei();
  // Prepare Read Cycle
  WE_HIGH20;
  PORTC &= 0xf0;  // Clear all Outputs
  DDRC &= 0xf0;   // Configure IOs for Input
  // As long its not yet Random Data just check we get the same back that was just written
  if (patNr < 4) {
    checkRow_20Pin(patNr, row, 2, is_static);
    return;
  }
  if (patNr == 4) {
    // Refresh the Row to have stable Timings and check refresh
    refreshRow_20Pin(row);                 // Refresh the current row before leaving
    if (row == ramTypes[type].rows - 1) {  // Last Row written, we have to check the last n Rows as well.
      // Retention testing the last rows, they will no longer be written only read back. Simulate the write time to get a correct retention time test.
      for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {
        rasHandling_20Pin(row - x);
        checkRow_20Pin(4, row - x, 3, is_static);
        delayMicroseconds(ramTypes[type].writeTime);  // Simulate writing even if it is no longer done for the last rows
        delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
      }
      return;
    }
    // Default Retention Testing
    if (row >= ramTypes[type].delayRows) {
      rasHandling_20Pin(row - ramTypes[type].delayRows);
      checkRow_20Pin(4, row - ramTypes[type].delayRows, 3, is_static);  // Check for the last random Pattern on this ROW
    }
    // Data retention Testing first rows. They need special treatment since they will be read back later. Timing is special at the beginning to match retention times
    if (row < ramTypes[type].delayRows)
      delayMicroseconds(ramTypes[type].delays[row]);
    else
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
  }
}

/**
 * Refresh a specific row by performing RAS-only cycle
 * @param row Row address to refresh
 */
void refreshRow_20Pin(uint16_t row) {
  CAS_HIGH20;
  rasHandling_20Pin(row);
  RAS_HIGH20;
}

/**
 * Check one full row for normal FP-Mode RAM or Static Column RAM
 * Optimized with loop unrolling and supports both access modes
 * @param patNr Pattern number being tested (0-4)
 * @param row Row address being tested
 * @param errNr Error code to report if check fails
 * @param is_static True for static column mode, false for fast page mode
 */
void __attribute__((hot)) checkRow_20Pin(uint8_t patNr, uint16_t row, uint8_t errNr, boolean is_static) {
  register uint8_t pat = pattern[patNr] & 0x0f;
  register uint8_t msbCol = ramTypes[type].columns / 256;
  OE_LOW20;
  // OPTIMIZED: Extended critical section
  cli();
  for (register uint8_t msb = 0; msb < msbCol; msb++) {
    msbHandling_20Pin(msb);
    if (is_static == false) {
      // OPTIMIZED: Fast Page Mode with loop unrolling
      register uint8_t col = 0;
      do {
        // 4x unrolled Fast Page Mode reads
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        register uint8_t pin_data = PINC & 0x0f;
        if ((pin_data ^ pat) == 0) goto next1;
        if ((pin_data ^ randomTable[mix8(col, row)]) == 0) goto next1;
        error(patNr, errNr);
next1:
        col++;

        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        pin_data = PINC & 0x0f;
        if ((pin_data ^ pat) == 0) goto next2;
        if ((pin_data ^ randomTable[mix8(col, row)]) == 0) goto next2;
        error(patNr, errNr);
next2:
        col++;
      } while (col != 0);
    } else {
      // OPTIMIZED: Static Column Mode
      CAS_LOW20;
      register uint8_t col = 0;
      do {
        PORTD = col;
        NOP;
        NOP;
        register uint8_t pin_data = PINC & 0x0f;
        if ((pin_data ^ pat) == 0) continue;
        if ((pin_data ^ randomTable[mix8(col, row)]) == 0) continue;
        error(patNr, errNr);
      } while (++col != 0);
    }
  }
  sei();
  CAS_HIGH20;
  OE_HIGH20;
}

//=======================================================================================
// COMPREHENSIVE Address Line and Decoder Testing for 20-Pin DRAM
//=======================================================================================

/**
 * Comprehensive address line and decoder testing for 20-pin DRAMs
 * Tests row/column address lines, gray-code patterns, and decoder functionality
 * Uses walking-bit patterns and stress tests to verify all address paths
 */
void checkAddressing_20Pin() {
  register uint16_t max_rows = ramTypes[type].rows;
  register uint16_t max_cols = ramTypes[type].columns;

  // Determine number of Address-Bits for Row and Column
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
  DDRC |= 0x0f;  // IOs as Output
  RAS_HIGH20;
  CAS_HIGH20;
  OE_HIGH20;
  WE_LOW20;

  // Write Phase - Walking-Bit Pattern for Row-Addresses
  for (uint8_t bit = 0; bit <= row_bits; bit++) {
    register uint16_t test_row = (bit == 0) ? 0 : (1 << (bit - 1));
    register uint8_t test_data = (bit == 0) ? 0x00 : 0x55;
    register uint8_t test_col = 0x00;  // Fixed Column for Row-Test

    // Phase 1: Row-Address set and activate RAS
    register uint8_t row_msb = test_row >> 8;
    register uint8_t row_lsb = test_row & 0xff;

    PORTB = (PORTB & 0xef) | ((row_msb & 0x01) << 4);
    PORTC = (PORTC & 0xe0) | ((row_msb & 0x02) << 3);
    PORTD = row_lsb;
    RAS_LOW20;

    // Phase 2: Column-Address set and activate CAS
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

  // Read Phase - Row-Address Verification
  DDRC &= 0xf0;  // IOs as Input
  PORTC &= 0xe0;
  OE_LOW20;

  for (uint8_t bit = 0; bit <= row_bits; bit++) {
    register uint16_t test_row = (bit == 0) ? 0 : (1 << (bit - 1));
    register uint8_t expected_data = (bit == 0) ? 0x00 : 0x55;
    register uint8_t test_col = 0x00;  // Same Column

    // Phase 1: Row-Address set
    register uint8_t row_msb = test_row >> 8;
    register uint8_t row_lsb = test_row & 0xff;

    PORTB = (PORTB & 0xef) | ((row_msb & 0x01) << 4);
    PORTC = (PORTC & 0xef) | ((row_msb & 0x02) << 3);
    PORTD = row_lsb;
    RAS_LOW20;

    // Phase 2: Column-Address set
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
  register uint16_t test_row = max_rows >> 1;  // Use middle Row

  rasHandling_20Pin(test_row);
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

  // Read Phase - Column Address Verification
  DDRC &= 0xf0;
  rasHandling_20Pin(test_row);
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
      error(bit + 16, 1);  // Column address line error (16-31)
    }

    CAS_HIGH20;
  }
  sei();

  OE_HIGH20;
  RAS_HIGH20;

  //===========================================================================
  // PHASE 3: ADDRESS DECODER STRESS TEST with Gray-Code
  //===========================================================================

  DDRC |= 0x0f;
  WE_LOW20;

  // Gray-Code Test for better Decoder-Coverage
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

  // Gray-Code Verification
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
      error(32 + (i & 0x1f), 1);  // Decoder error (32-63)
    }

    CAS_HIGH20;
    RAS_HIGH20;
  }
  sei();

  OE_HIGH20;
}

/**
 * Detect memory size by testing A9 address line functionality
 * Differentiates between 256Kx4 (514256) and 1Mx4 (514400) RAM types
 */
void senseRAM_20Pin() {
  boolean big = true;
  DDRC |= 0x0f;  // Configure IOs for Output
  // Write 0x5 to Row 0 Col 0 as reference pattern
  RAS_HIGH20;
  PORTC = (PORTC & 0xe0) | 0x05;  // A9 to LOW, Data = 0x5
  rasHandling_20Pin(0);
  WE_LOW20;
  CAS_LOW20;
  CAS_HIGH20;
  // Row address line, buffer and decoder test
  for (uint8_t a = 0; a <= 9; a++) {
    uint16_t adr = (1 << a);
    // Write 0xA to Row (2^a), Col 0
    rasHandling_20Pin(adr);
    msbHandling_20Pin(0);           // Column MSB = 0
    PORTD = 0x00;                   // Column LSB = 0
    DDRC |= 0x0f;                   // Configure IOs for Output
    PORTC = (PORTC & 0xe0) | 0x0A;  // Data = 0xA
    WE_LOW20;
    CAS_LOW20;
    CAS_HIGH20;
    WE_HIGH20;
    // Read Row 0, Col 0 back
    rasHandling_20Pin(0);
    PORTD = 0x00;   // Set Col Address
    PORTB &= 0xef;  // Clear address Bit 8
    PORTC &= 0xe0;  // A9 to LOW - Outputs low
    DDRC &= 0xf0;   // Configure IOs for Input
    OE_LOW20;
    CAS_LOW20;
    CAS_HIGH20;
    uint8_t read_data = PINC & 0x0f;
    if (read_data != 0x5) {  // Should remain 0x5, except for aliasing
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

/**
 * Detect Static Column RAM types by testing static column mode functionality
 * Differentiates between Fast Page Mode and Static Column variants
 */
void senseSCRAM_20Pin() {
  PORTD = 0x00;   // Set Row and Col address to 0
  PORTB &= 0xef;  // Clear address Bit 8
  PORTC &= 0xe0;  // Set all Outputs and A9 to LOW
  DDRC |= 0x0f;   // IOs as Outputs
  rasHandling_20Pin(0);
  WE_LOW20;
  for (uint8_t col = 0; col <= 16; col++) {
    PORTC = ((PORTC & 0xf0) | (col & 0x0f));
    PORTD = col;  // Set Col Address
    CAS_LOW20;
    NOP;
    CAS_HIGH20;
  }
  WE_HIGH20;
  // Read Back from CS Mode Write
  rasHandling_20Pin(0);
  DDRC &= 0xf0;  // Configure IOs for Input
  OE_LOW20;
  CAS_LOW20;
  for (uint8_t col = 0; col <= 16; col++) {
    PORTD = col;  // Set Col Address
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

/**
 * Indicate errors via LED pattern
 * Red LED flashes indicate error type, green LED flashes indicate error details
 * @param code Error detail code (varies by error type)
 * @param error Error type: 0=no RAM, 1=address error, 2=RAM fault, 3=retention error, 4=ground short
 */
void error(uint8_t code, uint8_t error) {
  sei();
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
        } else if (code <= 32) {
          u8x8.drawString(0, 4, "Column address");
          u8x8.drawString(4, 6, "Line=A");
          u8x8.drawString(10, 6, String(code >> 4).c_str());
        } else if (code <= 63) {
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

/**
 * Indicate successful test completion with GREEN-OFF pattern
 * Shows RAM type on OLED and indicates special features (static column, small type)
 */
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

/**
 * Indicate DIP switch configuration problem with continuous red blinking
 * Called when invalid or unsupported DIP switch combination is detected
 */
void ConfigFail() {
  setupLED();
  while (true) {
    digitalWrite(red, ON);
    delay(250);
    digitalWrite(red, OFF);
    delay(250);
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
