// 20Pin.cpp - Implementation of 20-Pin DRAM testing functions
//=======================================================================================
//
// This file contains all test logic for 20-pin DRAM packages. It implements:
// - Standard 20-pin DRAM testing (514256, 514258, 514400, 514402)
// - 4116 adapter support (16Kx1 with voltage conversion)
// - Static column mode optimization for 514258/514402
// - Checkerboard coupling test (passes 0-1) + pseudo-random patterns (4-5)
// - Optimized loop unrolling for performance (2x/4x unroll)
// - Page mode read/write operations with minimal timing overhead
//
// Supported chips:
// - 514256 (256Kx4, 512 rows x 512 cols, 4ms refresh)
// - 514258 (256Kx4 Static Column, 512 rows x 512 cols, 4ms refresh)
// - 514400 (1Mx4, 1024 rows x 1024 cols, 16ms refresh)
// - 514402 (1Mx4 Static Column, 1024 rows x 1024 cols, 16ms refresh)
// - 4116 (16Kx1 via adapter, 128 rows x 128 cols, 2ms refresh, -5V/+12V)
// - 4027 (4Kx1 via adapter, 64 rows x 64 cols, 2ms refresh)
//
//=======================================================================================

#include "Arduino.h"
#include "20Pin.h"

//=======================================================================================
// MAIN TEST FUNCTION
//=======================================================================================

// Forward declarations
static inline void casBeforeRasRefresh_20Pin();  // CBR (defined below; used by pass_20Pin)
static void pass_20Pin(uint8_t patNr, bool runCBR);  // checkerboard 0-1; runCBR = 60s CBR test



/**
 * Main test function for 20-pin DRAMs (514256, 514258, 514400, 514402, 4116, 4027)
 *
 * Performs complete test sequence:
 * 1. Checks for 4116/4027 adapter presence (voltage detection)
 * 2. If adapter found: Executes 4116/4027-specific tests and exits
 * 3. Otherwise: Tests standard 20-pin DRAMs (514xxx series)
 * 4. Detects RAM presence and capacity (256K vs 1M)
 * 5. Detects static column mode capability (514258/514402)
 * 6. Verifies address line functionality
 * 7. Runs the checkerboard passes (0-1) and pseudo-random patterns (4-5, retention)
 * 8. Performs refresh time test to verify CBR counter
 * 9. Calls testOK() on success (never returns)
 *
 * @note Function never returns - ends in testOK() or error()
 */
void test_20Pin(void) {
  // Check for 4116 Adapter first
  if (test_4116() == true) {
    type = T_4116;
    configureIO_20Pin();
    test_4116_logic();  // Call 4116-specific test logic
    return;
  } else {
    configureIO_20Pin();
    if (!ram_present_20Pin())
      error(0, 0);
    senseRAM_20Pin();
    senseSCRAM_20Pin();
  }
  writeRAMType((const __FlashStringHelper*)ramTypes[type].name);
  configureIO_20Pin();
  checkAddressing_20Pin();

  boolean is_static = ramTypes[type].flags & RAM_FLAG_STATIC_COLUMN;
  uint16_t total_rows = ramTypes[type].rows;
  do {
    // Patterns 0-1: checkerboard coupling (write-all -> read-all, FPM for all incl. SC).
    pass_20Pin(0, false);
    pass_20Pin(1, false);
    // Patterns 4-5: pseudo-random WITH retention testing (write row -> controlled
    // aging via delayRows/delays[]/writeTime -> read back). This is the REAL retention
    // test (rows are NOT refreshed while they age). SC chips use SC write+read,
    // non-SC use FPM (writeRow_20Pin / checkRow_20Pin branch on is_static).
    for (uint8_t patNr = 4; patNr <= 5; patNr++) {
      if (patNr == 5) invertRandomTable();
      for (uint16_t row = 0; row < total_rows; row++)
        writeRow_20Pin(row, patNr, is_static);
    }
    // CBR refresh-COUNTER test AFTER the full regular test (so the success screen is
    // not shown prematurely): loop mode only, every 10th run (s_runCount % 10 == 1 ->
    // runs 1, 11, 21, ...). pass_20Pin(0,true) re-writes the array, runs ~60 s of CBR
    // (own "CBR:<sec>" screen + LED off), then verifies. All 514xxx are CBR-capable.
    if (CFG_LOOP_ACTIVE && (s_runCount % 10 == 1)) {
      cbrScreenPrep();  // render BEFORE the write phase (25-60 ms zero-refresh)
      pass_20Pin(0, true);
    }
    testOK();
  } while (CFG_LOOP_ACTIVE);
}


//=======================================================================================
// 4116/4027 ADAPTER DETECTION AND TESTING - OPTIMIZED FOR MINIMAL RAM
//=======================================================================================

/**
 * Test for 4116/4027 adapter presence by checking voltage levels and pin states
 * @return true if adapter is detected, false otherwise
 */
bool test_4116(void) {
  adc_init();
  DDRB &= ~((1 << PB2) | (1 << PB4));
  PORTB &= ~((1 << PB2) | (1 << PB4));
  DDRC &= ~((1 << PC2) | (1 << PC3));
  PORTC &= ~((1 << PC2) | (1 << PC3));

  delayMicroseconds(5);
  if (!(PINB & (1 << PB4)) || !(PINB & (1 << PB2))) return false;

  uint16_t adc_pc2 = adc_read(2);
  if (adc_pc2 < ADC_4116_LOW || adc_pc2 > ADC_4116_HIGH) return false;

  uint16_t adc_pc3 = adc_read(3);
  if (adc_pc3 < ADC_4116_LOW || adc_pc3 > ADC_4116_HIGH) return false;

  return true;
}

/**
 * Write to 4116/4027 RAM at row 0, column 0
 * @param data Data bit to write (0 or 1)
 */
static inline void write_4116_00(uint8_t data) {
  setAddr_4116(0);
  RAS_LOW20;
  SET_DIN_4116(data);
  WE_LOW20;
  CAS_LOW20;
  delay_1us();
  CAS_HIGH20;
  WE_HIGH20;
  RAS_HIGH20;
  delay_1us();
}

/**
 * Read from 4116/4027 RAM at row 0, column 0
 * @return Data bit read (0 or 1)
 */
static inline uint8_t read_4116_00(void) {
  RAS_LOW20;
  CAS_LOW20;
  delay_1us();
  uint8_t result = GET_DOUT_4116();
  CAS_HIGH20;
  RAS_HIGH20;
  return result;
}

