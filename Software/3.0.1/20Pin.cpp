#include "20Pin.h"

//=======================================================================================
// MAIN TEST FUNCTION
//=======================================================================================

/**
 * Main test function for 20-pin DRAMs (514256, 514258, 514400, 514402)
 * Configures I/O ports, detects RAM type, performs address testing and pattern tests
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
  writeRAMType();
  configureIO_20Pin();
  checkAddressing_20Pin();

  // OPTIMIZED: Pre-cache frequently used values
  register uint16_t total_rows = ramTypes[type].rows;
  register boolean is_static = ramTypes[type].staticColumn;

  for (register uint8_t patNr = 0; patNr <= 5; patNr++) {
    if (patNr == 5)
      randomizeData();
    for (register uint16_t row = 0; row < total_rows; row++) {
      writeRow_20Pin(row, patNr, is_static);
    }
  }

  // Good Candidate.
  testOK();
}



//=======================================================================================
// 4116 ADAPTER DETECTION AND TESTING
//=======================================================================================

/**
 * Test for 4116 adapter presence by checking voltage levels and pin states
 * @return true if 4116 adapter is detected, false otherwise
 */
bool test_4116(void) {
  adc_init();
  DDRB &= ~((1 << PB2) | (1 << PB4));
  PORTB &= ~((1 << PB2) | (1 << PB4));
  DDRC &= ~((1 << PC2) | (1 << PC3));
  PORTC &= ~((1 << PC2) | (1 << PC3));

  for (volatile int i = 0; i < 1000; i++)
    ;

  if (!(PINB & (1 << PB4))) {
    return false;
  }
  if (!(PINB & (1 << PB2))) {
    return false;
  }

  uint16_t adc_pc2 = adc_read(2);  // ADC2 = PC2
  float voltage_pc2 = adc_to_voltage(adc_pc2);
  if (voltage_pc2 < (TARGET_VOLTAGE - VOLTAGE_TOLERANCE) || voltage_pc2 > (TARGET_VOLTAGE + VOLTAGE_TOLERANCE)) {
    return false;
  }

  uint16_t adc_pc3 = adc_read(3);  // ADC3 = PC3
  float voltage_pc3 = adc_to_voltage(adc_pc3);
  if (voltage_pc3 < (TARGET_VOLTAGE - VOLTAGE_TOLERANCE) || voltage_pc3 > (TARGET_VOLTAGE + VOLTAGE_TOLERANCE)) {
    return false;
  }

  return true;
}

/**
 * Main test function for 4116 RAM - called when adapter is detected
 */
void test_4116_logic(void) {
#ifdef OLED
  display.drawString(1, 4, "Detected:");
  display.drawString(0, 6, ramTypes[T_4116].name);
  display.drawString(2, 2, "Checking...");
  configureIO_20Pin();  // Restore IO config after OLED
#endif

  checkAddressing_4116();
  // PC0 is Data out - always.
  DDRC = 0b00011110;
  // Pattern tests for all 128 rows
  for (uint8_t patNr = 0; patNr <= 5; patNr++) {
    if (patNr == 5)
      randomizeData();
    for (uint8_t row = 0; row < 128; row++) {
      writeRow_4116(row, patNr);
    }
  }

  testOK();
}

/**
 * Address testing for 4116 RAM (reduced complexity for 16K capacity)
 * Tests critical address lines A0, A6 for rows and columns
 */
