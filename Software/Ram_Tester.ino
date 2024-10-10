// RAM Tester Sketch for the RAM Tester PCB
// ========================================
//
// Author : Andreas Hoffmann
// Version: 1.0
// Date   : 15.9.2024
//
// This Software is published under GPL 3.0 - Respect the License
// This Project is hosted at: https://github.com/tops4u/Ram-Tester/tree/main
//
// Fit a DRAM Test Candidate in the Board. Either by using the DIP/ZIF Socket (allign towards the LED) or by
// using the ZIP Adapter (Pin 1 of Adapter & ZIP Chip towards LED).
// Upon Connection to Power the Programm will start.
// It will first check the Configuration of the Boards DIP Switch (20, 18 or 16 Pin RAM).
// If the config is valid, it will check for ShortCircuit to GND on all Pins (except Vcc and 15 - 20Pin Config)
// Once completed and no Ground Short was found it will start filling all Columns of one Row with given Patterns
// and check them. This has the advantage that the RAM does not need to be refreshed.
// To check another Chip, cycle Power and restart procedure as described above.
//
// Currently a 256k*4 DRAM Chip takes around 2 Sec to Test.
//
// Error Blink Codes:
// Continuous Red Blinking 	- Configuration Error, check DIP Switches. Occasionally a RAM Defect may cause this.
// 1 Red & n Green		- Short to Ground. Count green Blinks = Pin on the DIP Socket
// 2 Red & n Green		- Error during RAM Test. Green Blinks indicate which Test Pattern failes (see below).
// Continuous Green Blinking	- Passed RAM TEst
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

// The Testpatterns to be used for 4Bit Databus RAM
const uint8_t pattern4[] = { 0x00, 0x0F, 0x0A, 0x03 };  // Equals to 0b0000, 0b1111, 0b1010, 0b0101

// Mapping for 4164 (2ms Refresh Rate) / 41256/257 (4 ms Refresh Rate)
// A0 = PD0   RAS = PC4   t RAS->CAS = 150-200ns -> Max Pulsewidth 10'000ns
// A1 = PD2   CAS = PC3   t CAS->dOut= 75 -100ns -> Max Pulsewidth 10'000ns
// A2 = PD1   WE  = PB3
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
const int RAS_16PIN = 18;  // Digital Out 18 / A4 on Arduino Uno is used for RAS

// Mapping for 4461 / 4464 - max Refresh 4ms
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

uint8_t Mode = 0;  // PinMode 2 = 16 Pin, 4 = 18 Pin, 5 = 20 Pin

void setup() {
  // Data Direction Register Port B, C & D - Preconfig as Input (Bit=0)
  DDRB &= 0b11100000;
  DDRC &= 0b11000000;
  DDRD = 0x0;
  // Wait for the Candidate to properly Startup
  delay(100);
  if (digitalRead(19) == 1) { Mode += Mode_20Pin; }
  if (digitalRead(3) == 1) { Mode += Mode_18Pin; }
  if (digitalRead(2) == 1) { Mode += Mode_16Pin; }
  // Check if the DIP Switch is set for a valid Configuration.
  if (Mode < 2 || Mode > 5) ConfigFail();
  // With a valid Config, activate the PullUps
  //PORTB |= 0b00011111;
  //PORTC |= 0b00111111;
  //PORTD = 0xFF;
  // Settle State - PullUps my require some time.
  delay(10);
  //checkGNDShort();  // Check for Shorts towards GND. Shorts on Vcc can't be tested as it would need Pull-Downs.
  delay(100);
  if (Mode == Mode_20Pin) {
    initRAM(RAS_20PIN);
    test20Pin();
  }
  setupLED();
}

// IF we reach Loop, then all Tests were successful.
void loop() {
}

// All RAM Chips require 8 RAS Cycles for proper initailization
void initRAM(int RASPin) {
  pinMode(RASPin, OUTPUT);
  // RAS is an Active LOW Signal
  digitalWrite(RASPin, HIGH);
  for (int i = 0; i < 8; i++) {
    digitalWrite(RASPin, LOW);
    digitalWrite(RASPin, HIGH);
  }
}

void test20Pin() {
  // Configure I/O for this Chip Type
  PORTB = 0b00011111;
  PORTC = 0b10000000;
  PORTD = 0x00;
  DDRB = 0b00011111;
  DDRC = 0b00011111;
  DDRD = 0xFF;
  for (uint8_t pat = 0; pat < 4; pat++)         // Check all 4Bit Patterns
    for (uint16_t row = 0; row < 512; row++) {  // Iterate over all ROWs
      write20PinRow(row, pat, 2);
    }
  // If all went smooth we checked 128kB of RAM now. Let's see if this is a 1Mx4 Chip
  if (Sense1Mx4 == true) {
    // Run the Tests for the larger Chip  again. The lower 512 Rows and Cols are rechecked, resulting in a 25% longer Test than really needed.
    // This could be optimized.
    for (uint8_t pat = 0; pat < 4; pat++) {        // Check all 4Bit Patterns
      for (uint16_t row = 0; row < 1024; row++) {  // Iterate over all ROWs
        write20PinRow(row, pat, 1024);
      }
    }
    testOK();
  } else {
    // Indicate with Green-Red flashlight that the "small" Version has been checked ok
    smallOK();
  }
}