/**
 * Distinguish between 4116 (16Kx1) and 4027 (4Kx1) by testing A6/CS pin behavior
 *
 * Detection method:
 * 1. Write '1' to address 0,0 and verify chip responds (not empty socket)
 * 2. Set pin PD6 (A6/CS) HIGH
 * 3. For 4027: PD6 is Chip Select (active LOW), so CS=HIGH disables chip
 * 4. For 4116: PD6 is address line A6, CS=HIGH has no effect on chip enable
 * 5. Perform test write with CS=HIGH:
 *    - 4027: Write ignored (chip disabled), previous data '1' remains
 *    - 4116: Write succeeds (chip still enabled), data changes to '0'
 * 6. Read back data to determine chip type
 *
 * @return true if 4027 detected (4Kx1, 64 rows × 64 cols)
 * @return false if 4116 detected (16Kx1, 128 rows × 128 cols)
 *
 * @note Calls error(0,0) if no RAM detected in socket
 */
static bool detect_4027(void) {
  // ASSUMPTION: PC2/PC3 are driven as OUTPUTS here (DDRC bits 1-4). On the 4116/4027
  // adapter these lines are level-shifted control signals, NOT the ADC dividers that
  // test_4116() sampled — re-purposing them as outputs after the ADC check is safe.
  DDRC = 0b00011110;
  PORTD = 0x00;
  RAS_HIGH20;
  CAS_HIGH20;
  WE_HIGH20;

  // Write 1 to address 0,0 and verify chip is present
  write_4116_00(1);
  if (read_4116_00() == 0) error(0, 0);  // No chip present

  // Test CS/A6 pin behavior
  PORTD = 0x40;  // Set CS/A6 High
  delay_1us();
  SET_DIN_4116(1);
  WE_LOW20;
  RAS_LOW20;
  delay_1us();
  CAS_LOW20;
  delay_1us();
  CAS_HIGH20;
  SET_DIN_4116(0);
  PORTD = 0x41;
  CAS_LOW20;
  delay_1us();
  CAS_HIGH20;
  RAS_HIGH20;
  WE_HIGH20;
  PORTD = 0x40;
  delay_1us();
  RAS_LOW20;
  delay_1us();
  CAS_LOW20;
  delay_1us();

  if (GET_DOUT_4116() == 1) {
    CAS_HIGH20;
    PORTD = 0x41;
    CAS_LOW20;
    delay_1us();
    if (GET_DOUT_4116() == 0) {
      CAS_HIGH20;
      RAS_HIGH20;
      return false;  // Disabling CS preserved the 1 since ram was inactive during last write
    }
  }
  CAS_HIGH20;
  RAS_HIGH20;
  return true;
}
// Checkerboard pass (patterns 0-1) for 4116/4027 (1-bit). Mirrors the 4164 logic in
// checkerQuadrant_16Pin: a 2-row write/read LAG pipeline (write row N, read row N-LAG,
// LAG=2). When a victim row is read, BOTH vertical neighbours are already written ->
// inter-row coupling is exercised, yet at most LAG rows are ever pending (written-but-
// not-read), so each row's write->read window stays ~0.7 ms — well under the 2 ms
// refresh spec. (The former write-all -> read-all let the first row age ~22 ms = out of
// spec, mixing a retention component into the coupling test.) These chips PRE-DATE
// CAS-before-RAS refresh, so no CBR; the tight LAG window removes the need for any extra
// refresh. tRAS bounded by a RAS-only recycle every 4 columns. Two passes: pass 0
// ascending, pass 1 descending + inverted background. No quadrant (no variants).
//   patNr 0/1 = checkerboard ((row^col)&1)^inv
static void pass_4116(uint8_t patNr) {
  uint8_t rows = ramTypes[type].rows;
  uint8_t cols = (uint8_t)ramTypes[type].columns;
  uint8_t inv = patNr & 1;        // pass 1 inverts the checkerboard background
  bool ascending = (patNr == 0);  // pass 0 up, pass 1 down (catch order-dependent coupling)
  const uint8_t LAG = 2;          // read trails write by 2 rows (both neighbours written)

  cli();
  for (uint8_t i = 0; i < rows + LAG; i++) {
    // ---------- WRITE row_w (write index i, while rows remain) ----------
    if (i < rows) {
      uint8_t row_w = ascending ? i : (uint8_t)(rows - 1 - i);
      uint8_t rp = (row_w & 1) ^ inv;
      rasHandling_4116(row_w);
      WE_LOW20;
      for (uint8_t col = 0; col < cols; col++) {
        uint8_t d = (rp ^ (col & 1)) & 1;
        SET_DIN_4116(d);
        setAddr_4116(col);
        NOP;
        NOP;
        CAS_LOW20;
        NOP;
        NOP;
        CAS_HIGH20;
        // tRAS-max guard (4116 spec ~10 us): per-col ~1.2 us -> recycle RAS every 4
        // cols (~5 us, 2x margin). RAS-only (no CBR; 4116 predates it); CAS already
        // high, WE stays low (harmless without a CAS strobe). Also refreshes the row.
        if ((col & 3) == 3) rasHandling_4116(row_w);
      }
      WE_HIGH20;
      RAS_HIGH20;
    }

    // ---------- READ row_r (trails write by LAG: index i-LAG) ----------
    if (i >= LAG) {
      uint8_t jr = i - LAG;
      uint8_t row_r = ascending ? jr : (uint8_t)(rows - 1 - jr);
      uint8_t rp = (row_r & 1) ^ inv;
      rasHandling_4116(row_r);
      for (uint8_t col = 0; col < cols; col++) {
        uint8_t d = (rp ^ (col & 1)) & 1;
        setAddr_4116(col);
        NOP;
        CAS_LOW20;
        NOP;
        NOP;
        CAS_HIGH20;
        if ((GET_DOUT_4116() ^ (d ? 0x01 : 0x00)) != 0) {
          sei();
          RAS_HIGH20;
          error(patNr, 2);
          return;
        }
        if ((col & 3) == 3) rasHandling_4116(row_r);  // tRAS-max guard: RAS-only recycle (~5 us)
      }
      RAS_HIGH20;
    }
  }
  sei();
}

/**
 * Main test function for 4116/4027 RAM
 */
void test_4116_logic(void) {
  type = detect_4027() ? T_4027 : T_4116;

  writeRAMType((const __FlashStringHelper*)ramTypes[type].name);
  checkAddressing_4116();
  DDRC = 0b00011110;
  uint8_t total_rows = (uint8_t)ramTypes[type].rows;
  do {
    // Patterns 0-1: checkerboard coupling via the 2-row write/read LAG pipeline
    // (write row N, read row N-2 -> the victim has BOTH neighbours freshly written;
    // write->read window ~0.7 ms < 2 ms spec, so no extra refresh needed — 4116/4027
    // predate CBR anyway). Pass 0 ascending, pass 1 descending + inverted.
    pass_4116(0);
    pass_4116(1);
    // Patterns 4-5: pseudo-random WITH retention testing (the REAL retention test:
    // write row -> controlled aging via delayRows/delays[]/writeTime -> read back,
    // the aging row is NOT refreshed in between).
    for (uint8_t patNr = 4; patNr <= 5; patNr++) {
      if (patNr == 5) invertRandomTable();
      for (uint8_t row = 0; row < total_rows; row++)
        writeRow_4116(row, patNr);
    }
    testOK();
  } while (CFG_LOOP_ACTIVE);
}

