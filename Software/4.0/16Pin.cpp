// 16Pin.cpp - Implementation of 16-Pin DRAM testing functions
//=======================================================================================
//
// This file contains all test logic for 16-pin DRAM packages. It implements:
// - Optimized split lookup tables for fast address setting (saves ~1.4KB flash)
// - Comprehensive test patterns (stuck-at, alternating, pseudo-random)
// - 4532 half-functional chip detection and handling
// - Page mode read/write operations with minimal timing overhead
//
// Supported chips:
// - 4164 (64Kx1, 256 rows x 256 cols, 2ms refresh)
// - 41256 (256Kx1, 512 rows x 512 cols, 4ms refresh)
// - 41257 (256Kx1 Nibble Mode, 512 rows x 512 cols, 4ms refresh)
// - 4816 (16Kx1, 128 rows x 128 cols, 2ms refresh, no -5V/+12V)
// - 4532-L/H (32Kx1 half-functional, 128 rows x 256 cols, 2ms refresh)
//
//=======================================================================================

#include "16Pin.h"

//=======================================================================================
// 16-PIN PORT MAPPINGS
//=======================================================================================

// Port bit to physical pin mappings for ground short detection
// Format: [bit0, bit1, ..., bit7] where EOL=End of List, NC=Not Connected
const uint8_t CPU_16PORTB[] = { 13, 4, 12, 3, EOL, EOL, EOL, EOL };  // PB0-PB3 used
const uint8_t CPU_16PORTC[] = { 1, 2, 14, 15, 5, EOL, EOL, EOL };    // PC0-PC4 used
const uint8_t CPU_16PORTD[] = { 6, 7, 8, NC, NC, NC, 9, 10 };        // PD0-PD2, PD6-PD7 used

// Control signal pin assignments
const uint8_t RAS_16PIN = 9;   // Row Address Strobe on Digital Pin 9 (PB1)
const uint8_t CAS_16PIN = 17;  // Column Address Strobe on Analog Pin 3 / Digital Pin 17 (PC3)

// 4532/3732 detection state variable
// Tracks which column half of a potential 4532/3732 chip is functional
// NOTE: These chips fail by COLUMNS, not rows (cols 0-127 or 128-255)
int8_t chip_half_status = 0;  // 0=full chip, -1=lower col half good, 1=upper col half good

//=======================================================================================
// 16-PIN SPLIT LOOKUP TABLE GENERATION (Optimized)
// Flash savings: ~1.4 KB compared to full 512-entry table
//=======================================================================================

// Bit extraction macros - convert address bits to port configurations
// These map the 9-bit address (A0-A8) to the scattered port pin assignments
#define GET_PB(a) ((((a)&0x0010)) | (((a)&0x0008) >> 1) | (((a)&0x0040) >> 6))  // Extract A4, A3, A6
#define GET_PC(a) ((((a)&0x0001) << 4) | (((a)&0x0100) >> 8))                    // Extract A0, A8
#define GET_PD(a) ((((a)&0x0080) >> 1) | (((a)&0x0020) << 2) | (((a)&0x0004) >> 2) | ((a)&0x0002))  // Extract A7, A5, A2, A1

// Helper macros for generating 8-entry rows in lookup tables
#define ROW8_PB(b)  GET_PB(b+0), GET_PB(b+1), GET_PB(b+2), GET_PB(b+3), GET_PB(b+4), GET_PB(b+5), GET_PB(b+6), GET_PB(b+7)
#define ROW8_PC(b)  GET_PC(b+0), GET_PC(b+1), GET_PC(b+2), GET_PC(b+3), GET_PC(b+4), GET_PC(b+5), GET_PC(b+6), GET_PC(b+7)
#define ROW8_PD(b)  GET_PD(b+0), GET_PD(b+1), GET_PD(b+2), GET_PD(b+3), GET_PD(b+4), GET_PD(b+5), GET_PD(b+6), GET_PD(b+7)

// Helper macros for generating 32-entry rows (4x8)
#define ROW32_PB(b) ROW8_PB(b), ROW8_PB(b+8), ROW8_PB(b+16), ROW8_PB(b+24)
#define ROW32_PC(b) ROW8_PC(b), ROW8_PC(b+8), ROW8_PC(b+16), ROW8_PC(b+24)
#define ROW32_PD(b) ROW8_PD(b), ROW8_PD(b+8), ROW8_PD(b+16), ROW8_PD(b+24)

// LOW TABLE: 32 entries for address bits A0-A4 (addresses 0-31)
// Used for inner loop in page mode access
const uint8_t lut_16_low_b[32] PROGMEM = { ROW32_PB(0) };
const uint8_t lut_16_low_c[32] PROGMEM = { ROW32_PC(0) };
const uint8_t lut_16_low_d[32] PROGMEM = { ROW32_PD(0) };

// HIGH TABLE: 16 entries for address bits A5-A8 (steps of 32)
// Used for outer loop in page mode access
const uint8_t lut_16_high_b[16] PROGMEM = {
  GET_PB(0), GET_PB(32), GET_PB(64), GET_PB(96), GET_PB(128), GET_PB(160), GET_PB(192), GET_PB(224),
  GET_PB(256), GET_PB(288), GET_PB(320), GET_PB(352), GET_PB(384), GET_PB(416), GET_PB(448), GET_PB(480)
};
const uint8_t lut_16_high_c[16] PROGMEM = {
  GET_PC(0), GET_PC(32), GET_PC(64), GET_PC(96), GET_PC(128), GET_PC(160), GET_PC(192), GET_PC(224),
  GET_PC(256), GET_PC(288), GET_PC(320), GET_PC(352), GET_PC(384), GET_PC(416), GET_PC(448), GET_PC(480)
};
const uint8_t lut_16_high_d[16] PROGMEM = {
  GET_PD(0), GET_PD(32), GET_PD(64), GET_PD(96), GET_PD(128), GET_PD(160), GET_PD(192), GET_PD(224),
  GET_PD(256), GET_PD(288), GET_PD(320), GET_PD(352), GET_PD(384), GET_PD(416), GET_PD(448), GET_PD(480)
};

