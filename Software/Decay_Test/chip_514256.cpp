#line 1 "C:\\Users\\Andreas\\Documents\\DRAM_Decay_Test\\chip_514256.cpp"
// chip_514256.cpp - 20-pin x4 DRAM driver (514256/514258, and same-socket 514400/514402)
//=======================================================================================
//
// Third driver for the DramDriver interface (chip.h). Pinout is the 20-pin socket of
// the RAM-Tester PCB, identical to 20Pin.cpp of Ram_Tester 5.1.2:
//
//   A0..A7 = PD0..PD7   A8 = PB4   A9 = PC4 (1Mx4 only, NC on the 256Kx4)
//   IO0..IO3 = PC0..PC3
//   CAS = PB0  RAS = PB1  OE = PB2  WE = PB3
//   PB5 = red LED / OLED SCL (parked LOW)   PC5 = 20-pin DIP sense (input)
//   No free pin on this socket -> no LA markers (rows are visible as RAS bursts).
//
// The static-column variants (514258/514402) behave exactly like the page-mode parts
// in single-cycle access, so one driver covers all four. The 4116/4027 adapter of the
// tester is NOT supported (different pinout, -5 V / +12 V rails).
//
// Access cycle (single cycle, one RAS per cell, early write; /WE is HIGH at every RAS
// fall -- write-per-bit parts latch a mask otherwise, see Ram_Tester 5.0.9):
//
//   RAS  ~~~~\_____________________/~~~~~~     tRAS ~0.9 us (spec 60-80 ns .. 10 us)
//   WE   ~~~~~~\_________________/~~~~~~~~     write only; low BEFORE CAS -> early write
//   CAS  ~~~~~~~~~~~~~~~\_____/~~~~~~~~~~~     tCAS 250 ns write / 440 ns read
//   OE   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~     write rows; LOW for the whole read row
//   ADDR ==row====><====column=========>       column low byte = PORTD directly
//   IO   ------------<==data (write)====>       latched at CAS fall
//   IO   -------------------<valid>-----        read: sampled ~250-310 ns after CAS fall
//
// No lookup table: A0..A7 ARE PORTD, so the column phase is a register increment. Both
// loops run at 30 cycles = 1.875 us per cell; a 512-column row takes ~1 ms, a 1024-column
// row ~2 ms (interrupts are let through every 32 cells), the 514256 array 0.5 s, the
// 514400 array 2 s.
//
//=======================================================================================

#include "chip.h"

#define CAS_LOW  CBI(PORTB, 0)
#define CAS_HIGH SBI(PORTB, 0)
#define RAS_LOW  CBI(PORTB, 1)
#define RAS_HIGH SBI(PORTB, 1)
#define OE_LOW   CBI(PORTB, 2)
#define OE_HIGH  SBI(PORTB, 2)
#define WE_LOW   CBI(PORTB, 3)
#define WE_HIGH  SBI(PORTB, 3)

// PORTB control images (bit0 CAS, bit1 RAS, bit2 OE, bit3 WE, bit4 A8, bit5 LED)
#define PB_IDLE_W  0x0F                      // all strobes HIGH
#define PB_IDLE_R  0x0B                      // OE LOW
#define PB_COL_W   0x05                      // RAS LOW, WE LOW, CAS HIGH, OE HIGH
#define PB_COL_R   0x09                      // RAS LOW, OE LOW, CAS HIGH, WE HIGH
#define A8_BIT(a)  ((uint8_t)(((a) & 0x100) >> 4))   // -> PB4
#define A9_BIT(a)  ((uint8_t)(((a) & 0x200) >> 5))   // -> PC4

static const char name_514256[] PROGMEM = "514256/8 256Kx4";
static const char name_514400[] PROGMEM = "514400/2 1Mx4";

//=======================================================================================
// PORT SETUP
//=======================================================================================

