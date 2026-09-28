#line 1 "C:\\Users\\Andreas\\Documents\\DRAM_Decay_Test\\chip_4164.cpp"
// chip_4164.cpp - 16-pin x1 DRAM driver (4164, and same-socket 41256/41257/4816)
//=======================================================================================
//
// Reference driver for the DramDriver interface (chip.h). Pinout is the 16-pin socket
// of the RAM-Tester PCB, identical to 16Pin.cpp of Ram_Tester 5.1.2:
//
//   A0 = PC4   A1 = PD1   A2 = PD0   A3 = PB2   A4 = PB4 (also OLED SDA / green LED)
//   A5 = PD7   A6 = PB0   A7 = PD6   A8 = PC0 (41256/41257 only, NC on the 4164/4816)
//   RAS = PB1  CAS = PC3  WE = PB3   Din = PC1  Dout = PC2
//   PB5 = red LED / OLED SCL (parked LOW)   PD2 = DIP (VCC sense)   PD4/PD5 = LA markers
//
// Access cycle (single cycle, one RAS per cell, early write):
//
//   RAS  ~~~~\_____________________/~~~~~~     tRAS ~0.9 us (spec 150-200 ns .. 10 us)
//   WE   ~~~~~~\_________________/~~~~~~~~     write only; low BEFORE CAS -> early write
//   CAS  ~~~~~~~~~~~~~~~\_____/~~~~~~~~~~~     tCAS ~250 ns (spec >= 75-120 ns)
//   ADDR ==row====><====column=========>       tASR/tASC >= 1 cycle, tRAH >= 2 cycles
//   Din  ------------<==data (write)====>       latched at CAS fall (tDS/tDH ok)
//   Dout ---------------------<valid>----       read: sampled ~310 ns after CAS fall
//
// Every cell costs 38 cycles = 2.38 us (write and read cycle-matched, inline asm); a
// 4164 row (256 cells) ~0.62 ms, the whole array ~0.16 s.
// The NMOS tRAS-max of 10 us is never approached, so no page mode, no burst recycle.
//
// Address translation uses two small lookup tables (32 low + 16 high entries, 3 port
// bytes each, built once in RAM): the column phase is 3 table reads + 3 port writes.
//
//=======================================================================================

#include "chip.h"

#define RAS_LOW  CBI(PORTB, 1)
#define RAS_HIGH SBI(PORTB, 1)
#define CAS_LOW  CBI(PORTC, 3)
#define CAS_HIGH SBI(PORTC, 3)
#define WE_LOW   CBI(PORTB, 3)
#define WE_HIGH  SBI(PORTB, 3)

// 9-bit address -> port images (only the address bits, everything else 0)
#define GET_PB(a) ((uint8_t)((((a) & 0x0010)) | (((a) & 0x0008) >> 1) | (((a) & 0x0040) >> 6)))                     // A4 A3 A6
#define GET_PC(a) ((uint8_t)((((a) & 0x0001) << 4) | (((a) & 0x0100) >> 8)))                                        // A0 A8
#define GET_PD(a) ((uint8_t)((((a) & 0x0080) >> 1) | (((a) & 0x0020) << 2) | (((a) & 0x0004) >> 2) | ((a) & 0x0002)))  // A7 A5 A2 A1

// PORTB image with RAS and WE HIGH, LED/SCL LOW; PORTC image with CAS HIGH, Din LOW.
#define ROW_PB(r) (uint8_t)(0x0A | GET_PB(r))
#define ROW_PC(r) (uint8_t)(0x08 | GET_PC(r))
// PORTD: keep PD2..PD5 (DIP inputs, LA markers) as they are.
#define ROW_PD(r) (uint8_t)((PORTD & 0x3C) | GET_PD(r))

struct Lut3 { uint8_t pb, pc, pd; };
static Lut3 s_lo[32];   // column & 31
static Lut3 s_hi[16];   // column >> 5

static const char name_4164[]  PROGMEM = "4164 64Kx1";
static const char name_41256[] PROGMEM = "41256/7 256Kx1";
static const char name_4816[]  PROGMEM = "4816 16Kx1";

//=======================================================================================
// PORT SETUP
//=======================================================================================

