// 20Pin.h - Header for 20-Pin DRAM functions and definitions
//=======================================================================================
//
// This header defines all constants, macros, and function prototypes for testing
// 20-pin DRAM packages including:
// - 514256 (256Kx4, 9 address lines, 4ms refresh)
// - 514258 (256Kx4 Static Column, 9 address lines, 4ms refresh)
// - 514400 (1Mx4, 10 address lines, 8ms refresh)
// - 514402 (1Mx4 Static Column, 10 address lines, 8ms refresh)
// - 4116 (16Kx1, 7 address lines, 2ms refresh, via adapter in 20-pin socket)
// - 4027 (4Kx1, 6 address lines, 2ms refresh, via adapter in 20-pin socket)
//
// Key features:
// - 4-bit data bus for 514xxx series (nibble-wide)
// - 1-bit data bus for 4116/4027 (via adapter)
// - Static Column mode support for 514258/514402
// - Inline assembly for fast address manipulation
// - Comprehensive stuck-at, pattern, and retention testing
//
//=======================================================================================

#ifndef PIN20_H
#define PIN20_H

#include "common.h"
#include <avr/io.h>       // for SBI/CBI macros
#include <avr/pgmspace.h> // for PROGMEM

//=======================================================================================
// 20-PIN SPECIFIC DEFINES AND MACROS (514xxx SERIES)
//=======================================================================================

// Pin mappings for 20-Pin DRAMs (514256/514258/514400/514402)
// These arrays map port bits to physical RAM tester connector pins for ground short detection
//
// Pin Assignment Summary:
//   A0 = PD0   RAS = PB1
//   A1 = PD1   CAS = PB0
//   A2 = PD2   WE  = PB3
//   A3 = PD3   OE  = PB2
//   A4 = PD4   IO0 = PC0
//   A5 = PD5   IO1 = PC1
//   A6 = PD6   IO2 = PC2
//   A7 = PD7   IO3 = PC3
//   A8 = PB4
//   A9 = PC4 (only for 1Mx4 RAM; NC on 256Kx4)

extern const uint8_t CPU_20PORTB[];  // PORTB bit to pin mapping
extern const uint8_t CPU_20PORTC[];  // PORTC bit to pin mapping
extern const uint8_t CPU_20PORTD[];  // PORTD bit to pin mapping

// RAS/CAS Pin definitions for 20-pin interface
extern const uint8_t RAS_20PIN;  // Row Address Strobe - Digital Pin 9 (PB1)
extern const uint8_t CAS_20PIN;  // Column Address Strobe - Digital Pin 8 (PB0)

// 20-Pin DRAM Signal Mapping (514xxx Series)
// Supports 514256/258 (256Kx4, 9 address lines) and 514400/402 (1Mx4, 10 address lines)
//
// Address Lines (multiplexed row/column):
//   A0-A7 = PD0-PD7 (lower 8 bits on PORTD)
//   A8    = PB4 (bit 8)
//   A9    = PC4 (bit 9, only for 1Mx4)
//
// Control Lines:
//   RAS = PB1 (Row Address Strobe)
//   CAS = PB0 (Column Address Strobe)
//   WE  = PB3 (Write Enable)
//   OE  = PB2 (Output Enable)
//
// Data Lines (4-bit bus):
//   IO0 = PC0   IO1 = PC1   IO2 = PC2   IO3 = PC3
//
// Critical Timing (per 514xxx datasheets):
//   t_RAS->CAS: 150-200ns minimum (RAS to CAS delay)
//   t_CAS->Out: 75-100ns minimum (CAS to data valid)
//   Max pulse width: 10,000ns for both RAS and CAS

// Control signal macros for fast DRAM signaling (514xxx)
// These use direct port manipulation via SBI/CBI for minimal timing overhead

#define CAS_LOW20     CBI(PORTB, 0)  // Assert Column Address Strobe (PB0, active low)
#define CAS_HIGH20    SBI(PORTB, 0)  // Deassert Column Address Strobe
#define RAS_LOW20     CBI(PORTB, 1)  // Assert Row Address Strobe (PB1, active low)
#define RAS_HIGH20    SBI(PORTB, 1)  // Deassert Row Address Strobe
#define OE_LOW20      CBI(PORTB, 2)  // Assert Output Enable (PB2, active low)
#define OE_HIGH20     SBI(PORTB, 2)  // Deassert Output Enable
#define WE_LOW20      CBI(PORTB, 3)  // Assert Write Enable (PB3, active low)
#define WE_HIGH20     SBI(PORTB, 3)  // Deassert Write Enable

