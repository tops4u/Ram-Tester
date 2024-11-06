// RAM Tester Sketch for the RAM Tester PCB
// ========================================
//
// Author : Andreas Hoffmann
// Version: 2.0.pre1
// Date   : 02.11.2024

//
// This Software is published under GPL 3.0 - Respect the License
// This Project is hosted at: https://github.com/tops4u/Ram-Tester/
//
// Note: There is a lot of Code duplication - This is really UGLY! This is no archtectual masterpice but a try to make things fast.
//
// Error Blink Codes:
// Continuous Red Blinking 	- Configuration Error, check DIP Switches. Occasionally a RAM Defect may cause this.
// 1 Red & n Green		  - Addressdecoder Test Fail - n Green Blinks = failing addressline (Keep in mind there will be no green blink for A0)
// 2 Red & n Green		  - Error during RAM Test. Green Blinks indicate which Test Pattern failed (see below).
// 3 Red & n Green      - Row Crosstalk Error or Refresh Data Lost Error. Green Blinks indicate which Test Pattern failed.
// Long Green/Short Red - Successful Test of a Smaller DRAM of this Test Config
// Long Green/Short Off - Successful Test of a Larger DRAM of this Test Config
//
// Assumptions:
// This Sketch assumes that the DRAM supports Page Mode for Read & Write
// It needs around 150us Read and Write time. RAM having shorter Refresh Cycles may loose its content.
// Dram with 4 Bit Databus will be checked Columnwise with 4 Testpatterns 0b0000, 0b1111, 0b1010 and 0b0101 to
// ensure that neighbouring Bits don't influence each other. This test is not performed agains other Rows.
// This program does not test for Ram Speed or Content retention time / Refresh Time.
//
// Version Information:
// Version 1.0	- Implement 20Pin DIP/ZIP for 256x4 DRAM up to 120ns (like: MSM514256C)
// Version 1.1  - Implemented Autodetect 1M or 256k x4 DRAM
// Version 1.2  - Implemented Check for 256kx1 (41256 like DRAM)
// Version 1.21 - Added Column Address Line Checks for 41256/4164. This checks all address lines / buffers and column-addressdecoders
// Version 1.22 - Added Check for 4164 / 41256
// Version 1.23 - Added Row Address Checking for 4164/41256 complementing Column address checks from 1.21
// Version 1.3  - Added Row and Column Tests for Pins, Buffers and Decoders for 514256 and 441000
// Version 1.4pre - Added Support for 4416/4464 but currently only tested for 4416 as 4464 Testchips not yet available
// Version 2.0  - Added Row Crosstalk Checks. Added Refresh Time Checks (2ms for 4164/4ms for 41256/8ms for all DIP/ZIP 20 Types)
//
// Disclaimer:
// This Project (Software & Hardware) is a hobbyist Project. I do not guarantee it's fitness for any purpose
// and I do not guarantee that it is Errorfree. All usage is on your risk.

// For slow RAM we may need to introduce an additional 62.5ns delay. 16MHz Clock -> 1 Cycle = 62.5ns
#define NOP __asm__ __volatile__("nop\n\t")

#define Mode_16Pin 2
#define Mode_18Pin 4
#define Mode_20Pin 5
#define EOL 254
#define NC 255
#define LED_R 13  // PB5
#define LED_G 12  // PB4 -> Co Used with RAM Test Socket, see comments below!

// The Testpatterns
const uint8_t pattern[] = { 0x00, 0xFF, 0xAA, 0x33, 0xAA, 0x33 };  // Equals to 0b00000000, 0b11111111, 0b10101010, 0b01010101
// 0xAA is doubled to simply alternate for even/uneven Rows between 0xAA and 0x33

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
// Address Distribution for 18Pin Types
#define SET_ADDR_PIN18(addr) \
  { \
    PORTB = (PORTB & 0xeb) | ((addr & 0x01) << 2) | ((addr & 0x02) << 3); \
    PORTD = ((addr & 0x04) << 5) | ((addr & 0x08) << 3) | ((addr & 0x80) >> 2) | ((addr & 0x20) >> 4) | ((addr & 0x40) >> 6) | ((addr & 0x10) >> 3); \
  }