void checkAddressing_4116(void) {
  // Derive bit counts from the selected RAM type
  DDRC = 0b00011110;
  uint16_t max_rows = ramTypes[type].rows;     // expected 128
  uint16_t max_cols = ramTypes[type].columns;  // expected 128
  uint8_t rowBits = 0, colBits = 0;
  for (uint16_t t = max_rows - 1; t; t >>= 1) rowBits++;
  for (uint16_t t = max_cols - 1; t; t >>= 1) colBits++;

  // Common precondition
  RAS_HIGH20;
  CAS_HIGH20;
  WE_HIGH20;  // default to read; switch to LOW only when writing
  DELAY_4116_PRECHARGE();

  //========================
  // ROW ADDRESS TEST (all bits)
  //========================
  // We use column 0 to avoid mixing with column tests.
  for (uint8_t b = 0; b < rowBits; b++) {
    const uint8_t base_row = 0;
    const uint8_t peer_row = (1U << b);
    const uint8_t col = 0;

    // --- Write base_row := 0
    rasHandling_4116(base_row);
    SET_DIN_4116(0);
    setAddr_4116(col);
    DELAY_4116_RAS_TO_CAS();
    WE_LOW20;
    CAS_LOW20;
    DELAY_4116_CAS_LOW();
    CAS_HIGH20;
    WE_HIGH20;
    RAS_HIGH20;
    DELAY_4116_PRECHARGE();

    // --- Write peer_row := 1
    rasHandling_4116(peer_row);
    SET_DIN_4116(1);
    setAddr_4116(col);
    DELAY_4116_RAS_TO_CAS();
    WE_LOW20;
    CAS_LOW20;
    DELAY_4116_CAS_LOW();
    CAS_HIGH20;
    WE_HIGH20;
    RAS_HIGH20;
    DELAY_4116_PRECHARGE();

    // --- Read back base_row (expect 0)
    rasHandling_4116(base_row);
    setAddr_4116(col);
    DELAY_4116_RAS_TO_CAS();
    CAS_LOW20;
    DELAY_4116_CAS_LOW();
    if (GET_DOUT_4116() != 0) {
      // row aliasing on bit b
      CAS_HIGH20;
      RAS_HIGH20;
      DELAY_4116_PRECHARGE();
      error(b, 1, base_row, col);
    }
    CAS_HIGH20;
    RAS_HIGH20;
    DELAY_4116_PRECHARGE();

    // --- Read back peer_row (expect 1)
    rasHandling_4116(peer_row);
    setAddr_4116(col);
    DELAY_4116_RAS_TO_CAS();
    CAS_LOW20;
    DELAY_4116_CAS_LOW();
    if (GET_DOUT_4116() != 1) {
      CAS_HIGH20;
      RAS_HIGH20;
      DELAY_4116_PRECHARGE();
      error(b, 1, peer_row, col);
    }
    CAS_HIGH20;
    RAS_HIGH20;
    DELAY_4116_PRECHARGE();
  }

  //========================
  // COLUMN ADDRESS TEST (all bits)
  //========================
  // Use a mid-row (64) so we don’t re-touch row 0 while toggling columns.
  // Any valid fixed row works, but 64 keeps A6 high to exercise that latch path.
  const uint8_t fixed_row = (max_rows > 64) ? 64 : 0;

  for (uint8_t b = 0; b < colBits; b++) {
    const uint8_t base_col = 0;
    const uint8_t peer_col = (1U << b);

    // --- Program base_col := 0 at fixed_row
    rasHandling_4116(fixed_row);
    SET_DIN_4116(0);
    setAddr_4116(base_col);
    DELAY_4116_RAS_TO_CAS();
    WE_LOW20;
    CAS_LOW20;
    DELAY_4116_CAS_LOW();
    CAS_HIGH20;
    WE_HIGH20;
    DELAY_4116_PRECHARGE();

    // --- Program peer_col := 1 at fixed_row
    SET_DIN_4116(1);
    setAddr_4116(peer_col);
    DELAY_4116_RAS_TO_CAS();
    WE_LOW20;
    CAS_LOW20;
    DELAY_4116_CAS_LOW();
    CAS_HIGH20;
    WE_HIGH20;
    RAS_HIGH20;
    DELAY_4116_PRECHARGE();

    // --- Verify base_col (expect 0)
    rasHandling_4116(fixed_row);
    setAddr_4116(base_col);
    DELAY_4116_RAS_TO_CAS();
    CAS_LOW20;
    DELAY_4116_CAS_LOW();
    if (GET_DOUT_4116() != 0) {
      CAS_HIGH20;
      RAS_HIGH20;
      DELAY_4116_PRECHARGE();
      error(16 + b, 1, fixed_row, base_col);
    }
    CAS_HIGH20;
    DELAY_4116_PRECHARGE();

    // --- Verify peer_col (expect 1)
    setAddr_4116(peer_col);
    DELAY_4116_RAS_TO_CAS();
    CAS_LOW20;
    DELAY_4116_CAS_LOW();
    if (GET_DOUT_4116() != 1) {
      CAS_HIGH20;
      RAS_HIGH20;
      DELAY_4116_PRECHARGE();
      error(16 + b, 1, fixed_row, peer_col);
    }
    CAS_HIGH20;
    RAS_HIGH20;
    DELAY_4116_PRECHARGE();
  }
}

/**
 * Timing-optimized RAS handling with precharge for 4116
 * @param row Row address (0-127)
 */
void rasHandling_4116(uint8_t row) {
  RAS_HIGH20;
  DELAY_4116_PRECHARGE();  // RAS precharge time
  setAddr_4116(row);
  DELAY_4116_DATA_SETUP();  // Address setup time
  RAS_LOW20;
}