// High address bit control for larger DRAMs (A8/A9)
// A8 is used by all 20-pin DRAMs (256Kx4 and 1Mx4)
// A9 is only used by 1Mx4 DRAMs (514400/514402)
#define SET_A8_LOW20   CBI(PORTB, 4)  // Set address bit A8 = 0 (PB4)
#define SET_A8_HIGH20  SBI(PORTB, 4)  // Set address bit A8 = 1 (PB4)
#define SET_A9_LOW20   CBI(PORTC, 4)  // Set address bit A9 = 0 (PC4, 1Mx4 only)
#define SET_A9_HIGH20  SBI(PORTC, 4)  // Set address bit A9 = 1 (PC4, 1Mx4 only)

//=======================================================================================
// 20-PIN FUNCTION PROTOTYPES (514xxx SERIES)
//=======================================================================================

/**
 * Main entry point for 20-pin DRAM testing
 *
 * Performs complete test sequence for 20-pin DRAM chips including:
 * 1. RAM presence detection
 * 2. Chip type identification (514256/514258/514400/514402, or 4116/4027 via adapter)
 * 3. Address line verification
 * 4. Pattern testing (stuck-at, alternating, random)
 * 5. Retention time testing with refresh timing verification
 * 6. Static Column mode testing for 514258/514402
 *
 * The function never returns during normal operation - it ends by calling
 * testOK() for success or error() for failures, both of which loop infinitely.
 */
void test_20Pin(void);

/**
 * Configure I/O pins for 20-pin DRAM interface
 *
 * Sets up data direction registers for address, control, and data pins.
 */
void configureIO_20Pin(void);

/**
 * Check if 20-pin RAM is present in socket
 *
 * @return true if 20-pin RAM detected, false otherwise
 */
bool ram_present_20Pin(void);

/**
 * Detect RAM capacity and type for 20-pin interface
 *
 * Differentiates between chip types by testing address lines:
 * - 514256: 256Kx4 (9 address lines, A9 not connected)
 * - 514400: 1Mx4 (10 address lines, A9 connected)
 *
 * Sets the global 'type' variable to the detected RAM type constant.
 */
void senseRAM_20Pin(void);

/**
 * Detect Static Column capability for 20-pin DRAMs
 *
 * Tests if chip supports Static Column mode:
 * - 514258: 256Kx4 Static Column
 * - 514402: 1Mx4 Static Column
 *
 * Updates global 'type' variable if Static Column mode detected.
 */
void senseSCRAM_20Pin(void);

/**
 * Verify all address lines and decoders function correctly (514xxx)
 *
 * Tests each address bit (row and column) independently by:
 * 1. Writing different patterns to addresses that differ by one bit
 * 2. Reading back and verifying no crosstalk or stuck address lines
 * 3. Ensuring all address decoder logic works properly
 */
void checkAddressing_20Pin(void);

/**
 * Perform RAS-only refresh cycle for specified row (514xxx)
 *
 * @param row Row address to refresh (0-511 for 256Kx4, 0-1023 for 1Mx4)
 */
void rasHandling_20Pin(uint16_t row);

/**
 * Set column address during page mode operations (514xxx)
 *
 * @param row Current row address (for calculating test patterns)
 * @param patNr Pattern number being tested
 * @param is_static true if using Static Column mode, false for normal mode
 */
void casHandling_20Pin(uint16_t row, uint8_t patNr, boolean is_static);

/**
 * Write test pattern to entire row using page mode (514xxx)
 *
 * Performs page mode write of 4-bit nibble data to all columns in a row.
 * Supports both normal and Static Column modes.
 *
 * @param row Row address to write (0-511 for 256Kx4, 0-1023 for 1Mx4)
 * @param pattern Pattern number (0-5 corresponding to pattern array index)
 * @param is_static true to use Static Column mode (514258/514402), false for normal
 */
void writeRow_20Pin(uint16_t row, uint8_t pattern, boolean is_static);

/**
 * Perform RAS-only refresh on specified row (514xxx)
 *
 * @param row Row address to refresh (0-511 for 256Kx4, 0-1023 for 1Mx4)
 */
void refreshRow_20Pin(uint16_t row);

/**
 * Verify test pattern in row using page mode read (514xxx)
 *
 * Reads back and verifies previously written test pattern using 4-bit data bus.
 *
 * @param patNr Pattern number being verified (0-5)
 * @param row Row address to verify (0-511 for 256Kx4, 0-1023 for 1Mx4)
 * @param errNr Error code to report if verification fails (2=pattern, 3=retention)
 * @param is_static true if using Static Column mode, false for normal mode
 */
void checkRow_20Pin(uint8_t patNr, uint16_t row, uint8_t errNr, boolean is_static);

//=======================================================================================
// 4116/4027 FUNCTION PROTOTYPES (16K/4K DRAM with adapter - uses 20-pin socket)
//=======================================================================================