#define SET_DATA_PIN18(data) \
  { \
    PORTB = (PORTB & 0xF6) | ((data & 0x02) << 2) | ((data & 0x04) >> 2); \
    PORTC = (PORTC & 0xF5) | ((data & 0x01) << 1) | (data & 0x08); \
  }

#define GET_DATA_PIN18 (((PINC & 0x02) >> 1) + ((PINB & 0x08) >> 2) + ((PINB & 0x01) << 2) + (PINC & 0x08))

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

uint8_t Mode = 0;  // PinMode 2 = 16 Pin, 4 = 18 Pin, 5 = 20 Pin

void setup() {
  // Data Direction Register Port B, C & D - Preconfig as Input (Bit=0)
  DDRB &= 0b11100000;
  DDRC &= 0b11000000;
  DDRD = 0x0;
  // Wait for the Candidate to properly Startup
  if (digitalRead(19) == 1) { Mode += Mode_20Pin; }
  if (digitalRead(3) == 1) { Mode += Mode_18Pin; }
  if (digitalRead(2) == 1) { Mode += Mode_16Pin; }
  // Check if the DIP Switch is set for a valid Configuration.
  if (Mode < 2 || Mode > 5) ConfigFail();

  // -> Currently GND Short Detection confuses the DRAMs and causes Tests to fail.
  // With a valid Config, activate the PullUps
  //PORTB |= 0b00011111;
  //PORTC |= 0b00111111;
  //PORTD = 0xFF;
  // Settle State - PullUps my require some time.
  //checkGNDShort();  // Check for Shorts towards GND. Shorts on Vcc can't be tested as it would need Pull-Downs.

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
  delay(200);  //200ms Startup Delay
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
  if (Sense41256() == true) {
    for (uint16_t row = 0; row < 512; row++) {  // Iterate over all ROWs
      write16PinRow(row, 512);
    }
    // Good Candidate.
    testOK();
  } else {                                      // A8 not used or defect - just run 8kB Test
    for (uint16_t row = 0; row < 256; row++) {  // Iterate over all ROWs
      write16PinRow(row, 256);
    }
    // Indicate with Green-Red flashlight that the "small" Version has been checked ok
    smallOK();
  }
}

// Prepare and execute ROW Access for 16 Pin Types
void RASHandlingPin16(uint16_t row) {
  PORTB |= (1 << PB1);  // Set RAS High - Inactive
  // Row Address distribution Logic for 41256/64 16 Pin RAM - more complicated as the PCB circuit is optimized for 256x4 / 1Mx4 Types.
  PORTB = (PORTB & 0xea) | (row & 0x0010) | ((row & 0x0008) >> 1) | ((row & 0x0040) >> 6);
  PORTC = (PORTC & 0xe8) | ((row & 0x0001) << 4) | ((row & 0x0100) >> 8);
  PORTD = ((row & 0x0080) >> 1) | ((row & 0x0020) << 2) | ((row & 0x0004) >> 2) | (row & 0x0002);
  PORTB &= ~(1 << PB1);  // RAS Latch Strobe
}