static void setupPorts16(void) {
  DDRB = 0b00111111;                         // A6 RAS A3 WE A4 + PB5 outputs
  PORTB = 0b00001010;                        // RAS = WE = HIGH, LED/SCL LOW
  DDRC = 0b00011011;                         // A8 Din CAS A0 outputs, Dout + PC5 inputs
  PORTC = 0b00001000;                        // CAS HIGH, Din LOW, no pull-ups
  DDRD = (DDRD & 0x30) | 0b11000011;         // A2 A1 A7 A5 outputs; PD2/PD3 inputs; markers untouched
  PORTD &= 0x30;
#if LA_MARKERS
  DDRD |= 0x30;                              // PD4 = ROW, PD5 = BUSY/ERR (free on this socket)
#endif
  static bool lutReady = false;
  if (!lutReady) {
    for (uint8_t i = 0; i < 32; i++) { s_lo[i].pb = GET_PB(i); s_lo[i].pc = GET_PC(i); s_lo[i].pd = GET_PD(i); }
    for (uint8_t i = 0; i < 16; i++) {
      uint16_t a = (uint16_t)i << 5;
      s_hi[i].pb = GET_PB(a); s_hi[i].pc = GET_PC(a); s_hi[i].pd = GET_PD(a);
    }
    lutReady = true;
  }
}

//=======================================================================================
// SINGLE-CELL ACCESS (detection only, not speed critical)
//=======================================================================================

static void setAddr(uint16_t a) {
  PORTB = (PORTB & 0xEA) | GET_PB(a);        // keep RAS, WE, PB5
  PORTC = (PORTC & 0xEE) | GET_PC(a);        // keep Din, Dout pull-up, CAS, PC5
  PORTD = (PORTD & 0x3C) | GET_PD(a);        // keep PD2..PD5
}

static void cellWrite(uint16_t row, uint16_t col, uint8_t bit) {
  DDRC |= 0x02;                              // Din output
  cli();                                     // keep the timer ISR out of the RAS-low window
  setAddr(row);
  RAS_LOW;
  WE_LOW;
  setAddr(col);
  if (bit) SBI(PORTC, 1); else CBI(PORTC, 1);
  CAS_LOW; NOP; NOP; CAS_HIGH;
  WE_HIGH;
  RAS_HIGH;
  sei();
}

static uint8_t cellRead(uint16_t row, uint16_t col) {
  CBI(PORTC, 1);
  DDRC &= ~0x02;                             // Din input (parts with Din tied to Dout)
  SBI(PORTC, 2);                             // Dout pull-up: an empty socket reads 1
  cli();
  setAddr(row);
  RAS_LOW;
  setAddr(col);
  CAS_LOW; NOP; NOP; NOP; CAS_HIGH;
  uint8_t got = PINC;
  RAS_HIGH;
  sei();
  CBI(PORTC, 2);
  return (got >> 2) & 1;
}

//=======================================================================================
// DETECTION
//=======================================================================================

// Presence via two cells that must hold different values (twice, in case the first
// pair sits in a dead region), then geometry by address aliasing:
//   A8 row alias  -> 512 x 512 (41256/41257)
//   A7 row alias  -> col A7 aliased too -> 128 x 128 (4816), else 256 x 256
static bool detect16(void) {
  delayMicroseconds(250);                    // power-up pause, then the 8 RAS-only init cycles
  for (uint8_t i = 0; i < 8; i++) { RAS_LOW; NOP; NOP; RAS_HIGH; NOP; NOP; }

  cellWrite(0, 0, 0); cellWrite(1, 1, 1);
  bool ok = (cellRead(0, 0) == 0 && cellRead(1, 1) == 1);
  if (!ok) {
    cellWrite(4, 32, 0); cellWrite(8, 64, 1);
    ok = (cellRead(4, 32) == 0 && cellRead(8, 64) == 1);
  }
  if (!ok) return false;

  cellWrite(0, 0, 0); cellWrite(256, 0, 1);
  if (cellRead(0, 0) == 0) { g_dram.rows = g_dram.cols = 512; g_dram.name = name_41256; g_dram.specUs = 4000; return true; }
  cellWrite(0, 0, 0); cellWrite(128, 0, 1);
  if (cellRead(0, 0) == 0) { g_dram.rows = g_dram.cols = 256; g_dram.name = name_4164; g_dram.specUs = 4000; return true; }
  cellWrite(0, 0, 0); cellWrite(0, 128, 1);
  if (cellRead(0, 0) == 0) { g_dram.rows = g_dram.cols = 256; g_dram.name = name_4164; g_dram.specUs = 4000; return true; }  // half-good 4164 class
  g_dram.rows = g_dram.cols = 128; g_dram.name = name_4816; g_dram.specUs = 2000;
  return true;
}