//=======================================================================================
// SHARED 4116/4027 HELPER FUNCTIONS
//=======================================================================================

/**
 * Combined RAS handling with precharge and address setup
 * @param row Row address (0-127)
 */
void rasHandling_4116(uint8_t row) {
  RAS_HIGH20;
  NOP;
  NOP;
  setAddr_4116(row);
  RAS_LOW20;
}

/**
 * Refresh function for 4116/4027
 * @param row Row address to refresh
 */
void refreshRow_4116(uint8_t row) {
  rasHandling_4116(row);
  NOP;
  RAS_HIGH20;
  NOP;
}

/**
 * Generic single cell write operation
 * @param col Column address
 * @param data Data bit to write
 */
void writeCell_4116(uint8_t col, uint8_t data) {
  setAddr_4116(col);
  SET_DIN_4116(data);
  NOP;
  NOP;
  CAS_LOW20;
  NOP;
  NOP;
  CAS_HIGH20;
}

/**
 * Generic single cell read operation
 * @param col Column address
 * @return Data bit read (0 or non-zero)
 */
uint8_t readCell_4116(uint8_t col) {
  setAddr_4116(col);
  NOP;
  NOP;  // Address setup (extra for 4027)
  CAS_LOW20;
  NOP;
  NOP;
  CAS_HIGH20;
  return GET_DOUT_4116();
}

//=======================================================================================
// MAIN WRITE/READ FUNCTIONS
//=======================================================================================

/**
 * Row write for the pseudo-random patterns 4-5 (with retention testing).
 * Constant-data / coupling coverage is provided by pass_4116() (checkerboard 0-1).
 * @param row Row address
 * @param patNr Pattern number (4-5)
 */
void writeRow_4116(uint8_t row, uint8_t patNr) {
  uint8_t last_row = ramTypes[type].rows - 1;
  uint8_t cols = (uint8_t)ramTypes[type].columns;
  rasHandling_4116(row);
  WE_LOW20;

  cli();
  for (uint8_t col = 0; col < cols; col++) {
    SET_DIN_4116(get_test_bit(col, row));
    setAddr_4116(col);
    NOP;
    NOP;
    CAS_LOW20;
    NOP;
    NOP;
    CAS_HIGH20;
    // tRAS-max guard: recycle every 2 cols. The random cell carries get_test_bit
    // (~25 cy), so the 4-col cadence transplanted from the cheap checkerboard cell
    // measured 13.8/14.1 us — over the 10 us spec; 2 cols = ~7.6 us. RAS-only (no
    // CBR; 4116 predates it), WE stays low (harmless without a CAS strobe).
    if ((col & 1) == 1) rasHandling_4116(row);
  }
  WE_HIGH20;
  RAS_HIGH20;  // close the row BEFORE sei() (pending-ISR stretch; tRAS max 10 us)
  sei();

  refreshRow_4116(row);
  // Retention testing for random pattern
  if (row == last_row) {
    // Last row: verify all delayed rows
    for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {
      checkRow_4116(row - x, patNr, 3);
      delayMicroseconds(ramTypes[type].writeTime * 20);
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows] * 20);
    }
  } else if (row >= ramTypes[type].delayRows) {
    // Normal row: verify row that's been waiting
    checkRow_4116(row - ramTypes[type].delayRows, patNr, 3);
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows] * 20);
  } else {
    // Early rows: just delay
    delayMicroseconds(ramTypes[type].delays[row] * 20);
  }
}

/**
 * Row check for the pseudo-random patterns 4-5 (retention read-back).
 * Constant-data coverage is provided by pass_4116() (checkerboard 0-1).
 * @param row Row address
 * @param patNr Pattern number (4-5)
 * @param errorNr Error code
 */
void checkRow_4116(uint8_t row, uint8_t patNr, uint8_t errorNr) {
  uint8_t cols = (uint8_t)ramTypes[type].columns;
  rasHandling_4116(row);

  cli();
  for (uint8_t col = 0; col < cols; col++) {
    setAddr_4116(col);
    NOP;
    CAS_LOW20;
    NOP;
    NOP;
    CAS_HIGH20;
    if ((GET_DOUT_4116() ^ (get_test_bit(col, row) ? 0x01 : 0x00)) != 0) {
      sei();
      error(patNr + 1, errorNr);
    }
    // tRAS-max guard: every 2 cols (see writeRow_4116 — get_test_bit makes the random
    // cell ~2x the checkerboard cell; 4-col cadence measured 14.1 us > 10 us spec).
    if ((col & 1) == 1) rasHandling_4116(row);
  }
  RAS_HIGH20;
  sei();
}

//=======================================================================================
// ADDRESS TESTING
//=======================================================================================

/**
 * Write address test pattern
 * @param row Row address
 * @param col Column address
 * @param value Value to write (0 or 1)
 */
void writeAddressTest_4116(uint8_t row, uint8_t col, uint8_t value) {
  rasHandling_4116(row);
  writeCell_4116(col, value);
  RAS_HIGH20;
  NOP;
  NOP;  // tRP (extra for 4027)
}

/**
 * Read and verify address test pattern
 * @param row Row address
 * @param col Column address
 * @param expected Expected value (0 or 1)
 * @param bitNum Bit number for error reporting
 */
void verifyAddressTest_4116(uint8_t row, uint8_t col, uint8_t expected, uint8_t bitNum) {
  rasHandling_4116(row);
  uint8_t result = readCell_4116(col);

  if (result != expected) {
    CAS_HIGH20;
    RAS_HIGH20;
    NOP;
    NOP;  // tRP (extra for 4027)
    error(bitNum, 1);
  }

  RAS_HIGH20;
  NOP;
  NOP;  // tRP (extra for 4027)
}

/**
 * Test single address bit (row or column)
 * @param base_addr Base address (0)
 * @param peer_addr Peer address (1 << bit)
 * @param fixed_other Fixed value for other dimension
 * @param is_row True if testing row, false if testing column
 * @param bitNum Bit number for error reporting
 */
void testAddressBit_4116(uint8_t base_addr, uint8_t peer_addr,
                         uint8_t fixed_other, bool is_row, uint8_t bitNum) {
  uint8_t row1, col1, row2, col2;

  if (is_row) {
    row1 = base_addr;
    col1 = fixed_other;
    row2 = peer_addr;
    col2 = fixed_other;
  } else {
    row1 = fixed_other;
    col1 = base_addr;
    row2 = fixed_other;
    col2 = peer_addr;
  }

  WE_LOW20;
  writeAddressTest_4116(row1, col1, 0);
  writeAddressTest_4116(row2, col2, 1);
  WE_HIGH20;

  verifyAddressTest_4116(row1, col1, 0, bitNum);
  verifyAddressTest_4116(row2, col2, 1, bitNum);
}