// Write and Read (&Check) Pattern from Cols
void write16PinRow(uint16_t row, uint16_t cols) {
  for (uint8_t patNr = 0; patNr < 4; patNr++) {
    // Prepare Write Cycle
    PORTC &= 0x08;          // Set CAS High
    RASHandlingPin16(row);  // Set the Row
    PORTB &= ~(1 << PB3);   // Set WE Low - Active
    uint8_t pat = pattern[patNr];
    for (uint16_t col = 0; col <= cols; col++) {
      // Column Address distribution logic for 41256/64 16 Pin RAM
      PORTB = (PORTB & 0xea) | (col & 0x0010) | ((col & 0x0008) >> 1) | ((col & 0x0040) >> 6);
      PORTC = (PORTC & 0xe8) | ((col & 0x0001) << 4) | ((col & 0x0100) >> 8) | ((pat & 0x01) << 1);
      PORTD = ((col & 0x0080) >> 1) | ((col & 0x0020) << 2) | ((col & 0x0004) >> 2) | (col & 0x0002);
      PORTC &= ~0x08;  // CAS Latch Strobe
      NOP;             // Just to be sure for slower RAM
      PORTC |= 0x08;   // CAS High - Cycle Time ~120ns
      // Rotate the Pattern 1 Bit to the LEFT (c has not rotate so there is a trick with 2 Shift)
      pat = (pat << 1) | (pat >> 7);
    }
    // Prepare Read Cycle
    PORTB |= (1 << PB3);  // Set WE High - Inactive
    // Read and check the Row we just wrote, otherwise Error 2
    rowCheck16Pin(cols, patNr, 2);
    // If this is not the first row to check lets see if current Row modified the previous one.
    // This Row just wrote Pattern Nr 2 and we check if the row before still has pattern Nr3
    // Datasheet Refresh Cycles: 4164: 2ms / 41256: 4ms
    // Cycletime for Read & Write tests: 4164: 1.98ms / 41256: 3.96ms - for one Cycle + some Overhead for Refresh and RAS
    // So this also checks if the last Row was able to reach Data retention Times as per Datasheet specs.
    // As we only test after PatternNr 2 this tests 3 Refresh Cycles per Row by using Ras Only Refresh (ROR)
    if (row > 0) {
      if (patNr == 2) {
        RASHandlingPin16(row - 1);
        rowCheck16Pin(cols, 3, 3);  // check if last Row still has Pattern Nr 3 - Otherwise Error 3
      } else {
        // just refreh the last row on any pattern change
        refreshRow16Pin(row - 1);
      }
    }
    delayMicroseconds(52);  // Fine Tuning to surely be at least 2ms / 4ms respectively for the Refresh Test
    // Measurement showed 2.045ms and 4.008 ms for my TestBoard
    refreshRow16Pin(row);  // Refresh the current row before leaving
  }
}

void refreshRow16Pin(uint16_t row) {
  RASHandlingPin16(row);  // Refresh this ROW
  NOP;
  NOP;
  PORTB |= (1 << PB1);  // Set RAS High - Inactive
}

void rowCheck16Pin(uint16_t cols, uint8_t patNr, uint8_t check) {
  uint8_t pat = pattern[patNr];
  // Iterate over the Columns and read & check Pattern
  for (uint16_t col = 0; col <= cols; col++) {
    PORTB = (PORTB & 0xea) | (col & 0x0010) | ((col & 0x0008) >> 1) | ((col & 0x0040) >> 6);
    PORTC = (PORTC & 0xe8) | ((col & 0x0001) << 4) | ((col & 0x0100) >> 8);
    PORTD = ((col & 0x0080) >> 1) | ((col & 0x0020) << 2) | ((col & 0x0004) >> 2) | (col & 0x0002);
    PORTC &= ~0x08;  // CAS Latch Strobe
    NOP;             // Input Settle Time for Digital Inputs = 93ns
    NOP;             // One NOP@16MHz = 62.5ns
    if (((PINC & 0x04) >> 2) != (pat & 0x01)) {
      PORTC |= 0x08;        // CAS High - Cycle Time ~120ns
      PORTB |= (1 << PB1);  // Set RAS High - Inactive
      error(patNr + 1, check);
    }               // Check if Pattern matches
    PORTC |= 0x08;  // CAS High - Cycle Time ~120ns
    pat = (pat << 1) | (pat >> 7);
  }
  PORTB |= (1 << PB1);  // Set RAS High - Inactive
}

