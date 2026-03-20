// 18Pin.h - Header for 18-Pin DRAM functions and definitions
//=======================================================================================
//
// This header defines all constants, macros, and function prototypes for testing
// 18-pin DRAM packages including:
// - 4416 (16Kx4, 8 address lines, 4ms refresh)
// - 4464 (64Kx4, 8 address lines, 4ms refresh)
// - 411000 (1Mx1, 10 address lines, 8ms refresh, alternative pinout)
//
// Two distinct interfaces are supported:
// 1. Standard 18-pin (4416/4464): 4-bit data bus with separate control signals
// 2. Alternative 18-pin (411000): 1-bit data bus with different pin mapping
//
// Key features:
// - Inline assembly for fast address/data manipulation (standard interface)
// - Split lookup tables for 411000 (saves ~1.9KB flash)
// - Page mode read/write operations
// - Comprehensive stuck-at, pattern, and retention testing
//
//=======================================================================================

#ifndef PIN18_H
#define PIN18_H

#include "common.h"
#include <avr/io.h>       // for SBI/CBI macros
#include <avr/pgmspace.h> // for PROGMEM

//=======================================================================================
// 18-PIN SPECIFIC DEFINES AND MACROS (STANDARD INTERFACE: 4416/4464)
//=======================================================================================

// Pin mappings for standard 18-Pin DRAMs (4416/4464)
// These arrays map port bits to physical RAM tester connector pins for ground short detection

extern const uint8_t CPU_18PORTB[];  // PORTB bit to pin mapping
extern const uint8_t CPU_18PORTC[];  // PORTC bit to pin mapping
extern const uint8_t CPU_18PORTD[];  // PORTD bit to pin mapping

// RAS/CAS Pin definitions for standard 18-pin interface
extern const uint8_t RAS_18PIN;  // Row Address Strobe
extern const uint8_t CAS_18PIN;  // Column Address Strobe

// Control signal macros for fast DRAM signaling (standard 18-pin: 4416/4464)
// These use direct port manipulation for minimal timing overhead
#define CAS_LOW18   PORTC &= 0xfb  // Assert Column Address Strobe (PC2, active low)
#define CAS_HIGH18  PORTC |= 0x04  // Deassert Column Address Strobe
#define RAS_LOW18   PORTC &= 0xef  // Assert Row Address Strobe (PC4, active low)
#define RAS_HIGH18  PORTC |= 0x10  // Deassert Row Address Strobe
#define OE_LOW18    PORTC &= 0xfe  // Assert Output Enable (PC0, active low)
#define OE_HIGH18   PORTC |= 0x01  // Deassert Output Enable
#define WE_LOW18    PORTB &= 0xfd  // Assert Write Enable (PB1, active low)
#define WE_HIGH18   PORTB |= 0x02  // Deassert Write Enable

// 18-Pin DRAM Signal Mapping (Standard Interface: 4416/4464)
// Supports 4416 (16Kx4, 8 address lines) and 4464 (64Kx4, 8 address lines)
//
// Address Lines (multiplexed row/column):
//   A0 = PB2   A1 = PB4   A2 = PD7   A3 = PD6
//   A4 = PD5   A5 = PD4   A6 = PD3   A7 = PD2
//
// Control Lines:
//   RAS = PC4 (Row Address Strobe)
//   CAS = PC2 (Column Address Strobe)
//   WE  = PB1 (Write Enable)
//   OE  = PC0 (Output Enable)
//
// Data Lines (4-bit bus):
//   D0 = PC1   D1 = PB0   D2 = PB3   D3 = PC3
//
// Critical Timing (per DRAM datasheets):
//   t_RAS->CAS: 150-200ns minimum (RAS to CAS delay)
//   t_CAS->Out: 75-100ns minimum (CAS to data valid)
//   Max pulse width: 10,000ns for both RAS and CAS

//=======================================================================================
// 18-PIN ALTERNATIVE INTERFACE (411000: 1Mx1)
//=======================================================================================

// Pin definitions for KM41C1000 and compatible 1Mx1 DRAMs
// Different pinout from standard 4416/4464 interface

extern const uint8_t RAS_18PIN_ALT;  // Row Address Strobe for 411000 - Digital Pin 11 (PB3)
extern const uint8_t CAS_18PIN_ALT;  // Column Address Strobe for 411000 - Analog Pin 2 / Digital Pin 16 (PC2)

