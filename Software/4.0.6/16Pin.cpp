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


//=======================================================================================
// 16-PIN SPLIT LOOKUP TABLE GENERATION (Optimized)
// Flash savings: ~1.4 KB compared to full 512-entry table
//=======================================================================================

// Bit extraction macros - convert address bits to port configurations
// These map the 9-bit address (A0-A8) to the scattered port pin assignments
#define GET_PB(a) ((((a)&0x0010)) | (((a)&0x0008) >> 1) | (((a)&0x0040) >> 6))                      // Extract A4, A3, A6
#define GET_PC(a) ((((a)&0x0001) << 4) | (((a)&0x0100) >> 8))                                       // Extract A0, A8
#define GET_PD(a) ((((a)&0x0080) >> 1) | (((a)&0x0020) << 2) | (((a)&0x0004) >> 2) | ((a)&0x0002))  // Extract A7, A5, A2, A1

// Helper macros for generating 8-entry rows in lookup tables
#define ROW8_PB(b) GET_PB(b + 0), GET_PB(b + 1), GET_PB(b + 2), GET_PB(b + 3), GET_PB(b + 4), GET_PB(b + 5), GET_PB(b + 6), GET_PB(b + 7)
#define ROW8_PC(b) GET_PC(b + 0), GET_PC(b + 1), GET_PC(b + 2), GET_PC(b + 3), GET_PC(b + 4), GET_PC(b + 5), GET_PC(b + 6), GET_PC(b + 7)
#define ROW8_PD(b) GET_PD(b + 0), GET_PD(b + 1), GET_PD(b + 2), GET_PD(b + 3), GET_PD(b + 4), GET_PD(b + 5), GET_PD(b + 6), GET_PD(b + 7)

// Helper macros for generating 32-entry rows (4x8)
#define ROW32_PB(b) ROW8_PB(b), ROW8_PB(b + 8), ROW8_PB(b + 16), ROW8_PB(b + 24)
#define ROW32_PC(b) ROW8_PC(b), ROW8_PC(b + 8), ROW8_PC(b + 16), ROW8_PC(b + 24)
#define ROW32_PD(b) ROW8_PD(b), ROW8_PD(b + 8), ROW8_PD(b + 16), ROW8_PD(b + 24)

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
  uint8_t idx_low = a & 0x1F;  // Lower 5 bits (0-31) for inner loop
  uint8_t idx_high = a >> 5;   // Upper 4 bits for outer loop

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
#define SET_DIN_16(d) \
  if (d & 0x04) PORTC |= 0x02; \
  else PORTC &= ~0x02;

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
static void detect41257_16Pin();      // Nibble mode detection after 41256 sensing

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
  DDRB = 0b00111111;
  PORTB = 0b00101010;

  // DDRC: Set PC0-PC4 as outputs, PC2 as input (data out from DRAM)
  // PORTC: Initialize with CAS=HIGH
  DDRC = 0b00011011;
  PORTC = 0b00001000;

  // DDRD: Set PD0-PD2, PD6-PD7 as outputs (address lines)
  DDRD = 0b11000011;
  PORTD = 0x00;

  // Verify RAM is present in socket
  if (!ram_present_16Pin())
    error(0, 0);  // No RAM detected - trigger error display

  // Determine chip type by testing address lines A7 and A8
  // Detects: T_41256, T_4164, T_4816, T_4532 (TMS), T_3732 (OKI)
  sense41256_16Pin();

  // If 41256 detected, check for nibble mode (41257)
  if (type == T_41256) {
    detect41257_16Pin();
  }

  // Display chip type on OLED
  if (type == T_4164) writeRAMType((const __FlashStringHelper *)F("4164/3732(?)"));
  else  writeRAMType((const __FlashStringHelper *)ramTypes[type].name);

  // Restore I/O configuration after chip sensing
  DDRB = 0b00111111;
  PORTB = 0b00101010;

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
 * Fast pattern test for patterns 0-3 using port-ordered column iteration
 *
 * Instead of translating logical addresses through lookup tables (3 port writes
 * + 3 PROGMEM reads per CAS cycle), this function iterates column addresses in
 * "port-natural order" — nesting loops by port register so the innermost loop
 * needs only ONE port write per CAS cycle.
 *
 * Each row is written then IMMEDIATELY read back within the same outer loop
 * iteration. This ensures we stay well within the DRAM retention time window.
 *
 * Loop nesting (innermost first):
 *   PORTD: 16 combinations (A1, A2, A5, A7)  → 1 port write per CAS
 *   PORTB:  8 combinations (A3, A4, A6)       → 1 port write per 16 CAS
 *   PORTC:  2-4 combinations (A0, [A8])       → 1 port write per 128 CAS
 *
 * Total: 16 × 8 × 2 = 256 (4164) or 16 × 8 × 4 = 512 (41256/41257)
 *
 * Inner loop cost: ~10 cycles/col vs ~39 cycles/col with LUT → ~4× speedup
 *
 * Data bit logic:
 *   Patterns 0/1: DIN constant (all 0 or all 1)
 *   Patterns 2/3: DIN determined by A0 (creates checkerboard in DRAM array)
 *   DIN is embedded in pre-computed PORTC values — zero per-column DIN overhead
 *
 * @param patNr Pattern number (0-3 only)
 */