/**
 * Address testing for 4116/4027 RAM
 */
void checkAddressing_4116(void) {
  checkAddressShorts(0x00, 0x00, (type == T_4027) ? 0x3F : 0x7F);

  // Restore I/O direction: checkAddressShorts leaves address pins as inputs with pull-ups
  DDRD = 0xFF;
  PORTD = 0x00;
  DDRC = 0b00011110;
  uint16_t max_rows = ramTypes[type].rows;
  uint16_t max_cols = ramTypes[type].columns;

  uint8_t rowBits = countBits(max_rows - 1);
  uint8_t colBits = countBits(max_cols - 1);

  RAS_HIGH20;
  CAS_HIGH20;
  WE_HIGH20;
  NOP;
  NOP;  // tRP

  // Row address test
  for (uint8_t b = 0; b < rowBits; b++) {
    testAddressBit_4116(0, (1U << b), 0, true, b);
  }

  // Column address test
  uint8_t fixed_row = (max_rows > 64) ? 64 : 0;
  for (uint8_t b = 0; b < colBits; b++) {
    testAddressBit_4116(0, (1U << b), fixed_row, false, 16 + b);
  }
}

//=======================================================================================
// 20Pin.cpp - Implementation of 20-Pin DRAM functions
//=======================================================================================

//=======================================================================================
// 20-PIN PORT MAPPINGS
//=======================================================================================

// Port-to-DIP-Pin mappings for 20-pin DRAMs
const uint8_t CPU_20PORTB[] = { 17, 4, 16, 3, EOL, EOL, EOL, EOL };
const uint8_t CPU_20PORTC[] = { 1, 2, 18, 19, 5, 10, EOL, EOL };
const uint8_t CPU_20PORTD[] = { 6, 7, 8, 9, 11, 12, 13, 14 };
const uint8_t RAS_20PIN = 9;  // Digital Out 9 on Arduino Uno is used for RAS
const uint8_t CAS_20PIN = 8;  // Digital Out 8 on Arduino Uno is used for CAS

//=======================================================================================
// RAM PRESENCE TEST
//=======================================================================================

/**
 * Test if 20-pin RAM is present using pullup/OE method
 * Writes 0 to address 0, then tests:
 * - OE=OFF: Should read HIGH (pullups active)
 * - OE=ON: Should read LOW (RAM driving lines)
 * @return true if RAM is present
 */
static inline bool test_ram_presence_20Pin(void) {
  // Write 0 to address 0
  rasHandling_20Pin(0);
  PORTC = (PORTC & 0xf0) | 0x0;
  WE_LOW20;
  CAS_LOW20;
  NOP;
  CAS_HIGH20;
  WE_HIGH20;
  RAS_HIGH20;

  PORTC &= 0xf0;  // Clear all outputs on lower nibble
  DDRC &= 0xf0;   // Configure lower nibble as INPUT (PC0-PC3)
  PORTC |= 0x0F;  // Activate pullups
  // Test with OE=OFF (should be HIGH through pullups)
  rasHandling_20Pin(0);
  OE_HIGH20;
  CAS_LOW20;
  NOP;
  NOP;
  uint8_t pullup_result = PINC & 0x0F;
  CAS_HIGH20;
  RAS_HIGH20;

  // Test with OE=ON (should be LOW through RAM)
  rasHandling_20Pin(0);
  OE_LOW20;
  CAS_LOW20;
  NOP;
  NOP;
  uint8_t ram_result = PINC & 0x0F;
  CAS_HIGH20;
  OE_HIGH20;
  RAS_HIGH20;

  return (pullup_result == 0x0F && ram_result == 0x00);
}

bool ram_present_20Pin(void) {
  PORTC &= 0xf0;  // Clear all outputs on lower nibble
  DDRC &= 0xf0;   // Configure lower nibble as INPUT (PC0-PC3)();
  RAS_HIGH20;
  CAS_HIGH20;
  OE_HIGH20;
  WE_HIGH20;

  return test_ram_presence_20Pin();
}

//=======================================================================================
// I/O CONFIGURATION
//=======================================================================================

/**
 * Configure I/O ports for 20-pin DRAM interface
 *
 * Sets up port directions (DDR) and initial states (PORT) for:
 * - Address lines (PORTD + PORTB MSBs)
 * - Data lines (PORTC low nibble)
 * - Control signals (RAS, CAS, WE, OE on PORTB)
 */
void configureIO_20Pin(void) {
  PORTB = 0b00111111;
  PORTC = 0b10000000;
  PORTD = 0x00;
  DDRB = 0b00011111;
  DDRC = 0b00011111;
  DDRD = 0xFF;
}

//=======================================================================================
// HELPER FUNCTIONS FOR ADDRESS TESTING
//=======================================================================================

/**
 * Write data to specified row/column address
 * Optimized helper to reduce code duplication in address tests
 *
 * @param row Row address
 * @param col Column address
 * @param data 4-bit data value to write (0x0-0xF)
 */
static inline void write20Pin(uint16_t row, uint16_t col, uint8_t data) {
  rasHandling_20Pin(row);
  msbHandling_20Pin(col >> 8);
  PORTD = (uint8_t)(col & 0xFF);
  PORTC = (PORTC & 0xF0) | (data & 0x0F);
  NOP;
  CAS_LOW20;
  NOP;
  CAS_HIGH20;
  RAS_HIGH20;
}

/**
 * Read data from specified row/column address
 * Optimized helper to reduce code duplication in address tests
 *
 * @param row Row address
 * @param col Column address
 * @return 4-bit data value read (0x0-0xF)
 */
static inline uint8_t read20Pin(uint16_t row, uint16_t col) {
  rasHandling_20Pin(row);
  msbHandling_20Pin(col >> 8);
  PORTD = (uint8_t)(col & 0xFF);
  NOP;
  CAS_LOW20;
  NOP;
  uint8_t result = PINC & 0x0F;
  CAS_HIGH20;
  RAS_HIGH20;
  return result;
}

//=======================================================================================
// STREAMLINED 20-PIN ADDRESS TESTING
//=======================================================================================

/**
 * Verify all address lines (row and column) function correctly for 20-pin DRAMs
 *
 * Tests each address bit independently using walking-bit method:
 * - For each bit position b (0 to max):
 *   1. Write 0x0 to base address (all bits LOW)
 *   2. Write 0xF to peer address (only bit b HIGH)
 *   3. Read back both addresses and verify distinct values
 *   4. If values match: address line stuck or not decoded properly
 *
 * Row address test: Fixed at COL=0, walks through ROW bits
 * Column address test: Fixed at ROW=middle, walks through COL bits
 *
 * @note Calls error(bit, 1) if any address line fails
 * @note Row errors report bit 0-9, column errors report bit 16-25
 */
