
#define Mode_16Pin 2
#define Mode_18Pin 4
#define Mode_20Pin 5
#define EOL 0

// So wissen wir welche Pin als Vcc genutzt wird
const char VccPin[] = { 0, 0, 8, 0, 9, 10 };

const uint8_t pattern4[] = { 0x0, 0xF, 0xA, 0x3 };  // Equals to 0b0000, 0b1111, 0b1010, 0b0101

// Pin Zuordnung der Ports zum RAM Sockel - EOL indicates End Of List
const char CPU_20PORTB[] = { 17, 4, 16, 3, 15, EOL };
const char CPU_20PORTC[] = { 1, 2, 18, 19, 5, 10, EOL };
const char CPU_20PORTD[] = { 6, 7, 8, 9, 11, 12, 13, 14, EOL };
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
  if (digitalRead(PC5) == 1) { Mode += Mode_20Pin; }
  if (digitalRead(PD3) == 1) { Mode += Mode_18Pin; }
  if (digitalRead(PD2) == 1) { Mode += Mode_16Pin; }
  // Check if the DIP Switch is set for a valid Configuration.
  if (Mode < 2 || Mode > 5) ConfigFail();
  // With a valid Config, activate the PullUps
  PORTB |= 0b00011111;
  PORTC |= 0b00111111;
  PORTD = 0xF;
  // Settle State - PullUps my require some time. 
  delay(50);
  checkGNDShort();  // Check for Shorts towards GND. Shorts on Vcc can't be tested as it would need Pull-Downs.
  PORTB &= 0b11100000;
  PORTC &= 0b11000000;
  PORTD &= 0x0;
  DDRB |= 0b00011111;
  DDRC |= 0b00111111;
  DDRD = 0xF;
  delay(10);
  if (Mode == Mode_20Pin) {
    test20Pin(Sense1Mx4());
  }
  pinMode(PB4, OUTPUT);
  pinMode(PB5, OUTPUT);
  digitalWrite(PB5, 0); // Switch RED Led off. 
}

// IF we reach Loop, then all Tests were successful.
void loop() {
  digitalWrite(PB4, 1);
  delay(500);
  digitalWrite(PB4, 0);
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
      PORTB &= ~PB0;              // CAS Latch Strobe
      PORTB |= PB0;               // CAS High - Cycle Time ~120ns
    }
    interrupts();
    // Prepare Read Cycle
    PORTB |= 1 << PB3;            // Set WE High - Inactive
    DDRC &= 0xF0;                 // Configure IOs for Input
    PORTC &= 0xF0;                // High Impedance Input (no PullUp)
    RASHandlingPin20(row);        // Set Row as we changed from Write -> Read
    msbHandlingPin20(msb << 8);   // Set the MSB as needed
    PORTB &= ~(1 << PB2);         // Set OE Low - Active
    // Iterate over 255 Columns and read & check Pattern
    noInterrupts();               // Let's not get disturbed while reading from RAM
    for (uint16_t col = 0; col <= 255; col++) {
      PORTB &= ~PB0;                                               // CAS Latch Strobe
      if (PINC & 0xF != pattern4[patNr]) { interrupts(); error(patNr + 1, 3); }  // Check if Pattern matches
      PORTB |= PB0;                                                // CAS High - Cycle Time ~120ns
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
  return false;  // YET TO IMPLEMENT
}

void checkGNDShort() {
  if (Mode == Mode_20Pin) {
    for (int i = 0; i < 5; i++) {
      int8_t mask = 1 << i;
      if (PINB & mask > 1) {
        error(CPU_20PORTB[i], 1);
      }
      if (PINC & mask > 1) {
        error(CPU_20PORTC[i], 1);
      }
    }
    for (int i = 0; i < 8; i++) {
      if (PIND & 1 << i > 1) {
        error(CPU_20PORTD[i], 1);
      }
    }
  }
}

// Output the GND Short Pin NR via LED (1 x Red + PinNr x Green)
void error(uint8_t code, uint8_t error) {
  pinMode(PB4, OUTPUT);
  pinMode(PB5, OUTPUT);
  digitalWrite(PB4, LOW);
  digitalWrite(PB5, LOW);
  while (true) {
    for (int i = 0; i < code; i++) {
      digitalWrite(PB5, 1);
      delay(500);
      digitalWrite(PB5, 0);
      delay(500);
    }
    for (int i = 0; i < error; i++) {
      digitalWrite(PB4, 1);
      delay(250);
      digitalWrite(PB4, 0);
      delay(250);
    }
    delay(1000);
  }
}

// Indicate a Problem with the DipSwitch (Continuous Red Blink)
void ConfigFail() {
  pinMode(PB5, OUTPUT);
  while (true) {
    digitalWrite(PB5, 1);
    delay(250);
    digitalWrite(PB5, 0);
    delay(250);
  }
}