// Control signal macros for alternative 18-pin interface (411000)
// These use direct port manipulation via SBI/CBI for minimal timing overhead
#define RAS_LOW_18PIN_ALT   CBI(PORTB, 3)  // Assert Row Address Strobe (active low)
#define RAS_HIGH_18PIN_ALT  SBI(PORTB, 3)  // Deassert Row Address Strobe
#define CAS_LOW_18PIN_ALT   CBI(PORTC, 2)  // Assert Column Address Strobe (active low)
#define CAS_HIGH_18PIN_ALT  SBI(PORTC, 2)  // Deassert Column Address Strobe
#define WE_LOW_18PIN_ALT    CBI(PORTC, 1)  // Assert Write Enable (active low)
#define WE_HIGH_18PIN_ALT   SBI(PORTC, 1)  // Deassert Write Enable

// Data macros for 1-bit DRAMs (411000 alternative interface)
#define SET_DIN_18PIN_ALT(data) \
  do { if (data) SBI(PORTC, 0); else CBI(PORTC, 0); } while (0)  // Set Data Input (PC0)
#define GET_DOUT_18PIN_ALT() ((PINC & 0x08) >> 3)  // Read Data Output (PC3, bit 3)

// 411000 Signal Mapping (Alternative 18-pin Interface)
// Supports KM41C1000 and compatible 1Mx1 DRAMs (10 address lines, 8ms refresh)
//
// Address Lines (multiplexed row/column):
//   A0-A6 = PD0-PD6 (lower 7 bits on PORTD)
//   A7    = PB0 (bit 7)
//   A8    = PB2 (bit 8)
//   A9    = PB4 (bit 9)
//
// Control Lines:
//   RAS = PB3 (Row Address Strobe)
//   CAS = PC2 (Column Address Strobe)
//   WE  = PC1 (Write Enable)
//
// Data Lines (1-bit):
//   Din  = PC0 (Data Input to DRAM)
//   Dout = PC3 (Data Output from DRAM)
//
// Critical Timing (per KM41C1000 datasheet):
//   t_RAS->CAS: 200ns minimum (RAS to CAS delay)
//   t_CAS->Out: 100ns minimum (CAS to data valid)
//   Max pulse width: 10,000ns for both RAS and CAS

//=======================================================================================
// 18-PIN LOOKUP TABLE STRUCTURES
//=======================================================================================

// Split lookup tables for alternative 18-pin interface (411000)
// Optimized storage: 128 + 8 = 136 bytes vs 1024 bytes for combined table
// Flash savings: ~1.9 KB
// Defined in 18Pin.cpp
extern const uint8_t PROGMEM lut_18Pin_Low[128];   // Lower 7 bits (A0-A6) -> PORTD (128 bytes)
extern const uint8_t PROGMEM lut_18Pin_High[8];    // Upper 3 bits (A7-A9) -> PORTB (8 bytes)

//=======================================================================================
// 18-PIN FUNCTION PROTOTYPES
//=======================================================================================

// Main test functions

/**
 * Main entry point for 18-pin DRAM testing
 *
 * Performs complete test sequence for 18-pin DRAM chips including:
 * 1. RAM presence detection (tries both standard and alternative interfaces)
 * 2. Chip type identification (4416/4464 for standard, 411000 for alternative)
 * 3. Address line verification
 * 4. Pattern testing (stuck-at, alternating, random)
 * 5. Retention time testing with refresh timing verification
 *
 * The function never returns during normal operation - it ends by calling
 * testOK() for success or error() for failures, both of which loop infinitely.
 */
void test_18Pin(void);

/**
 * Check if standard 18-pin RAM (4416/4464) is present in socket
 *
 * @return true if standard 18-pin RAM detected, false otherwise
 */
bool ram_present_18Pin(void);

/**
 * Check if alternative 18-pin RAM (411000) is present in socket
 *
 * @return true if 411000-type RAM detected, false otherwise
 */
bool ram_present_18Pin_alt(void);

// Standard 18-pin DRAM functions (4416/4464)

/**
 * Detect RAM capacity for standard 18-pin interface
 *
 * Differentiates between chip types:
 * - 4416: 16Kx4 (64 rows x 64 cols)
 * - 4464: 64Kx4 (256 rows x 256 cols)
 *
 * Sets the global 'type' variable to the detected RAM type constant.
 */
void sense4464_18Pin(void);

/**
 * Verify all address lines and decoders function correctly (4416/4464)
 *
 * Tests each address bit (row and column) independently by:
 * 1. Writing different patterns to addresses that differ by one bit
 * 2. Reading back and verifying no crosstalk or stuck address lines
 * 3. Ensuring all address decoder logic works properly
 */
void checkAddressing_18Pin(void);

/**
 * Perform RAS-only refresh cycle for specified row (4416/4464)
 *
 * @param row Row address to refresh (0-63 for 4416, 0-255 for 4464)
 */
