#line 1 "C:\\Users\\Andreas\\Documents\\DRAM_Decay_Test\\chip_4464.cpp"
// chip_4464.cpp - 18-pin x4 DRAM driver (4464, and same-socket 4416)
//=======================================================================================
//
// Second driver for the DramDriver interface (chip.h). Pinout is the standard 18-pin
// socket of the RAM-Tester PCB, taken from the setAddr18Pin() assembly of Ram_Tester
// 5.1.2 (the authoritative source -- an older comment block there, and the DRAM Decay
// Test 0.1 prototype, had A4..A7 wrong):
//
//   A0 = PB2   A1 = PB4   A2 = PD7   A3 = PD6   A4 = PD2   A5 = PD1   A6 = PD0   A7 = PD5
//   D0 = PC1   D1 = PB0   D2 = PB3   D3 = PC3        (DQ1..DQ4 of the part)
//   RAS = PC4  CAS = PC2  WE = PB1   OE = PC0
//   PB5 = red LED / OLED SCL (parked LOW)   PC5 = 20-pin DIP sense (input)
//   PD3 = 18-pin DIP sense (input)          PD4 = NC on this socket -> ROW marker
//
// Access cycle (single cycle, one RAS per cell, early write, OE high while writing):
//
//   RAS  ~~~~\_____________________/~~~~~~     tRAS ~0.94 us (spec 100-150 ns .. 10 us)
//   WE   ~~~~~~\_________________/~~~~~~~~     write only; low BEFORE CAS -> early write
//   CAS  ~~~~~~~~~~~~~~~\_____/~~~~~~~~~~~     tCAS 250 ns write / 500 ns read
//   OE   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~     write rows; LOW for the whole read row
//   ADDR ==row====><====column=========>       tASR/tASC >= 1 cycle, tRAH >= 2 cycles
//   DQ   ------------<==data (write)====>       latched at CAS fall
//   DQ   -------------------<valid>-----        read: sampled 220-340 ns after CAS fall
//
// The one pattern bit of a column is written to all four data lines (0x0 / 0xF) and
// all four are verified -- see the xN note in chip.h.
//
// Every cell costs 37 cycles = 2.31 us (write and read cycle-matched, inline asm); a
// 4464 row (256 cells) ~0.6 ms, the whole array ~0.16 s; the 4416 (64 columns) ~0.16 ms
// per row. The 4416 puts its columns on A1..A6: the lookup tables are built with the
// column address shifted left by one, nothing else changes.
//
//=======================================================================================

#include "chip.h"

#define RAS_LOW  CBI(PORTC, 4)
#define RAS_HIGH SBI(PORTC, 4)
#define CAS_LOW  CBI(PORTC, 2)
#define CAS_HIGH SBI(PORTC, 2)
#define WE_LOW   CBI(PORTB, 1)
#define WE_HIGH  SBI(PORTB, 1)
#define OE_LOW   CBI(PORTC, 0)
#define OE_HIGH  SBI(PORTC, 0)

// 8-bit address -> port images (address bits only)
#define GET_PB(a) ((uint8_t)((((a) & 0x01) << 2) | (((a) & 0x02) << 3)))                                       // A0 A1
#define GET_PD(a) ((uint8_t)((((a) & 0x04) << 5) | (((a) & 0x08) << 3) | (((a) & 0x10) >> 2) | \
                             (((a) & 0x20) >> 4) | (((a) & 0x40) >> 6) | (((a) & 0x80) >> 2)))                 // A2..A7

// Port images. PORTB: WE HIGH, data 0, LED 0. PORTC: OE/CAS/RAS HIGH = 0x15 (write rows),
// OE LOW = 0x14 (read rows). PORTD: keep PD3 (DIP input) and PD4 (marker).
#define ROW_PB(r) (uint8_t)(0x02 | GET_PB(r))
#define ROW_PD(r) (uint8_t)((PORTD & 0x18) | GET_PD(r))
#define PC_IDLE_W 0x15
#define PC_IDLE_R 0x14
#define DATA_PB   0x09                       // D1 = PB0, D2 = PB3
#define DATA_PC   0x0A                       // D0 = PC1, D3 = PC3