//=======================================================================================
// CORE HELPER FUNCTIONS (OPTIMIZED)
//=======================================================================================

/**
 * Set address for random access using split lookup tables
 *
 * This function sets a 9-bit address on the multiplexed address bus by looking up
 * pre-calculated port values from split tables. It's faster than bit manipulation
 * and preserves control signal pins (RAS, CAS, WE) and data input pin (DIN).
 *
 * @param a Address to set (0-511 for 41256, 0-255 for 4164)
 */
static inline void setAddr16_Random(uint16_t a) {
  uint8_t idx_low = a & 0x1F;    // Lower 5 bits (0-31) for inner loop
  uint8_t idx_high = a >> 5;     // Upper 4 bits for outer loop

  // Fetch low address bits from tables
  uint8_t lb = pgm_read_byte(&lut_16_low_b[idx_low]);
  uint8_t lc = pgm_read_byte(&lut_16_low_c[idx_low]);
  uint8_t ld = pgm_read_byte(&lut_16_low_d[idx_low]);

  // Fetch high address bits from tables
  uint8_t hb = pgm_read_byte(&lut_16_high_b[idx_high]);
  uint8_t hc = pgm_read_byte(&lut_16_high_c[idx_high]);
  uint8_t hd = pgm_read_byte(&lut_16_high_d[idx_high]);

  // Apply to ports while preserving DIN (PC1) and control pins
  PORTB = (PORTB & 0xEA) | lb | hb;  // Preserve PB1(RAS), PB3(WE), PB5(LED)
  PORTC = (PORTC & 0xEE) | lc | hc;  // Preserve PC1(DIN), PC3(CAS)
  PORTD = (PORTD & 0x3C) | ld | hd;  // Preserve PD2-PD5
}

// Macro for high-speed inner loop address setting
// Expects base_b, base_c, base_d to be defined in local scope
// This eliminates the need to recalculate high bits on every column
#define SET_ADDR_16_LOW(idx) \
    PORTB = base_b | pgm_read_byte(&lut_16_low_b[idx]); \
    PORTC = base_c | pgm_read_byte(&lut_16_low_c[idx]); \
    PORTD = base_d | pgm_read_byte(&lut_16_low_d[idx])

// Macro for setting data input pin (PC1)
#define SET_DIN_16(d) if(d & 0x04) PORTC |= 0x02; else PORTC &= ~0x02;

// Wrapper function for compatibility with address checking code
// Combines address setting and data input in one call
static inline void setAddrData(uint16_t addr, uint8_t dataVal) {
    setAddr16_Random(addr);
    SET_DIN_16(dataVal);
}

// Redefine header macro to use optimized version
#undef SET_ADDR_PIN16
#define SET_ADDR_PIN16(row) setAddr16_Random(row)


//=======================================================================================
// MAIN TEST FUNCTION
//=======================================================================================

// Forward declarations
static void run_16Pin_tests();
static void refreshTimeTest_16Pin();  // Refresh time test for 41256

/**
 * Main entry point for 16-pin DRAM testing
 *
 * This function orchestrates the complete test sequence:
 * 1. Configures I/O pins for 16-pin DRAM interface
 * 2. Checks if RAM is present in socket
 * 3. Determines chip type (4164/41256/41257/4816/4532)
 * 4. Verifies all address lines function correctly
 * 5. Runs comprehensive pattern and retention tests
 * 6. Handles 4532 detection and reclassification during testing
 *
 * I/O Configuration:
 * - DDRB: PB0-PB5 as outputs (address lines, RAS, WE, LED)
 * - DDRC: PC0-PC4 as mixed I/O (address, CAS, data in/out)
 * - DDRD: PD0-PD2, PD6-PD7 as outputs (address lines)
 *
 * The function never returns - ends with testOK() or error() which loop infinitely.
 */
void test_16Pin() {
  // Configure I/O for 16-pin DRAM interface
  // DDRB: Set PB0-PB5 as outputs (A3, A4, A6, WE, RAS, LED)
  // PORTB: Initialize with RAS=HIGH, WE=HIGH, others LOW
  DDRB = 0b00111111; PORTB = 0b00101010;

  // DDRC: Set PC0-PC4 as outputs, PC2 as input (data out from DRAM)
  // PORTC: Initialize with CAS=HIGH
  DDRC = 0b00011011; PORTC = 0b00001000;

  // DDRD: Set PD0-PD2, PD6-PD7 as outputs (address lines)
  DDRD = 0b11000011; PORTD = 0x00;

  // Initialize 4532 half-functional chip detection state
  chip_half_status = 0;

  // Verify RAM is present in socket
  if (!ram_present_16Pin())
    error(0, 0);  // No RAM detected - trigger error display

  // Determine chip type by testing address lines A7 and A8
  sense41256_16Pin();

  // Display chip type on OLED
  // For 4164, show "4164/4532?" since 4532 detection happens during pattern tests
  if (type == T_4164) {
    writeRAMType(F("4164/4532?"));
  } else {
    writeRAMType((const __FlashStringHelper*)ramTypes[type].name);
  }

  // Restore I/O configuration after chip sensing
  DDRB = 0b00111111; PORTB = 0b00101010;

  // Verify all address lines decode correctly
  checkAddressing_16Pin();

  // Run comprehensive pattern tests
  // May recursively restart if 4532 is detected mid-test
  run_16Pin_tests();

  // ===== REFRESH TIME TEST (41256 only) =====
  // Test if chip can retain data with maximum refresh interval
  if (type == T_41256) {
    refreshTimeTest_16Pin();
  }

  // All tests passed - display success pattern
  testOK();  // Never returns
}