void rasHandling_18Pin(uint8_t row);

/**
 * Write test pattern to entire row using page mode (4416/4464)
 *
 * Performs optimized page mode write of test pattern to all columns in a row.
 * Supports 4-bit data bus with column-by-column testing.
 *
 * @param row Row address to write (0-63 for 4416, 0-255 for 4464)
 * @param patNr Pattern number (0-5 corresponding to pattern array index)
 * @param width Number of columns to write (64 for 4416, 256 for 4464)
 */
void writeRow_18Pin(uint8_t row, uint8_t patNr, uint16_t width);

/**
 * Perform RAS-only refresh on specified row (4416/4464)
 *
 * @param row Row address to refresh (0-63 for 4416, 0-255 for 4464)
 */
void refreshRow_18Pin(uint8_t row);

/**
 * Verify test pattern in row using page mode read (4416/4464)
 *
 * Reads back and verifies previously written test pattern using 4-bit data bus.
 *
 * @param width Number of columns to verify (64 for 4416, 256 for 4464)
 * @param row Row address to verify (0-63 for 4416, 0-255 for 4464)
 * @param patNr Pattern number being verified (0-5)
 * @param init_shift Initial bit rotation offset for pattern
 * @param errorNr Error code to report if verification fails
 */
void checkRow_18Pin(uint16_t width, uint8_t row, uint8_t patNr, uint8_t init_shift, uint8_t errorNr);

/**
 * Configure data pins as outputs for write operations (4416/4464)
 */
void configDataOut_18Pin(void);

/**
 * Configure data pins as inputs for read operations (4416/4464)
 */
void configDataIn_18Pin(void);

// Alternative 18-pin DRAM functions (411000 type)

/**
 * Detect RAM capacity for alternative 18-pin interface (411000)
 *
 * Verifies that this is a 411000 (1Mx1) DRAM by testing address line A9.
 * Sets the global 'type' variable to T_411000.
 */
void sense411000_18Pin_Alt(void);

/**
 * Verify all address lines and decoders function correctly (411000)
 *
 * Tests each address bit (row and column) independently for 10-bit addressing.
 * Similar to 16-pin checkAddressing but adapted for alternative pinout.
 */
void checkAddressing_18Pin_Alt(void);

/**
 * Perform RAS-only refresh cycle for specified row (411000)
 *
 * @param row Row address to refresh (0-1023 for 411000)
 */
void rasHandling_18Pin_Alt(uint16_t row);

/**
 * Write test pattern to entire row using page mode (411000)
 *
 * Performs optimized page mode write similar to 16-pin implementation
 * but using alternative interface pinout and lookup tables.
 *
 * @param row Row address to write (0-1023 for 411000)
 * @param patNr Pattern number (0-5 corresponding to pattern array index)
 */
void writeRow_18Pin_Alt(uint16_t row, uint8_t patNr);

/**
 * Verify test pattern in row using page mode read (411000)
 *
 * Reads back and verifies previously written test pattern using 1-bit data bus.
 *
 * @param row Row address to verify (0-1023 for 411000)
 * @param patNr Pattern number being verified (0-5)
 * @param check Error code to report if verification fails (2=pattern, 3=retention)
 */
void checkRow_18Pin_Alt(uint16_t row, uint8_t patNr, uint8_t check);

/**
 * Perform RAS-only refresh on specified row (411000)
 *
 * @param row Row address to refresh (0-1023 for 411000)
 */
void refreshRow_18Pin_Alt(uint16_t row);

//=======================================================================================
// 18-PIN INLINE FUNCTIONS (STANDARD INTERFACE: 4416/4464)
//=======================================================================================

/**
 * Set address on multiplexed bus for standard 18-pin interface (4416/4464)
 *
 * Uses highly optimized inline assembly to set 8-bit address across PORTB and PORTD.
 * Marked __attribute__((always_inline, hot)) for maximum performance.
 *
 * Address Bit Mapping:
 *   A0 → PB2   A1 → PB4   A2 → PD7   A3 → PD6
 *   A4 → PD5   A5 → PD4   A6 → PD3   A7 → PD2
 *
 * @param addr 8-bit address value (0-255 for row/column addressing)
 */
