// 16Pin.h - Header for 16-Pin DRAM functions and definitions
//=======================================================================================
//
// This header defines all constants, macros, and function prototypes for testing
// 16-pin DRAM packages including:
// - 4164 (64Kx1, 2ms refresh)
// - 41256 (256Kx1, 4ms refresh)
// - 41257 (256Kx1 Nibble Mode, 4ms refresh)
// - 4816 (16Kx1, 2ms refresh, no -5V/+12V required)
// - 4532-L and 4532-H (half-functional 4164 variants, 32Kx1 each half)
//
// Key features:
// - Optimized split lookup tables for fast address setting
// - 4532 detection logic for half-functional chips
// - Page mode read/write operations
// - Comprehensive stuck-at, pattern, and retention testing
//
//=======================================================================================

#ifndef PIN16_H
#define PIN16_H

#include "common.h"
#include <avr/io.h>       // for SBI/CBI macros
#include <avr/pgmspace.h> // for PROGMEM

//=======================================================================================
// 16-PIN SPECIFIC DEFINES AND MACROS
//=======================================================================================

// RAS/CAS Pin definitions for 16-pin interface
extern const uint8_t RAS_16PIN;  // Row Address Strobe - Digital Pin 9 (PB1)
extern const uint8_t CAS_16PIN;  // Column Address Strobe - Analog Pin 3 / Digital Pin 17 (PC3)

// Control signal macros for fast DRAM signaling
// These use direct port manipulation via SBI/CBI for minimal timing overhead
#define CAS_LOW16  CBI(PORTC, 3)  // Assert Column Address Strobe (active low)
#define CAS_HIGH16 SBI(PORTC, 3)  // Deassert Column Address Strobe
#define RAS_LOW16  CBI(PORTB, 1)  // Assert Row Address Strobe (active low)
#define RAS_HIGH16 SBI(PORTB, 1)  // Deassert Row Address Strobe
#define WE_LOW16   CBI(PORTB, 3)  // Assert Write Enable (active low)
#define WE_HIGH16  SBI(PORTB, 3)  // Deassert Write Enable

// 16-Pin DRAM Signal Mapping and Timing Specifications
// Supports 4164 (2ms refresh), 41256/257 (4ms refresh), 4816, and 4532 variants
//
// Address Lines (multiplexed row/column):
//   A0 = PC4   A1 = PD1   A2 = PD0   A3 = PB2
//   A4 = PB4   A5 = PD7   A6 = PB0   A7 = PD6
//   A8 = PC0 (41256/257 only, NC on 4164/4816/4532)
//
// Control Lines:
//   RAS = PB1 (Row Address Strobe)
//   CAS = PC3 (Column Address Strobe)
//   WE  = PB3 (Write Enable)
//
// Data Lines:
//   Din  = PC1 (Data Input to DRAM)
//   Dout = PC2 (Data Output from DRAM)
//
// Critical Timing (per DRAM datasheets):
//   t_RAS->CAS: 150-200ns minimum (RAS to CAS delay)
//   t_CAS->Out: 75-100ns minimum (CAS to data valid)
//   Max pulse width: 10,000ns for both RAS and CAS

// Legacy address setting macro (replaced by optimized LUT in implementation)
// This macro directly manipulates port registers to set a 9-bit address
#define SET_ADDR_PIN16(addr) \
  { \
    PORTB = ((PORTB & 0xEA) | (addr & 0x0010) | ((addr & 0x0008) >> 1) | ((addr & 0x0040) >> 6)); \
    PORTC = ((PORTC & 0xE8) | ((addr & 0x0001) << 4) | ((addr & 0x0100) >> 8)); \
    PORTD = ((addr & 0x0080) >> 1) | ((addr & 0x0020) << 2) | ((addr & 0x0004) >> 2) | (addr & 0x0002); \
  }

//=======================================================================================
// 16-PIN LOOKUP TABLE STRUCTURES
//=======================================================================================

// Packed structure for optimized address lookup (3 bytes per entry, saves ~1.4KB flash)
// This structure holds pre-calculated port values for each possible address,
// eliminating the need for bit manipulation during time-critical operations
struct __attribute__((packed)) port_config16_t {
  uint8_t portb;  // PORTB configuration for this address
  uint8_t portc;  // PORTC configuration (excludes PC1/DIN which is set separately based on data)
  uint8_t portd;  // PORTD configuration for this address
};

// Lookup table for optimized address setting (stored in flash memory)
// Contains 512 entries for all possible 9-bit addresses (0-511)
// Used by optimized address setting functions in 16Pin.cpp
extern const struct port_config16_t PROGMEM lut_combined[512];

