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

#define Mode_16Pin 2
#define Mode_18Pin 4
#define Mode_20Pin 5
#define EOL 0
#define LED_R 13
#define LED_G 12

// The Testpatterns to be used for 4Bit Databus RAM
const uint8_t pattern4[] = { 0x00, 0x0F, 0x0A, 0x03 };  // Equals to 0b0000, 0b1111, 0b1010, 0b0101

// PORT to Ram Socket Pin mapping
const char CPU_20PORTB[] = { 17, 4, 16, 3, EOL, EOL, EOL, EOL }; // Position 4 would be A8 but the LED is attached to PB4 as well
const char CPU_20PORTC[] = { 1, 2, 18, 19, 5, 10, EOL, EOL };
const char CPU_20PORTD[] = { 6, 7, 8, 9, 11, 12, 13, 14, EOL };
// Nice to Know special Pins
enum RAM_20_IO { CAS_20 = 8,
                 RAS_20 = 9,
                 OE_20 = 10,
                 WE_20 = 11,
                 IO1_20 = 14,
                 IO2_20 = 15,
                 IO3_20 = 16,
                 IO4_20 = 17,
                 MSB1_20 = 12,
                 MSB2_20 = 18 };

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
  PORTB |= 0b00011111;
  PORTC |= 0b00111111;
  PORTD = 0xFF;
  // Settle State - PullUps my require some time. 
  delay(50);  
  checkGNDShort();  // Check for Shorts towards GND. Shorts on Vcc can't be tested as it would need Pull-Downs.
  PORTB &= 0b11100000;
  PORTC &= 0b11000000;
  PORTD &= 0x0;
  DDRB |= 0b00011111;
  DDRC |= 0b00111111;
  DDRD = 0xFF;
  delay(100);
  if (Mode == Mode_20Pin) {
   test20Pin(Sense1Mx4());
  }
  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);
  digitalWrite(LED_R, 0); // Switch RED Led off. 
  digitalWrite(LED_G, 0);
}

// IF we reach Loop, then all Tests were successful.
void loop() {
  digitalWrite(LED_G, 1);
  delay(500);
  digitalWrite(LED_G, 0);
  delay(500);
}

void test20Pin(boolean bigMode) {
  uint16_t width = bigMode ? 1024 : 512;          // If 1M Chip it has 1024 Row/Col / 512 otherwise
  PORTB |= 0xF;                                   // Set all RAM Control Pins to High
  for (uint8_t pat = 0; pat < 4; pat++)           // Check all 4Bit Patterns
    for (uint16_t row = 0; row < width; row++) {  // Iterate over all ROWs
      write20PinRow(row, pat, width);
    }
}

void msbHandlingPin20(uint16_t address) {
  PORTB &= ~PORTB4;                      // Clear address Bit 8
  PORTB |= PORTB4 && (address & 0x100);  // Set Bit 8 if required by the address
  PORTC &= ~PORTC4;                      // Clear address Bit 9
  PORTC |= PORTC4 && (address & 0x200);  // Address Bit 9 if 1Mx4 RAM - Set if needed
  PORTD = address & 0xFF;                // Cut off above Bit 7
}

// Write and Read (&Check) Pattern from Cols
void CASHandlingPin20(int8_t patNr, uint16_t row, uint16_t colWidth) {
  for (int msb = 0; msb < colWidth >> 8; msb++) {
    // Prepare Write Cycle
    PORTC &= 0xF0;                // Set all Outputs to LOW
    DDRC |= 0x0F;                 // Configure IOs for Output
    RASHandlingPin20(row);        // Set the Row 
    msbHandlingPin20(msb << 8);   // Set the MSB as needed
    PORTB &= ~(1 << PB3);         // Set WE Low - Active
    PORTC |= pattern4[patNr];
    // Iterate over 255 Columns and write the Pattern
    noInterrupts();               // Let's not get disturbed while writing to the RAM
    for (uint16_t col = 0; col <= 255; col++) {
      PORTD = col;                // Set Col Adress
      PORTB &= ~1;                // CAS Latch Strobe
      PORTB |= 1;                 // CAS High - Cycle Time ~120ns
    }
    interrupts();
    // Prepare Read Cycle
    PORTB |= 1 << PB3;            // Set WE High - Inactive
    DDRC &= 0xF0;                 // Configure IOs for Input
    PORTC |= 0x0F;                // Use PullUps to be sure no residue Charge gives False-Positives
    RASHandlingPin20(row);        // Set Row as we changed from Write -> Read
    msbHandlingPin20(msb << 8);   // Set the MSB as needed
    PORTB &= ~(1 << PB2);         // Set OE Low - Active
    // Iterate over 255 Columns and read & check Pattern
    noInterrupts();               // Let's not get disturbed while reading from RAM
    for (uint16_t col = 0; col <= 255; col++) {
      PORTD = col;                // Set Col Adress
      PORTB &= ~1;
      if ((PINC & 0xF) != pattern4[patNr]) { interrupts(); error(patNr + 1, 2); }  // Check if Pattern matches
      PORTB |= 1;         
    }
    interrupts();
    PORTB |= 1 << PB2;            // Set OE High - Inactive
  }
}

void RASHandlingPin20(uint16_t row) {
  PORTB |= 1 << PB1;      // Set RAS High - Inactive
  msbHandlingPin20(row);  // Preset ROW Adress
  PORTB &= ~(1 << PB1);   // RAS Latch Strobe
}

void write20PinRow(uint16_t row, char pattern, uint16_t width) {
  PORTB |= 0xF;                           // Set all RAM Controll Lines to HIGH = Inactive
  CASHandlingPin20(pattern, row, width);  // Do the Test
  PORTB |= 0xF;                           // Set all RAM Controll Lines to HIGH = Inactive
  // Enhanced PageMode Row Write & Read Done
}

boolean Sense1Mx4() {
  // Implement Check that writes different Patterns on ROW 0 / COL 511 / 1023 respectively. If identical it is an 9Bit(256k*4) Adressbus otherwise a 10Bit (1M*4). 
  return false;  // YET TO IMPLEMENT
}

// Check for Shorts to GND, when Inputs are Pullup
void checkGNDShort() {
  if (Mode == Mode_20Pin) {
    for (int i = 0; i <= 7; i++) {
      int8_t mask = 1<<i;
      if (CPU_20PORTB[i]!=EOL && ((PINB & mask)==0)) {
        error(CPU_20PORTB[i], 1);
      }
      if (CPU_20PORTC[i]!=EOL && ((PINC & mask)==0)) {
        error(CPU_20PORTC[i], 1);
      }
      if (CPU_20PORTD[i]!=EOL && ((PIND & mask)==0)) {
        error(CPU_20PORTD[i], 1);
      }
    }
  }
}

// Indicate Errors. Red LED for Error Type, and green for additional Error Info.
void error(uint8_t code, uint8_t error) {
  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_R, LOW);
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

// Indicate a Problem with the DipSwitch Config (Continuous Red Blink)
void ConfigFail() {
  pinMode(LED_R, OUTPUT);
  while (true) {
    digitalWrite(LED_R, 1);
    delay(250);
    digitalWrite(LED_R, 0);
    delay(250);
  }
}
