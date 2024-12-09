// RAM Tester Sketch for the RAM Tester PCB
// ========================================
//
// Author : Andreas Hoffmann
// Version: 2.1
// Date   : 09.12.2024
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
// 4 Red & n Green      - GND Short on Pin (n Green)
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
// Version 1.4  - Added Support for 4416/4464 but currently only tested for 4416 as 4464 Testchips not yet available
// Version 2.0pre1 - Added Row Crosstalk Checks. Added Refresh Time Checks (2ms for 4164/4ms for 41256/8ms for all DIP/ZIP 20 Types)
// Version 2.0pre  - Merge with 2.0 Branch No Refresh Tests for 4416/4464 yet.
//                   Switch ON red light during testing = yellowish light during test
//                   Some cleanup of old code style in 20Pin code section
//                   Re-Enabled the GND-Short-Test code. Was deactivated until RAM test works reliably
//                   Code Streamlining
// Version 2.0       Bugfixes for 4464 and timing adjustmens for Refresh Checks
//                   Todo:
//                    - Eliminate Corner Cases when Testing Crosstalk (Start and End of Rows)
//                    - Eventually Run Checks in reverse Order (i.e. starting with last Row) to really cover all cases
// Version 2.1   - Added installation testing after soldering the PCB. At first it will start in Test Mode. Instructions in Github. 
//                 To Exit Test Mode : Set all DIP Switches to ON - Reset - all DIP Switches to OFF - Reset. Further starts will be in normal Mode
//
// Disclaimer:
// This Project (Software & Hardware) is a hobbyist Project. I do not guarantee it's fitness for any purpose
// and I do not guarantee that it is Errorfree. All usage is on your risk.

// For slow RAM we may need to introduce an additional 62.5ns delay. 16MHz Clock -> 1 Cycle = 62.5ns
#include <EEPROM.h>

#define NOP __asm__ __volatile__("nop\n\t")

#define Mode_16Pin 2
#define Mode_18Pin 4
#define Mode_20Pin 5
#define EOL 254
#define NC 255
// ON / OFF for the LED, depends on the Circuit. P-FET require a inverted Signal to lite the LED
#define ON HIGH
#define OFF LOW
#define TESTING 0x00
#define LED_FLAG 0x01