struct Lut2 { uint8_t pb, pd; };
static Lut2 s_lo[32];   // column & 31   (address << shift)
static Lut2 s_hi[8];    // column >> 5   (shift 1 for the 4416: columns on A1..A6)

static const char name_4464[] PROGMEM = "4464 64Kx4";
static const char name_4416[] PROGMEM = "4416 16Kx4";

//=======================================================================================
// PORT SETUP
//=======================================================================================

static void buildLut(uint8_t shift) {
  for (uint8_t i = 0; i < 32; i++) {
    uint16_t a = (uint16_t)i << shift;
    s_lo[i].pb = GET_PB(a); s_lo[i].pd = GET_PD(a);
  }
  for (uint8_t i = 0; i < 8; i++) {
    uint16_t a = ((uint16_t)i << 5) << shift;
    s_hi[i].pb = GET_PB(a); s_hi[i].pd = GET_PD(a);
  }
}

static void setupPorts18(void) {
  DDRB = 0b00111111;                         // D1 WE A0 D2 A1 + PB5 outputs
  PORTB = 0b00000010;                        // WE HIGH, data 0, LED/SCL LOW
  DDRC = 0b00011111;                         // OE D0 CAS D3 RAS outputs; PC5 input
  PORTC = PC_IDLE_W;                         // OE = CAS = RAS HIGH
  DDRD = (DDRD & 0x10) | 0b11100111;         // A6 A5 A4 A7 A3 A2 outputs; PD3 input; PD4 as is
  PORTD &= 0x10;
#if LA_MARKERS
  DDRD |= 0x10;                              // PD4 = ROW marker (NC on this socket)
#endif
  static bool lutReady = false;
  if (!lutReady) { buildLut(0); lutReady = true; }
}

//=======================================================================================
// SINGLE-CELL ACCESS (detection only, not speed critical)
//=======================================================================================

static void setAddr(uint8_t a) {
  PORTB = (PORTB & 0xEB) | GET_PB(a);        // keep WE, data, PB5
  PORTD = (PORTD & 0x18) | GET_PD(a);
}

static void setData(uint8_t nib) {
  PORTB = (PORTB & ~DATA_PB) | ((nib & 0x02) >> 1) | ((nib & 0x04) << 1);   // D1->PB0, D2->PB3
  PORTC = (PORTC & ~DATA_PC) | ((nib & 0x01) << 1) | (nib & 0x08);          // D0->PC1, D3->PC3
}

static uint8_t getData(void) {
  uint8_t b = PINB, c = PINC;
  return (uint8_t)(((c >> 1) & 1) | ((b & 1) << 1) | (((b >> 3) & 1) << 2) | (((c >> 3) & 1) << 3));
}

static void cellWrite(uint8_t row, uint8_t col, uint8_t nib) {
  DDRB |= DATA_PB; DDRC |= DATA_PC;          // data out
  OE_HIGH;
  cli();
  setAddr(row);
  RAS_LOW;
  WE_LOW;
  setAddr(col);
  setData(nib);
  CAS_LOW; NOP; NOP; CAS_HIGH;
  WE_HIGH;
  RAS_HIGH;
  sei();
}

static uint8_t cellRead(uint8_t row, uint8_t col) {
  DDRB &= ~DATA_PB; DDRC &= ~DATA_PC;        // data in
  PORTB |= DATA_PB; PORTC |= DATA_PC;        // pull-ups: an empty socket reads 0xF
  OE_LOW;
  cli();
  setAddr(row);
  RAS_LOW;
  setAddr(col);
  CAS_LOW; NOP; NOP; NOP; NOP;
  uint8_t got = getData();
  CAS_HIGH;
  RAS_HIGH;
  sei();
  OE_HIGH;
  PORTB &= ~DATA_PB; PORTC &= ~DATA_PC;
  return got;
}