static void setupPorts20(void) {
  DDRB = 0b00111111;                         // CAS RAS OE WE A8 + PB5 outputs
  PORTB = PB_IDLE_W;                         // strobes HIGH, A8 = 0, LED/SCL LOW
  DDRC = 0b00011111;                         // IO0..3 + A9 outputs; PC5 input
  PORTC = 0x00;
  DDRD = 0xFF;                               // A0..A7
  PORTD = 0x00;
}

//=======================================================================================
// SINGLE-CELL ACCESS (detection only)
//=======================================================================================

static void setAddr(uint16_t a) {
  PORTD = (uint8_t)a;
  PORTB = (PORTB & 0xEF) | A8_BIT(a);
  PORTC = (PORTC & 0xEF) | A9_BIT(a);
}

static void cellWrite(uint16_t row, uint16_t col, uint8_t nib) {
  DDRC |= 0x0F;                              // IO out
  OE_HIGH;
  cli();
  setAddr(row);
  RAS_LOW;
  WE_LOW;
  setAddr(col);
  PORTC = (PORTC & 0xF0) | (nib & 0x0F);
  CAS_LOW; NOP; NOP; CAS_HIGH;
  WE_HIGH;
  RAS_HIGH;
  sei();
}

static uint8_t cellRead(uint16_t row, uint16_t col) {
  DDRC &= 0xF0;                              // IO in
  PORTC |= 0x0F;                             // pull-ups: an empty socket reads 0xF
  cli();
  setAddr(row);
  RAS_LOW;
  setAddr(col);
  OE_LOW;
  CAS_LOW; NOP; NOP; NOP; NOP;
  uint8_t got = PINC & 0x0F;
  CAS_HIGH;
  OE_HIGH;
  RAS_HIGH;
  sei();
  PORTC &= 0xF0;
  return got;
}

//=======================================================================================
// DETECTION
//=======================================================================================

// Presence: a nibble must come back as written (0x5, then 0xA). Geometry: row 512
// distinct from row 0 -> A9 decoded -> 1024 x 1024 (514400/514402), else 512 x 512.
static bool detect20(void) {
  delayMicroseconds(250);
  for (uint8_t i = 0; i < 8; i++) { RAS_LOW; NOP; NOP; RAS_HIGH; NOP; NOP; }

  cellWrite(0, 0, 0x5);
  if (cellRead(0, 0) != 0x5) {
    cellWrite(0, 0, 0xA);
    if (cellRead(0, 0) != 0xA) return false;
  }
  cellWrite(0, 0, 0x5);
  cellWrite(512, 0, 0xA);
  if (cellRead(0, 0) == 0x5) { g_dram.rows = g_dram.cols = 1024; g_dram.name = name_514400; g_dram.specUs = 16000; }
  else                       { g_dram.rows = g_dram.cols = 512;  g_dram.name = name_514256; g_dram.specUs = 8000; }
  return true;
}

//=======================================================================================
// ROW WRITE / READ (the paced passes; called with interrupts disabled)
//=======================================================================================
//
// Same structure as chip_4164.cpp (see there for why this is assembly): 32-cell chunk,
// pattern byte reloaded every 8 cells, both loops 30 cycles per cell.
//
// Per cell (write):                            Per cell (read):
//   mov        PORTC base (A9)      1            --
//   sbrc/ori   data on 4 lines      2            --
//   lsr        next bit             1            --
//   out x3     row address          3            out x3   (OE low)              3
//   cbi RAS                         2            cbi RAS                        2
//   cbi WE     early write          2            nop (tRAH)                     1
//   out x3     column + data        3            out x3   column                3
//   cbi CAS, nop, nop, sbi CAS      6            cbi CAS, nop x4                6
//   --                                           in PINC                        1
//   --                                           sbi CAS                        2
//   sbi WE                          2            --
//   sbi RAS                         2            sbi RAS                        2
//   inc        next column          1            andi,sbrc,eor  compare         3
//   nop x2     (pad)                2            breq (ok path)                 2
//   --                                           lsr, inc                       2
//   dec/brne                        3            dec/brne                       3
//                                  30                                          30