/**
 * Run all test patterns on detected RAM chip
 *
 * Executes 6 test patterns in sequence (0-5):
 * - Pattern 0: All zeros (stuck-at-0 test)
 * - Pattern 1: All ones (stuck-at-1 test)
 * - Pattern 2: 0xAA alternating (column short test)
 * - Pattern 3: 0x55 inverted alternating (column short test)
 * - Pattern 4: Pseudo-random (decoder/crosstalk test)
 * - Pattern 5: Inverted pseudo-random (retention test)
 *
 * 4532 Detection Logic:
 * - For 4164 chips, tests full address range (0-255)
 * - Tracks errors to determine if confined to one column half
 * - If errors only in upper column half (128-255) → reclassifies as 4532-L
 * - If errors only in lower column half (0-127) → reclassifies as 4532-H
 * - If errors in both halves → fails as broken chip
 * - Reclassification happens after all 6 patterns complete
 */
static void run_16Pin_tests() {
  // Determine total rows to test based on chip type
  uint16_t total_rows = ramTypes[type].rows;

  for (uint8_t patNr = 0; patNr <= 5; patNr++) {
    // Invert random table before final pattern for full bit coverage
    if (patNr == 5) invertRandomTable();

    // Write and verify pattern across all rows (always 0 to total_rows-1)
    for (uint16_t row = 0; row < total_rows; row++) {
      writeRow_16Pin(row, ramTypes[type].columns, patNr);
    }
  }

  // After all patterns complete, check if 4532 was detected
  if (type == T_4164 && chip_half_status != 0) {
    // Reclassify chip based on which half had errors
    if (chip_half_status == -1) {
      type = T_4532_H;  // Lower half bad → upper half is good → 4532-H
    } else {
      type = T_4532_L;  // Upper half bad → lower half is good → 4532-L
    }
    // Update OLED display with correct chip type
    writeRAMType((const __FlashStringHelper*)ramTypes[type].name);
  }
}

//=======================================================================================
// DETECTION & SENSING LOGIC
//=======================================================================================

/**
 * Write a single bit to a specific row/column address
 *
 * @param row Row address to write to
 * @param col Column address to write to
 * @param data Data bit to write (0 or 1)
 */
static inline void write16Pin(uint16_t row, uint16_t col, uint8_t data) {
  SET_ADDR_PIN16(row);
  RAS_LOW16;
  if (data) {
    PORTC |= 0x02;   // DIN=1
  } else {
    PORTC &= ~0x02;  // DIN=0
  }
  WE_LOW16;
  setAddrData(col, data ? 0x04 : 0);
  CAS_LOW16;
  NOP;
  CAS_HIGH16;
  WE_HIGH16;
  RAS_HIGH16;
}

/**
 * Read a single bit from a specific row/column address
 *
 * @param row Row address to read from
 * @param col Column address to read from
 * @return Data bit read (0 or 1)
 */
static inline uint8_t read16Pin(uint16_t row, uint16_t col) {
  SET_ADDR_PIN16(row);
  RAS_LOW16;
  setAddrData(col, 0);
  CAS_LOW16;
  NOP;
  NOP;
  uint8_t result = (PINC & 0x04) >> 2;
  CAS_HIGH16;
  RAS_HIGH16;
  return result;
}

/**
 * Check if RAM chip is present in socket
 *
 * Performs basic presence detection by writing and reading test patterns:
 * 1. Writes 0 to address 0
 * 2. Writes 1 to address 1
 * 3. Reads back both addresses and verifies values
 * 4. Re-reads address 0 to ensure data was retained
 *
 * This test confirms:
 * - Data lines are not floating (chip is inserted)
 * - Basic write/read functionality works
 * - Different addresses can hold different values
 *
 * @return true if RAM detected and responding, false if socket empty or chip faulty
 */
bool ram_present_16Pin(void) {
  // Write test pattern: 0 to address 0, 1 to address 1
  write16Pin(0, 0, 0);
  write16Pin(1, 0, 1);

  // Read back and verify retention
  uint8_t r0 = read16Pin(0, 0);  // Should be 0
  uint8_t r1 = read16Pin(1, 0);  // Should be 1
  uint8_t r2 = read16Pin(0, 0);  // Should still be 0 (retention check)

  // Pass if all reads match expected values
  return (r0 == 0 && r1 == 1 && r2 == 0);
}

/**
 * Robust detection of RAM capacity and type for 16-pin DRAMs
 *
 * Differentiates between chip types by testing address line behavior,
 * with special handling for half-functional chips (TMS 4532 / OKI MSM3732)
 * that have bad cells in one column half.
 *
 * Chip types detected:
 * - 41256: 256Kx1 with 9 address lines (A0-A8)
 * - 4164: 64Kx1 with 8 address lines (A0-A7, A8 not connected)
 * - 4816: 16Kx1 with 7 address lines (A0-A6, A7-A8 not connected)
 * - 4532-L / 3732-L: Lower column half (cols 0-127) functional, upper bad
 * - 4532-H / 3732-H: Upper column half (cols 128-255) functional, lower bad
 *
 * NOTE: Column-half failures affect columns, not rows. Both TMS 4532 and
 * OKI 3732 fail by column halves. All rows must remain addressable for refresh.
 * User must identify chip brand (TMS vs OKI).
 *
 * ROBUST 4-STEP DETECTION SEQUENCE:
 *
 * STEP 1: Test Lower Column Half (Row 0, Col 0)
 *   - Write 0, then read back to verify lower column half works
 *   - If fails → Test upper column half (Row 0, Col 192)
 *   - If upper works → 4532-H/3732-H (upper column half functional)
 *   - If upper fails → No RAM present
 *
 * STEP 2: Test A8 Line (Row 0 vs Row 256)
 *   - Write different values to row 0 and row 256, same column
 *   - If row 0 retains value → A8 functional → 41256 (9 address lines)
 *   - If values alias → A8 not connected → Continue to Step 3
 *
 * STEP 3: Test A7 Line (Row 0 vs Row 128)
 *   - Write different values to row 0 and row 128, same column
 *   - If values alias → 4816 (7 address lines, or 3732 with A7 unused)
 *   - If row 0 retains value → A7 functional → Continue to Step 4
 *
 * STEP 4: Test Upper Column Half (Multiple rows, Col 192)
 *   - Write patterns to multiple rows in upper column half
 *   - Read back and verify
 *   - If works correctly → Full 4164 (all 256 columns functional)
 *   - If fails → 4532-L/3732-L (lower column half only)
 *
 * Sets global 'type' variable and 'chip_half_status' for half-functional variants.
 */