/**
 * Test for 4116 or 4027 DRAM presence via adapter
 *
 * Checks if a 4116 (16Kx1) or 4027 (4Kx1) DRAM is inserted using adapter.
 * These older DRAMs use the 20-pin socket but with different pinout via adapter.
 *
 * @return true if 4116/4027 detected, false otherwise
 */
bool test_4116(void);

/**
 * Main test logic for 4116/4027 DRAMs
 *
 * Executes complete test sequence for adapter-based 16K/4K DRAMs.
 * Determines if chip is 4116 (16Kx1) or 4027 (4Kx1) and runs appropriate tests.
 */
void test_4116_logic(void);

/**
 * Verify all address lines and decoders function correctly (4116/4027)
 *
 * Tests each address bit for 4116 (7-bit) or 4027 (6-bit) addressing.
 */
void checkAddressing_4116(void);

/**
 * Perform RAS-only refresh cycle for specified row (4116/4027)
 *
 * @param row Row address to refresh (0-127 for 4116, 0-63 for 4027)
 */
void rasHandling_4116(uint8_t row);

/**
 * Write test pattern to entire row using page mode (4116/4027)
 *
 * Performs page mode write for 1-bit data bus.
 *
 * @param row Row address to write (0-127 for 4116, 0-63 for 4027)
 * @param patNr Pattern number (0-5 corresponding to pattern array index)
 */
void writeRow_4116(uint8_t row, uint8_t patNr);

/**
 * Verify test pattern in row using page mode read (4116/4027)
 *
 * @param row Row address to verify (0-127 for 4116, 0-63 for 4027)
 * @param patNr Pattern number being verified (0-5)
 * @param errorNr Error code to report if verification fails
 */
void checkRow_4116(uint8_t row, uint8_t patNr, uint8_t errorNr);

/**
 * Perform RAS-only refresh on specified row (4116/4027)
 *
 * @param row Row address to refresh (0-127 for 4116, 0-63 for 4027)
 */
void refreshRow_4116(uint8_t row);

//=======================================================================================
// 4116/4027 SPECIFIC DEFINES AND MACROS (16K/4K DRAM with adapter)
//=======================================================================================

// 4116/4027 use the 20-pin socket via adapter with different pin assignments
// Data lines:
//   DIN  = PC1 (Data Input to DRAM, bit 1)
//   DOUT = PC0 (Data Output from DRAM, bit 0)

#define SET_DIN_4116(data) \
  do { if (data) PORTC |= _BV(PC1); else PORTC &= ~_BV(PC1); } while (0)  // Set Data Input

#define GET_DOUT_4116() (PINC & _BV(PC0))   // Read Data Output from PC0

//=======================================================================================
// 20-PIN INLINE FUNCTIONS
//=======================================================================================

/**
 * Set high address bits A8/A9 for 20-pin DRAMs (514xxx)
 *
 * Uses highly optimized inline assembly to set upper address bits.
 * Marked __attribute__((always_inline)) for maximum performance.
 *
 * Address Bit Mapping:
 *   bit 0 → A8 (PB4)   Used by all 514xxx chips
 *   bit 1 → A9 (PC4)   Only used by 1Mx4 chips (514400/514402)
 *
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
 * Extract single test bit for 1-bit data bus DRAMs (4116/4027)
 *
 * Generates pseudo-random test data for 1-bit DRAMs by extracting bits from
 * the randomTable. Uses optimized bit indexing to avoid expensive modulo operations.
 *
 * The bit index is calculated from row and column to ensure pseudo-random
 * distribution across the entire memory array.
 *
 * @param col Column address (0-127 for 4116, 0-63 for 4027)
 * @param row Row address (0-127 for 4116, 0-63 for 4027)
 * @return Test bit value (0x00 or 0x04, matches DOUT bit position)
 */
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

/**
 * Set 7-bit address for 4116/4027 DRAMs via adapter
 *
 * Uses direct PORTD write for fast address setting.
 * A0-A6 are mapped directly to PD0-PD6.
 *
 * @param addr 7-bit address (A0-A6 for 4116, A0-A5 for 4027)
 */
static inline __attribute__((always_inline)) void setAddr_4116(uint8_t addr) {
  PORTD = addr & 0x7F;  // A0-A6 on PD0-PD6 (bit 7 unused)
}

/**
 * Simple 1 microsecond delay for 4116/4027 timing requirements
 *
 * Provides precise 1μs delay using NOP instructions.
 * At 16MHz: 16 cycles = 1μs (each NOP = 1 cycle)
 *
 * Used for meeting timing requirements of older 4116/4027 DRAMs which need
 * longer setup/hold times than newer chips.
 */
static inline __attribute__((always_inline)) void delay_1us(void) {
  NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP;   // 8 cycles
  NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP;   // 8 cycles = 16 total = 1μs @ 16MHz
}

#endif // PIN20_H