/**
 * Timing-optimized row write function for 4116
 * @param row Row address
 * @param patNr Pattern number (0-4)
 */
void writeRow_4116(uint8_t row, uint8_t patNr) {
  rasHandling_4116(row);
  WE_LOW20;
  uint8_t pat = pattern[patNr];

  cli();
  if (patNr < 2) {
    // Stuck-at patterns with timing-optimized loops
    SET_DIN_4116(pat & 0x01);
    for (uint8_t col = 0; col < 128; col++) {
      setAddr_4116(col);
      DELAY_4116_RAS_TO_CAS();
      CAS_LOW20;
      DELAY_4116_CAS_LOW();
      CAS_HIGH20;
      DELAY_4116_PRECHARGE();

      // Immediate read verification
      WE_HIGH20;
      CAS_LOW20;
      DELAY_4116_CAS_LOW();  // Data available time
      if (((GET_DOUT_4116() ^ pat) & 0x01) != 0) {
        sei();  // Re-enable interrupts before error
        error(patNr, 2, row, col);
      }
      CAS_HIGH20;
      DELAY_4116_PRECHARGE();
      WE_LOW20;
    }
    sei();
    RAS_HIGH20;
    return;
  } else if (patNr < 4) {
    // Walking patterns
    for (uint8_t col = 0; col < 128; col++) {
      SET_DIN_4116(pat & 0x01);
      setAddr_4116(col);
      DELAY_4116_RAS_TO_CAS();
      CAS_LOW20;
      DELAY_4116_CAS_LOW();
      pat = rotate_left(pat);
      CAS_HIGH20;
      DELAY_4116_PRECHARGE();
    }
  } else {
    // Random pattern
    for (uint8_t col = 0; col < 128; col++) {
      SET_DIN_4116(get_test_bit(col, row));
      setAddr_4116(col);
      DELAY_4116_RAS_TO_CAS();
      CAS_LOW20;
      DELAY_4116_CAS_LOW();
      CAS_HIGH20;
      DELAY_4116_PRECHARGE();
    }
  }
  sei();

  WE_HIGH20;
  RAS_HIGH20;
  DELAY_4116_PRECHARGE();

  // Read back and verify
  if (patNr < 4) {
    checkRow_4116(row, patNr, 2);
    return;
  }

  refreshRow_4116(row);
  if (row == ramTypes[type].rows - 1) {  // Last Row written, we have to check the last n Rows as well.
    // Retention testing the last rows, they will no longer be written only read back. Simulate the write time to get a correct retention time test.
    for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {
      checkRow_4116(row - x, patNr, 3);
      delayMicroseconds(ramTypes[type].writeTime);  // Simulate writing even if it is no longer done for the last rows
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
    }
    return;
  }
  // Default Retention Testing
  if (row >= ramTypes[type].delayRows) {
    checkRow_4116(row - ramTypes[type].delayRows, patNr, 3);  // Check for the last random Pattern on this ROW
  }
  // Data retention Testing first rows. They need special treatment since they will be read back later. Timing is special at the beginning to match retention times
  if (row < ramTypes[type].delayRows)
    delayMicroseconds(ramTypes[type].delays[row]);
  else
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
}

/**
 * Timing-optimized row check function for 4116
 * @param row Row address
 * @param patNr Pattern number 
 * @param errorNr Error code
 */
void checkRow_4116(uint8_t row, uint8_t patNr, uint8_t errorNr) {
  uint8_t pat = pattern[patNr];
  rasHandling_4116(row);

  cli();
  if (patNr < 4) {
    for (uint8_t col = 0; col < 128; col++) {
      setAddr_4116(col);
      DELAY_4116_RAS_TO_CAS();
      CAS_LOW20;
      DELAY_4116_CAS_LOW();  // Data available time
      if ((GET_DOUT_4116() ^ (pat & 0x01)) != 0) {
        sei();
        error(patNr + 1, errorNr, row, col);
      }
      CAS_HIGH20;
      DELAY_4116_PRECHARGE();
      pat = rotate_left(pat);
    }
  } else {
    // Random pattern check
    for (uint8_t col = 0; col < 128; col++) {
      setAddr_4116(col);
      DELAY_4116_RAS_TO_CAS();
      CAS_LOW20;
      DELAY_4116_CAS_LOW();
      uint8_t expected_bit = get_test_bit(col, row) ? 0x01 : 0x00;
      if ((GET_DOUT_4116() ^ (expected_bit ? 1 : 0)) != 0) {
        sei();
        error(patNr + 1, errorNr, row, col);
      }
      CAS_HIGH20;
      DELAY_4116_PRECHARGE();
    }
  }
  sei();

  RAS_HIGH20;
  DELAY_4116_PRECHARGE();
}