//=======================================================================================
// DETECTION
//=======================================================================================

// Presence: a nibble must come back as written (0x5, then 0xA). Geometry: write 0 to
// column 0 and 0xF to column 1 -- if column 0 changed, A0 is not decoded, i.e. the
// columns sit on A1..A6 (4416, 256 x 64). Same for A7 (0x80) as confirmation.
static bool detect18(void) {
  delayMicroseconds(250);
  for (uint8_t i = 0; i < 8; i++) { RAS_LOW; NOP; NOP; RAS_HIGH; NOP; NOP; }

  cellWrite(0, 0, 0x5);
  if (cellRead(0, 0) != 0x5) {
    cellWrite(0, 0, 0xA);
    if (cellRead(0, 0) != 0xA) return false;
  }
  cellWrite(0, 0, 0x0);
  cellWrite(0, 1, 0xF);
  bool is4416 = (cellRead(0, 0) != 0x0);
  if (!is4416) {
    cellWrite(0, 0, 0x0);
    cellWrite(0, 0x80, 0xF);
    is4416 = (cellRead(0, 0) != 0x0);
  }
  if (is4416) { g_dram.rows = 256; g_dram.cols = 64;  g_dram.name = name_4416; buildLut(1); }
  else        { g_dram.rows = 256; g_dram.cols = 256; g_dram.name = name_4464; buildLut(0); }
  g_dram.specUs = 4000;
  return true;
}

//=======================================================================================
// ROW WRITE / READ (the paced passes; called with interrupts disabled)
//=======================================================================================
//
// Same structure as chip_4164.cpp (see there for why this is assembly): 32-cell chunk,
// pattern byte reloaded every 8 cells, both loops 37 cycles per cell.
//
// Per cell (write):                            Per cell (read):
//   ld/or x2   column images        6            ld/or x2                       6
//   mov        PORTC base           1            --
//   sbrc/ori x2  data on 4 lines    4            --
//   lsr        next bit             1            --
//   out x3     row address          3            out x3   (OE low)              3
//   cbi RAS                         2            cbi RAS                        2
//   cbi WE     early write          2            nop (tRAH)                     1
//   out x3     column + data        3            out x2   column                2
//   cbi CAS, nop, nop, sbi CAS      6            cbi CAS, nop x4                6
//   --                                           in PINC, in PINB               2
//   --                                           sbi CAS                        2
//   sbi WE                          2            --
//   sbi RAS                         2            sbi RAS                        2
//   nop x2     (pad)                2            andi,andi,or,sbrc,eor  compare 5
//   --                                           breq (ok path)                 2
//   --                                           lsr  next bit                  1
//   dec/brne                        3            dec/brne                       3
//                                  37                                          37