// The Testpatterns
const uint8_t pattern[] = { 0x00, 0xff, 0xaa, 0x33, 0xff, 0x33 };  // Equals to 0b00000000, 0b11111111, 0b10101010, 0b01010101

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
#define CAS_LOW16 PORTC &= 0xf7
#define CAS_HIGH16 PORTC |= 0x08
#define RAS_LOW16 PORTB &= 0xfd
#define RAS_HIGH16 PORTB |= 0x02
#define WE_LOW16 PORTB &= 0xf7
#define WE_HIGH16 PORTB |= 0x08
#define SET_ADDR_PIN16(addr, data) \
  { \
    PORTB = (PORTB & 0xea) | (addr & 0x0010) | ((addr & 0x0008) >> 1) | ((addr & 0x0040) >> 6); \
    PORTC = (PORTC & 0xe8) | ((addr & 0x0001) << 4) | ((addr & 0x0100) >> 8) | ((data & 0x01) << 1); \
    PORTD = ((addr & 0x0080) >> 1) | ((addr & 0x0020) << 2) | ((addr & 0x0004) >> 2) | (addr & 0x0002); \
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
#define CAS_LOW18 PORTC &= 0xfb
#define CAS_HIGH18 PORTC |= 0x04
#define RAS_LOW18 PORTC &= 0xef
#define RAS_HIGH18 PORTC |= 0x10
#define OE_LOW18 PORTC &= 0xfe
#define OE_HIGH18 PORTC |= 0x01
#define WE_LOW18 PORTB &= 0xfd
#define WE_HIGH18 PORTB |= 0x02
// Address Distribution for 18Pin Types
#define SET_ADDR_PIN18(addr) \
  { \
    PORTB = (PORTB & 0xeb) | ((addr & 0x01) << 2) | ((addr & 0x02) << 3); \
    PORTD = ((addr & 0x04) << 5) | ((addr & 0x08) << 3) | ((addr & 0x80) >> 2) | ((addr & 0x20) >> 4) | ((addr & 0x40) >> 6) | ((addr & 0x10) >> 3); \
  }

#define SET_DATA_PIN18(data) \
  { \
    PORTB = (PORTB & 0xf6) | ((data & 0x02) << 2) | ((data & 0x04) >> 2); \
    PORTC = (PORTC & 0xf5) | ((data & 0x01) << 1) | (data & 0x08); \
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
#define CAS_LOW20 PORTB &= 0xfe
#define CAS_HIGH20 PORTB |= 0x01
#define RAS_LOW20 PORTB &= 0xfd
#define RAS_HIGH20 PORTB |= 0x02
#define OE_LOW20 PORTB &= 0xfb
#define OE_HIGH20 PORTB |= 0x04
#define WE_LOW20 PORTB &= 0xf7
#define WE_HIGH20 PORTB |= 0x08

uint8_t Mode = 0;    // PinMode 2 = 16 Pin, 4 = 18 Pin, 5 = 20 Pin
uint8_t red = 13;    // PB5
uint8_t green = 12;  // PB4 -> Co Used with RAM Test Socket, see comments below!

void setup() {
  // Data Direction Register Port B, C & D - Preconfig as Input (Bit=0)
  DDRB &= 0b11100000;
  DDRC &= 0b11000000;
  DDRD = 0x00;
  // If TESTING is set enter "Factory Test" Mode
  if (EEPROM.read(TESTING) != 0) {
    buildTest();
  }
  // Wait for the Candidate to properly Startup
  if (digitalRead(19) == 1) { Mode += Mode_20Pin; }
  if (digitalRead(3) == 1) { Mode += Mode_18Pin; }
  if (digitalRead(2) == 1) { Mode += Mode_16Pin; }
  // Check if the DIP Switch is set for a valid Configuration.
  if (Mode < 2 || Mode > 5) ConfigFail();
  // With a valid Config, activate the PullUps
  PORTB |= 0b00011111;
  PORTC |= 0b00111111;
  PORTD = 0xff;
  digitalWrite(13, ON);  // Switch the LED on PB5 on for the rest of the test as it will show as yellow not to confuse Users as steady green.
  // Settle State - PullUps my require some time.
  checkGNDShort();  // Check for Shorts towards GND. Shorts on Vcc can't be tested as it would need Pull-Downs.
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
void rASHandlingPin16(uint16_t row) {
  RAS_HIGH16;
  // Row Address distribution Logic for 41256/64 16 Pin RAM - more complicated as the PCB circuit is optimized for 256x4 / 1Mx4 Types.
  SET_ADDR_PIN16(row, 0);
  RAS_LOW16;
}

// Write and Read (&Check) Pattern from Cols
void write16PinRow(uint16_t row, uint16_t cols) {
  for (uint8_t patNr = 0; patNr < 4; patNr++) {
    // Prepare Write Cycle
    CAS_HIGH16;
    rASHandlingPin16(row);  // Set the Row
    WE_LOW16;
    uint8_t pat = pattern[patNr];
    for (uint16_t col = 0; col <= cols; col++) {
      // Column Address distribution logic for 41256/64 16 Pin RAM
      SET_ADDR_PIN16(col, pat);
      CAS_LOW16;
      NOP;  // Just to be sure for slower RAM
      CAS_HIGH16;
      // Rotate the Pattern 1 Bit to the LEFT (c has not rotate so there is a trick with 2 Shift)
      pat = (pat << 1) | (pat >> 7);
    }
    // Prepare Read Cycle
    WE_HIGH16;
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
        rASHandlingPin16(row - 1);
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
  rASHandlingPin16(row);  // Refresh this ROW
  NOP;
  NOP;
  RAS_HIGH16;
}

void rowCheck16Pin(uint16_t cols, uint8_t patNr, uint8_t check) {
  uint8_t pat = pattern[patNr];
  // Iterate over the Columns and read & check Pattern
  for (uint16_t col = 0; col <= cols; col++) {
    SET_ADDR_PIN16(col, 0);
    CAS_LOW16;
    NOP;  // Input Settle Time for Digital Inputs = 93ns
    NOP;  // One NOP@16MHz = 62.5ns
    if (((PINC & 0x04) >> 2) != (pat & 0x01)) {
      error(patNr + 1, check);
    }  // Check if Pattern matches
    CAS_HIGH16;
    pat = (pat << 1) | (pat >> 7);
  }
  RAS_HIGH16;
}

// Address Line Checks and sensing for 41256 or 4164
boolean Sense41256() {
  boolean big = true;
  CAS_HIGH16;
  // RAS Testing set Row 0 Col 0 and set Bit Low.
  rASHandlingPin16(0);
  PORTC &= 0xfd;
  WE_LOW16;
  CAS_LOW16;
  NOP;
  CAS_HIGH16;
  WE_HIGH16;
  for (uint8_t a = 0; a <= 8; a++) {
    uint16_t adr = (1 << a);
    rASHandlingPin16(adr);
    // Write Bit Col 0 High
    WE_LOW16;
    PORTC |= 0x02;
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
    WE_HIGH16;
    // Back to Row 0 then check if Bit at Col 0 is still 0
    rASHandlingPin16(0);
    CAS_LOW16;
    NOP;
    NOP;
    // Check for Dout = 0
    if (((PINC & 0x04) >> 2) != (0 & 0x01)) {
      // If A8 Line is set and it is a fail, this might be a 6464 Type
      if (a == 8)
        big = false;
      else {
        error(a, 1);
      }
    }
    CAS_HIGH16;
  }
  // CAS address Tests performed on ROW 0
  rASHandlingPin16(0);
  WE_LOW16;
  // Set Column 0 and DataIn = Low -> Preset 0 at R=0 / C=0
  PORTB = (PORTB & 0xea);
  PORTC = (PORTC & 0xe8);
  PORTD = 0;
  CAS_LOW16;
  NOP;
  CAS_HIGH16;
  // Cycle through all Address lines and set 1 on each address. Check for 0 at Col 0.
  // If an address Pin or address decoder is dead we should get a 1 at Col 0
  for (uint8_t a = 0; a <= 8; a++) {
    uint16_t adr = (1 << a);
    WE_LOW16;
    // Set aadress
    SET_ADDR_PIN16(adr, 0x02);
    CAS_LOW16;
    NOP;  // Just to be sure for slower RAM
    CAS_HIGH16;
    WE_HIGH16;
    PORTB = (PORTB & 0xea);  // Reset Col Addr to 0
    PORTC = (PORTC & 0xe8);
    PORTD = 0;
    CAS_LOW16;
    NOP;
    NOP;
    // Check for Dout = 0
    if (((PINC & 0x04) >> 2) != (0 & 0x01)) {
      // If A8 Line is set and it is a fail, this might be a 6464 Type
      if ((a == 8) && (big == false))
        NOP;  // Row Testing showed already the small type. If it did not we have a Problem.
      else {
        error(a, 1);
      }
    }
    CAS_HIGH16;
  }
  return big;
}

//=======================================================================================
// 18 - Pin DRAM Test Code
//=======================================================================================

void test18Pin() {
  // Configure I/O for this Chip Type
  DDRB = 0b00111111;
  PORTB = 0b00100010;
  DDRC = 0b00011111;
  PORTC = 0b00010101;
  DDRD = 0b11100111;
  if (sense4464() == true) {
    noInterrupts();
    for (uint16_t row = 0; row < 256; row++) {  // Iterate over all ROWs
      write18PinRow(row, 0, 256);
    }
    interrupts();
    // Good Candidate.
    testOK();
  } else {                                      // 4416 has 256 ROW but only 64 Columns (Bit 1-6)
    for (uint16_t row = 0; row < 256; row++) {  // Iterate over all ROWs
      write18PinRow(row, 1, 64);
    }
    // Indicate with Green-Red flashlight that the "small" Version has been checked ok
    smallOK();
  }
}

void write18PinRow(uint8_t row, uint8_t init_shift, uint16_t width) {
  uint16_t colAddr;  // Prepared Column Adress to safe Init Time. This is needed when A0 & A8 are not used for Col addressing.
  for (uint8_t patNr = 0; patNr < 4; patNr++) {
    // Prepare Write Cycle
    rASHandling18Pin(row);
    WE_LOW18;
    SET_DATA_PIN18(pattern[patNr]);
    configDOut18Pin();
    for (uint16_t col = 0; col < width; col++) {
      colAddr = (col << init_shift);
      SET_ADDR_PIN18(colAddr);
      CAS_LOW18;
      NOP;
      CAS_HIGH18;
    }
    WE_HIGH18;
    // If we check 255 Columns the time for Write & Read(Check) exceeds the Refresh time. We need to add a Refresh in the Middle
    if (init_shift == 0) {
      refreshRow18Pin(row - 1);  // Refresh the last row
      delayMicroseconds(41);     // Minor delay to really check for 2ms Refresh of the last row
      rASHandling18Pin(row);     // Reselect the just written row for checking
    }
    checkColumn18Pin(width, patNr, init_shift, 2);
    // If Pattern 2 was written check last row for Crosstalk, it should still read Pattern 3
    if (init_shift == 1) {
      if (row > 1) {
        if (patNr == 2) {
          rASHandling18Pin(row - 2);
          checkColumn18Pin(width, 3, init_shift, 3);
        } else if (patNr == 0) {
          // In case of the 4416 with 64 Cols, the Time to Write/Read two Patterns is almost 2ms = Refresh inervals, so we refesh 2 Pattern Columns after the last access to this column1
          refreshRow18Pin(row - 2);
          delayMicroseconds(79);
        }
      }
    } else if (row > 0) {
      if (patNr == 2) {
        rASHandling18Pin(row - 1);
        checkColumn18Pin(width, 3, init_shift, 3);
      } else {
        refreshRow18Pin(row - 1);
      }
    }
  }
  RAS_HIGH18;
}

void checkColumn18Pin(uint16_t width, uint8_t patNr, uint8_t init_shift, uint8_t errorNr) {
  configDIn18Pin();
  uint8_t pat = pattern[patNr] & 0x0f;
  OE_LOW18;
  for (uint16_t col = 0; col < width; col++) {
    SET_ADDR_PIN18(col << init_shift);
    CAS_LOW18;
    NOP;
    NOP;
    if ((GET_DATA_PIN18) != pat) {
      error(patNr, errorNr);
    }
    CAS_HIGH18;
  }
  OE_HIGH18;
}

void configDOut18Pin() {
  DDRB |= 0x09;  // Configure D1 & D2 as Outputs
  DDRC |= 0x0a;  // Configure D0 & D3 as Outputs
}

void configDIn18Pin() {
  DDRB &= 0x16;  // Config Data Lines for input
  DDRC &= 0x15;
}

void refreshRow18Pin(uint8_t row) {
  rASHandling18Pin(row);
  NOP;
  NOP;
  RAS_HIGH18;
}

void rASHandling18Pin(uint8_t row) {
  RAS_HIGH18;
  SET_ADDR_PIN18(row);
  RAS_LOW18;
}

boolean sense4464() {
  boolean big = true;
  rASHandling18Pin(0);  // Use Row 0 for Size Tests
  WE_LOW18;
  SET_DATA_PIN18(0x0);
  SET_ADDR_PIN18(0x00);
  CAS_LOW18;
  NOP;
  CAS_HIGH18;
  // 4416 CAS addressing does not Use A0 nor A7 we set A0 to check and Write 1111 to this Column
  // First test the CAS addressing
  for (uint8_t a = 0; a <= 7; a++) {
    uint8_t col = (1 << a);
    configDOut18Pin();
    WE_LOW18;
    SET_DATA_PIN18(0xf);
    SET_ADDR_PIN18(col);
    CAS_LOW18;
    NOP;
    CAS_HIGH18;
    SET_ADDR_PIN18(0x00);
    WE_HIGH18;
    configDIn18Pin();
    OE_LOW18;
    CAS_LOW18;
    NOP;
    NOP;
    if ((GET_DATA_PIN18 & 0xf) != (0x0)) {
      if (a == 0) {
        big = false;
        CAS_HIGH18;
        OE_HIGH18;
        WE_LOW18;
        configDOut18Pin();
        SET_DATA_PIN18(0x0);
        SET_ADDR_PIN18(0x01);
        CAS_LOW18;
      } else if ((a == 7) && (big == false))
        NOP;
      else {
        error(a, 1);
      }
    }
    PORTC |= 0x05;
  }
  // Now we check Row addressing
  configDOut18Pin();
  rASHandling18Pin(0);  // Use Row 0 for Size Tests
  WE_LOW18;
  SET_DATA_PIN18(0x0);
  SET_ADDR_PIN18(0x18);  // Randomly choose a Column not on the edge since 4416 does not use A0 / A7 Columns
  CAS_LOW18;
  NOP;
  CAS_HIGH18;
  for (uint8_t a = 0; a <= 7; a++) {
    uint8_t row = (1 << a);
    configDOut18Pin();
    WE_LOW18;
    SET_DATA_PIN18(0xf);
    rASHandling18Pin(row);
    SET_ADDR_PIN18(0x18);
    CAS_LOW18;
    NOP;
    CAS_HIGH18;
    rASHandling18Pin(0x00);
    SET_ADDR_PIN18(0x18);
    WE_HIGH18;
    configDIn18Pin();
    OE_LOW18;
    CAS_LOW18;
    NOP;
    NOP;
    if ((GET_DATA_PIN18 & 0xf) != (0x0)) {
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
  PORTB = 0b00111111;
  PORTC = 0b10000000;
  PORTD = 0x00;
  DDRB = 0b00011111;
  DDRC = 0b00011111;
  DDRD = 0xFF;
  if (sense1Mx4() == true) {
    // Run the Tests for the larger Chip if A9 is used we run the larger test for 512kB
    // This could be optimized.
    for (uint8_t pat = 0; pat < 4; pat++) {        // Check all 4Bit Patterns
      for (uint16_t row = 0; row < 1024; row++) {  // Iterate over all ROWs
        write20PinRow(row, pat, 4);
      }
    }
    // Good Candidate.
    testOK();
  } else {                                        // A9 most probably not used or defect - just run 128kB Test
    for (uint8_t pat = 0; pat < 4; pat++)         // Check all 4Bit Patterns
      for (uint16_t row = 0; row < 512; row++) {  // Iterate over all ROWs
        write20PinRow(row, pat, 2);
      }
    // Indicate with Green-Red flashlight that the "small" Version has been checked ok
    smallOK();
  }
}

// Prepare and execute ROW Access for 20 Pin Types
void rASHandlingPin20(uint16_t row) {
  RAS_HIGH20;
  msbHandlingPin20(row >> 8);  // Preset ROW Adress
  PORTD = (uint8_t)(row & 0xff);
  RAS_LOW20;
}

// Prepare Controll Lines and perform Checks
void write20PinRow(uint16_t row, uint8_t pattern, uint16_t width) {
  PORTB |= 0x0f;                          // Set all RAM Controll Lines to HIGH = Inactive
  cASHandlingPin20(row, pattern, width);  // Do the Test
  PORTB |= 0x0f;                          // Set all RAM Controll Lines to HIGH = Inactive
  // Enhanced PageMode Row Write & Read Done
}

void msbHandlingPin20(uint16_t address) {
  PORTB = (PORTB & 0xef) | ((address & 0x01) << 4);
  PORTC = (PORTC & 0xef) | ((address & 0x02) << 3);
}

// Write and Read (&Check) Pattern from Cols
// Write and Read (&Check) Pattern from Cols
void cASHandlingPin20(uint16_t row, uint8_t patNr, uint16_t colWidth) {
  rASHandlingPin20(row);  // Set the Row
  for (uint8_t msb = 0; msb < colWidth; msb++) {
    // Prepare Write Cycle
    PORTC &= 0xf0;          // Set all Outputs to LOW
    DDRC |= 0x0f;           // Configure IOs for Output
    msbHandlingPin20(msb);  // Set the MSB as needed
    WE_LOW20;
    PORTC |= (pattern[(patNr + (row & 0x0001))] & 0x0f);  // Alternative Pattern Odd & Even so we can check for Crosstalk later.
    // Iterate over 255 Columns and write the Pattern
    for (uint16_t col = 0; col <= 255; col++) {
      PORTD = (uint8_t)col;  // Set Col Adress
      CAS_LOW20;
      CAS_HIGH20;
    }
    // Prepare Read Cycle
    WE_HIGH20;
    PORTC &= 0xf0;  // Clear all Outputs
    DDRC &= 0xf0;   // Configure IOs for Input
    checkRow20Pin(msb, (patNr + (row & 0x0001)), 2);
  }
  if (row >= 7) {  // Delay Row Crosstalk Testing until we reach Row 7 as this also tests Data Retention (~1.021ms per Row for Write/Read Tests)
    if (patNr == (3 + (row & 0x0001))) {
      rASHandlingPin20(row - 7);
      for (uint8_t msb = 0; msb < colWidth; msb++)
        checkRow20Pin(msb, (3 + ((row - 7) & 0x0001)), 3);  // check if last Row still has Pattern Nr 3 - Otherwise Error 3
    }
  }
  delayMicroseconds(126);  // Fine Tuning to surely be at least 8ms  for the Refresh Test
  // Measurement showed  for my TestBoard
  //refreshRow20Pin(row);  // Refresh the current row before leaving
}

void refreshRow20Pin(uint16_t row) {
  CAS_HIGH20;
  rASHandlingPin20(row);
  RAS_HIGH20;
}

void checkRow20Pin(uint8_t msb, uint8_t patNr, uint8_t errNr) {
  msbHandlingPin20(msb);  // Set the MSB as needed
  uint8_t pat = pattern[patNr] & 0x0f;
  OE_LOW20;
  // Iterate over 255 Columns and read & check Pattern
  for (uint16_t col = 0; col <= 255; col++) {
    PORTD = (uint8_t)col;  // Set Col Adress
    CAS_LOW20;
    NOP;  // Input Settle Time for Digital Inputs = 93ns
    NOP;  // One NOP@16MHz = 62.5ns
    if ((PINC & 0x0f) != pat) {
      PORTB |= 0x03;  // Set CAS & RAS High
      error(patNr + 1, errNr);
    }  // Check if Pattern matches
    CAS_HIGH20;
  }
  OE_HIGH20;
}


// The following Routine checks if A9 Pin is used - which is the case for 1Mx4 DRAM in 20Pin Mode
boolean sense1Mx4() {
  boolean big = true;
  DDRC |= 0x0f;  // Configure IOs for Output
  // Prepare for Test and write Row 0 Col 0 to LOW LOW LOW LOW
  RAS_HIGH20;
  PORTD = 0x00;   // Set Row and Col address to 0
  PORTB &= 0xef;  // Clear address Bit 8
  PORTC &= 0xe0;  // Set all Outputs and A9 to LOW
  rASHandlingPin20(0);
  WE_LOW20;
  CAS_LOW20;
  CAS_HIGH20;
  // Row address line, buffer and decoder test
  for (uint8_t a = 0; a <= 9; a++) {
    uint16_t adr = (1 << a);
    rASHandlingPin20(adr);
    PORTD = 0x00;   // Set Col Adress
    PORTB &= 0xef;  // Clear address Bit 8
    PORTC &= 0xef;  // A9 to LOW
    DDRC |= 0x0f;   // Configure IOs for Output
    PORTC |= 0x0f;  // Set all Bit
    WE_LOW20;
    CAS_LOW20;
    CAS_HIGH20;
    WE_HIGH20;
    rASHandlingPin20(0);
    PORTD = 0x00;   // Set Col Adress
    PORTB &= 0xef;  // Clear address Bit 8
    PORTC &= 0xe0;  // A9 to LOW - Outputs low
    DDRC &= 0xf0;   // Configure IOs for Input
    OE_LOW20;
    CAS_LOW20;
    NOP;
    NOP;
    if ((PINC & 0xf) != 0x0) {  // Read the Data at this address
      if (a == 9)
        big = false;
      else {
        error(a, 3);
      }
    }
    CAS_HIGH20;
    OE_HIGH20;
  }
  // Check Column address lines, buffers and decoders
  rASHandlingPin20(0);
  DDRC |= 0x0f;   // Configure IOs for Output
  PORTC &= 0xe0;  // Set all Outputs and A9 to LOW
  WE_LOW20;
  CAS_LOW20;
  CAS_HIGH20;
  for (uint8_t a = 0; a <= 9; a++) {
    uint16_t adr = (1 << a);
    PORTD = (adr & 0xff);  // Set Col Adress
    msbHandlingPin20(adr >> 8);
    DDRC |= 0x0f;   // Configure IOs for Output
    PORTC |= 0x0f;  // Set all Bit
    WE_LOW20;
    CAS_LOW20;
    CAS_HIGH20;
    WE_HIGH20;
    PORTD = 0x00;   // Set Col Adress
    PORTB &= 0xef;  // Clear address Bit 8
    PORTC &= 0xef;  // A9 to LOW
    DDRC &= 0xf0;   // Configure IOs for Input
    OE_LOW20;
    CAS_LOW20;
    NOP;
    NOP;
    if ((PINC & 0xf) != 0x0) {  // Read the Data at this address
      if ((a == 9) && (big == false))
        NOP;  // Row Testing showed already the small type. If it did not we have a Problem.
      else {
        error(a, 1);
      }
    }
    CAS_HIGH20;
    OE_HIGH20;
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

// Prepare LED for inidcation of Results or Errors
void setupLED() {
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

// Indicate Errors. Red LED for Error Type, and green for additional Error Info.
void error(uint8_t code, uint8_t error) {
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
      delay(250);
      digitalWrite(green, OFF);
      delay(250);
    }
    delay(1000);
  }
}

// GREEN - OFF Flashlight - Indicate a successfull test
void testOK() {
  setupLED();
  while (true) {
    digitalWrite(green, ON);
    delay(850);
    digitalWrite(green, OFF);
    delay(250);
  }
}

// RED-GREEN Flashlight - Indicate a successfull test for the "smaller" Variant of this Pin Config
void smallOK() {
  setupLED();
  while (true) {
    digitalWrite(green, ON);
    digitalWrite(red, OFF);
    delay(850);
    digitalWrite(green, OFF);
    digitalWrite(red, ON);
    delay(150);
  }
}

// Indicate a Problem with the DipSwitch Config (Continuous Red Blink)
void ConfigFail() {
  setupLED();
  while (true) {
    digitalWrite(red, ON);
    delay(250);
    digitalWrite(red, OFF);
    delay(250);
  }
}

// This is the initial Test for soldering Problems
// Switch all DIP Switches to 0
// The LED will be green for 1 sec and red for 1 sec to test LED function. If first the Red and then the Green lites up, write 0x00 to Position 0x01 of the EEPROM.
// All Inputs will become PullUP
// One by One short the Inputs to GND which checks connection to GND. If Green LED comes on one Pin Grounded was detected, RED if it was more than one
// If Green does not lite then this contact has a problem.
// To Quit Test Mode forever set all DIP Switches to ON and Short Pin 1 to Ground for 5 Times until the Green LED is steady on. This indicates the EEPROM stored the Information.
void buildTest() {
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
  if ((digitalRead(19) && digitalRead(3) && digitalRead(2)) == true) {
    EEPROM.update(TESTING, 0x00);
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
        if (i==13)
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