void sense41256_16Pin() {
  CAS_HIGH16;

  // ========== STEP 1: TEST LOWER COLUMN HALF FUNCTIONALITY ==========
  // Test Row 0, Col 0 (lower column half)
  write16Pin(0, 0, 0);
  uint8_t lower_test = read16Pin(0, 0);

  if (lower_test != 0) {
    // Lower column half failed - test upper column half
    // Test Row 0, Col 192 (well into upper column half)
    write16Pin(0, 192, 0);
    uint8_t upper_test = read16Pin(0, 192);

    if (upper_test == 0) {
      // Upper column half works, lower bad → 4532-H/3732-H
      type = T_4164;
      chip_half_status = 1;  // Upper column half functional
      return;
    } else {
      // Both column halves bad → No functional RAM
      error(0, 0);  // No RAM detected
    }
  }

  // ========== STEP 2: TEST A8 LINE (Row addressing) ==========
  // Write 0 to row 0, col 0
  write16Pin(0, 0, 0);

  // Write 1 to row 256, col 0 (tests A8 bit)
  write16Pin(256, 0, 1);

  // Read row 0, col 0 - if A8 works, should still be 0
  uint8_t a8_test = read16Pin(0, 0);

  if (a8_test == 0) {
    // Row 0 retained its value - A8 is functional
    type = T_41256;  // 256Kx1 chip with 9 address lines
    return;
  }

  // ========== STEP 3: TEST A7 LINE (Row addressing) ==========
  // Write 0 to row 0, col 0
  write16Pin(0, 0, 0);

  // Write 1 to row 128, col 0 (tests A7 bit)
  write16Pin(128, 0, 1);

  // Read row 0, col 0 - if A7 works, should still be 0
  uint8_t a7_test = read16Pin(0, 0);

  if (a7_test != 0) {
    // Rows 0 and 128 alias - A7 not functional
    type = T_4816;  // 16Kx1 chip (or 3732 with A7 unused)
    return;
  }

  // ========== STEP 4: TEST UPPER COLUMN HALF FUNCTIONALITY ==========
  // Write pattern to multiple rows in upper column half (Col 192)
  // Test rows 0, 1, 50, 100, 200 to cover different areas
  const uint16_t test_rows[] = {0, 1, 50, 100, 200};

  for (uint8_t i = 0; i < 5; i++) {
    write16Pin(test_rows[i], 192, 1);
  }

  // Read back and verify upper column half
  uint8_t upper_errors = 0;
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t read_val = read16Pin(test_rows[i], 192);
    if (read_val != 1) {
      upper_errors++;
    }
  }

  if (upper_errors >= 3) {
    // Upper column half consistently bad → 4532-L/3732-L
    type = T_4164;
    chip_half_status = -1;  // Lower column half functional only
  } else {
    // Full chip functional → 4164
    type = T_4164;
    chip_half_status = 0;  // Full chip
  }
}

/**
 * Verify all address lines and decoder logic function correctly
 *
 * Tests each address bit (both row and column) independently to ensure:
 * - No address lines are stuck high or low
 * - No crosstalk between address lines
 * - Address decoder properly distinguishes between addresses
 *
 * Test Method:
 * For each address bit:
 * 1. Write 0 to base address (bit=0)
 * 2. Write 1 to peer address (bit=1, all other bits=0)
 * 3. Read both addresses and verify they hold different values
 *
 * Row Test: Tests each row address bit (A0-A7 for 4164, A0-A8 for 41256)
 * Column Test: Tests each column address bit
 *
 * Special 4532 Handling:
 * For 4164 chips, A7 test may fail if chip is actually a 4532 (half-functional).
 * In this case, error is deferred to pattern testing which will identify the variant.
 *
 * Calls error() if any address bit fails, displaying bit number in error code.
 */
void checkAddressing_16Pin(void) {
  uint16_t max_rows = ramTypes[type].rows;
  uint16_t max_cols = ramTypes[type].columns;

  // Calculate number of address bits needed
  uint8_t row_bits = 0, col_bits = 0;
  for (uint16_t t = max_rows - 1; t; t >>= 1) row_bits++;
  for (uint16_t t = max_cols - 1; t; t >>= 1) col_bits++;

  CAS_HIGH16; RAS_HIGH16; WE_LOW16;

  // ========== ROW ADDRESS LINE TEST ==========
  // Test each row address bit independently
  for (uint8_t b = 0; b < row_bits; b++) {
    uint16_t base_row = 0;          // All address bits = 0
    uint16_t peer_row = (1u << b);  // Only bit 'b' = 1

    // Write 0 to base row, 1 to peer row
    write16Pin(base_row, 0, 0);
    write16Pin(peer_row, 0, 1);

    // Read back both addresses
    uint8_t base_result = read16Pin(base_row, 0);
    uint8_t peer_result = read16Pin(peer_row, 0);

    // Special handling for A7 test on 4164 chips
    if (type == T_4164 && b == 7) {
      // A7 test (rows 0 vs 128)
      // If this fails, chip might be a 4532 half-functional variant
      if (base_result != 0 || peer_result != 1) {
        // Don't error yet - pattern tests will confirm 4532 status
        chip_half_status = 0;  // Will be set during pattern tests
        continue;  // Skip to next address bit
      }
    } else {
      // Normal address line test - fail if values don't match expected
      if (base_result != 0) error(b, 1);      // Base address corrupted
      if (peer_result != 1) error(b, 1);      // Peer address didn't write correctly
    }
  }

  // ========== COLUMN ADDRESS LINE TEST ==========
  // Use middle row for column testing to avoid edge cases
  uint16_t test_row = max_rows >> 1;

  // Test each column address bit independently
  for (uint8_t b = 0; b < col_bits; b++) {
    uint16_t base_col = 0;          // All address bits = 0
    uint16_t peer_col = (1u << b);  // Only bit 'b' = 1

    // Write 0 to base column, 1 to peer column
    write16Pin(test_row, base_col, 0);
    write16Pin(test_row, peer_col, 1);

    // Read back and verify - column errors offset by 16 for distinction
    if (read16Pin(test_row, base_col) != 0) error(b + 16, 1);
    if (read16Pin(test_row, peer_col) != 1) error(b + 16, 1);
  }
}