/**
 * Timing-optimized refresh function for 4116
 * @param row Row address to refresh
 */
void refreshRow_4116(uint8_t row) {
  rasHandling_4116(row);
  DELAY_4116_CAS_LOW();  // RAS active time
  RAS_HIGH20;
  DELAY_4116_PRECHARGE();
}

//=======================================================================================
// 20Pin.cpp - Implementation of 20-Pin DRAM functions
//=======================================================================================

//=======================================================================================
// 20-PIN PORT MAPPINGS
//=======================================================================================

// Port-to-DIP-Pin mappings for 20-pin DRAMs
const int CPU_20PORTB[] = { 17, 4, 16, 3, EOL, EOL, EOL, EOL };
const int CPU_20PORTC[] = { 1, 2, 18, 19, 5, 10, EOL, EOL };
const int CPU_20PORTD[] = { 6, 7, 8, 9, 11, 12, 13, 14 };
const int RAS_20PIN = 9;  // Digital Out 9 on Arduino Uno is used for RAS
const int CAS_20PIN = 8;  // Digital Out 8 on Arduino Uno is used for CAS

//=======================================================================================
// RAM PRESENCE TEST
//=======================================================================================

bool ram_present_20Pin(void) {
  uint8_t sDDRB = DDRB, sPORTB = PORTB, sDDRC = DDRC, sPORTC = PORTC, sDDRD = DDRD, sPORTD = PORTD;

  DDRC |= 0x0f;
  RAS_HIGH20;
  CAS_HIGH20;
  OE_HIGH20;
  WE_HIGH20;

  // Write 0 to address 0
  rasHandling_20Pin(0);
  PORTC = (PORTC & 0xf0) | 0x0;  // Write all 0
  WE_LOW20;
  CAS_LOW20;
  NOP;
  CAS_HIGH20;
  WE_HIGH20;
  RAS_HIGH20;

  // Configure for read and activate pullups
  DDRC &= 0xf0;   // Data as input
  PORTC |= 0x0F;  // Activate pullups

  // Test with OE=OFF (should be HIGH through pullups)
  rasHandling_20Pin(0);
  OE_HIGH20;  // OE inactive
  CAS_LOW20;
  NOP;
  NOP;
  uint8_t pullup_result = PINC & 0x0F;
  CAS_HIGH20;
  RAS_HIGH20;

  // Test with OE=ON (should be LOW through RAM)
  rasHandling_20Pin(0);
  OE_LOW20;  // OE active
  CAS_LOW20;
  NOP;
  NOP;
  uint8_t ram_result = PINC & 0x0F;
  CAS_HIGH20;
  OE_HIGH20;
  RAS_HIGH20;

  // Restore original state
  DDRB = sDDRB;
  PORTB = sPORTB;
  DDRC = sDDRC;
  PORTC = sPORTC;
  DDRD = sDDRD;
  PORTD = sPORTD;

  // RAM check: Pullup HIGH + RAM LOW = RAM present
  return (pullup_result == 0x0F && ram_result == 0x00);
}

//=======================================================================================
// I/O CONFIGURATION
//=======================================================================================

void configureIO_20Pin(void) {
  PORTB = 0b00111111;
  PORTC = 0b10000000;
  PORTD = 0x00;
  DDRB = 0b00011111;
  DDRC = 0b00011111;
  DDRD = 0xFF;
}