void checkAddressing_20Pin() {
  checkAddressShorts(0x10, (type == T_514400 || type == T_514402) ? 0x10 : 0x00, 0xFF);

  // Restore I/O direction: checkAddressShorts leaves address pins as inputs with pull-ups
  configureIO_20Pin();

  // Bit counts from current RAM type
  uint16_t rows = ramTypes[type].rows;
  uint16_t cols = ramTypes[type].columns;
  uint8_t rowBits = countBits(rows - 1);
  uint8_t colBits = countBits(cols - 1);

  // Safe idle levels
  RAS_HIGH20;
  CAS_HIGH20;
  OE_HIGH20;
  WE_HIGH20;

  // =========================
  // ROW ADDRESS DECODER TEST
  // =========================
  // Write phase: at fixed COL=0, write 0x0 to base_row=0 and 0xF to peer_row=(1<<b).
  DDRC = (DDRC & 0xF0) | 0x0F;  // data nibble output (PC0..PC3), PC4 kept as output (A9)
  WE_LOW20;

  for (uint8_t b = 0; b < rowBits; b++) {
    write20Pin(0, 0, 0x0);                 // base_row=0, col=0, data=0x0
    write20Pin((uint16_t)1 << b, 0, 0xF);  // peer_row=(1<<b), col=0, data=0xF
  }

  // Switch to READ properly (WE must be HIGH before any CAS↓ read!)
  WE_HIGH20;
  PORTC &= 0xf0;  // Clear all outputs on lower nibble
  DDRC &= 0xf0;   // Configure lower nibble as INPUT (PC0-PC3)
  OE_LOW20;

  // Read/verify: base must be 0x0, peer must be 0xF
  for (uint8_t b = 0; b < rowBits; b++) {
    if (read20Pin(0, 0) != 0x0) error(b, 1);                 // base_row -> expect 0x0
    if (read20Pin((uint16_t)1 << b, 0) != 0xF) error(b, 1);  // peer_row -> expect 0xF
  }

  OE_HIGH20;

  // ============================
  // COLUMN ADDRESS DECODER TEST
  // ============================
  // Fixed ROW in the middle; for each COL bit b:
  //   base_col=0 -> 0x0, peer_col=(1<<b) -> 0xF, then verify both.
  uint16_t test_row = rows >> 1;

  // write
  DDRC = (DDRC & 0xF0) | 0x0F;  // data out
  WE_LOW20;

  for (uint8_t b = 0; b < colBits; b++) {
    write20Pin(test_row, 0, 0x0);                 // base_col=0, data=0x0
    write20Pin(test_row, (uint16_t)1 << b, 0xF);  // peer_col=(1<<b), data=0xF
  }

  WE_HIGH20;

  // read
  PORTC &= 0xf0;  // Clear all outputs on lower nibble
  DDRC &= 0xf0;   // Configure lower nibble as INPUT (PC0-PC3)  // data in
  OE_LOW20;

  for (uint8_t b = 0; b < colBits; b++) {
    if (read20Pin(test_row, 0) != 0x0) error(b + 16, 1);                 // base_col -> expect 0x0
    if (read20Pin(test_row, (uint16_t)1 << b) != 0xF) error(b + 16, 1);  // peer_col -> expect 0xF
  }

  OE_HIGH20;
}


//=======================================================================================
// OPTIMIZED 20-PIN DRAM DETECTION
//=======================================================================================

/**
 * Detect RAM capacity: 256Kx4 (514256) vs 1Mx4 (514400)
 *
 * Detection method:
 * 1. Write pattern 0x5 to row 0, column 0
 * 2. Write pattern 0xA to row 512, column 0
 * 3. Read back from row 0, column 0
 * 4. Analysis:
 *    - 256Kx4 chips: Only 512 rows (0-511), so row 512 wraps to row 0
 *      Reading row 0 returns 0xA (overwritten by row 512 write)
 *    - 1Mx4 chips: 1024 rows (0-1023), row 512 is distinct from row 0
 *      Reading row 0 returns 0x5 (original value preserved)
 *
 * Sets global 'type' variable to T_514256 or T_514400
 *
 * @note Must be called after ram_present_20Pin() confirms RAM exists
 */
void senseRAM_20Pin() {
  RAS_HIGH20;
  CAS_HIGH20;
  OE_HIGH20;
  WE_HIGH20;
  PORTC &= 0xf0;  // Clear all outputs on lower nibble
  DDRC |= 0x0f;   // Configure lower nibble as OUTPUT (PC0-PC3)

  rasHandling_20Pin(0);
  PORTC = (PORTC & 0xe0) | 0x05;
  WE_LOW20;
  CAS_LOW20;
  CAS_HIGH20;

  rasHandling_20Pin(512);
  PORTC = (PORTC & 0xe0) | 0x0A;
  WE_LOW20;
  CAS_LOW20;
  CAS_HIGH20;
  WE_HIGH20;

  rasHandling_20Pin(0);
  PORTD = 0x00;
  PORTB &= 0xef;
  PORTC &= 0xf0;  // Clear all outputs on lower nibble
  DDRC &= 0xf0;   // Configure lower nibble as INPUT (PC0-PC3)
  OE_LOW20;
  CAS_LOW20;
  CAS_HIGH20;

  if ((PINC & 0x0f) != 0x5) {
    type = T_514256;
  } else {
    type = T_514400;
  }

  OE_HIGH20;
}
//=======================================================================================
// OPTIMIZED STATIC COLUMN DETECTION
//=======================================================================================

/**
 * Detect static column mode capability: 514258/514402 vs 514256/514400
 *
 * Static column mode allows multiple column accesses within same row
 * with CAS held LOW (faster than cycling CAS for each column).
 *
 * Detection method:
 * 1. Assert RAS for row 0 and hold it
 * 2. Write 4 different values (0, 5, 10, 15) to 4 different columns
 *    with CAS pulsing but RAS staying LOW throughout
 * 3. Keep RAS LOW and assert CAS (hold LOW)
 * 4. Change only column address (via PORTD) while CAS remains LOW
 * 5. Read data at each column position
 * 6. Analysis:
 *    - Static column chips (514258/514402): Column changes while CAS=LOW work,
 *      each read returns the correct value written to that column
 *    - Fast page mode only (514256/514400): Column must change when CAS=HIGH,
 *      reads may return incorrect or previous column data
 *
 * Updates global 'type' variable:
 * - T_514256 → T_514258 if static column detected
 * - T_514400 → T_514402 if static column detected
 *
 * @note Must be called after senseRAM_20Pin() sets base type (256K or 1M)
 */