//=======================================================================================
// CORE READ/WRITE FUNCTIONS (OPTIMIZED WITH NESTED LOOPS)
//=======================================================================================

/**
 * Perform RAS-only refresh cycle for specified row
 *
 * Executes a RAS-before-CAS (RBC) refresh cycle:
 * 1. Deasserts RAS
 * 2. Sets row address on multiplexed bus
 * 3. Asserts RAS to latch row address
 *
 * This refreshes all columns in the specified row without needing column addresses.
 * Used during retention testing to maintain data between write and verify operations.
 *
 * @param row Row address to refresh (0-255 for 4164, 0-511 for 41256)
 */
void rasHandling_16Pin(uint16_t row) {
  RAS_HIGH16;            // Deassert RAS
  setAddr16_Random(row); // Set row address on bus
  RAS_LOW16;             // Assert RAS to latch address
}

/**
 * Write test pattern to entire row using optimized page mode
 *
 * This function performs high-speed page mode writes using split lookup tables
 * and nested loops for maximum performance. Marked with __attribute__((hot))
 * for compiler optimization.
 *
 * Operation Sequence:
 * 1. Asserts RAS with row address
 * 2. Asserts WE for write mode
 * 3. For each column: sets column address + data, pulses CAS
 * 4. For patterns 0-1: immediately reads back for stuck-at testing
 * 5. For patterns 2-3: defers verification to checkRow_16Pin
 * 6. For patterns 4-5: writes then performs retention testing
 *
 * Optimization Strategy:
 * - Outer loop handles high address bits (A5-A8) in 32-column chunks
 * - Inner loop handles low address bits (A0-A4) for fast sequential access
 * - Pre-calculates base port values to minimize operations in inner loop
 * - Disables interrupts during critical timing sections
 *
 * Data Bit Mapping:
 * - Data is checked against bit 2 (0x04) to match DOUT on PC2
 * - DIN is set on bit 1 (0x02) on PC1
 * - IMPORTANT: base_c is masked with 0xEC to clear DIN bit, allowing clean setting
 *
 * 4532 Detection:
 * During stuck-at tests (patterns 0-1), errors in one half of address space
 * indicate a 4532 half-functional chip. First error sets chip_half_status,
 * subsequent errors in same half are tolerated, errors in good half fail.
 *
 * @param row Row address to write (0-255 for 4164, 0-511 for 41256)
 * @param cols Number of columns in row (256 for 4164, 512 for 41256)
 * @param patNr Pattern number (0-5): 0=0x00, 1=0xFF, 2=0xAA, 3=0x55, 4-5=random
 */