// One 32-cell write chunk.
static inline void writeChunk18(uint8_t rb, uint8_t rd, uint8_t hb, uint8_t hd,
                                const uint8_t *&lp, const uint8_t *&bits) {
  uint8_t cb, cc, cd, b, n, k;
  __asm__ __volatile__(
    "ldi  %[k], 4                  \n\t"
    "1:                            \n\t"
    "ld   %[b], X+                 \n\t"
    "ldi  %[n], 8                  \n\t"
    "2:                            \n\t"
    "ld   %[cb], Z+                \n\t"   // column images (WE bit 0 -> stays LOW)
    "or   %[cb], %[hb]             \n\t"
    "ld   %[cd], Z+                \n\t"
    "or   %[cd], %[hd]             \n\t"
    "mov  %[cc], %[ccw]            \n\t"   // OE high, CAS high, RAS low, data 0
    "sbrc %[b], 0                  \n\t"
    "ori  %[cb], 0x09              \n\t"   // D1 D2
    "sbrc %[b], 0                  \n\t"
    "ori  %[cc], 0x0A              \n\t"   // D0 D3
    "lsr  %[b]                     \n\t"
    "out  %[portb], %[rb]          \n\t"   // row address, WE high
    "out  %[portd], %[rd]          \n\t"
    "out  %[portc], %[rcw]         \n\t"   // OE CAS RAS high
    "cbi  %[portc], 4              \n\t"   // RAS low
    "cbi  %[portb], 1              \n\t"   // WE low: early write
    "out  %[portb], %[cb]          \n\t"   // column + data
    "out  %[portd], %[cd]          \n\t"
    "out  %[portc], %[cc]          \n\t"
    "cbi  %[portc], 2              \n\t"   // CAS low
    "nop                           \n\t"
    "nop                           \n\t"
    "sbi  %[portc], 2              \n\t"   // CAS high
    "sbi  %[portb], 1              \n\t"   // WE high
    "sbi  %[portc], 4              \n\t"   // RAS high
    "nop                           \n\t"   // pad: match the read cell (37 cycles)
    "nop                           \n\t"
    "dec  %[n]                     \n\t"
    "brne 2b                       \n\t"
    "dec  %[k]                     \n\t"
    "brne 1b                       \n\t"
    : [cb] "=&d"(cb), [cc] "=&d"(cc), [cd] "=&r"(cd), [b] "=&r"(b), [n] "=&d"(n), [k] "=&d"(k),
      "+z"(lp), "+x"(bits)
    : [rb] "r"(rb), [rd] "r"(rd), [hb] "r"(hb), [hd] "r"(hd),
      [rcw] "r"((uint8_t)PC_IDLE_W), [ccw] "r"((uint8_t)0x05),
      [portb] "I"(_SFR_IO_ADDR(PORTB)), [portc] "I"(_SFR_IO_ADDR(PORTC)), [portd] "I"(_SFR_IO_ADDR(PORTD))
    : "memory", "cc");
}

// One 32-cell read chunk (OE low throughout). errs saturates at 255; errK/errN mark the
// first error (index within the chunk = (4 - errK) * 8 + (8 - errN)).
static inline void readChunk18(uint8_t rb, uint8_t rd, uint8_t hb, uint8_t hd,
                               const uint8_t *&lp, const uint8_t *&bits,
                               uint8_t &errs, uint8_t &errK, uint8_t &errN) {
  uint8_t cb, cd, b, n, k, x, y;
  __asm__ __volatile__(
    "ldi  %[k], 4                  \n\t"
    "1:                            \n\t"
    "ld   %[b], X+                 \n\t"
    "ldi  %[n], 8                  \n\t"
    "2:                            \n\t"
    "ld   %[cb], Z+                \n\t"
    "or   %[cb], %[hb]             \n\t"   // hb carries WE high; data bits 0 = no pull-up
    "ld   %[cd], Z+                \n\t"
    "or   %[cd], %[hd]             \n\t"
    "out  %[portb], %[rb]          \n\t"   // row address
    "out  %[portd], %[rd]          \n\t"
    "out  %[portc], %[rcr]         \n\t"   // OE low, CAS RAS high
    "cbi  %[portc], 4              \n\t"   // RAS low
    "nop                           \n\t"   // tRAH
    "out  %[portb], %[cb]          \n\t"   // column
    "out  %[portd], %[cd]          \n\t"
    "cbi  %[portc], 2              \n\t"   // CAS low
    "nop                           \n\t"
    "nop                           \n\t"
    "nop                           \n\t"
    "nop                           \n\t"
    "in   %[x], %[pinc]            \n\t"   // D0 D3   ~220-280 ns after the CAS fall
    "in   %[y], %[pinb]            \n\t"   // D1 D2   ~280-340 ns
    "sbi  %[portc], 2              \n\t"   // CAS high
    "sbi  %[portc], 4              \n\t"   // RAS high
    "andi %[x], 0x0A               \n\t"
    "andi %[y], 0x09               \n\t"
    "or   %[x], %[y]               \n\t"   // 0x0B = all four lines read 1
    "sbrc %[b], 0                  \n\t"
    "eor  %[x], %[kb]              \n\t"   // expected 1 -> 0x0B ^ 0x0B = 0
    "breq 3f                       \n\t"   // all four lines correct
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
    "dec  %[n]                     \n\t"
    "brne 2b                       \n\t"
    "dec  %[k]                     \n\t"
    "brne 1b                       \n\t"
    : [cb] "=&r"(cb), [cd] "=&r"(cd), [b] "=&r"(b), [n] "=&d"(n), [k] "=&d"(k),
      [x] "=&d"(x), [y] "=&d"(y), [errs] "+d"(errs), [errK] "+r"(errK), [errN] "+r"(errN),
      "+z"(lp), "+x"(bits)
    : [rb] "r"(rb), [rd] "r"(rd), [hb] "r"(hb), [hd] "r"(hd),
      [rcr] "r"((uint8_t)PC_IDLE_R), [kb] "r"((uint8_t)0x0B),
      [portb] "I"(_SFR_IO_ADDR(PORTB)), [portc] "I"(_SFR_IO_ADDR(PORTC)), [portd] "I"(_SFR_IO_ADDR(PORTD)),
      [pinb] "I"(_SFR_IO_ADDR(PINB)), [pinc] "I"(_SFR_IO_ADDR(PINC))
    : "memory", "cc");
}