// Address Line Checks and sensing for 41256 or 4164
boolean Sense41256() {
  boolean big = true;
  PORTC &= 0x08;  // Set CAS High
  // RAS Testing set Row 0 Col 0 and set Bit Low.
  RASHandlingPin16(0);
  PORTB &= ~(1 << PB3);  // Set WE Low - Active
  PORTC &= ~0x08;        // CAS  Strobe
  NOP;
  PORTC |= 0x08;        // CAS High
  PORTB |= (1 << PB3);  // Set WE High - Inactive
  for (uint8_t a = 0; a <= 8; a++) {
    uint16_t adr = (1 << a);
    RASHandlingPin16(adr);
    // Write Bit Col 0 High
    PORTB &= ~(1 << PB3);  // Set WE Low - Active
    PORTC |= 0x02;
    PORTC &= ~0x08;  // CAS  Strobe
    NOP;
    PORTC |= 0x08;        // CAS High
    PORTB |= (1 << PB3);  // Set WE High - Inactive
    // Back to Row 0 then check if Bit at Col 0 is still 0
    RASHandlingPin16(0);
    PORTC &= ~0x08;  // CAS  Strobe
    NOP;
    NOP;
    // Check for Dout = 0
    if (((PINC & 0x04) >> 2) != (0 & 0x01)) {
      // If A8 Line is set and it is a fail, this might be a 6464 Type
      if (a == 8)
        big = false;
      else {
        PORTC |= 0x08;  // CAS High
        error(a, 1);
      }
    }
    PORTC |= 0x08;  // CAS High
  }
  // CAS address Tests performed on ROW 0
  RASHandlingPin16(0);
  PORTB &= ~(1 << PB3);  // Set WE Low - Active
  // Set Column 0 and DataIn = Low -> Preset 0 at R=0 / C=0
  PORTB = (PORTB & 0xea);
  PORTC = (PORTC & 0xe8);
  PORTD = 0;
  PORTC &= ~0x08;  // CAS  Strobe
  NOP;
  PORTC |= 0x08;  // CAS High
  // Cycle through all Address lines and set 1 on each address. Check for 0 at Col 0.
  // If an address Pin or address decoder is dead we should get a 1 at Col 0
  for (uint8_t a = 0; a <= 8; a++) {
    uint16_t adr = (1 << a);
    PORTB &= ~(1 << PB3);  // Set WE Low - Active
    // Set address
    PORTB = (PORTB & 0xea) | (adr & 0x0010) | ((adr & 0x0008) >> 1) | ((adr & 0x0040) >> 6);
    PORTC = (PORTC & 0xe8) | ((adr & 0x0001) << 4) | ((adr & 0x0100) >> 8) | 0x02;  // Set Bit 2 -> Data In
    PORTD = ((adr & 0x0080) >> 1) | ((adr & 0x0020) << 2) | ((adr & 0x0004) >> 2) | (adr & 0x0002);
    PORTC &= ~0x08;          // CAS  Strobe
    NOP;                     // Just to be sure for slower RAM
    PORTC |= 0x08;           // CAS High
    PORTB |= (1 << PB3);     // Set WE High - Inactive
    PORTB = (PORTB & 0xea);  // Reset Col Addr to 0
    PORTC = (PORTC & 0xe8);
    PORTD = 0;
    PORTC &= ~0x08;  // CAS  Strobe
    NOP;
    NOP;
    // Check for Dout = 0
    if (((PINC & 0x04) >> 2) != (0 & 0x01)) {
      // If A8 Line is set and it is a fail, this might be a 6464 Type
      if ((a == 8) && (big == false))
        NOP;  // Row Testing showed already the small type. If it did not we have a Problem.
      else {
        PORTC |= 0x08;  // CAS High
        error(a, 1);
      }
    }
    PORTC |= 0x08;  // CAS High
  }
  return big;
}

//=======================================================================================
// 18 - Pin DRAM Test Code
//=======================================================================================

void test18Pin() {
  // Configure I/O for this Chip Type
  DDRB = 0b00111111;
  PORTB = 0b00000000;
  DDRC = 0b00011111;
  PORTC = 0b00000000;

  DDRD = 0b11100111;
  PORTD = 0x00;
  if (Sense4464() == true) {
    for (uint16_t row = 0; row < 256; row++) {  // Iterate over all ROWs
      write18PinRow(row, 0, 256);
    }
    // Good Candidate.
    testOK();
  } else {                                      // 4416 has 256 ROW but only 64 Columns (Bit 1-6)
    for (uint16_t row = 0; row < 256; row++) {  // Iterate over all ROWs
      write18PinRow(row, 1, 128);
    }
    // Indicate with Green-Red flashlight that the "small" Version has been checked ok
    smallOK();
  }
}