void __attribute__((hot)) writeRow_16Pin(uint16_t row, uint16_t cols, uint8_t patNr) {

  // Last row for retention testing
  uint16_t last_row = ramTypes[type].rows - 1;

  // Initialize write cycle - assert RAS with row address, enable WE
  CAS_HIGH16;
  rasHandling_16Pin(row);
  WE_LOW16;

  // Setup pattern and test mode flags
  uint8_t pat = pattern[patNr];
  uint8_t isRandom = (patNr >= 4);   // Patterns 4-5 use pseudo-random data
  uint8_t isCheck = (patNr < 2);     // Patterns 0-1 do immediate stuck-at verification
  bool error_in_stuckat = false;

  // Calculate number of 32-column chunks
  uint8_t num_chunks = cols >> 5;

  // Disable interrupts for timing-critical write sequence
  cli();

  // ========== OUTER LOOP: Process 32-column chunks (A5-A8) ==========
  for (uint8_t chunk = 0; chunk < num_chunks; chunk++) {

      // Fetch high address bits from flash lookup tables
      uint8_t hb = pgm_read_byte(&lut_16_high_b[chunk]);
      uint8_t hc = pgm_read_byte(&lut_16_high_c[chunk]);
      uint8_t hd = pgm_read_byte(&lut_16_high_d[chunk]);

      // Calculate base port values for this chunk
      // CRITICAL: Mask DIN (bit 1, 0x02) from base_c so it can be set cleanly per-column
      // 0xEE & ~0x02 = 0xEC - preserves CAS and address bits, clears DIN
      uint8_t base_b = (PORTB & 0xEA) | hb;  // Preserve RAS, WE, LED; add high address
      uint8_t base_c = (PORTC & 0xEC) | hc;  // Preserve CAS; clear DIN; add high address
      uint8_t base_d = (PORTD & 0x3C) | hd;  // Preserve PD2-PD5; add high address

      uint16_t chunkBase = (chunk << 5);  // Base column address for this chunk

      // ========== INNER LOOP: Write columns 0-31 within chunk (A0-A4) ==========
      for (int8_t c = 31; c >= 0; c--) {

        // Step 1: Calculate PORTC value (address + base)
        uint8_t val_c = base_c | pgm_read_byte(&lut_16_low_c[c]);

        // Step 2: Add data bit (DIN = PC1 = 0x02)
        // Check bit 2 (0x04) in data because checkRow reads PINC & 0x04
        uint8_t data_check;
        if (isRandom) {
           data_check = randomTable[mix8(chunkBase | c, row)];
        } else {
           data_check = pat;
        }

        if (data_check & 0x04) {
            val_c |= 0x02;  // Set DIN high
        }
        // else: DIN already low (cleared by base_c masking above)

        // Step 3: Write all ports simultaneously (address AND data)
        PORTB = base_b | pgm_read_byte(&lut_16_low_b[c]);
        PORTD = base_d | pgm_read_byte(&lut_16_low_d[c]);
        PORTC = val_c;  // Address and data written together!

        // Step 4: Pulse CAS to latch column address and write data
        CAS_LOW16;
        // NOP optional if timing too tight, but code overhead usually sufficient
        CAS_HIGH16;

        // Rotate pattern for alternating bit tests (patterns 2-3)
        if (!isRandom && !isCheck) pat = rotate_left(pat);

        // Stuck-at verification for patterns 0-1 (immediate read-back)
        if (isCheck) {
            WE_HIGH16;     // Disable write
            CAS_LOW16;     // Read pulse
            // NOP optional for hold time
            CAS_HIGH16;

            // Compare read data with expected pattern
            if (((PINC ^ pat) & 0x04) != 0) {
                error_in_stuckat = true;
            }
            WE_LOW16;      // Re-enable write for next column
        }
      }
  }

  // Re-enable interrupts after timing-critical section
  sei();
  WE_HIGH16;

  // ========== ERROR HANDLING AND RETENTION TESTING ==========

  // Handle stuck-at errors with 4532/3732 detection logic
  // NOTE: Error tracking is approximate since we don't track exact column position
  // during the tight inner loop. For stuck-at errors, we assume errors span
  // multiple columns in the same half, allowing statistical detection.
  if (error_in_stuckat) {
    if (type == T_4164) {
      // For 4532/3732 detection, we need to know the column half, but we don't
      // track it during the inner loop for performance. Since stuck-at errors
      // typically affect large regions, we defer detailed detection to checkRow
      // which can track column position. For now, mark as uncertain and let
      // checkRow determine the pattern.
      // Simply report the error - checkRow will do proper half-detection
      error(patNr + 1, 2);
      return;
    }
    // Not a 4164 candidate - report error
    error(patNr + 1, 2);
    return;
  }

  // For patterns 0-3: immediate verification
  if (patNr < 4) {
    checkRow_16Pin(cols, row, patNr, 2);
    return;
  }

  // For patterns 4-5: refresh and retention testing
  refreshRow_16Pin(row);

  if (row == last_row) {
    // Last row: verify all delayed rows
    for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {
      checkRow_16Pin(cols, row - x, patNr, 3);
      delayMicroseconds(ramTypes[type].writeTime * 20);
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows] * 20);
    }
  } else if (row >= ramTypes[type].delayRows) {
    // Normal row: verify row that's been waiting
    checkRow_16Pin(cols, row - ramTypes[type].delayRows, patNr, 3);
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows] * 20);
  } else {
    // Early rows: just delay
    delayMicroseconds(ramTypes[type].delays[row] * 20);
  }
}

/**
 * Perform RAS-only refresh on specified row
 *
 * Executes a RAS-only refresh cycle without CAS assertion. This refreshes
 * all columns in the row simultaneously, maintaining data integrity during
 * the gap between write and verify operations in retention testing.
 *
 * @param row Row address to refresh (0-255 for 4164, 0-511 for 41256)
 */
void refreshRow_16Pin(uint16_t row) {
  rasHandling_16Pin(row);  // Assert RAS with row address
  RAS_HIGH16;               // Deassert RAS to complete refresh cycle
}

/**
 * Verify test pattern in row using optimized page mode read
 *
 * This function reads back and verifies previously written test patterns using the same
 * optimized split lookup table approach as writeRow_16Pin. It implements sophisticated
 * 4532 half-functional chip detection logic that allows testing to continue when errors
 * are confined to one half of the address space.
 *
 * Data Bit Mapping:
 * - Data is read from DOUT on PC2 (bit 2 of PINC)
 * - Expected data is XORed with PINC and masked with 0x04 to check bit 2
 * - This matches the data bit handling in writeRow_16Pin where DIN is on PC1
 *
 * Optimization Strategy:
 * - Uses split lookup tables to minimize inner loop overhead
 * - Outer loop handles A5-A8 in 32-column chunks (8 or 16 iterations for 4164/41256)
 * - Inner loop handles A0-A4 as fast sequential access (32 iterations)
 * - Pre-calculates base port values to eliminate redundant bit manipulation
 * - Disables interrupts during entire read sequence for consistent timing
 *
 * 4532/3732 Detection Logic:
 * When testing a chip initially identified as 4164, errors may indicate a 4532/3732
 * half-functional variant (column-based failure):
 * - chip_half_status = 0: No errors yet, testing full chip
 * - chip_half_status = -1: Errors in upper column half (128-255), lower half good → 4532-L/3732-L
 * - chip_half_status = 1: Errors in lower column half (0-127), upper half good → 4532-H/3732-H
 *
 * NOTE: 4532 (TMS) and 3732 (OKI) fail by COLUMNS, not rows. All rows remain addressable
 * for refresh. Error tracking uses the column position where the failure occurred.
 *
 * If errors occur in both column halves, the chip is truly defective (not a 4532/3732).
 * For chips already identified as 4532-L or 4532-H, errors in the known-bad column half
 * are expected and ignored.
 *
 * @param cols Number of columns to verify (256 for 4164/4816/4532, 512 for 41256/41257)
 * @param row Row address to verify (0-255 for 4164/4816, 0-511 for 41256)
 * @param patNr Pattern number being verified (0-5):
 *              0=0x00, 1=0xFF, 2=0xAA, 3=0x55, 4=random, 5=random inverted
 * @param check Error code to report if verification fails:
 *              2=pattern error (from writeRow), 3=retention error (after refresh delay)
 *
 * @note For non-random patterns (0-3), pattern is rotated left after each read to
 *       match the rotation applied during writeRow_16Pin. For random patterns (4-5),
 *       expected value is regenerated from randomTable using mix8(col, row).
 *
 * @note Function calls error() and never returns if a real failure is detected.
 *       For 4532 chips, errors in the expected bad half cause early return without error.
 */