//=======================================================================================
// ROW WRITE / READ (the paced passes; called with interrupts disabled)
//=======================================================================================
//
// The 32-cell chunk loop is inline assembly, for one reason: a cell's age is
// t_read - t_write, and within a row both passes advance at their loop's per-cell rate.
// A read cell one cycle slower than a write cell makes the last cell of a 256-column
// row age 16 us longer than the first -- 0.8 % at T = 2 ms, and GCC's code for the two
// C loops drifted between 38 and 56 cycles per cell depending on loop shape and register
// pressure. In assembly both loops are the same instruction sequence except for the
// strobe section, and the write loop carries three pad NOPs so that BOTH COME OUT AT 38
// CYCLES PER CELL (2.38 us; a 256-cell row = 0.62 ms, the array 0.16 s). Verify with the LA: the
// ROW-marker period of a write row must equal that of a read row (docs/LA_Testkonzept.md).
//
// Per cell (write):                            Per cell (read):
//   ld/or x3   column images        9            ld/or x3                       9
//   sbrc/ori   Din                  2            --
//   lsr        next bit             1            --
//   out x3     row address          3            out x3                         3
//   cbi RAS                         2            cbi RAS                        2
//   cbi WE     early write          2            nop (tRAH)                     1
//   out x3     column + Din         3            out x3                         3
//   cbi CAS, nop, nop, sbi CAS      6            cbi CAS, nop x3, sbi CAS       7
//   sbi WE                          2            in  PINC (sample)              1
//   sbi RAS                         2            sbi RAS                        2
//   nop x3     (pad)                3            lsr,lsr,eor,andi  compare      4
//   --                                           breq (ok path)                 2
//   --                                           lsr  next bit                  1
//   dec/brne                        3            dec/brne                       3
//                                  38                                          38
// plus, every 8 cells in both: ld X+ (pattern byte), ldi, dec/brne = 6 cycles.
//
// Timing inside a cell (16 MHz, 62.5 ns/cycle): RAS low 0.94 / 0.88 us (spec 150 ns .. 10 us),
// row address stable 3 cycles before and >= 2 cycles after the RAS fall, column address
// and Din >= 1 cycle before the CAS fall, CAS low 4-5 cycles (250-310 ns), WE low from
// 2 cycles before CAS to 2 cycles after (early write), RAS precharge >= 1.3 us. The read
// samples PINC ~5 cycles (310 ns) after the CAS fall: above every tCAC grade, and the
// input synchroniser delivers the level of ~1 cycle earlier, i.e. still inside CAS low.
//
// Between chunks (RAS high, nothing pending) interrupts are let through for one
// instruction so the millis() timer never loses a tick on the 512-column parts -- the
// caller's cli/sei stays the outer frame.

#if LA_MARKERS
#define ASM_MARK_ERR "sbi %[pind], 5 \n\t nop \n\t nop \n\t nop \n\t nop \n\t sbi %[pind], 5 \n\t"
#else
#define ASM_MARK_ERR ""
#endif