// Global half-good error tracking (set by fastPatternTest/checkRow error handlers)
static uint8_t error_in_lower_half;  // Errors where A7=0 (RAS for 4532, CAS for 3732)
static uint8_t error_in_upper_half;  // Errors where A7=1

static void __attribute__((hot)) fastPatternTest_16Pin(uint8_t patNr) {
  uint16_t total_rows = ramTypes[type].rows;
  bool is_nibble = (ramTypes[type].flags & RAM_FLAG_NIBBLE_MODE);
  uint8_t num_pc = (ramTypes[type].columns > 256) ? 4 : 2;

  // --- Determine DIN value per A0 state ---
  // DIN is on PC1 (0x02), DOUT is on PC2 (0x04)
  uint8_t pat = pattern[patNr];
  uint8_t din_a0_0 = (pat & 0x04) ? 0x02 : 0x00;  // DIN when A0=0
  uint8_t din_a0_1;
  if (patNr <= 1) {
    din_a0_1 = din_a0_0;  // Constant pattern
  } else {
    din_a0_1 = (rotate_left(pat) & 0x04) ? 0x02 : 0x00;  // Alternating
  }

  // --- Build PORTD LUT: 16 entries for A2(PD0), A1(PD1), A7(PD6), A5(PD7) ---
  uint8_t pd_base = PORTD & 0x3C;  // Preserve PD2-PD5 (inputs)
  uint8_t pd_lut[16];
  for (uint8_t i = 0; i < 16; i++) {
    // i bits 0,1 → PD0,PD1 (A2,A1);  i bits 2,3 → PD6,PD7 (A7,A5)
    pd_lut[i] = pd_base | (i & 0x03) | ((i & 0x0C) << 4);
  }

  // --- Build PORTB address parts: 8 entries for A6(PB0), A3(PB2), A4(PB4) ---
  uint8_t pb_addr[8];
  for (uint8_t i = 0; i < 8; i++) {
    pb_addr[i] = (i & 1) | ((i & 2) << 1) | ((i & 4) << 2);
  }

  // --- Build PORTC LUTs: A0(PC4), A8(PC0), DIN(PC1), CAS(PC3)=HIGH ---
  // Write LUT: DIN embedded for write phase
  uint8_t pc_w[4];
  pc_w[0] = 0x08 | din_a0_0;         // A8=0 A0=0
  pc_w[1] = 0x08 | 0x10 | din_a0_1;  // A8=0 A0=1
  pc_w[2] = 0x08 | 0x01 | din_a0_0;  // A8=1 A0=0
  pc_w[3] = 0x08 | 0x11 | din_a0_1;  // A8=1 A0=1

  // Read LUT: no DIN (PC1 is input during read)
  uint8_t pc_r[4];
  pc_r[0] = 0x08;         // A8=0 A0=0
  pc_r[1] = 0x08 | 0x10;  // A8=0 A0=1
  pc_r[2] = 0x08 | 0x01;  // A8=1 A0=0
  pc_r[3] = 0x08 | 0x11;  // A8=1 A0=1

  // Expected DOUT (PC2 = 0x04) per A0 state
  uint8_t exp_a0[4];
  exp_a0[0] = exp_a0[2] = din_a0_0 ? 0x04 : 0x00;
  exp_a0[1] = exp_a0[3] = din_a0_1 ? 0x04 : 0x00;

  // ===================== WRITE + READ PER ROW =====================
  // Each row is written then immediately read back within the same
  // outer loop iteration, staying well within DRAM retention time.

  CAS_HIGH16;

  cli();
  for (uint16_t row = 0; row < total_rows; row++) {

    // ---------- WRITE phase for this row ----------
    DDRC |= 0x02;  // DIN as OUTPUT
    rasHandling_16Pin(row);
    WE_LOW16;
    uint8_t pb_base = PORTB & 0xEA;

    for (uint8_t pc_i = 0; pc_i < num_pc; pc_i++) {
      PORTC = pc_w[pc_i];

      for (uint8_t pb_i = 0; pb_i < 8; pb_i++) {
        PORTB = pb_base | pb_addr[pb_i];

        // Tight inner loop: only PORTD changes per CAS cycle
        const uint8_t *p = pd_lut;
        for (uint8_t k = 0; k < 16; k++) {
          CAS_HIGH16;
          PORTD = *p++;
          CAS_LOW16;
        }
        CAS_HIGH16;
        // RAS cycling for nibble mode (tRAS max ~10μs)
        if (is_nibble) {
          rasHandling_16Pin(row);
          pb_base = PORTB & 0xEA;
          PORTC = pc_w[pc_i];
        }
      }
    }

    WE_HIGH16;
    RAS_HIGH16;

    // ---------- READ phase for same row ----------
    PORTC &= ~0x02;
    DDRC &= ~0x02;  // DIN as INPUT (shared Din/Dout chips)

    rasHandling_16Pin(row);
    pb_base = PORTB & 0xEA;

    for (uint8_t pc_i = 0; pc_i < num_pc; pc_i++) {
      PORTC = pc_r[pc_i];
      uint8_t exp = exp_a0[pc_i];

      for (uint8_t pb_i = 0; pb_i < 8; pb_i++) {
        PORTB = pb_base | pb_addr[pb_i];

        for (uint8_t pd_i = 0; pd_i < 16; pd_i++) {
          PORTD = pd_lut[pd_i];
          CAS_LOW16;
          NOP;  // tCAC: CAS access time
          CAS_HIGH16;

          if ((PINC ^ exp) & 0x04) {
            // ---- Error: mismatch on DOUT ----
            // Track half-errors for 4164/4532/3732 half-good detection
            if (row >= 128) error_in_upper_half |= 0x01; else error_in_lower_half |= 0x01;
            if (PORTD & 0x40) error_in_upper_half |= 0x02; else error_in_lower_half |= 0x02;
            if (type == T_4164 || type == T_4532 || type == T_3732) {
              if ((error_in_lower_half & error_in_upper_half & 0x03) != 0x03)
                continue;
            }
            sei(); RAS_HIGH16; DDRC |= 0x02;
            error(patNr + 1, 2); return;
          }
        }

        if (is_nibble) {
          rasHandling_16Pin(row);
          pb_base = PORTB & 0xEA;
          PORTC = pc_r[pc_i];
        }
      }
    }
    RAS_HIGH16;
  }
  sei();
  DDRC |= 0x02;
}