void senseSCRAM_20Pin() {
  PORTD = 0x00;
  PORTB &= 0xef;
  PORTC &= 0xf0;
  DDRC |= 0x0f;

  rasHandling_20Pin(0);
  WE_LOW20;

  // Quick static column test (only 4 positions instead of 16)
  uint8_t test_cols[] = { 0, 5, 10, 15 };
  for (uint8_t i = 0; i < 4; i++) {
    PORTC = ((PORTC & 0xf0) | (test_cols[i] & 0x0f));
    PORTD = test_cols[i];
    CAS_LOW20;
    NOP;
    CAS_HIGH20;
  }

  WE_HIGH20;

  // Verify static column mode
  rasHandling_20Pin(0);
  PORTC &= 0xf0;  // Clear all outputs on lower nibble
  DDRC &= 0xf0;   // Configure lower nibble as INPUT (PC0-PC3)
  OE_LOW20;
  CAS_LOW20;

  bool static_column = true;
  for (uint8_t i = 0; i < 4; i++) {
    PORTD = test_cols[i];
    NOP;
    NOP;
    if ((test_cols[i] & 0x0f) != (PINC & 0x0f)) {
      static_column = false;
      break;
    }
  }

  CAS_HIGH20;
  OE_HIGH20;
  RAS_HIGH20;

  if (static_column) {
    type = (type == T_514400) ? T_514402 : T_514258;
  }
}

//=======================================================================================
// CHECKERBOARD COUPLING TEST (replaces former constant patterns 0-3)
//=======================================================================================
//
// The old write-all/read-all path RAS-activated (refreshed) each row only once
// at write time; the first-written row then went unrefreshed for the ENTIRE
// write phase (~0.1 s on 514256, ~0.5 s on 514400 — ~60x over the 8/16 ms
// refresh spec). That latently risks FALSE failures on weak-but-in-spec cells,
// at elevated temperature, or from die self-heating in long loop sessions.
//
// This checkerboard test bounds the write->read window using the SAME pipeline
// shape as the retention path (write row N, read row N-d) but with a FIXED
// 1-row lag and NO retention delays. The lag only guarantees the vertically
// adjacent row is already written before the victim is read (inter-row
// coupling coverage), keeping each cell's write->read window sub-millisecond,
// safely inside spec.
//
// Background per cell:  nibble = ((row ^ col) & 1) ? 0x5 : 0xA
//   0xA (1010) and 0x5 (0101) are bitwise complements -> every spatial
//   neighbour differs in all 4 bits AND bits alternate within each nibble
//   (intra-DQ coupling). Two passes give every bit both 0 and 1 (stuck-at).
//   Only col bit 0 matters for parity, and 256 is even, so the inner 8-bit
//   column counter is correct across the column-MSB boundary.
//
// Two passes for direction coverage (state coupling is address-order dependent):
//   Pass A: ascending  write, background (even,odd)=(0xA,0x5), read lag N-1.
//   Pass B: descending write, background swapped (0x5,0xA),    read lag N+1.
//
// KNOWN LIMITATION (out of scope): pure transition / idempotent coupling faults
// (CFin/CFid) need per-cell read-modify-write; the per-cell WE/DDRC toggle cost
// in page mode is prohibitive, so they are intentionally not covered here.
//=======================================================================================

// Catch-up CBR burst for the 20-pin checkerboard. Entry: CAS LOW, RAS LOW (row open).
// Precharges the row, then runs EIGHT back-to-back CBR cycles while CAS stays low
// ("hidden", no extra CAS edges), then reopens the row and restores the column MSB.
// Rationale: the refresh AVERAGE must be 1 CBR per 15.625 us (8 ms/512 = 16 ms/1024);
// batching 8 CBRs after each 64-col burst keeps that average (1 per 8 cols) but pays
// the row-reopen + address decode only ONCE per burst instead of 8 times. The 64-col
// burst window (~60-70 us RAS-low) stays under the 100 us tRAS(page) limit.
// WE is left as-is (the write loop raises it around the call -> no WCBR test-mode
// entry). noinline: body exists once, only a call per burst.
static void __attribute__((noinline)) cbrCatchup_20Pin(uint16_t row, uint8_t msb) {
  RAS_HIGH20;                      // precharge current row (refresh it)
  for (uint8_t i = 8; i; i--) {    // 8 back-to-back CBR cycles (CAS held low)
    NOP;                           // tRP
    RAS_LOW20;                     // RAS down with CAS low -> CBR (hidden) refresh
    NOP;
    RAS_HIGH20;                    // end CBR (precharge)
  }
  CAS_HIGH20;
  msbHandling_20Pin(row >> 8);     // reopen: row MSB (A8/A9)
  PORTD = (uint8_t)(row & 0xff);   // row low
  RAS_LOW20;                       // latch row
  msbHandling_20Pin(msb);          // restore column MSB
}

// Checkerboard coupling pass (0-1) for 20-pin 514xxx (514256/258/400/402), 4-bit.
// FPM only — CAS-strobed (fast-page) control works for SC chips too (an SC part
// behaves identically under CAS strobing). The Static-Column write/read is used only
// in the retention pipeline (writeRow_20Pin / checkRow_20Pin). write-all -> read-all
// so inter-row coupling is caught; 64-col bursts with a batch of 8 catch-up CBRs after
// each burst keep the refresh AVERAGE at 1 CBR per 8 cols (15.625 us/row budget,
// 8 ms/512 rows = 16 ms/1024) while paying the row-reopen only once per burst, and
// bound tRAS (~60-70 us < 100 us tRAS(page)). No quadrant logic.
//   patNr 0/1 = checkerboard ((row^col)&1)^pass ? 0x5 : 0xA
static void __attribute__((hot)) pass_20Pin(uint8_t patNr, bool runCBR) {
  uint8_t msbCol = ramTypes[type].columns / 256;
  uint16_t total_rows = ramTypes[type].rows;
  uint8_t pass = patNr & 1;

  // ---------------- WRITE ALL ROWS (FPM) ----------------
  for (uint16_t row = 0; row < total_rows; row++) {
    uint8_t rp = ((uint8_t)row & 1) ^ pass;
    rasHandling_20Pin(row);
    PORTC &= 0xF0;
    DDRC |= 0x0F;  // PC0-PC3 OUTPUT
    OE_HIGH20;
    WE_LOW20;
    cli();
    for (uint8_t msb = 0; msb < msbCol; msb++) {
      msbHandling_20Pin(msb);
      uint8_t io = PORTC & 0xF0;
      uint8_t col = 0;
      // 64-col bursts (~60 us RAS-low < 100 us tRAS(page)) + 8 catch-up CBRs after
      // EVERY burst: refresh average stays 1 CBR per 8 cols (15.625 us budget), but
      // only one row-reopen/address-decode per burst instead of 8. The catch-up after
      // burst 3 also recycles RAS across the MSB boundary (former special case).
      for (uint8_t b = 0; b < 4; b++) {
        for (uint8_t k = 64; k; k--) {
          uint8_t d = (rp ^ (col & 1)) ? 0x05 : 0x0A;
          CAS_HIGH20;       // raise CAS (precharge); burst ends CAS-low
          PORTC = io | d;
          PORTD = col;
          CAS_LOW20;        // latch the write (data on CAS falling edge)
          col++;
        }
        WE_HIGH20;          // WCBR guard (entry CAS low = true hidden refresh)
        cbrCatchup_20Pin(row, msb);
        WE_LOW20;
        io = PORTC & 0xF0;  // re-cache (A9/PC4 changed); CAS stays HIGH — the next
                            // cell begins with CAS_HIGH anyway (no strobe in between)
      }
    }
    CAS_HIGH20;  // bursts end CAS-low -> raise it before the read phase
    WE_HIGH20;
    RAS_HIGH20;  // close the row BEFORE sei() (pending-ISR stretch)
    sei();
  }

  // ---------------- CBR REFRESH-COUNTER TEST (~60 s, loop mode only) ------------
  // The whole array is now written; refresh it for ~60 s via CAS-before-RAS only, then
  // the read-all phase below verifies it survived (tests the on-chip refresh counter).
  // Shared helper: millis-bounded 60 s + LED off + "R:<sec>" OLED countdown.
  if (runCBR) cbrRefreshPhase(&casBeforeRasRefresh_20Pin);

  // ---------------- READ ALL ROWS (FPM) ----------------
  for (uint16_t row = 0; row < total_rows; row++) {
    uint8_t rp = ((uint8_t)row & 1) ^ pass;
    rasHandling_20Pin(row);
    PORTC &= 0xF0;
    DDRC &= 0xF0;  // PC0-PC3 INPUT
    OE_LOW20;
    cli();
    for (uint8_t msb = 0; msb < msbCol; msb++) {
      msbHandling_20Pin(msb);
      uint8_t col = 0;
      // 64-col bursts + 8 catch-up CBRs after every burst (see write phase). The FPM
      // read ends each cell CAS-high -> lower CAS first, then the CBR batch.
      for (uint8_t b = 0; b < 4; b++) {
        for (uint8_t k = 64; k; k--) {
          PORTD = col;
          CAS_LOW20;
          CAS_HIGH20;
          uint8_t pd = PINC & 0x0F;
          uint8_t d = (rp ^ (col & 1)) ? 0x05 : 0x0A;
          if ((pd ^ d) != 0) {
            sei();
            OE_HIGH20;
            RAS_HIGH20;
            error(patNr, runCBR ? 5 : 2);  // 5 = "CBR Timer fault" (CBR run), 2 = checkerboard
            return;
          }
          col++;
        }
        CAS_LOW20;  // read ends CAS-high -> lower CAS, then the hidden CBR batch
        cbrCatchup_20Pin(row, msb);
      }
    }
    OE_HIGH20;
    RAS_HIGH20;  // close the row BEFORE sei() (pending-ISR stretch)
    sei();
  }
}