static inline __attribute__((always_inline, hot)) void setAddr18Pin(uint8_t addr) {
  __asm__ volatile(
    "in   r16, %[portb]     \n\t"
    "andi r16, 0xEB         \n\t"
    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x01         \n\t"
    "lsl  r17               \n\t"
    "lsl  r17               \n\t"
    "or   r16, r17          \n\t"
    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x02         \n\t"
    "lsl  r17               \n\t"
    "lsl  r17               \n\t"
    "lsl  r17               \n\t"
    "or   r16, r17          \n\t"
    "out  %[portb], r16     \n\t"

    "clr  r16               \n\t"

    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x04         \n\t"
    "swap r17               \n\t"
    "lsl  r17               \n\t"
    "or   r16, r17          \n\t"

    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x08         \n\t"
    "swap r17               \n\t"
    "lsr  r17               \n\t"
    "or   r16, r17          \n\t"

    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x80         \n\t"
    "lsr  r17               \n\t"
    "lsr  r17               \n\t"
    "or   r16, r17          \n\t"

    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x20         \n\t"
    "swap r17               \n\t"
    "andi r17, 0x0F         \n\t"
    "or   r16, r17          \n\t"

    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x40         \n\t"
    "swap r17               \n\t"
    "lsr  r17               \n\t"
    "lsr  r17               \n\t"
    "or   r16, r17          \n\t"

    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x10         \n\t"
    "lsr  r17               \n\t"
    "lsr  r17               \n\t"
    "or   r16, r17          \n\t"

    "out  %[portd], r16     \n\t"
    :
    : [portb] "I"(_SFR_IO_ADDR(PORTB)),
      [portd] "I"(_SFR_IO_ADDR(PORTD)),
      [addr] "r"(addr)
    : "r16", "r17");
}

/**
 * Set 4-bit data value for standard 18-pin interface (4416/4464)
 *
 * Uses highly optimized inline assembly to set 4-bit data across PORTB and PORTC.
 * Marked __attribute__((always_inline, hot)) for maximum performance.
 *
 * Data Bit Mapping:
 *   D0 → PC1   D1 → PB0   D2 → PB3   D3 → PC3
 *
 * @param data 4-bit data value (uses lower 4 bits: 0x0-0xF)
 */
static inline __attribute__((always_inline, hot)) void setData18Pin(uint8_t data) {
  __asm__ volatile(
    "in   r16, %[portb]     \n\t"
    "andi r16, 0xf6         \n\t"
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

    "in   r20, %[portc]     \n\t"
    "andi r20, 0xf5         \n\t"
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
    : "r16", "r17", "r20");
}

/**
 * Read 4-bit data value from standard 18-pin interface (4416/4464)
 *
 * Uses highly optimized inline assembly to read 4-bit data from PINB and PINC.
 * Marked __attribute__((always_inline, hot)) for maximum performance.
 *
 * Data Bit Mapping (input):
 *   D0 ← PC1   D1 ← PB0   D2 ← PB3   D3 ← PC3
 *
 * @return 4-bit data value (0x0-0xF)
 */
static inline __attribute__((always_inline, hot)) uint8_t getData18Pin(void) {
  uint8_t result;
  __asm__ volatile(
    "in   r16, %[pinc]      \n\t"
    "in   r17, %[pinb]      \n\t"

    "clr  r18               \n\t"

    "mov  r19, r16          \n\t"
    "andi r19, 0x02         \n\t"
    "lsr  r19               \n\t"
    "add  r18, r19          \n\t"

    "mov  r19, r17          \n\t"
    "andi r19, 0x08         \n\t"
    "lsr  r19               \n\t"
    "lsr  r19               \n\t"
    "add  r18, r19          \n\t"

    "mov  r19, r17          \n\t"
    "andi r19, 0x01         \n\t"
    "lsl  r19               \n\t"
    "lsl  r19               \n\t"
    "add  r18, r19          \n\t"

    "andi r16, 0x08         \n\t"
    "add  r18, r16          \n\t"

    "mov  %0, r18           \n\t"
    : "=r"(result)
    : [pinc] "I"(_SFR_IO_ADDR(PINC)),
      [pinb] "I"(_SFR_IO_ADDR(PINB))
    : "r16", "r17", "r18", "r19");
  return result;
}

//=======================================================================================
// 18-PIN INLINE FUNCTIONS (ALTERNATIVE INTERFACE: 411000)
//=======================================================================================

/**
 * Set address using split lookup tables for alternative 18-pin interface (411000)
 *
 * Uses optimized split lookup tables similar to 16-pin implementation.
 * - Low table: 128 entries for A0-A6 (PORTD)
 * - High table: 8 entries for A7-A9 (PORTB)
 *
 * Address Bit Mapping:
 *   A0-A6 → PD0-PD6   A7 → PB0   A8 → PB2   A9 → PB4
 *
 * @param addr 10-bit address value (0-1023 for 411000)
 */
void setAddr_18Pin_Alt(uint16_t addr);

#endif // PIN18_H