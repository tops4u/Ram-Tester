// 20Pin.cpp - Implementation of 20-Pin DRAM testing functions
//=======================================================================================
//
// This file contains all test logic for 20-pin DRAM packages. It implements:
// - Standard 20-pin DRAM testing (514256, 514258, 514400, 514402)
// - 4116 adapter support (16Kx1 with voltage conversion)
// - Static column mode optimization for 514258/514402
// - Comprehensive test patterns (stuck-at, alternating, pseudo-random)
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
static void refreshTimeTest_20Pin();
static void fastPatternTest_20Pin(uint8_t patNr);



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
 * 7. Runs all 6 test patterns (stuck-at, alternating, random, retention)
 * 8. Performs refresh time test to verify CBR counter
 * 9. Calls testOK() on success (never returns)
 *
 * @note Function never returns - ends in testOK() or error()
 */
void test_20Pin() {
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

  // Pre-cache frequently used values
  register uint16_t total_rows = ramTypes[type].rows;
  register boolean is_static = ramTypes[type].flags & RAM_FLAG_STATIC_COLUMN;

  // Patterns 0-3: Fast write-all/read-all (constant data, no per-column variation)
  for (register uint8_t patNr = 0; patNr < 4; patNr++) {
    fastPatternTest_20Pin(patNr);
  }

  // Patterns 4-5: Pseudo-random with retention testing (sequential row access required)
  for (register uint8_t patNr = 4; patNr <= 5; patNr++) {
    if (patNr == 5)
      invertRandomTable();
    for (register uint16_t row = 0; row < total_rows; row++) {
      writeRow_20Pin(row, patNr, is_static);
    }
  }

  // ===== REFRESH TIME TEST =====
  // Test if chip can retain data with maximum refresh interval
  if (type == T_514256 || type == T_514258 || type == T_514400 || type == T_514402) {
    refreshTimeTest_20Pin();
  }

  // Good Candidate.
  testOK();
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
  float voltage_pc2 = adc_to_voltage(adc_pc2);
  if (voltage_pc2 < (TARGET_VOLTAGE - VOLTAGE_TOLERANCE) || voltage_pc2 > (TARGET_VOLTAGE + VOLTAGE_TOLERANCE)) return false;

  uint16_t adc_pc3 = adc_read(3);
  float voltage_pc3 = adc_to_voltage(adc_pc3);
  if (voltage_pc3 < (TARGET_VOLTAGE - VOLTAGE_TOLERANCE) || voltage_pc3 > (TARGET_VOLTAGE + VOLTAGE_TOLERANCE)) return false;

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
/**
 * Main test function for 4116/4027 RAM
 */
void test_4116_logic(void) {
  type = detect_4027() ? T_4027 : T_4116;

  writeRAMType((const __FlashStringHelper*)ramTypes[type].name);
  checkAddressing_4116();
  DDRC = 0b00011110;

  for (uint8_t patNr = 0; patNr <= 5; patNr++) {
    if (patNr == 5) invertRandomTable();
    for (uint8_t row = 0; row < ramTypes[type].rows; row++) {
      writeRow_4116(row, patNr);
    }
  }
  testOK();
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

/**
 * Write and verify single cell (for stuck-at patterns)
 * @param col Column address
 * @param pat Pattern value
 * @param row Row for error reporting
 */
void writeCellVerify_4116(uint8_t col, uint8_t pat, uint8_t row) {
  writeCell_4116(col, pat & 0x01);

  WE_HIGH20;
  if (((readCell_4116(col) ^ pat) & 0x01) != 0) {
    sei();
    error(pat, 2, row, col);
  }
  WE_LOW20;
}

//=======================================================================================
// MAIN WRITE/READ FUNCTIONS
//=======================================================================================

/**
 * Optimized row write with pattern dispatch
 * @param row Row address
 * @param patNr Pattern number (0-5)
 */
void writeRow_4116(uint8_t row, uint8_t patNr) {
  uint8_t last_row = ramTypes[type].rows - 1;
  rasHandling_4116(row);
  WE_LOW20;
  uint8_t pat = pattern[patNr];

  cli();

  if (patNr < 2) {
    uint8_t cols = (uint8_t)ramTypes[type].columns;  // 64 für 4027, 128 für 4116
    for (uint8_t col = 0; col < cols; col++) {
      writeCellVerify_4116(col, pat, row);
    }
    sei();
    RAS_HIGH20;
    return;
  } else if (patNr < 4) {
    uint8_t cols = (uint8_t)ramTypes[type].columns;
    for (uint8_t col = 0; col < cols; col++) {
      writeCell_4116(col, pat & 0x01);
      pat = rotate_left(pat);
    }
  } else {
    uint8_t cols = (uint8_t)ramTypes[type].columns;
    for (uint8_t col = 0; col < cols; col++) {
      SET_DIN_4116(get_test_bit(col, row));
      setAddr_4116(col);
      NOP;
      NOP;
      CAS_LOW20;
      NOP;
      NOP;
      CAS_HIGH20;
    }
  }
  sei();
  WE_HIGH20;
  RAS_HIGH20;

  if (patNr < 4) {
    checkRow_4116(row, patNr, 2);
    return;
  }

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
 * Unified row check function for all patterns
 * @param row Row address
 * @param patNr Pattern number
 * @param errorNr Error code
 */
void checkRow_4116(uint8_t row, uint8_t patNr, uint8_t errorNr) {
  uint8_t pat = pattern[patNr];
  uint8_t cols = (uint8_t)ramTypes[type].columns;
  rasHandling_4116(row);

  cli();
  for (uint8_t col = 0; col < cols; col++) {
    uint8_t expected = (patNr < 4) ? (pat & 0x01) : (get_test_bit(col, row) ? 0x01 : 0x00);
    setAddr_4116(col);
    NOP;
    CAS_LOW20;
    NOP;
    NOP;
    CAS_HIGH20;
    if ((GET_DOUT_4116() ^ expected) != 0) {
      sei();
      error(patNr + 1, errorNr, row, col);
    }
    if (patNr < 4) pat = rotate_left(pat);
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
    error(bitNum, 1, row, col);
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
  NOP;
  CAS_LOW20;
  NOP;
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
// FAST PATTERN TEST (PATTERNS 0-3): WRITE-ALL THEN READ-ALL
//=======================================================================================

/**
 * Fast pattern test for 20-pin DRAMs (patterns 0-3 only)
 *
 * Writes constant pattern data to ALL rows, then reads back and verifies ALL rows.
 * Much faster than per-row inline verify because:
 * - No DDRC toggling per column (patterns 0-1 were especially wasteful)
 * - No per-column branch overhead for pattern type dispatch
 * - A0-A7 map directly to PORTD — no LUTs needed
 *
 * Uses Fast Page Mode for all chips (Static Column chips support FP mode too).
 *
 * @param patNr Pattern number (0-3 only)
 */
static void fastPatternTest_20Pin(uint8_t patNr) {
  uint16_t total_rows = ramTypes[type].rows;
  uint8_t msbCol = ramTypes[type].columns / 256;
  uint8_t pat_nibble = pattern[patNr] & 0x0F;

  // ==================== WRITE ALL ROWS ====================
  PORTC &= 0xF0;         // Clear data nibble
  DDRC |= 0x0F;          // PC0-PC3 as OUTPUT
  PORTC |= pat_nibble;   // Set constant pattern data
  OE_HIGH20;
  WE_LOW20;

  cli();
  for (uint16_t row = 0; row < total_rows; row++) {
    rasHandling_20Pin(row);

    for (uint8_t msb = 0; msb < msbCol; msb++) {
      msbHandling_20Pin(msb);

      // Inner loop: PORTD = col IS the full column address (A0-A7 direct)
      register uint8_t col = 0;
      do {
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        col++;
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        col++;
      } while (col != 0);
    }
  }
  sei();
  WE_HIGH20;
  RAS_HIGH20;

  // ==================== READ AND VERIFY ALL ROWS ====================
  PORTC &= 0xF0;
  DDRC &= 0xF0;    // PC0-PC3 as INPUT
  OE_LOW20;

  cli();
  for (uint16_t row = 0; row < total_rows; row++) {
    rasHandling_20Pin(row);

    for (uint8_t msb = 0; msb < msbCol; msb++) {
      msbHandling_20Pin(msb);

      register uint8_t col = 0;
      do {
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        if (((PINC ^ pat_nibble) & 0x0F) != 0) {
          sei();
          RAS_HIGH20;
          OE_HIGH20;
          error(patNr, 2, row, ((uint16_t)msb << 8) | col);
        }
        col++;

        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        if (((PINC ^ pat_nibble) & 0x0F) != 0) {
          sei();
          RAS_HIGH20;
          OE_HIGH20;
          error(patNr, 2, row, ((uint16_t)msb << 8) | col);
        }
        col++;
      } while (col != 0);
    }

    RAS_HIGH20;
  }
  sei();
  OE_HIGH20;
}

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
 * Only called for patterns 4-5 (pseudo-random). Patterns 0-3 are handled
 * by fastPatternTest_20Pin() which uses the faster write-all/read-all approach.
 *
 * @param row Row address to write/test
 * @param patNr Pattern number (4-5 only)
 * @param is_static True for static column mode, false for fast page mode
 */
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
  WE_LOW20;
  register uint8_t msbCol = ramTypes[type].columns / 256;

  // Random pattern write with cached PORTC upper bits and precomputed row_mix
  register uint8_t io = (PORTC & 0xf0);
  uint16_t row_mix = row + (row >> 4);

  cli();
  for (register uint8_t msb = 0; msb < msbCol; msb++) {
    msbHandling_20Pin(msb);
    io = (PORTC & 0xf0);  // Re-cache after msbHandling may change PC4

    register uint8_t col = 0;
    do {
      uint16_t v = col ^ row_mix;
      PORTC = io | (randomTable[(uint8_t)(v ^ (v >> 8))]);
      PORTD = col;
      CAS_LOW20;
      CAS_HIGH20;
      col++;
      v = col ^ row_mix;
      PORTC = io | (randomTable[(uint8_t)(v ^ (v >> 8))]);
      PORTD = col;
      CAS_LOW20;
      CAS_HIGH20;
      col++;
    } while (col != 0);
  }
  sei();

  // Prepare Read Cycle
  WE_HIGH20;
  PORTC &= 0xf0;
  DDRC &= 0xf0;   // PC0-PC3 as INPUT

  // Retention testing
  refreshRow_20Pin(row);
  if (row == ramTypes[type].rows - 1) {
    // Last row: verify all delayed rows
    for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {
      rasHandling_20Pin(row - x);
      checkRow_20Pin(4, row - x, 3, is_static);
      delayMicroseconds(ramTypes[type].writeTime * 20);
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows] * 20);
    }
    // All control lines HIGH before return
    SBI(PORTB, 0);
    SBI(PORTB, 1);
    SBI(PORTB, 2);
    SBI(PORTB, 3);
    return;
  }
  if (row >= ramTypes[type].delayRows) {
    rasHandling_20Pin(row - ramTypes[type].delayRows);
    checkRow_20Pin(4, row - ramTypes[type].delayRows, 3, is_static);
  }
  if (row < ramTypes[type].delayRows)
    delayMicroseconds(ramTypes[type].delays[row] * 20);
  else
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows] * 20);

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
 * Only called for random patterns during retention testing.
 * Patterns 0-3 are verified by fastPatternTest_20Pin().
 *
 * @param patNr Pattern number (always 4, used for error reporting)
 * @param row Row address being tested
 * @param errNr Error code to report if check fails
 * @param is_static True for static column mode, false for fast page mode
 */
void checkRow_20Pin(uint8_t patNr, uint16_t row, uint8_t errNr, boolean is_static) {
  register uint8_t msbCol = ramTypes[type].columns / 256;
  uint16_t row_mix = row + (row >> 4);
  OE_LOW20;

  cli();
  for (register uint8_t msb = 0; msb < msbCol; msb++) {
    msbHandling_20Pin(msb);
    if (is_static == false) {
      // Fast Page Mode with 2x unrolling
      register uint8_t col = 0;
      do {
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        register uint8_t pin_data = PINC & 0x0f;
        uint16_t v = col ^ row_mix;
        if ((pin_data ^ randomTable[(uint8_t)(v ^ (v >> 8))]) != 0) {
          sei();
          error(patNr, errNr, row, col);
        }
        col++;

        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        pin_data = PINC & 0x0f;
        v = col ^ row_mix;
        if ((pin_data ^ randomTable[(uint8_t)(v ^ (v >> 8))]) != 0) {
          sei();
          error(patNr, errNr, row, col);
        }
        col++;
      } while (col != 0);
    } else {
      // Static Column Mode
      CAS_LOW20;
      register uint8_t col = 0;
      do {
        PORTD = col;
        NOP;
        NOP;
        register uint8_t pin_data = PINC & 0x0f;
        uint16_t v = col ^ row_mix;
        if ((pin_data ^ randomTable[(uint8_t)(v ^ (v >> 8))]) != 0) {
          sei();
          error(patNr, errNr, row, col);
        }
      } while (++col != 0);
    }
  }
  sei();
  CAS_HIGH20;
  OE_HIGH20;
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

/**
 * Refresh time test for 20-pin 4-bit DRAMs
 * All chips follow the industry standard: 15.625μs per refresh cycle
 *
 * - 514256/514258: 8ms refresh, 512 CBR cycles → 8ms/512 = 15.625μs/cycle
 * - 514400/514402: 16ms refresh, 1024 CBR cycles → 16ms/1024 = 15.625μs/cycle
 * - Write/read 2 columns (4-bit nibble, test 2 bits per nibble)
 *
 * Test duration:
 * - 514256/514258: 125 × 8ms = 1000ms (1 second)
 * - 514400/514402: 125 × 16ms = 2000ms (2 seconds)
 */
static void refreshTimeTest_20Pin() {
  uint16_t rows = ramTypes[type].rows;
  uint16_t refresh_cycles;
  uint8_t test_cycles;
  PORTC &= 0xf0;
  DDRC |= 0x0f;

  // Determine refresh parameters based on chip type
  // Test CBR counter with 10 full cycles through all addresses
  if (type == T_514256 || type == T_514258) {
    refresh_cycles = 512;   // 8ms / 512 = 15.625μs per cycle
    test_cycles = 10;       // 10 × 8ms = 80ms counter test
  } else {                  // T_514400 or T_514402
    refresh_cycles = 1024;  // 16ms / 1024 = 15.625μs per cycle
    test_cycles = 10;       // 10 × 16ms = 160ms counter test
  }

  // ===== PHASE 1: WRITE 2 NIBBLES TO EACH ROW =====
  CAS_HIGH20;

  for (uint16_t row = 0; row < rows; row++) {
    uint8_t dataNibble = randomTable[row & 0xFF] & 0x0F;

    rasHandling_20Pin(row);
    // Write 2 columns
    WE_LOW20;
    for (uint8_t col = 0; col < 2; col++) {
      uint8_t nibble = (dataNibble >> (col * 2)) & 0x0F;
      PORTC = (PORTC & 0xF0) | nibble;
      PORTD = col;  // Set column address directly
      CAS_LOW20;
      NOP;
      CAS_HIGH20;
    }
    WE_HIGH20;
    casBeforeRasRefresh_20Pin();
  }

  // ===== PHASE 2: CBR REFRESH CYCLES =====
  for (uint8_t cycle = 0; cycle < test_cycles; cycle++) {
    // Perform CBR refresh cycles
    for (uint16_t refresh_count = 0; refresh_count < refresh_cycles; refresh_count++) {
      casBeforeRasRefresh_20Pin();

      // Delay to achieve ~15.625μs per cycle
      // CBR refresh takes ~1-2μs, so delay ~14μs
      delayMicroseconds(15);
      NOP;
      NOP;
      NOP;
    }
  }

  PORTC &= 0xf0;  // Clear all Outputs
  DDRC &= 0xf0;   // Configure IOs for Input

  // ===== PHASE 3: READ AND VERIFY =====
  for (uint16_t row = 0; row < rows; row++) {
    uint8_t expectedNibble = randomTable[row & 0xFF] & 0x0F;

    rasHandling_20Pin(row);
    OE_LOW20;

    for (uint8_t col = 0; col < 2; col++) {
      PORTD = col;  // Set column address directly
      CAS_LOW20;
      NOP;
      NOP;

      uint8_t actual = PINC & 0x0F;
      uint8_t expected = (expectedNibble >> (col * 2)) & 0x0F;

      CAS_HIGH20;

      if (actual != expected) {
        RAS_HIGH20;
        OE_HIGH20;
        error(0, 5);
        return;
      }
    }

    OE_HIGH20;
    casBeforeRasRefresh_20Pin();
  }
}