// One 32-cell write chunk: cd = PORTD image of the first column, hb/hc = A8/A9 chunk bits.
static inline void writeChunk20(uint8_t rd, uint8_t rb, uint8_t rc, uint8_t cd, uint8_t hb, uint8_t hc,
                                const uint8_t *&bits) {
  uint8_t cc, b, n, k;
  __asm__ __volatile__(
    "ldi  %[k], 4                  \n\t"
    "1:                            \n\t"
    "ld   %[b], X+                 \n\t"
    "ldi  %[n], 8                  \n\t"
    "2:                            \n\t"
    "mov  %[cc], %[hc]             \n\t"   // A9, data 0
    "sbrc %[b], 0                  \n\t"
    "ori  %[cc], 0x0F              \n\t"   // IO0..3
    "lsr  %[b]                     \n\t"
    "out  %[portd], %[rd]          \n\t"   // row address, strobes HIGH
    "out  %[portb], %[rb]          \n\t"
    "out  %[portc], %[rc]          \n\t"
    "cbi  %[portb], 1              \n\t"   // RAS low (WE still high)
    "cbi  %[portb], 3              \n\t"   // WE low: early write
    "out  %[portd], %[cd]          \n\t"   // column + data
    "out  %[portb], %[hb]          \n\t"
    "out  %[portc], %[cc]          \n\t"
    "cbi  %[portb], 0              \n\t"   // CAS low
    "nop                           \n\t"
    "nop                           \n\t"
    "sbi  %[portb], 0              \n\t"   // CAS high
    "sbi  %[portb], 3              \n\t"   // WE high
    "sbi  %[portb], 1              \n\t"   // RAS high
    "inc  %[cd]                    \n\t"
    "nop                           \n\t"   // pad: match the read cell (30 cycles)
    "nop                           \n\t"
    "dec  %[n]                     \n\t"
    "brne 2b                       \n\t"
    "dec  %[k]                     \n\t"
    "brne 1b                       \n\t"
    : [cc] "=&d"(cc), [b] "=&r"(b), [n] "=&d"(n), [k] "=&d"(k), [cd] "+r"(cd), "+x"(bits)
    : [rd] "r"(rd), [rb] "r"(rb), [rc] "r"(rc), [hb] "r"(hb), [hc] "r"(hc),
      [portb] "I"(_SFR_IO_ADDR(PORTB)), [portc] "I"(_SFR_IO_ADDR(PORTC)), [portd] "I"(_SFR_IO_ADDR(PORTD))
    : "memory", "cc");
}