//=======================================================================================
// STREAMLINED 20-PIN ADDRESS TESTING
//=======================================================================================
void checkAddressing_20Pin() {
  // Bit counts from current RAM type
  uint16_t rows = ramTypes[type].rows;
  uint16_t cols = ramTypes[type].columns;
  uint8_t rowBits = 0, colBits = 0;
  for (uint16_t t = rows - 1; t; t >>= 1) rowBits++;
  for (uint16_t t = cols - 1; t; t >>= 1) colBits++;

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
    uint16_t base_row = 0;
    uint16_t peer_row = (uint16_t)1 << b;

    // base_row := 0x0 @ COL=0
    rasHandling_20Pin(base_row);   // present ROW, RAS falls inside
    msbHandling_20Pin(0);          // COL MSBs = 0
    PORTD = 0x00;                  // COL LSBs = 0
    PORTC = (PORTC & 0xF0) | 0x0;  // DATA = 0000b
    NOP;
    NOP;  // tRCD margin
    CAS_LOW20;
    NOP;  // tCAC margin
    CAS_HIGH20;
    RAS_HIGH20;

    // peer_row := 0xF @ COL=0
    rasHandling_20Pin(peer_row);
    msbHandling_20Pin(0);
    PORTD = 0x00;
    PORTC = (PORTC & 0xF0) | 0xF;  // DATA = 1111b
    NOP;
    NOP;
    CAS_LOW20;
    NOP;
    CAS_HIGH20;
    RAS_HIGH20;
  }

  // Switch to READ properly (WE must be HIGH before any CAS↓ read!)
  WE_HIGH20;
  DDRC &= 0xF0;  // data nibble input (PC0..PC3), PC4 unchanged
  OE_LOW20;

  // Read/verify: base must be 0x0, peer must be 0xF
  for (uint8_t b = 0; b < rowBits; b++) {
    uint16_t base_row = 0;
    uint16_t peer_row = (uint16_t)1 << b;

    // base_row -> expect 0x0 @ COL=0
    rasHandling_20Pin(base_row);
    msbHandling_20Pin(0);
    PORTD = 0x00;
    NOP;
    NOP;
    CAS_LOW20;
    NOP;
    NOP;
    if ((PINC & 0x0F) != 0x0) error(b, 1);
    CAS_HIGH20;
    RAS_HIGH20;

    // peer_row -> expect 0xF @ COL=0
    rasHandling_20Pin(peer_row);
    msbHandling_20Pin(0);
    PORTD = 0x00;
    NOP;
    NOP;
    CAS_LOW20;
    NOP;
    NOP;
    if ((PINC & 0x0F) != 0xF) error(b, 1);
    CAS_HIGH20;
    RAS_HIGH20;
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
    uint16_t base_col = 0;
    uint16_t peer_col = (uint16_t)1 << b;

    rasHandling_20Pin(test_row);

    // base_col := 0x0
    msbHandling_20Pin(base_col >> 8);
    PORTD = (uint8_t)(base_col & 0xFF);
    PORTC = (PORTC & 0xF0) | 0x0;
    NOP;
    CAS_LOW20;
    NOP;
    CAS_HIGH20;

    // peer_col := 0xF
    msbHandling_20Pin(peer_col >> 8);
    PORTD = (uint8_t)(peer_col & 0xFF);
    PORTC = (PORTC & 0xF0) | 0xF;
    NOP;
    CAS_LOW20;
    NOP;
    CAS_HIGH20;

    RAS_HIGH20;
  }

  WE_HIGH20;

  // read
  DDRC &= 0xF0;  // data in
  OE_LOW20;

  for (uint8_t b = 0; b < colBits; b++) {
    uint16_t base_col = 0;
    uint16_t peer_col = (uint16_t)1 << b;

    // base_col -> expect 0x0
    rasHandling_20Pin(test_row);
    msbHandling_20Pin(base_col >> 8);
    PORTD = (uint8_t)(base_col & 0xFF);
    CAS_LOW20;
    NOP;
    NOP;
    if ((PINC & 0x0F) != 0x0) error(b + 16, 1);
    CAS_HIGH20;
    RAS_HIGH20;

    // peer_col -> expect 0xF
    rasHandling_20Pin(test_row);
    msbHandling_20Pin(peer_col >> 8);
    PORTD = (uint8_t)(peer_col & 0xFF);
    CAS_LOW20;
    NOP;
    NOP;
    if ((PINC & 0x0F) != 0xF) error(b + 16, 1);
    CAS_HIGH20;
    RAS_HIGH20;
  }

  OE_HIGH20;
}


//=======================================================================================
// OPTIMIZED 20-PIN DRAM DETECTION
//=======================================================================================