// One 32-cell write chunk. lp -> s_lo (advanced by 96), bits -> 4 pattern bytes.
static inline void writeChunk16(uint8_t rb, uint8_t rc, uint8_t rd, uint8_t hb, uint8_t hc, uint8_t hd,
                                const uint8_t *&lp, const uint8_t *&bits) {
  uint8_t cb, cc, cd, b, n, k;
  __asm__ __volatile__(
    "ldi  %[k], 4                  \n\t"
    "1:                            \n\t"
    "ld   %[b], X+                 \n\t"   // pattern byte, LSB = first column
    "ldi  %[n], 8                  \n\t"
    "2:                            \n\t"
    "ld   %[cb], Z+                \n\t"   // column images, prepared before the RAS edge
    "or   %[cb], %[hb]             \n\t"
    "ld   %[cc], Z+                \n\t"
    "or   %[cc], %[hc]             \n\t"
    "ld   %[cd], Z+                \n\t"
    "or   %[cd], %[hd]             \n\t"
    "sbrc %[b], 0                  \n\t"
    "ori  %[cc], 0x02              \n\t"   // Din
    "lsr  %[b]                     \n\t"
    "out  %[portb], %[rb]          \n\t"   // row address, RAS/CAS/WE high
    "out  %[portc], %[rc]          \n\t"
    "out  %[portd], %[rd]          \n\t"
    "cbi  %[portb], 1              \n\t"   // RAS low: row latched
    "cbi  %[portb], 3              \n\t"   // WE low: early write
    "out  %[portb], %[cb]          \n\t"   // column address + Din (RAS/WE stay low)
    "out  %[portd], %[cd]          \n\t"
    "out  %[portc], %[cc]          \n\t"
    "cbi  %[portc], 3              \n\t"   // CAS low: column + data latched
    "nop                           \n\t"
    "nop                           \n\t"
    "sbi  %[portc], 3              \n\t"   // CAS high
    "sbi  %[portb], 3              \n\t"   // WE high
    "sbi  %[portb], 1              \n\t"   // RAS high
    "nop                           \n\t"   // pad: match the read cell (38 cycles)
    "nop                           \n\t"
    "nop                           \n\t"
    "dec  %[n]                     \n\t"
    "brne 2b                       \n\t"
    "dec  %[k]                     \n\t"
    "brne 1b                       \n\t"
    : [cb] "=&r"(cb), [cc] "=&d"(cc), [cd] "=&r"(cd), [b] "=&r"(b), [n] "=&d"(n), [k] "=&d"(k),
      "+z"(lp), "+x"(bits)
    : [rb] "r"(rb), [rc] "r"(rc), [rd] "r"(rd), [hb] "r"(hb), [hc] "r"(hc), [hd] "r"(hd),
      [portb] "I"(_SFR_IO_ADDR(PORTB)), [portc] "I"(_SFR_IO_ADDR(PORTC)), [portd] "I"(_SFR_IO_ADDR(PORTD))
    : "memory", "cc");
}

// One 32-cell read chunk. errs saturates at 255; on the FIRST error errK/errN hold the
// loop counters (index within the chunk = (4 - errK) * 8 + (8 - errN)).
static inline void readChunk16(uint8_t rb, uint8_t rc, uint8_t rd, uint8_t hb, uint8_t hc, uint8_t hd,
                               const uint8_t *&lp, const uint8_t *&bits,
                               uint8_t &errs, uint8_t &errK, uint8_t &errN) {
  uint8_t cb, cc, cd, b, n, k, got;
  __asm__ __volatile__(
    "ldi  %[k], 4                  \n\t"
    "1:                            \n\t"
    "ld   %[b], X+                 \n\t"
    "ldi  %[n], 8                  \n\t"
    "2:                            \n\t"
    "ld   %[cb], Z+                \n\t"
    "or   %[cb], %[hb]             \n\t"
    "ld   %[cc], Z+                \n\t"
    "or   %[cc], %[hc]             \n\t"
    "ld   %[cd], Z+                \n\t"
    "or   %[cd], %[hd]             \n\t"
    "out  %[portb], %[rb]          \n\t"   // row address, RAS/CAS/WE high
    "out  %[portc], %[rc]          \n\t"
    "out  %[portd], %[rd]          \n\t"
    "cbi  %[portb], 1              \n\t"   // RAS low
    "nop                           \n\t"   // tRAH: row address held 2 cycles like the write
    "out  %[portb], %[cb]          \n\t"   // column address (WE stays high)
    "out  %[portd], %[cd]          \n\t"
    "out  %[portc], %[cc]          \n\t"
    "cbi  %[portc], 3              \n\t"   // CAS low
    "nop                           \n\t"
    "nop                           \n\t"
    "nop                           \n\t"
    "sbi  %[portc], 3              \n\t"   // CAS high
    "in   %[got], %[pinc]          \n\t"   // Dout sampled ~310 ns after the CAS fall
    "sbi  %[portb], 1              \n\t"   // RAS high
    "lsr  %[got]                   \n\t"   // PC2 -> bit 0
    "lsr  %[got]                   \n\t"
    "eor  %[got], %[b]             \n\t"
    "andi %[got], 1                \n\t"
    "breq 3f                       \n\t"   // match
    "cpi  %[errs], 0               \n\t"   // ---- mismatch (adds cycles to failing cells only)
    "brne 4f                       \n\t"
    "mov  %[errK], %[k]            \n\t"   // first error of this chunk: remember where
    "mov  %[errN], %[n]            \n\t"
    "4:                            \n\t"
    "cpi  %[errs], 255             \n\t"
    "breq 5f                       \n\t"
    "inc  %[errs]                  \n\t"
    "5:                            \n\t"
    ASM_MARK_ERR
    "3:                            \n\t"
    "lsr  %[b]                     \n\t"
    "dec  %[n]                     \n\t"
    "brne 2b                       \n\t"
    "dec  %[k]                     \n\t"
    "brne 1b                       \n\t"
    : [cb] "=&r"(cb), [cc] "=&r"(cc), [cd] "=&r"(cd), [b] "=&r"(b), [n] "=&d"(n), [k] "=&d"(k),
      [got] "=&d"(got), [errs] "+d"(errs), [errK] "+r"(errK), [errN] "+r"(errN),
      "+z"(lp), "+x"(bits)
    : [rb] "r"(rb), [rc] "r"(rc), [rd] "r"(rd), [hb] "r"(hb), [hc] "r"(hc), [hd] "r"(hd),
      [portb] "I"(_SFR_IO_ADDR(PORTB)), [portc] "I"(_SFR_IO_ADDR(PORTC)), [portd] "I"(_SFR_IO_ADDR(PORTD)),
      [pinc] "I"(_SFR_IO_ADDR(PINC)), [pind] "I"(_SFR_IO_ADDR(PIND))
    : "memory", "cc");
}