// One 32-cell read chunk (OE low throughout).
static inline void readChunk20(uint8_t rd, uint8_t rb, uint8_t rc, uint8_t cd, uint8_t hb, uint8_t hc,
                               const uint8_t *&bits, uint8_t &errs, uint8_t &errK, uint8_t &errN) {
  uint8_t b, n, k, x;
  __asm__ __volatile__(
    "ldi  %[k], 4                  \n\t"
    "1:                            \n\t"
    "ld   %[b], X+                 \n\t"
    "ldi  %[n], 8                  \n\t"
    "2:                            \n\t"
    "out  %[portd], %[rd]          \n\t"   // row address, OE low, others HIGH
    "out  %[portb], %[rb]          \n\t"
    "out  %[portc], %[rc]          \n\t"
    "cbi  %[portb], 1              \n\t"   // RAS low
    "nop                           \n\t"   // tRAH
    "out  %[portd], %[cd]          \n\t"   // column
    "out  %[portb], %[hb]          \n\t"
    "out  %[portc], %[hc]          \n\t"
    "cbi  %[portb], 0              \n\t"   // CAS low
    "nop                           \n\t"
    "nop                           \n\t"
    "nop                           \n\t"
    "nop                           \n\t"
    "in   %[x], %[pinc]            \n\t"   // IO0..3 ~250-310 ns after the CAS fall
    "sbi  %[portb], 0              \n\t"   // CAS high
    "sbi  %[portb], 1              \n\t"   // RAS high
    "andi %[x], 0x0F               \n\t"
    "sbrc %[b], 0                  \n\t"
    "eor  %[x], %[kf]              \n\t"   // expected 1 -> 0x0F ^ 0x0F = 0
    "breq 3f                       \n\t"
    "cpi  %[errs], 0               \n\t"
    "brne 4f                       \n\t"
    "mov  %[errK], %[k]            \n\t"
    "mov  %[errN], %[n]            \n\t"
    "4:                            \n\t"
    "cpi  %[errs], 255             \n\t"
    "breq 3f                       \n\t"
    "inc  %[errs]                  \n\t"
    "3:                            \n\t"
    "lsr  %[b]                     \n\t"
    "inc  %[cd]                    \n\t"
    "dec  %[n]                     \n\t"
    "brne 2b                       \n\t"
    "dec  %[k]                     \n\t"
    "brne 1b                       \n\t"
    : [b] "=&r"(b), [n] "=&d"(n), [k] "=&d"(k), [x] "=&d"(x), [cd] "+r"(cd),
      [errs] "+d"(errs), [errK] "+r"(errK), [errN] "+r"(errN), "+x"(bits)
    : [rd] "r"(rd), [rb] "r"(rb), [rc] "r"(rc), [hb] "r"(hb), [hc] "r"(hc), [kf] "r"((uint8_t)0x0F),
      [portb] "I"(_SFR_IO_ADDR(PORTB)), [portc] "I"(_SFR_IO_ADDR(PORTC)), [portd] "I"(_SFR_IO_ADDR(PORTD)),
      [pinc] "I"(_SFR_IO_ADDR(PINC))
    : "memory", "cc");
}

static void __attribute__((hot)) writeRow20(uint16_t row, const uint8_t *bits) {
  const uint8_t rd = (uint8_t)row, rb = PB_IDLE_W | A8_BIT(row), rc = A9_BIT(row);
  const uint8_t nhi = (uint8_t)(g_dram.cols >> 5);
  DDRC |= 0x0F;                              // IO out
  for (uint8_t h = 0; h < nhi; h++) {
    const uint16_t c0 = (uint16_t)h << 5;
    writeChunk20(rd, rb, rc, (uint8_t)c0, PB_COL_W | A8_BIT(c0), A9_BIT(c0), bits);
    sei(); NOP; cli();
  }
}

static uint16_t __attribute__((hot)) readRow20(uint16_t row, const uint8_t *bits, uint16_t *firstBadCol) {
  const uint8_t rd = (uint8_t)row, rb = PB_IDLE_R | A8_BIT(row), rc = A9_BIT(row);
  const uint8_t nhi = (uint8_t)(g_dram.cols >> 5);
  uint8_t errs = 0, errK = 0, errN = 0, errH = 0;
  DDRC &= 0xF0;                              // IO in, no pull-ups
  PORTC &= 0xF0;
  for (uint8_t h = 0; h < nhi; h++) {
    const uint16_t c0 = (uint16_t)h << 5;
    const uint8_t before = errs;
    readChunk20(rd, rb, rc, (uint8_t)c0, PB_COL_R | A8_BIT(c0), A9_BIT(c0), bits, errs, errK, errN);
    if (!before && errs) errH = h;
    sei(); NOP; cli();
  }
  PORTB = PB_IDLE_W;                         // OE back to HIGH
  if (errs) *firstBadCol = (uint16_t)errH * 32 + (4 - errK) * 8 + (8 - errN);
  return errs;
}

//=======================================================================================
// BINDER
//=======================================================================================

void bind_20pin_x4(void) {
  g_dram.name = name_514256;
  g_dram.rows = 512;
  g_dram.cols = 512;
  g_dram.specUs = 8000;
  g_dram.markRow = 0;                        // every pin of this socket is in use
  g_dram.markBusy = 0;
  g_dram.setupPorts = setupPorts20;
  g_dram.detect = detect20;
  g_dram.writeRow = writeRow20;
  g_dram.readRow = readRow20;
}