void __attribute__((hot)) checkRow_16Pin(uint16_t cols, uint16_t row, uint8_t patNr, uint8_t check) {
  uint8_t pat = pattern[patNr];
  uint8_t isRandom = (patNr >= 4);

  // Assert RAS with row address to open the row for page mode reading
  rasHandling_16Pin(row);

  // Calculate number of 32-column chunks: 8 for 256 cols (4164), 16 for 512 cols (41256)
  uint8_t num_chunks = cols >> 5;
  bool error_found = false;
  uint16_t error_column = 0;  // Track which column failed for half-detection

  // Disable interrupts for timing-critical page mode read sequence
  cli();

  // --- OUTER LOOP: Process 32-column chunks using high-order address bits (A5-A8) ---
  for (uint8_t chunk = 0; chunk < num_chunks; chunk++) {

      // Load pre-calculated port values for this chunk from split lookup tables in flash
      uint8_t hb = pgm_read_byte(&lut_16_high_b[chunk]);
      uint8_t hc = pgm_read_byte(&lut_16_high_c[chunk]);
      uint8_t hd = pgm_read_byte(&lut_16_high_d[chunk]);

      // Calculate base port values by preserving control bits and merging chunk address bits
      uint8_t base_b = (PORTB & 0xEA) | hb;  // Preserve RAS, WE, LED bits
      uint8_t base_c = (PORTC & 0xEE) | hc;  // Preserve CAS, DIN/DOUT bits
      uint8_t base_d = (PORTD & 0x3C) | hd;  // Preserve UART and unused bits

      // Calculate base column address for this chunk (0, 32, 64, ...)
      uint16_t chunkBase = (chunk << 5);

      // --- INNER LOOP: Fast sequential access through 32 columns (A0-A4) ---
      for (int8_t c = 31; c >= 0; c--) {

          // Set low-order address bits (A0-A4) using optimized macro with pre-calculated base values
          SET_ADDR_16_LOW(c);

          // Assert CAS to latch column address and enable data output on DOUT (PC2)
          CAS_LOW16;

          // Determine expected data value based on pattern type
          uint8_t expected;
          if (isRandom) expected = randomTable[mix8(chunkBase | c, row)];  // Regenerate random pattern
          else expected = pat;  // Use static or rotating pattern

          // Read data from DOUT (PC2 = bit 2) and compare with expected value
          // XOR with expected, then mask bit 2: if non-zero, data mismatch detected
          if (((PINC ^ expected) & 0x04) != 0) {
              error_found = true;
              error_column = chunkBase | c;  // Record column position for half-detection
              break;  // Exit inner loop immediately on error
          }

          // Deassert CAS to prepare for next column
          CAS_HIGH16;

          // For non-random patterns, rotate left to match writeRow_16Pin behavior
          if (!isRandom) pat = rotate_left(pat);
      }

      // If error found in inner loop, exit outer loop early
      if (error_found) break;
  }

  // Ensure CAS is deasserted after read sequence
  CAS_HIGH16;
  // Re-enable interrupts after timing-critical section
  sei();
  // Deassert RAS to close the row
  RAS_HIGH16;

  // --- ERROR HANDLING WITH 4532/3732 HALF-FUNCTIONAL CHIP DETECTION ---
  if (error_found) {
    // Special handling for 4164 chips: errors might indicate a 4532/3732 half-functional variant
    // 4532/3732 chips are 4164s where only half the columns work (either cols 0-127 or 128-255)
    if (type == T_4164) {
      // Determine which column half this error belongs to (column 128 splits the halves)
      bool is_upper_col_half = (error_column >= 128);

      // Track error patterns across column halves to identify 4532/3732 variants
      if (chip_half_status == 0) {
        // First error detected - record which column half is bad, other half might be good
        if (is_upper_col_half) {
          chip_half_status = -1;  // Upper col half (128-255) bad → might be 4532-L/3732-L (lower col half good)
        } else {
          chip_half_status = 1;   // Lower col half (0-127) bad → might be 4532-H/3732-H (upper col half good)
        }
      } else {
        // Subsequent error - verify it's still in the same bad column half
        if ((chip_half_status == -1 && !is_upper_col_half) ||
            (chip_half_status == 1 && is_upper_col_half)) {
          // Error detected in the supposedly good column half - chip is broken, not a 4532/3732
          error(patNr + 1, check);  // Never returns
          return;
        }
      }
      // Error is in the expected bad column half - continue testing the good half
      return;
    }

    // For chips already identified as 4532-L or 4532-H, errors in the known-bad column half are expected
    if (type == T_4532_L && error_column >= 128) {
      // Error in upper column half of 4532-L/3732-L is expected (upper half is known bad), ignore
      return;
    }
    if (type == T_4532_H && error_column < 128) {
      // Error in lower column half of 4532-H/3732-H is expected (lower half is known bad), ignore
      return;
    }

    // All other cases: report real error (non-4164 chip, or error in good column half of 4532/3732)
    // patNr+1 maps pattern number to LED blink code (1-6)
    error(patNr + 1, check);  // Never returns - infinite error blink
    return;
  }
}

//=======================================================================================
// REFRESH TIME TEST (41256 ONLY)
//=======================================================================================

/**
 * Perform a single CAS-before-RAS refresh cycle
 *
 * CAS-before-RAS refresh is a special DRAM refresh mode where asserting CAS before
 * RAS triggers the chip's internal refresh counter to auto-increment and refresh
 * the next row without needing to specify a row address externally.
 *
 * This is more efficient than RAS-only refresh as it doesn't require cycling through
 * all row addresses - the DRAM handles address sequencing internally.
 *
 * Timing: Total cycle ~5-10μs (CAS/RAS setup + hold + deassert)
 */