//=======================================================================================
// 16-PIN FUNCTION PROTOTYPES
//=======================================================================================

// Main test functions

/**
 * Main entry point for 16-pin DRAM testing
 *
 * Performs complete test sequence for 16-pin DRAM chips including:
 * 1. RAM presence detection
 * 2. Chip type identification (4164/41256/41257/4816/4532)
 * 3. Address line verification
 * 4. Pattern testing (stuck-at, alternating, random)
 * 5. Retention time testing with refresh timing verification
 * 6. 4532 half-functional chip detection and handling
 *
 * The function never returns during normal operation - it ends by calling
 * testOK() for success or error() for failures, both of which loop infinitely.
 */
void test_16Pin(void);

/**
 * Detect RAM capacity and type for 16-pin DRAMs
 *
 * Differentiates between chip types by testing address line A8:
 * - 4164/4532: 64K/32K (8 address lines, A8 not connected)
 * - 41256: 256K (9 address lines, A8 connected)
 * - 41257: 256K Nibble Mode (9 address lines, special access mode)
 * - 4816: 16K (7 address lines effectively used)
 *
 * Sets the global 'type' variable to the detected RAM type constant.
 */
void sense41256_16Pin(void);

/**
 * Verify all address lines and decoders function correctly
 *
 * Tests each address bit (row and column) independently by:
 * 1. Writing different patterns to addresses that differ by one bit
 * 2. Reading back and verifying no crosstalk or stuck address lines
 * 3. Ensuring all address decoder logic works properly
 *
 * Special handling for 4532 chips where A7 (row bit 7) may not fully decode,
 * indicating a half-functional chip.
 */
void checkAddressing_16Pin(void);

/**
 * Check if RAM chip is present in the socket
 *
 * Performs presence detection by:
 * 1. Writing test patterns to DRAM
 * 2. Attempting to read back the data
 * 3. Verifying data lines respond (not floating)
 *
 * @return true if RAM detected, false if socket empty or chip not responding
 */
bool ram_present_16Pin(void);

// 4532 detection support
// Global variable tracking half-functional 4164 chip detection
// Values: 0 = full 4164/other chip
//        -1 = 4532-L (lower half 0-127 good, upper 128-255 bad)
//         1 = 4532-H (upper half 128-255 good, lower 0-127 bad)
extern int8_t chip_half_status;

// Address and data handling

/**
 * Perform RAS-only refresh cycle for specified row
 *
 * Executes a RAS-only refresh (ROR) cycle to maintain data integrity
 * during retention testing. The ROR cycle asserts RAS with the row address
 * without asserting CAS, refreshing all columns in that row simultaneously.
 *
 * @param row Row address to refresh (0-255 for 4164, 0-511 for 41256)
 */
void rasHandling_16Pin(uint16_t row);

// Write/Read functions

/**
 * Write test pattern to entire row using page mode
 *
 * Performs optimized page mode write of test pattern to all columns in a row.
 * Uses split lookup tables for fast address setting. Supports all test patterns:
 * - Patterns 0-1: Stuck-at testing with immediate read-back verification
 * - Patterns 2-3: Alternating bit patterns for column short detection
 * - Patterns 4-5: Pseudo-random data for comprehensive decoder/retention testing
 *
 * For 4532 detection: Stuck-at errors in one half trigger 4532 identification logic.
 *
 * @param row Row address to write (0-255 for 4164, 0-511 for 41256)
 * @param cols Number of columns to write (256 for 4164, 512 for 41256)
 * @param patNr Pattern number (0-5 corresponding to pattern array index)
 */
void writeRow_16Pin(uint16_t row, uint16_t cols, uint8_t patNr);

/**
 * Perform RAS-only refresh on specified row
 *
 * Executes a single RAS-only refresh cycle to maintain DRAM contents
 * between write and verify operations during retention testing.
 *
 * @param row Row address to refresh (0-255 for 4164, 0-511 for 41256)
 */
void refreshRow_16Pin(uint16_t row);

/**
 * Verify test pattern in row using page mode read
 *
 * Reads back and verifies previously written test pattern. Uses optimized
 * page mode access for fast verification. Implements 4532 detection logic:
 * - Tracks which half of memory shows errors
 * - Continues testing if errors confined to one half
 * - Reports failure if both halves show errors
 *
 * @param cols Number of columns to verify (256 for 4164, 512 for 41256)
 * @param row Row address to verify (0-255 for 4164, 0-511 for 41256)
 * @param patNr Pattern number being verified (0-5)
 * @param check Error code to report if verification fails (2=pattern error, 3=retention error)
 */
void checkRow_16Pin(uint16_t cols, uint16_t row, uint8_t patNr, uint8_t check);

#endif // PIN16_H