// (checkerReadRow_20Pin / checkerboardTest_20Pin removed in 5.0.3 — 514xxx now
//  runs checkerboard + random through the unified pass_20Pin write-all -> read-all
//  path with CBR refresh.)

//=======================================================================================
// ADDRESS AND TIMING HANDLING
//=======================================================================================

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
 * Write random pattern and handle retention testing for a single row
 *
 * Only called for patterns 4-5 (pseudo-random). This is the REAL retention test:
 * the row is written, then read back delayRows rows later (after the controlled
 * aging delay) WITHOUT being refreshed in between. Constant-data / inter-row coupling
 * coverage is provided separately by pass_20Pin() (checkerboard 0-1). SC chips
 * (514258/514402) use Static-Column write+read; others use Fast Page Mode.
 *
 * @param row Row address to write/test
 * @param patNr Pattern number (4-5 only)
 * @param is_static True for static column mode, false for fast page mode
 */
// Retention read-back thunk for the shared retentionTail (patterns 4-5).
// Re-derives is_static from the type flags and opens the row before the check
// (checkRow_20Pin expects the row already RAS-latched).
static void retCheck_20Pin(uint16_t row, uint8_t patNr) {
  rasHandling_20Pin(row);
  checkRow_20Pin(patNr, row, 3, (ramTypes[type].flags & RAM_FLAG_STATIC_COLUMN) != 0);
}