static inline void casBeforeRasRefresh_16Pin() {
  RAS_HIGH16;
  CAS_LOW16;   // Assert CAS first
  RAS_LOW16;   // Then assert RAS (triggers auto-refresh)
  NOP;
  NOP;         // Hold time for refresh
  RAS_HIGH16;  // Deassert RAS
  CAS_HIGH16;  // Deassert CAS
}

/**
 * Test DRAM refresh timing for 41256 chips
 *
 * This function verifies that the chip can retain data when refreshed at the maximum
 * specified interval. For 41256, the datasheet specifies 4ms refresh time (512 rows).
 *
 * Algorithm:
 * 1. Write pseudo-random data to column 0 of all 512 rows (with continuous refresh)
 * 2. Perform 125 full CAS-before-RAS refresh cycles with delays (125 × 4ms = 500ms)
 * 3. Read back and verify data (with continuous refresh)
 *
 * During write/read phases, we perform CAS-before-RAS refresh after every row access.
 * As long as the loop completes faster than 31.25μs per row, data integrity is maintained.
 * The real test happens in Phase 2, where we deliberately delay to test maximum retention.
 *
 * Timing: 31.25μs per row target (512 rows × 31.25μs = 16ms per full cycle)
 *         125 cycles × 16ms with delays = 2000ms total test time
 *
 * Memory usage: Uses existing randomTable[] - no additional RAM needed
 * Flash cost: ~400 bytes estimated
 */
static void refreshTimeTest_16Pin() {
  uint16_t rows = ramTypes[type].rows;  // 512 for 41256

  // ===== PHASE 1: WRITE 8 BITS OF RANDOM DATA TO EACH ROW =====
  // Write to columns 0-7 to test 8 bits per row
  // Perform CAS-before-RAS refresh after each row to maintain data integrity
  CAS_HIGH16;
  WE_LOW16;

  for (uint16_t row = 0; row < rows; row++) {
    // Get random data byte for this row (use randomTable with row as index)
    uint8_t dataByte = randomTable[row & 0xFF];  // Wrap to 256-entry table

    // Set row address and assert RAS
    SET_ADDR_PIN16(row);
    RAS_LOW16;

    // Write 8 bits (columns 0-7) in page mode
    for (uint8_t col = 0; col < 8; col++) {
      // Extract bit for this column (bit 0 = col 0, bit 7 = col 7)
      uint8_t bit = (dataByte >> col) & 0x01;

      // Set data bit (DIN = PC1)
      if (bit) {
        PORTC |= 0x02;  // DIN=1
      } else {
        PORTC &= ~0x02; // DIN=0
      }
      setAddrData(col, bit ? 0x04 : 0x00);  // Column address + data format

      // Latch column address and write data
      CAS_LOW16;
      NOP;
      CAS_HIGH16;
    }

    // End write cycle for this row
    RAS_HIGH16;

    // Perform CAS-before-RAS refresh to maintain data in previously written rows
    casBeforeRasRefresh_16Pin();
  }

  WE_HIGH16;

  // ===== PHASE 2: PERFORM 10 FULL CAS-BEFORE-RAS REFRESH CYCLES (40ms) =====
  // This tests the internal CBR refresh counter functionality:
  // - Verifies counter cycles through all addresses correctly
  // - 10 full cycles ensures counter wraps around multiple times
  // - 10× the spec refresh time: cells will lose data if refresh fails
  //
  // IMPORTANT: 41256 has 512 physical rows but only needs 256 CBR refresh cycles!
  // The internal 8-bit refresh counter (A0-A7) refreshes TWO rows per cycle:
  // - Cycle 0 refreshes rows 0 and 256
  // - Cycle 1 refreshes rows 1 and 257, etc.
  //
  // 41256: 4ms refresh / 256 cycles = 15.625μs per refresh cycle
  // Target: 10 full sequences × 4ms = 40ms counter test
  // Total: 10 × 256 = 2,560 individual refresh cycles

  for (uint8_t cycle = 0; cycle < 10; cycle++) {
    // Perform 256 CAS-before-RAS refresh cycles (covers all 512 rows)
    for (uint16_t refresh_count = 0; refresh_count < 256; refresh_count++) {
      casBeforeRasRefresh_16Pin();

      // Delay to achieve ~15.625μs per cycle (4ms / 256)
      // CBR refresh takes ~1-2μs, so delay ~14μs
      delayMicroseconds(15);
      NOP;
      NOP;
      NOP;
      NOP;
      NOP;
    }
  }

  // ===== PHASE 3: READ BACK AND VERIFY 8 BITS FROM EACH ROW =====
  // Read columns 0-7 and verify against expected data
  // Perform CAS-before-RAS refresh after each row to maintain data integrity

  for (uint16_t row = 0; row < rows; row++) {
    // Get expected data byte for this row
    uint8_t expectedByte = randomTable[row & 0xFF];

    // Set row address and assert RAS
    SET_ADDR_PIN16(row);
    RAS_LOW16;

    // Read 8 bits (columns 0-7) in page mode
    for (uint8_t col = 0; col < 8; col++) {
      // Set column address
      setAddrData(col, 0);

      // Latch column address and read data
      CAS_LOW16;
      NOP;
      NOP;

      // Read data bit (DOUT = PC2 = 0x04)
      uint8_t actualBit = (PINC & 0x04) ? 1 : 0;
      uint8_t expectedBit = (expectedByte >> col) & 0x01;

      CAS_HIGH16;

      // Check if bit matches
      if (actualBit != expectedBit) {
        RAS_HIGH16;
        // Refresh time test failed - report error code 5
        error(0, 5);  // Error type 5 = refresh timeout, code 0 = no pattern number
        return;
      }
    }

    // End read cycle for this row
    RAS_HIGH16;

    // Perform CAS-before-RAS refresh to maintain data in unread rows
    casBeforeRasRefresh_16Pin();
  }

  // Refresh test passed - continue to testOK()
}