void write18PinRow(uint8_t row, uint8_t init_shift, uint8_t width) {
  for (uint8_t patNr = 0; patNr < 4; patNr++) {
    // Prepare Write Cycle
    RASHandling18Pin(row);
    PORTB &= ~0x02;  // Set WE LOW
    SET_DATA_PIN18(pattern[patNr]);
    configDOut18Pin();
    for (uint16_t col = 0; col < width; col++) {
      SET_ADDR_PIN18(col << init_shift);
      PORTC &= ~0x04;  // CAS Strobe
      NOP;
      PORTC |= 0x04;
    }
    PORTB |= 0x02;  // Disable WE
    configDIn18Pin();
    PORTC &= ~0x01;  // Set OE LOW
    for (uint16_t col = 0; col < width; col++) {
      SET_ADDR_PIN18(col << init_shift);
      PORTC &= ~0x04;  // CAS Strobe
      NOP;
      NOP;
      if ((GET_DATA_PIN18 & 0xF) != (pattern[patNr] & 0xF)) {
        PORTC |= 0x15;
        error(patNr, 2);
      }
      PORTC |= 0x04;
    }
  }
  PORTC &= ~0x10; // RAS High
}

void configDOut18Pin() {
  DDRB |= 0x09;  // Configure D1 & D2 as Outputs
  DDRC |= 0x0A;  // Configure D0 & D3 as Outputs
}

void configDIn18Pin() {
  DDRB &= 0x16;  // Config Data Lines for input
  DDRC &= 0x15;
}

void RASHandling18Pin(uint8_t row) {
  PORTC |= 0x10;  // Set RAS High
  SET_ADDR_PIN18(row);
  PORTC &= ~0x10;  // RAS Active
}

boolean Sense4464() {
  boolean big = true;
  RASHandling18Pin(0);  // Use Row 0 for Size Tests
  PORTB &= ~0x02;       // Set WE LOW - Active
  SET_DATA_PIN18(0x0);
  SET_ADDR_PIN18(0x00);
  PORTC &= ~0x04;  // CAS Strobe
  NOP;
  PORTC |= 0x04;
  // 4416 CAS addressing does not Use A0 nor A7 we set A0 to check and Write 1111 to this Column
  // First test the CAS addressing
  for (uint8_t a = 0; a <= 7; a++) {
    uint8_t col = (1 << a);
    configDOut18Pin();
    PORTB &= ~0x02;  // Set WE LOW - Active
    SET_DATA_PIN18(0xF);
    SET_ADDR_PIN18(col);
    PORTC &= ~0x04;  // CAS Strobe
    NOP;
    PORTC |= 0x04;
    SET_ADDR_PIN18(0x00);
    PORTB |= 0x02;  // WE inactive - high
    configDIn18Pin();
    PORTC &= ~0x05;  // CAS Strobe and OE
    NOP;
    NOP;
    if ((GET_DATA_PIN18 & 0xF) != (0x0)) {
      if (a == 0) {
        big = false;
        PORTC |= 0x05;   // End CAS and OE
        PORTB &= ~0x02;  // Set WE LOW - Active
        configDOut18Pin();
        SET_DATA_PIN18(0x0);
        SET_ADDR_PIN18(0x01);
        PORTC &= ~0x04;  // CAS Strobe
      } else if ((a == 7) && (big == false))
        NOP;
      else {
        PORTC |= 0x15;
        error(a, 1);
      }
    }
    PORTC |= 0x05;
  }
  // Now we check Row addressing
  configDOut18Pin();
  RASHandling18Pin(0);  // Use Row 0 for Size Tests
  PORTB &= ~0x02;       // Set WE LOW - Active
  SET_DATA_PIN18(0x0);
  SET_ADDR_PIN18(0x18);  // Randomly choose a Column not on the edge since 4416 does not use A0 / A7 Columns
  PORTC &= ~0x04;        // CAS Strobe
  NOP;
  PORTC |= 0x04;
  for (uint8_t a = 0; a <= 7; a++) {
    uint8_t row = (1 << a);
    configDOut18Pin();
    PORTB &= ~0x02;  // Set WE LOW - Active
    SET_DATA_PIN18(0xF);
    RASHandling18Pin(row);
    SET_ADDR_PIN18(0x18);
    PORTC &= ~0x04;  // CAS Strobe
    NOP;
    PORTC |= 0x04;
    RASHandling18Pin(0x00);
    SET_ADDR_PIN18(0x18);
    PORTB |= 0x02;  // WE inactive - high
    configDIn18Pin();
    PORTC &= ~0x05;  // CAS Strobe and OE
    NOP;
    NOP;
    if ((GET_DATA_PIN18 & 0xF) != (0x0)) {
      PORTC |= 0x15;
      error(a, 1);
    }
    PORTC |= 0x15;
  }
  return big;
}