void senseRAM_20Pin() {
  DDRC |= 0x0f;
  RAS_HIGH20;
  CAS_HIGH20;
  OE_HIGH20;  // OE inactive
  WE_HIGH20;

  // ELEGANT RAM TEST
  // 1. Write 0 to address 0
  rasHandling_20Pin(0);
  PORTC = (PORTC & 0xf0) | 0x0;  // Write all 0
  WE_LOW20;
  CAS_LOW20;
  NOP;
  CAS_HIGH20;
  WE_HIGH20;
  RAS_HIGH20;

  // 2. Configure for read and activate pullups
  DDRC &= 0xf0;   // Data as input
  PORTC |= 0x0F;  // Activate pullups

  // 3. Test with OE=OFF (should be HIGH through pullups)
  rasHandling_20Pin(0);
  OE_HIGH20;  // OE inactive
  CAS_LOW20;
  NOP;
  NOP;
  uint8_t pullup_result = PINC & 0x0F;
  CAS_HIGH20;
  RAS_HIGH20;

  // 4. Test with OE=ON (should be LOW through RAM)
  rasHandling_20Pin(0);
  OE_LOW20;  // OE active
  CAS_LOW20;
  NOP;
  NOP;
  uint8_t ram_result = PINC & 0x0F;
  CAS_HIGH20;
  OE_HIGH20;
  RAS_HIGH20;

  // RAM CHECK: Pullup HIGH + RAM LOW = RAM present
  if (pullup_result != 0x0F || ram_result != 0x00) {
    error(0, 0);  // No RAM
    return;
  }

  // END RAM TEST - Original A9 test
  DDRC |= 0x0f;

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
  PORTC &= 0xe0;
  DDRC &= 0xf0;
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

void senseSCRAM_20Pin() {
  PORTD = 0x00;
  PORTB &= 0xef;
  PORTC &= 0xe0;
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
  DDRC &= 0xf0;
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
        // Only 2x unrolled instead of 8x (saves ~25 bytes)
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        WE_HIGH20;
        PORTC &= 0xf0;
        DDRC &= 0xf0;  // PC0-PC3 as Inputs
        OE_LOW20;
        CAS_LOW20;
        CAS_HIGH20;
        if (((PINC ^ pattern_data) & 0x0f) != 0)
          error(patNr, 2, row, col);
        OE_HIGH20;
        DDRC |= 0x0f;  // Back to Outputs for the next Write
        PORTC |= pattern_data;  // set the Testpattern back
        WE_LOW20;
        col++;

        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        WE_HIGH20;
        PORTC &= 0xf0;
        DDRC &= 0xf0;  // PC0-PC3 as Inputs
        OE_LOW20;
        CAS_LOW20;
        CAS_HIGH20;
        if (((PINC ^ pattern_data) & 0x0f) != 0)
          error(patNr, 2, row, col);
        OE_HIGH20;
        DDRC |= 0x0f;  // Back to Outputs for the next Write
        PORTC |= pattern_data;  // set the Testpattern back        
        WE_LOW20;
        col++;
      } while (col != 0);
    } else if (patNr < 4) {
      // REDUCED: 2x unrolling instead of 8x (saves ~20 bytes)
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
    if (patNr < 2)
      return;  // Pattern 0 and 1 are checked inline - we are done here.
    checkRow_20Pin(patNr, row, 2, is_static);
    return;
  }

  if (patNr >= 4) {
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
  bool pat4 = (patNr == 4);
  // OPTIMIZED: Extended critical section
  cli();
  for (register uint8_t msb = 0; msb < msbCol; msb++) {
    msbHandling_20Pin(msb);
    if (is_static == false) {
      // OPTIMIZED: Fast Page Mode with loop unrolling
      register uint8_t col = 0;
      do {
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        register uint8_t pin_data = PINC & 0x0f;
        if (pat4) {
          NOP;
          NOP;  // kompensiert entfallene "pat"-Prüfung (~2 Zyklen)
          if ((pin_data ^ randomTable[mix8(col, row)]) == 0) goto next1;
        } else {
          if ((pin_data ^ pat) == 0) goto next1;
        }
        error(patNr, errNr, row, col);
next1:
        col++;
        PORTD = col;
        CAS_LOW20;
        CAS_HIGH20;
        pin_data = PINC & 0x0f;
        if (pat4) {
          NOP;
          NOP;  // kompensiert entfallene "pat"-Prüfung (~2 Zyklen)
          if ((pin_data ^ randomTable[mix8(col, row)]) == 0) goto next2;
        } else {
          if ((pin_data ^ pat) == 0) goto next2;
        }
        error(patNr, errNr, row, col);
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
        if (pat4) {
          NOP;
          NOP;  // kompensiert entfallene "pat"-Prüfung (~2 Zyklen)
          if ((pin_data ^ randomTable[mix8(col, row)]) == 0) continue;
        } else {
          if ((pin_data ^ pat) == 0) continue;
        }
        error(patNr, errNr, row, col);
      } while (++col != 0);
    }
  }
  sei();
  CAS_HIGH20;
  OE_HIGH20;
}