/**
 * Run all test patterns on detected RAM chip
 *
 * Patterns 0-3: Fast port-ordered write-then-read per row (~4× faster)
 * Patterns 4-5: Pseudo-random with retention testing (ordered access required)
 *
 * Half-good chip handling (T_4532, T_3732):
 *   Both types are tested as full 256x256 (same as 4164). Errors in the
 *   expected bad half are ignored (continue). After all patterns complete,
 *   the subtype is determined by which half had errors:
 *   - T_4532: RAS-split → track by row A7 → NL3 (lower good) / NL4 (upper good)
 *   - T_3732: CAS-split → track by col A7 → -L (lower good) / -H (upper good)
 */

static void run_16Pin_tests() {
  uint16_t total_rows = ramTypes[type].rows;

  // Reset half-good error tracking
  error_in_lower_half = 0;
  error_in_upper_half = 0;

  // Patterns 0-3: Fast port-ordered per-row write-then-read
  for (uint8_t patNr = 0; patNr < 4; patNr++) {
    fastPatternTest_16Pin(patNr);
  }

  if (ramTypes[type].flags & RAM_FLAG_NIBBLE_MODE) total_rows /= 2;  // Nibble Mode: A8 used for nibble bit selection
  // Patterns 4-5: Pseudo-random with retention testing (needs sequential access)
  for (uint8_t patNr = 4; patNr <= 5; patNr++) {
    if (patNr == 5) invertRandomTable();

    for (uint16_t row = 0; row < total_rows; row++) {
      writeRow_16Pin(row, ramTypes[type].columns, patNr);
    }
  }

  // ========== HALF-GOOD SUBTYPE DETERMINATION ==========
  // error_in_lower/upper_half bits: 0x01=row, 0x02=col
  // T_4532 already detected in sense. T_3732: reclassify from T_4164 if col-half errors.
  if (type == T_4164 && (error_in_lower_half & 0x02) != (error_in_upper_half & 0x02))
    type = T_3732;
  if (type == T_3732)
    typeSuffix = (error_in_lower_half & 0x02) ? F("(H)") : F("(L)");
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
  DDRC |= 0x02;  // Ensure PC1 (Din) is OUTPUT for shared Din/Dout chips
  SET_ADDR_PIN16(row);
  RAS_LOW16;
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
 * NOTE: For chips where Din/Dout are connected (e.g., 41256 in A501),
 * PC1 (Din) must be switched to INPUT before reading to avoid bus contention.
 *
 * @param row Row address to read from
 * @param col Column address to read from
 * @return Data bit read (0 or 1)
 */
static inline uint8_t read16Pin(uint16_t row, uint16_t col) {
  // Set Din to Input - fix for in-circuit testing
  PORTC &= ~0x02;
  DDRC &= ~0x02;
  SET_ADDR_PIN16(row);
  RAS_LOW16;
  setAddrData(col, 0);
  CAS_LOW16;
  NOP;  // Hold time
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
 * Detect RAM capacity and type for 16-pin DRAMs
 *
 * Detection sequence:
 *   1. Basic write/read at (0,0) to verify chip responds
 *   2. A8 alias test: row 0 vs row 256 → 41256 if distinct, else 4164-class
 *   3. Row A7 alias test: row 0 vs row 128 → if aliased:
 *      - Col A7 test: col 0 vs 128 → aliased = 4816, independent = TMS4532
 *   4. Otherwise → T_4164 (MSM3732 detected during pattern tests)
 *
 * Sets global 'type' to T_4164, T_41256, T_4816, or T_4532.
 */
void sense41256_16Pin() {
  CAS_HIGH16;

  // ========== STEP 1: BASIC FUNCTIONALITY CHECK ==========
  write16Pin(0, 0, 0);
  if (read16Pin(0, 0) != 0) {
    // Row 0, Col 0 doesn't hold data — try another location
    write16Pin(0, 192, 0);
    if (read16Pin(0, 192) != 0) {
      error(0, 0);  // No functional RAM
    }
    // At least some cells work, continue detection
  }

  // ========== STEP 2: TEST A8 LINE (4164 vs 41256) ==========
  write16Pin(0, 0, 0);
  write16Pin(256, 0, 1);
  if (read16Pin(0, 0) == 0) {
    type = T_41256;  // A8 functional → 256Kx1
    return;
  }

  // ========== STEP 3: TEST A7 ROW LINE (4164 vs 4816 vs 4532) ==========
  write16Pin(0, 0, 0);
  write16Pin(128, 0, 1);
  if (read16Pin(0, 0) != 0) {
    // Row A7 aliased — could be 4816 (128 cols) or 4532 (256 cols, RAS-split)
    // Distinguish by testing col A7: write col 0, write col 128, read col 0
    write16Pin(0, 0, 0);
    write16Pin(0, 128, 1);
    if (read16Pin(0, 0) == 0) {
      type = T_4532;  // Col A7 works (256 cols) + Row A7 aliased → TMS4532
    } else {
      type = T_4816;  // Col A7 also aliased (128 cols) → 16Kx1
    }
    return;
  }

  // 3732 detection deferred to pattern tests — defective col-half passes single-cell tests
  type = T_4164;
}

/**
 * Detect if a 41256 is actually a 41257 (nibble mode DRAM)
 *
 * Uses nibble mode for BOTH writing and reading, making the detection
 * robust against CAS timing margins. Since write and read use the same
 * CAS toggle mechanism, the internal nibble counter behaves identically
 * in both phases — even if CAS HIGH time is borderline and causes counter
 * resets, both phases reset symmetrically and the pattern still matches.
 *
 * Detection sequence:
 *   1. Clear cols 0-3 to 0 (standard single writes)
 *   2. Nibble WRITE: 1,0,1,0 via 4 CAS toggles (WE low, address fixed at 0)
 *   3. Nibble READ: 4 CAS toggles (WE high), capture Dout after each CAS↓
 *   4. Evaluate captured values
 *
 * Expected results:
 *   41257 (nibble mode): Write reaches cols 0-3, read returns [1,0,1,0] ✓
 *   41256 (no nibble):   All CAS hits col 0 only
 *                        Write: col 0 overwritten 4× with last value 0
 *                        Read:  col 0 read 4× → [0,0,0,0]
 *
 * CAS timing note:
 *   Din for the next nibble is pre-loaded while CAS is still LOW (data is
 *   latched at CAS↓). This minimizes CAS HIGH time to ~125ns.
 *
 * Must be called after sense41256_16Pin() detects T_41256.
 */
static void detect41257_16Pin() {
  // ===== STEP 1: Clear all 4 A8 row/col combinations =====
  // A8 is shared for row and column addressing. On 41257, A8 maps to nibble counter.
  // Must clear all combinations to ensure nibble test reads known-zero state.
  write16Pin(0, 0, 0);      // row_A8=0, col_A8=0
  write16Pin(0, 256, 0);    // row_A8=0, col_A8=1
  write16Pin(256, 0, 0);    // row_A8=1, col_A8=0
  write16Pin(256, 256, 0);  // row_A8=1, col_A8=1

  // ===== STEP 2: Nibble mode WRITE — 1,0,1,0 to cols 0-3 =====
  // Din pre-loaded during CAS LOW to keep CAS HIGH minimal (tNCH)
  DDRC |= 0x02;       // Ensure DIN is OUTPUT
  SET_ADDR_PIN16(0);  // Address bus = 0 (row 0, column 0)
  RAS_LOW16;          // Latch row 0
  WE_LOW16;           // Write mode

  // Nibble 0 → col 0: Din=1
  PORTC |= 0x02;   // Din=1
  NOP;             // tDS: data setup before CAS↓
  CAS_LOW16;       // CAS↓: latch write, counter starts at col 0
  NOP;             // tCAS: CAS low pulse width
  CAS_HIGH16;      // CAS↑: counter → col 1
  PORTC &= ~0x02;  // Pre-load Din=0 for nibble
  NOP;             // tNRH: nibble recovery
  // Nibble 1 → col 1: Din=0
  CAS_LOW16;      // CAS↓: latch write to col 1
  NOP;            // tCAS
  CAS_HIGH16;     // counter → col 2
  PORTC |= 0x02;  // Pre-load Din=1 for nibble 2
  NOP;            // tNRH
  // Nibble 2 → col 2: Din=1
  CAS_LOW16;       // CAS↓: latch write to col 2
  NOP;             // tCAS
  CAS_HIGH16;      // counter → col 3
  PORTC &= ~0x02;  // Pre-load Din=0 for nibble 3
  NOP;             // tNRH
  // Nibble 3 → col 3: Din=0
  CAS_LOW16;  // CAS↓: latch write to col 3
  NOP;
  NOP;  // tCAS: ensure last write completes
  CAS_HIGH16;
  NOP;  // tWR: write recovery
  WE_HIGH16;
  NOP;  // tRWL: WE-to-RAS recovery
  RAS_HIGH16;
  // ===== STEP 3: Nibble mode READ — 4× CAS toggle, capture Dout =====
  PORTC &= ~0x02;
  DDRC &= ~0x02;  // Din as INPUT (shared Din/Dout chips)

  SET_ADDR_PIN16(0);  // Address bus = 0 (row 0, column 0)
  RAS_LOW16;          // Latch row 0
  NOP;                // tRCD: RAS-to-CAS delay

  // Nibble 0 → read col 0
  CAS_LOW16;  // CAS↓: latch col 0, Dout becomes valid after tCAC
  NOP;        // tCAC: CAS access time
  NOP;
  uint8_t v0 = PINC;  // Capture Dout (bit 2)
  CAS_HIGH16;         // CAS↑: counter → col 1
  NOP;                // tNRH

  // Nibble 1 → read col 1
  CAS_LOW16;
  NOP;  // tNCAC: nibble CAS access time
  NOP;
  uint8_t v1 = PINC;
  CAS_HIGH16;
  NOP;

  // Nibble 2 → read col 2
  CAS_LOW16;
  NOP;
  NOP;
  uint8_t v2 = PINC;
  CAS_HIGH16;
  NOP;

  // Nibble 3 → read col 3
  CAS_LOW16;
  NOP;
  NOP;
  uint8_t v3 = PINC;
  CAS_HIGH16;

  RAS_HIGH16;
  DDRC |= 0x02;  // Restore Din as OUTPUT

  // ===== STEP 4: Evaluate — check Dout (PC2 = 0x04) =====
  // 41257: [1,0,1,0] → nibble mode confirmed
  // 41256: [0,0,0,0] → no nibble mode (col 0 = 0, read 4×)
  if ((v0 & 0x04) && !(v1 & 0x04) && (v2 & 0x04) && !(v3 & 0x04)) {
    type = T_41257;
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
  uint8_t row_bits = countBits(max_rows - 1);
  uint8_t col_bits = countBits(max_cols - 1);

  CAS_HIGH16;
  RAS_HIGH16;
  WE_LOW16;

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

    // T_4532: RAS A7 aliased — skip A7 row test
    if (type == T_4532 && b == 7) continue;

    // Normal address line test - fail if values don't match expected
    if (base_result != 0) error(b, 1);  // Base address corrupted
    if (peer_result != 1) error(b, 1);  // Peer address didn't write correctly
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
  RAS_HIGH16;             // Deassert RAS
  setAddr16_Random(row);  // Set row address on bus
  RAS_LOW16;              // Assert RAS to latch address
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
 * 4. For patterns 0-3: defers verification to checkRow_16Pin (fresh RAS cycle)
 * 5. For patterns 4-5: writes then performs retention testing
 *
 * All patterns write the complete row before any read-back occurs.
 * This ensures proper RAS cycling between write and read, which is
 * required by chips like the Toshiba 41257 (nibble mode DRAM).
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
 * All pattern errors are detected by checkRow_16Pin which tracks column
 * position for half-functional chip detection (4532/3732 variants).
 *
 * @param row Row address to write (0-255 for 4164, 0-511 for 41256)
 * @param cols Number of columns in row (256 for 4164, 512 for 41256)
 * @param patNr Pattern number (0-5): 0=0x00, 1=0xFF, 2=0xAA, 3=0x55, 4-5=random
 */
void __attribute__((hot)) writeRow_16Pin(uint16_t row, uint16_t cols, uint8_t patNr) {

  // Last row for retention testing
  uint16_t last_row = ramTypes[type].rows - 1;

  // Ensure PC1 (Din) is OUTPUT for writing (required for shared Din/Dout chips)
  DDRC |= 0x02;
  CAS_HIGH16;

  // Calculate number of 32-column chunks
  uint8_t num_chunks = cols >> 5;
  bool is_nibble = (ramTypes[type].flags & RAM_FLAG_NIBBLE_MODE);

  if (!is_nibble) {
    // ====== NORMAL PATH (4164/41256/4816/4532/3732): Full-row page mode, NO RAS cycling ======
    rasHandling_16Pin(row);
    WE_LOW16;
    cli();
    for (uint8_t chunk = 0; chunk < num_chunks; chunk++) {
      uint8_t hb = pgm_read_byte(&lut_16_high_b[chunk]);
      uint8_t hc = pgm_read_byte(&lut_16_high_c[chunk]);
      uint8_t hd = pgm_read_byte(&lut_16_high_d[chunk]);
      uint8_t base_b = (PORTB & 0xEA) | hb;
      uint8_t base_c = (PORTC & 0xEC) | hc;
      uint8_t base_d = (PORTD & 0x3C) | hd;
      uint16_t chunkBase = (chunk << 5);

      for (int8_t c = 31; c >= 0; c--) {
        uint8_t val_c = base_c | pgm_read_byte(&lut_16_low_c[c]);
        uint8_t data_check = randomTable[mix8(chunkBase | c, row)];
        if (data_check & 0x04) val_c |= 0x02;
        CAS_HIGH16;
        PORTB = base_b | pgm_read_byte(&lut_16_low_b[c]);
        PORTD = base_d | pgm_read_byte(&lut_16_low_d[c]);
        PORTC = val_c;
        CAS_LOW16;
      }
    }
    CAS_HIGH16;
    sei();
    WE_HIGH16;
    refreshRow_16Pin(row);
  } else {
    // ====== NIBBLE PATH (41257): Single access — one RAS cycle per cell ======
    // No page mode → no tRAS max problem. ~1.2µs/cell, ~315ms per pass.
    WE_LOW16;
    for (uint16_t col = 0; col < cols / 2; col++) {
      setAddr16_Random(row);
      RAS_LOW16;
      uint8_t data0 = randomTable[mix8(col, row)];
      uint8_t data1 = data0 >> 1;
      uint8_t data2 = data0 << 1;
      uint8_t data3 = data0 << 2;
      setAddr16_Random(col);
      SET_DIN_16(data0);
      CAS_LOW16;
      NOP;
      CAS_HIGH16;
      SET_DIN_16(data1);
      CAS_LOW16;
      NOP;
      CAS_HIGH16;
      SET_DIN_16(data2);
      CAS_LOW16;
      NOP;
      CAS_HIGH16;
      SET_DIN_16(data3);
      CAS_LOW16;
      NOP;
      CAS_HIGH16;
      RAS_HIGH16;
    }
    WE_HIGH16;
  }

  // ========== RETENTION TESTING (patterns 4-5 only) ==========

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
  RAS_HIGH16;              // Deassert RAS to complete refresh cycle
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
 * 4532/3732 Half-Good Detection Logic:
 * For chips identified as T_4532 or T_3732 by sense41256_16Pin(), errors are tracked
 * per A7-half using error_in_lower_half / error_in_upper_half:
 * - T_4532 (TMS): RAS-split. Row A7 determines half (row >= 128 = upper half)
 * - T_3732 (OKI): CAS-split. Column A7 determines half (error_column >= 128 = upper half)
 *
 * Errors in the defective half are expected and ignored (return without error()).
 * After all 6 patterns complete, run_16Pin_tests() determines the subtype
 * (NL3/NL4 for TMS, -L/-H for OKI) based on which half had errors.
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

  // Calculate number of 32-column chunks
  uint8_t num_chunks = cols >> 5;
  bool error_found = false;
  uint16_t error_column = 0;  // Track which column failed for half-detection
  bool is_nibble = (ramTypes[type].flags & RAM_FLAG_NIBBLE_MODE);

  // Switch PC1 (Din) to INPUT for shared Din/Dout chips (e.g., 41256 in A501)
  PORTC &= ~0x02;
  DDRC &= ~0x02;

  if (!is_nibble) {
    // ====== NORMAL PATH (4164/41256/4816/4532/3732): Full-row page mode, NO RAS cycling ======
    rasHandling_16Pin(row);
    cli();
    for (uint8_t chunk = 0; chunk < num_chunks; chunk++) {
      uint8_t hb = pgm_read_byte(&lut_16_high_b[chunk]);
      uint8_t hc = pgm_read_byte(&lut_16_high_c[chunk]);
      uint8_t hd = pgm_read_byte(&lut_16_high_d[chunk]);
      uint8_t base_b = (PORTB & 0xEA) | hb;
      uint8_t base_c = (PORTC & 0xEC) | hc;
      uint8_t base_d = (PORTD & 0x3C) | hd;
      uint16_t chunkBase = (chunk << 5);

      for (int8_t c = 31; c >= 0; c--) {
        SET_ADDR_16_LOW(c);
        CAS_LOW16;
        uint8_t expected;
        if (isRandom) expected = randomTable[mix8(chunkBase | c, row)];
        else expected = pat;
        if (((PINC ^ expected) & 0x04) != 0) {
          if (!error_found) error_column = chunkBase | c;
          error_found = true;
        }
        CAS_HIGH16;
        if (!isRandom) pat = rotate_left(pat);
      }
    }
    CAS_HIGH16;
    sei();
    RAS_HIGH16;

  // } else if (patNr < 4) {
  //   // ====== NIBBLE PATH (41257): Single access read ======
  //   // One RAS cycle per cell — no tRAS max issue.
  //   for (uint16_t col = 0; col < cols; col++) {
  //     setAddr16_Random(row);
  //     RAS_LOW16;
  //     setAddr16_Random(col);
  //     CAS_LOW16;
  //     CAS_HIGH16;
  //     if (((PINC ^ pat) & 0x04) != 0) {
  //       CAS_HIGH16;
  //       RAS_HIGH16;
  //       error_found = true;
  //       error_column = col;
  //       break;
  //     }
  //     RAS_HIGH16;
  //     pat = rotate_left(pat);
  //   }
  } else {
    for (uint16_t col = 0; col < cols / 2; col++) {
      uint8_t data0 = randomTable[mix8(col, row)];
      uint8_t data1 = data0 >> 1;
      uint8_t data2 = data0 << 1;
      uint8_t data3 = data0 << 2;
      setAddr16_Random(row);
      RAS_LOW16;
      setAddr16_Random(col);
      CAS_LOW16;
      CAS_HIGH16;
      data0 = PINC ^ data0;
      CAS_LOW16;
      CAS_HIGH16;
      data1 = PINC ^ data1;
      CAS_LOW16;
      CAS_HIGH16;
      data2 = PINC ^ data2;
      CAS_LOW16;
      CAS_HIGH16;
      data3 = PINC ^ data3;
      if ((data0 | data1 | data2 | data3) & 0x04) {
        CAS_HIGH16;
        RAS_HIGH16;
        error_found = true;
        error_column = col;
        break;
      }
      RAS_HIGH16;
    }
  }

  // Switch PC1 (Din) back to OUTPUT for next write operation
  DDRC |= 0x02;

  // --- ERROR HANDLING WITH HALF-FUNCTIONAL CHIP DETECTION ---
  if (error_found) {
    if (row >= 128) error_in_upper_half |= 0x01; else error_in_lower_half |= 0x01;
    if (error_column >= 128) error_in_upper_half |= 0x02; else error_in_lower_half |= 0x02;
    if ((type == T_4164 || type == T_4532 || type == T_3732) &&
        (error_in_lower_half & error_in_upper_half & 0x03) != 0x03)
      return;
    error(patNr + 1, check);
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
  CAS_LOW16;  // Assert CAS first
  RAS_LOW16;  // Then assert RAS (triggers auto-refresh)
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
  DDRC |= 0x02;  // Ensure PC1 (Din) is OUTPUT for shared Din/Dout chips
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
        PORTC &= ~0x02;  // DIN=0
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

  // Switch PC1 (Din) to INPUT for shared Din/Dout chips
  PORTC &= ~0x02;
  DDRC &= ~0x02;
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
        DDRC |= 0x02;  // Restore PC1 to OUTPUT before error
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

  // Switch PC1 (Din) back to OUTPUT
  DDRC |= 0x02;

  // Refresh test passed - continue to testOK()
}