//=======================================================================================
// 20 - Pin DRAM Test Code
//=======================================================================================

void test20Pin() {
  // Configure I/O for this Chip Type
  DDRB = 0b00011111;
  DDRC = 0b00011111;
  DDRD = 0xFF;
  PORTB = 0b00111111;
  PORTC = 0b10000000;
  PORTD = 0x00;
  if (Sense1Mx4() == true) {
    // Run the Tests for the larger Chip if A9 is used we run the larger test for 512kB
    // This could be optimized.
    for (uint8_t pat = 0; pat < 4; pat++) {        // Check all 4Bit Patterns
      for (uint16_t row = 0; row < 1024; row++) {  // Iterate over all ROWs
        if (pat > 2)
          pat+ (row & 0x0001);
        write20PinRow(row, pat, 4);
      }
    }
    // Good Candidate.
    testOK();
  } else {                                        // A9 most probably not used or defect - just run 128kB Test
    for (uint8_t pat = 0; pat < 4; pat++)         // Check all 4Bit Patterns
      for (uint16_t row = 0; row < 512; row++) {  // Iterate over all ROWs
        if (pat > 2)
          pat+ (row & 0x0001);
        write20PinRow(row, pat, 2);
      }
    // Indicate with Green-Red flashlight that the "small" Version has been checked ok
    smallOK();
  }
}

// Prepare and execute ROW Access for 20 Pin Types
void RASHandlingPin20(uint16_t row) {
  PORTB |= (1 << PB1);         // Set RAS High - Inactive
  msbHandlingPin20(row >> 8);  // Preset ROW Adress
  PORTD = (uint8_t)(row & 0xFF);
  PORTB &= ~(1 << PB1);  // RAS Latch Strobe
}

// Prepare Controll Lines and perform Checks
void write20PinRow(uint16_t row, uint8_t pattern, uint16_t width) {
  PORTB |= 0x0F;                          // Set all RAM Controll Lines to HIGH = Inactive
  CASHandlingPin20(row, pattern, width);  // Do the Test
  PORTB |= 0x0F;                          // Set all RAM Controll Lines to HIGH = Inactive
  // Enhanced PageMode Row Write & Read Done
}

void msbHandlingPin20(uint16_t address) {
  PORTB = (PORTB & 0xEF) | ((address & 0x01) << 4);
  PORTC = (PORTC & 0xEF) | ((address & 0x02) << 3);
}

// Write and Read (&Check) Pattern from Cols
void CASHandlingPin20(uint16_t row, uint8_t patNr, uint16_t colWidth) {
  RASHandlingPin20(row);  // Set the Row
  for (uint8_t msb = 0; msb < colWidth; msb++) {
    // Prepare Write Cycle
    PORTC &= 0xF0;          // Set all Outputs to LOW
    DDRC |= 0x0F;           // Configure IOs for Output
    msbHandlingPin20(msb);  // Set the MSB as needed
    PORTB &= ~(1 << PB3);   // Set WE Low - Active
    PORTC |= (pattern[patNr] & 0x0F);
    // Iterate over 255 Columns and write the Pattern
    for (uint16_t col = 0; col <= 255; col++) {
      PORTD = (uint8_t)col;  // Set Col Adress
      PORTB &= ~1;           // CAS Latch Strobe
      PORTB |= 1;            // CAS High - Cycle Time ~120ns
    }
    // Prepare Read Cycle
    PORTB |= (1 << PB3);  // Set WE High - Inactive
    PORTC &= 0xF0;        // Clear all Outputs
    DDRC &= 0xF0;         // Configure IOs for Input
    checkRow20Pin(msb, patNr, 2);
  }
  if (row >= 7) { // Delay Row Crosstalk Testing until we reach Row 7 as this also tests Data Retention (~1.021ms per Row for Write/Read Tests)
    if (patNr == (3+(row & 0x0001))) {
      RASHandlingPin20(row - 7);
      for (uint8_t msb = 0; msb < colWidth; msb++)
        checkRow20Pin(msb, (3+(row & 0x0001)), 3);  // check if last Row still has Pattern Nr 3 - Otherwise Error 3
    } 
  }
  delayMicroseconds(126);  // Fine Tuning to surely be at least 8ms  for the Refresh Test
  // Measurement showed  for my TestBoard 
  //refreshRow20Pin(row);  // Refresh the current row before leaving
}