static void __attribute__((hot)) writeRow18(uint16_t row, const uint8_t *bits) {
  const uint8_t rb = ROW_PB(row), rd = ROW_PD(row);
  const uint8_t nhi = (uint8_t)(g_dram.cols >> 5);
  DDRB |= DATA_PB; DDRC |= DATA_PC;          // data out
  for (uint8_t h = 0; h < nhi; h++) {
    const uint8_t hb = s_hi[h].pb;           // WE bit 0: LOW in the column phase
    const uint8_t hd = s_hi[h].pd | (rd & 0x18);
    const uint8_t *lp = &s_lo[0].pb;
    writeChunk18(rb, rd, hb, hd, lp, bits);
    sei(); NOP; cli();
  }
}

static uint16_t __attribute__((hot)) readRow18(uint16_t row, const uint8_t *bits, uint16_t *firstBadCol) {
  const uint8_t rb = ROW_PB(row), rd = ROW_PD(row);
  const uint8_t nhi = (uint8_t)(g_dram.cols >> 5);
  uint8_t errs = 0, errK = 0, errN = 0, errH = 0;
  DDRB &= ~DATA_PB; DDRC &= ~DATA_PC;        // data in, no pull-ups
  PORTB &= ~DATA_PB; PORTC &= ~DATA_PC;
  for (uint8_t h = 0; h < nhi; h++) {
    const uint8_t hb = s_hi[h].pb | 0x02;    // WE HIGH throughout
    const uint8_t hd = s_hi[h].pd | (rd & 0x18);
    const uint8_t *lp = &s_lo[0].pb;
    const uint8_t before = errs;
    readChunk18(rb, rd, hb, hd, lp, bits, errs, errK, errN);
    if (!before && errs) errH = h;
    sei(); NOP; cli();
  }
  PORTC = PC_IDLE_W;                         // OE back to HIGH
  if (errs) *firstBadCol = (uint16_t)errH * 32 + (4 - errK) * 8 + (8 - errN);
  return errs;
}

//=======================================================================================
// BINDER
//=======================================================================================

void bind_18pin_x4(void) {
  g_dram.name = name_4464;
  g_dram.rows = 256;
  g_dram.cols = 256;
  g_dram.specUs = 4000;
  g_dram.markRow = LA_MARKERS ? 0x10 : 0;    // PD4 (NC on the 18-pin socket)
  g_dram.markBusy = 0;                       // no free pin
  g_dram.setupPorts = setupPorts18;
  g_dram.detect = detect18;
  g_dram.writeRow = writeRow18;
  g_dram.readRow = readRow18;
}