static void __attribute__((hot)) writeRow16(uint16_t row, const uint8_t *bits) {
  const uint8_t rb = ROW_PB(row), rc = ROW_PC(row), rd = ROW_PD(row);
  const uint8_t nhi = (uint8_t)(g_dram.cols >> 5);
  DDRC |= 0x02;                              // Din output
  for (uint8_t h = 0; h < nhi; h++) {
    const uint8_t hb = s_hi[h].pb;           // RAS/WE bits 0: both LOW in the column phase
    const uint8_t hc = s_hi[h].pc | 0x08;    // CAS stays HIGH until the strobe
    const uint8_t hd = s_hi[h].pd | (rd & 0x3C);
    const uint8_t *lp = &s_lo[0].pb;
    writeChunk16(rb, rc, rd, hb, hc, hd, lp, bits);
    sei(); NOP; cli();                       // timer ISR window at the chunk boundary
  }
}

static uint16_t __attribute__((hot)) readRow16(uint16_t row, const uint8_t *bits, uint16_t *firstBadCol) {
  const uint8_t rb = ROW_PB(row), rc = ROW_PC(row), rd = ROW_PD(row);
  const uint8_t nhi = (uint8_t)(g_dram.cols >> 5);
  uint8_t errs = 0, errK = 0, errN = 0, errH = 0;
  CBI(PORTC, 1);
  DDRC &= ~0x02;                             // Din input (Din/Dout-tied parts), no pull-up
  for (uint8_t h = 0; h < nhi; h++) {
    const uint8_t hb = s_hi[h].pb | 0x08;    // WE HIGH throughout
    const uint8_t hc = s_hi[h].pc | 0x08;
    const uint8_t hd = s_hi[h].pd | (rd & 0x3C);
    const uint8_t *lp = &s_lo[0].pb;
    const uint8_t before = errs;
    readChunk16(rb, rc, rd, hb, hc, hd, lp, bits, errs, errK, errN);
    if (!before && errs) errH = h;           // chunk of the first error
    sei(); NOP; cli();                       // timer ISR window at the chunk boundary
  }
  if (errs) *firstBadCol = (uint16_t)errH * 32 + (4 - errK) * 8 + (8 - errN);
  return errs;
}

//=======================================================================================
// BINDER
//=======================================================================================

void bind_16pin_x1(void) {
  g_dram.name = name_4164;
  g_dram.rows = 256;
  g_dram.cols = 256;
  g_dram.specUs = 4000;
  g_dram.markRow = LA_MARKERS ? 0x10 : 0;    // PD4
  g_dram.markBusy = LA_MARKERS ? 0x20 : 0;   // PD5 (ERR pulses come from the read loop)
  g_dram.setupPorts = setupPorts16;
  g_dram.detect = detect16;
  g_dram.writeRow = writeRow16;
  g_dram.readRow = readRow16;
}