void writeRow_20Pin(uint16_t row, uint8_t patNr, boolean is_static) {
  // All control lines HIGH (idle state)
  SBI(PORTB, 0);
  SBI(PORTB, 1);
  SBI(PORTB, 2);
  SBI(PORTB, 3);

  rasHandling_20Pin(row);

  PORTC &= 0xf0;  // Clear data nibble
  DDRC |= 0x0f;   // PC0-PC3 as OUTPUT
  OE_HIGH20;
  const RAM_Definition *rt = &ramTypes[type];
  uint8_t msbCol = rt->columns / 256;

  // Random pattern write with cached PORTC upper bits and precomputed row_mix
  uint8_t io = (PORTC & 0xf0);
  uint16_t row_mix = row + (row >> 4);
  // K-hoist: table index = (K ^ msb) ^ col — K once per row, K_msb = K ^ msb per MSB
  // block (5.0.5: the old index ignored the column MSB since pre-5.0, so every MSB
  // block of a row repeated the same 256-entry sequence on 1M parts). Per-cell cost
  // identical -> timing/calibration unaffected; only the DATA in msb>0 blocks differs.
  uint8_t K = (uint8_t)(row_mix ^ (row_mix >> 8));

  cli();
  if (is_static) {
    // Static-column write (514258/514402): CAS held LOW for the whole row, the column
    // address is static; each cell is committed by a WE low pulse (WE high between
    // columns so address changes never ripple-write). RAS-only recycle (no CBR) every
    // 64 cols AND at each MSB boundary keeps the RAS-low window < 100 us (SC max:
    // ~0.9 us/col -> 64 cols ~60 us; an un-split MSB boundary would be ~120 us).
    WE_HIGH20;
    CAS_LOW20;
    for (uint8_t msb = 0; msb < msbCol; msb++) {
      msbHandling_20Pin(msb);
      // Mix the column MSB into the table key (matches the 411000's Kb = Krow ^ blk):
      // without it every MSB block of a row repeats the same 256-entry data sequence
      // (the column high bits never reached the index — true since pre-5.0).
      uint8_t K_msb = K ^ msb;
      if (msb) {  // MSB-boundary recycle (raise CAS, RAS-only, resume SC)
        CAS_HIGH20;
        rasHandling_20Pin(row);
        msbHandling_20Pin(msb);
        CAS_LOW20;
      }
      io = (PORTC & 0xf0);
      // Burst loop instead of a per-cell `(col & 0x3F)` check (see 16Pin): 4 bursts of
      // 64 cols, RAS-only recycle BETWEEN bursts — identical cadence to the old check
      // (which skipped the recycle after the last cell).
      uint8_t col = 0;
      for (uint8_t b = 0; b < 4; b++) {
        if (b) {  // RAS-only recycle (<100us); resume SC
          CAS_HIGH20;
          rasHandling_20Pin(row);
          msbHandling_20Pin(msb);
          CAS_LOW20;
          io = PORTC & 0xf0;
        }
        for (uint8_t k = 64; k; k--) {
          PORTD = col;
          PORTC = io | (randomTable[(uint8_t)(K_msb ^ col)]);
          WE_LOW20;
          col++;
          WE_HIGH20;
        }
      }
    }
    CAS_HIGH20;  // release the held CAS
  } else {
    // Fast-page write (514256/514400), 2x unrolled, WE held low for the whole row.
    WE_LOW20;
    for (uint8_t msb = 0; msb < msbCol; msb++) {
      msbHandling_20Pin(msb);
      uint8_t K_msb = K ^ msb;  // mix the column MSB into the table key (see SC write)
      io = (PORTC & 0xf0);      // Re-cache after msbHandling may change PC4

      // Burst loop (see SC write): 4 bursts of 64 cols (32 x 2 unrolled cells), RAS
      // recycle between bursts — identical cadence, no per-cell mask check.
      uint8_t col = 0;
      for (uint8_t b = 0; b < 4; b++) {
        if (b) {                         // tRAS-max guard: recycle RAS every 64 cols
          rasHandling_20Pin(row);        // CAS HIGH, WE stays low -> refreshes the row
          msbHandling_20Pin(msb);        // restore column MSB (A8/A9)
          io = PORTC & 0xf0;             // re-cache (A9/PC4 changed)
        }
        for (uint8_t k = 32; k; k--) {
          PORTC = io | (randomTable[(uint8_t)(K_msb ^ col)]);
          PORTD = col;
          CAS_LOW20;
          CAS_HIGH20;
          col++;
          PORTC = io | (randomTable[(uint8_t)(K_msb ^ col)]);
          PORTD = col;
          CAS_LOW20;
          CAS_HIGH20;
          col++;
        }
      }
    }
  }
  RAS_HIGH20;  // close the row BEFORE sei(): a pending timer ISR fires right at sei
  sei();       // and would stretch the open RAS window ~7 us (page limit is 100 us;
               // the last burst already sits at ~60-90 us). refreshRow re-opens below.

  // Prepare Read Cycle
  WE_HIGH20;
  PORTC &= 0xf0;
  DDRC &= 0xf0;  // PC0-PC3 as INPUT

  // Retention aging + pipelined read-back via the shared tail (common.cpp).
  // NOTE (5.0.4): order normalised to the 16/18-pin shape — aging delay BEFORE
  // the check, last-row drain simulates writeTime for x < delayRows (previously
  // this path checked first and delayed after; delays[] re-calibration covers it).
  refreshRow_20Pin(row);
  retentionTail(row, rt->rows - 1, patNr, retCheck_20Pin);

  // All control lines HIGH
  SBI(PORTB, 0);
  SBI(PORTB, 1);
  SBI(PORTB, 2);
  SBI(PORTB, 3);
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
 * Check one full row of random pattern data (patterns 4-5 only)
 *
 * Only called for random patterns during retention testing (read-back after aging).
 * Constant-data / coupling coverage is provided by pass_20Pin() (checkerboard 0-1).
 *
 * @param patNr Pattern number (always 4, used for error reporting)
 * @param row Row address being tested
 * @param errNr Error code to report if check fails
 * @param is_static True for static column mode, false for fast page mode
 */
void checkRow_20Pin(uint8_t patNr, uint16_t row, uint8_t errNr, boolean is_static) {
  uint8_t msbCol = ramTypes[type].columns / 256;
  uint16_t row_mix = row + (row >> 4);
  // K-hoist (see writeRow_20Pin): table index = (K ^ msb) ^ col, MSB mixed per block.
  uint8_t K = (uint8_t)(row_mix ^ (row_mix >> 8));
  OE_LOW20;

  cli();
  for (uint8_t msb = 0; msb < msbCol; msb++) {
    msbHandling_20Pin(msb);
    uint8_t K_msb = K ^ msb;  // mix the column MSB into the table key (see writeRow_20Pin)
    if (is_static == false) {
      // Fast Page Mode, 2x unrolled; burst loop (see writeRow_20Pin): 4 bursts of
      // 64 cols, RAS recycle BETWEEN bursts — identical cadence, no per-cell check.
      uint8_t col = 0;
      for (uint8_t b = 0; b < 4; b++) {
        if (b) {                         // tRAS-max guard: recycle RAS every 64 cols
          rasHandling_20Pin(row);        // CAS HIGH; OE stays low (preserved)
          msbHandling_20Pin(msb);        // restore column MSB (A8/A9)
        }
        for (uint8_t k = 32; k; k--) {
          PORTD = col;
          CAS_LOW20;
          CAS_HIGH20;
          uint8_t pin_data = PINC & 0x0f;
          if ((pin_data ^ randomTable[(uint8_t)(K_msb ^ col)]) != 0) {
            sei();
            error(patNr, errNr);
          }
          col++;

          PORTD = col;
          CAS_LOW20;
          CAS_HIGH20;
          pin_data = PINC & 0x0f;
          if ((pin_data ^ randomTable[(uint8_t)(K_msb ^ col)]) != 0) {
            sei();
            error(patNr, errNr);
          }
          col++;
        }
      }
    } else {
      // Static Column Mode; burst loop: 4 bursts of 64 cols, recycle after EVERY burst
      // (the old `(col & 0x3F) == 0x3F` fired after cells 63/127/191/255 incl. the last
      // -> identical cadence), no per-cell mask check.
      CAS_LOW20;
      uint8_t col = 0;
      for (uint8_t b = 0; b < 4; b++) {
        for (uint8_t k = 64; k; k--) {
          PORTD = col;
          NOP; NOP;
          uint8_t pin_data = PINC & 0x0f;
          if ((pin_data ^ randomTable[(uint8_t)(K_msb ^ col)]) != 0) {
            sei();
            error(patNr, errNr);
          }
          col++;
        }
        // tRAS-max guard (static col): refresh row every 64 cols
        CAS_HIGH20;
        rasHandling_20Pin(row);
        msbHandling_20Pin(msb);
        CAS_LOW20;
      }
    }
  }
  CAS_HIGH20;
  OE_HIGH20;
  RAS_HIGH20;  // close the row BEFORE sei() (pending-ISR stretch; page limit 100 us)
  sei();
}

//=======================================================================================
// REFRESH TIME TEST (20-PIN: 514256, 514258, 514400, 514402)
//=======================================================================================

/**
 * CAS-before-RAS refresh for 20-pin (4-bit nibble)
 */
static inline void casBeforeRasRefresh_20Pin() {
  RAS_HIGH20;
  CAS_LOW20;
  RAS_LOW20;
  NOP;
  NOP;
  RAS_HIGH20;
  CAS_HIGH20;
}

// (refreshTimeTest_20Pin removed in 5.0.3 — the ~60 s CBR refresh-counter test is now
//  folded into pass_20Pin via the runCBR flag, reusing its write-all/read-all phases.
//  casBeforeRasRefresh_20Pin above is the per-chip CBR cycle it calls.)