void refreshRow20Pin(uint16_t row) {
  PORTB |= 1;  // CAS High
  RASHandlingPin20(row);
  PORTB |= (1 << PB1);  // Set RAS High - Inactive
}

void checkRow20Pin(uint8_t msb, uint8_t patNr, uint8_t errNr) {
  msbHandlingPin20(msb);  // Set the MSB as needed
  PORTB &= ~(1 << PB2);   // Set OE Low - Active
  // Iterate over 255 Columns and read & check Pattern
  for (uint16_t col = 0; col <= 255; col++) {
    PORTD = (uint8_t)col;  // Set Col Adress
    PORTB &= ~1;
    NOP;  // Input Settle Time for Digital Inputs = 93ns
    NOP;  // One NOP@16MHz = 62.5ns
    if ((PINC & 0x0F) != (pattern[patNr] & 0x0f)) {
      PORTB |= 1;           // Set CAS High
      PORTB |= (1 << PB1);  // Set RAS High - Inactive
      error(patNr + 1, errNr);
    }  // Check if Pattern matches
    PORTB |= 1;
  }
  PORTB |= (1 << PB2);  // Set OE High - Inactive
}


// The following Routine checks if A9 Pin is used - which is the case for 1Mx4 DRAM in 20Pin Mode
boolean Sense1Mx4() {
  boolean big = true;
  DDRC |= 0x0F;  // Configure IOs for Output
  // Prepare for Test and write Row 0 Col 0 to LOW LOW LOW LOW
  PORTB |= (1 << PB1);  // Set RAS High - Inactive
  PORTD = 0x00;         // Set Row and Col address to 0
  PORTB &= 0xEF;        // Clear address Bit 8
  PORTC &= 0xE0;        // Set all Outputs and A9 to LOW
  RASHandlingPin20(0);
  PORTB &= ~(1 << PB3);  // Set WE Low - Active
  PORTB &= ~1;           // CAS Latch Strobe
  PORTB |= 1;            // CAS High - Cycle Time ~120ns -> Write 0000 to Row 0, Col 0
  // Row address line, buffer and decoder test
  for (uint8_t a = 0; a <= 9; a++) {
    uint16_t adr = (1 << a);
    RASHandlingPin20(adr);
    PORTD = 0x00;          // Set Col Adress
    PORTB &= 0xEF;         // Clear address Bit 8
    PORTC &= 0xEF;         // A9 to LOW
    DDRC |= 0x0F;          // Configure IOs for Output
    PORTC |= 0x0F;         // Set all Bit
    PORTB &= ~(1 << PB3);  // Set WE Low - Active
    PORTB &= ~1;           // CAS Latch Strobe
    PORTB |= 1;            // CAS High - Cycle Time ~120ns -> Write 0000 to Row 0, Col 0
    PORTB |= (1 << PB3);   // Set WE High - Inactive
    RASHandlingPin20(0);
    PORTD = 0x00;          // Set Col Adress
    PORTB &= 0xEF;         // Clear address Bit 8
    PORTC &= 0xE0;         // A9 to LOW - Outputs low
    DDRC &= 0xF0;          // Configure IOs for Input
    PORTB &= ~(1 << PB2);  // Set OE Low - Active
    PORTB &= ~1;           // CAS Latch Strobe
    NOP;
    NOP;
    if ((PINC & 0xF) != 0x0) {  // Read the Data at this address
      if (a == 9)
        big = false;
      else {
        PORTB |= 1;  // CAS High - Cycle Time ~120ns
        error(a, 3);
      }
    }
    PORTB |= 1;  // CAS High - Cycle Time ~120ns
  }
  // Check Column address lines, buffers and decoders
  RASHandlingPin20(0);
  DDRC |= 0x0F;          // Configure IOs for Output
  PORTC &= 0xE0;         // Set all Outputs and A9 to LOW
  PORTB &= ~(1 << PB3);  // Set WE Low - Active
  PORTB &= ~1;           // CAS Latch Strobe
  PORTB |= 1;            // CAS High - Cycle Time ~120ns -> Write 0000 to Row 0, Col 0
  for (uint8_t a = 0; a <= 9; a++) {
    uint16_t adr = (1 << a);
    PORTD = (adr & 0xff);  // Set Col Adress
    msbHandlingPin20(adr >> 8);
    DDRC |= 0x0F;          // Configure IOs for Output
    PORTC |= 0x0F;         // Set all Bit
    PORTB &= ~(1 << PB3);  // Set WE Low - Active
    PORTB &= ~1;           // CAS Latch Strobe
    PORTB |= 1;            // CAS High - Cycle Time ~120ns -> Write 0000 to Row 0, Col 0
    PORTB |= (1 << PB3);   // Set WE High - Inactive
    PORTD = 0x00;          // Set Col Adress
    PORTB &= 0xEF;         // Clear address Bit 8
    PORTC &= 0xEF;         // A9 to LOW
    DDRC &= 0xF0;          // Configure IOs for Input
    PORTB &= ~(1 << PB2);  // Set OE Low - Active
    PORTB &= ~1;           // CAS Latch Strobe
    NOP;
    NOP;
    if ((PINC & 0xF) != 0x0) {  // Read the Data at this address
      if ((a == 9) && (big == false))
        NOP;  // Row Testing showed already the small type. If it did not we have a Problem.
      else {
        PORTC |= 0x08;  // CAS High
        error(a, 1);
      }
      PORTB |= 1;  // CAS High - Cycle Time ~120ns
    }
  }
  return big;
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
void checkGNDShort4Port(int *portb, int *portc, int *portd) {
  for (int i = 0; i <= 7; i++) {
    int8_t mask = 1 << i;
    if (portb[i] != EOL && portb[i] != NC && ((PINB & mask) == 0)) {
      error(portb[i], 1);
    }
    if (portc[i] != EOL && portc[i] != NC && ((PINC & mask) == 0)) {
      error(portc[i], 1);
    }
    if (portd[i] != EOL && portd[i] != NC && ((PIND & mask) == 0)) {
      error(portd[i], 1);
    }
  }
}

// Prepare LED for inidcation of Results or Errors
void setupLED() {
  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
}

// Indicate Errors. Red LED for Error Type, and green for additional Error Info.
void error(uint8_t code, uint8_t error) {
  setupLED();
  while (true) {
    for (int i = 0; i < error; i++) {
      digitalWrite(LED_R, 1);
      delay(500);
      digitalWrite(LED_R, 0);
      delay(500);
    }
    for (int i = 0; i < code; i++) {
      digitalWrite(LED_G, 1);
      delay(250);
      digitalWrite(LED_G, 0);
      delay(250);
    }
    delay(1000);
  }
}

// GREEN - OFF Flashlight - Indicate a successfull test
void testOK() {
  setupLED();
  while (true) {
    digitalWrite(LED_G, 1);
    delay(850);
    digitalWrite(LED_G, 0);
    delay(250);
  }
}

// RED-GREEN Flashlight - Indicate a successfull test for the "smaller" Variant of this Pin Config
void smallOK() {
  setupLED();
  while (true) {
    digitalWrite(LED_G, HIGH);
    digitalWrite(LED_R, LOW);
    delay(850);
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_R, HIGH);
    delay(150);
  }
}

// Indicate a Problem with the DipSwitch Config (Continuous Red Blink)
void ConfigFail() {
  setupLED();
  while (true) {
    digitalWrite(LED_R, 1);
    delay(250);
    digitalWrite(LED_R, 0);
    delay(250);
  }
}