// Prepare and execute ROW Access for 20 Pin Types
void RASHandlingPin20(uint16_t row) {
  PORTB |= (1 << PB1);          // Set RAS High - Inactive
  msbHandlingPin20(row / 256);  // Preset ROW Adress
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
  PORTB &= ~0x10;
  if (address & 0x01)
    PORTB |= 0x10;  // Set Bit 8 if required by the address
  PORTC &= ~0x10;
  if (address & 0x02)
    PORTC |= 0x10;
}

// Write and Read (&Check) Pattern from Cols
void CASHandlingPin20(uint16_t row, uint8_t patNr, uint16_t colWidth) {
  for (uint8_t msb = 0; msb < colWidth; msb++) {
    // Prepare Write Cycle
    PORTC &= 0xF0;          // Set all Outputs to LOW
    DDRC |= 0x0F;           // Configure IOs for Output
    RASHandlingPin20(row);  // Set the Row
    msbHandlingPin20(msb);  // Set the MSB as needed
    PORTB &= ~(1 << PB3);   // Set WE Low - Active
    PORTC |= (pattern4[patNr] & 0x0F);
    // Iterate over 255 Columns and write the Pattern
    for (uint16_t col = 0; col <= 255; col++) {
      PORTD = (uint8_t)col;  // Set Col Adress
      PORTB &= ~1;           // CAS Latch Strobe
      PORTB |= 1;            // CAS High - Cycle Time ~120ns
    }
    // Prepare Read Cycle
    PORTB |= (1 << PB3);  // Set WE High - Inactive
    PORTC &= 0xF0;
    DDRC &= 0xF0;  // Configure IOs for Input
    //PORTC |= 0x0F;  // Use PullUps to be sure no residue Charge gives False-Positives
    //RASHandlingPin20(row);       // Set Row as we changed from Write -> Read
    //msbHandlingPin20(msb << 8);  // Set the MSB as needed
    PORTB &= ~(1 << PB2);  // Set OE Low - Active
    // Iterate over 255 Columns and read & check Pattern
    for (uint16_t col = 0; col <= 255; col++) {
      PORTD = (uint8_t)col;  // Set Col Adress
      PORTB &= ~1;
      NOP; // Input Settle Time for Digital Inputs = 93ns
      NOP; // One NOP@16MHz = 62.5ns
      if ((PINC & 0x0F) != pattern4[patNr]) {
        PORTB |= 1;
        interrupts();
        error(patNr + 1, 2);
      }  // Check if Pattern matches
      PORTB |= 1;
    }
    PORTB |= (1 << PB2);  // Set OE High - Inactive
  }
}

boolean Sense1Mx4() {
  PORTB |= (1 << PB1);   // Set RAS High - Inactive
  PORTD = 0x00;          // Set Row and Col address to 0
  PORTB &= ~PORTB4;      // Clear address Bit 8
  PORTC &= 0xE0;         // Set all Outputs and A9 to LOW
  DDRC |= 0x0F;          // Configure IOs for Output
  PORTB &= ~(1 << PB1);  // RAS Latch Strobe
  PORTB &= ~(1 << PB3);  // Set WE Low - Active
  PORTB &= ~1;           // CAS Latch Strobe
  PORTB |= 1;            // CAS High - Cycle Time ~120ns -> Write 0000 to Row 0, Col 0
  PORTC |= 0x1F;         // Set all Outputs and A9 High
  PORTB &= ~1;           // CAS Latch Strobe
  PORTB |= 1;            // CAS High - Cycle Time ~120ns -> Write 1111 to Row 0, Col 512
  PORTB |= (1 << PB3);   // Set WE High - Inactive
  PORTB |= (1 << PB1);   // Set RAS High - Inactive
  DDRC &= 0xE0;          // Configure IOs for Input and clear A9 for Row access
  PORTC |= 0x0F;         // Use PullUps to be sure no residue Charge gives False-Positives
  PORTB &= ~(1 << PB1);  // RAS Latch Strobe
  PORTB &= ~(1 << PB2);  // Set OE Low - Active
  PORTB &= ~1;           // CAS Latch Strobe
  NOP;
  NOP;
  uint8_t val = (PINC & 0xF);  // Read the Data at this address
  PORTB |= 1;                  // CAS High - Cycle Time ~120ns
  PORTB |= (1 << PB1);         // Set RAS High - Inactive
  PORTB |= (1 << PB2);         // Set OE High - Inactive
  if (val == 0x00)             // If we read 0b0000 col 0 was not overwritten
    return true;               // In this Case A9 was used and it is a 1M x 4 RAM
  else
    return false;  // If A9 was not used we find 1111 in the Position and the Pin is not used
}

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

void testOK() {
  setupLED();
  while (true) {
    digitalWrite(LED_G, 1);
    delay(500);
    digitalWrite(LED_G, 0);
    delay(500);
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
