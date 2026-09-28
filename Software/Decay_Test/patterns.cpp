#line 1 "C:\\Users\\Andreas\\Documents\\DRAM_Decay_Test\\patterns.cpp"
// patterns.cpp - Data backgrounds for the retention test
//=======================================================================================

#include "patterns.h"

// 256 x 8-bit pseudo-random values, regenerated whenever the seed changes.
static uint8_t  s_rnd[256];
static uint16_t s_rndSeed = 0;

static const char n_cb[]  PROGMEM = "CB";
static const char n_00[]  PROGMEM = "00";
static const char n_ff[]  PROGMEM = "FF";
static const char n_row[] PROGMEM = "Row";
static const char n_col[] PROGMEM = "Col";
static const char n_rnd[] PROGMEM = "Rnd";
// indexed by kind >> 1 (each name covers the normal and the inverted kind)
static const char *const s_names[] PROGMEM = { n_cb, n_00, n_row, n_col, n_rnd };

void patFixed(uint8_t k, Pattern *p) {
  p->kind = k;
  p->seed = 1;
}

void patSoak(uint16_t n, Pattern *p) {
  p->kind = (n & 1) ? PAT_RND_INV : PAT_RND;
  p->seed = 2 + (n >> 1);
}

// xorshift16 (period 65535, seed != 0). Table entry = low byte ^ high byte.
void patPrepare(const Pattern *p) {
  if (p->kind < PAT_RND || p->seed == s_rndSeed) return;
  uint16_t x = p->seed;
  uint8_t i = 0;
  do {
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    s_rnd[i] = (uint8_t)(x ^ (x >> 8));
  } while (++i != 0);
  s_rndSeed = p->seed;
}

void patRow(const Pattern *p, uint16_t row, uint8_t *bits, uint16_t cols) {
  uint8_t n = (uint8_t)(cols >> 3);
  uint8_t inv = (p->kind & 1) ? 0xFF : 0x00;   // odd kinds are the inverted ones
  uint8_t v;
  switch (p->kind & ~1) {
    case PAT_CB:   v = (row & 1) ? 0x55 : 0xAA; break;   // bit0 = column 0: (row^col)&1
    case PAT_ZERO: v = 0x00; break;                       // PAT_ONE = inverted
    case PAT_ROW:  v = (row & 1) ? 0xFF : 0x00; break;
    case PAT_COL:  v = 0xAA; break;                       // bit = col & 1
    default: {                                            // PAT_RND
      // Two table reads per byte, both row- and column-dependent, so neighbouring
      // rows and neighbouring bytes do not share a sequence.
      uint8_t r1 = (uint8_t)row;
      uint8_t r2 = (uint8_t)(row * 0x35) + (uint8_t)(row >> 8) * 0x5B;
      for (uint8_t b = 0; b < n; b++)
        bits[b] = s_rnd[(uint8_t)(b ^ r1)] ^ s_rnd[(uint8_t)(b + r2)] ^ inv;
      return;
    }
  }
  v ^= inv;
  for (uint8_t b = 0; b < n; b++) bits[b] = v;
}

void patName(const Pattern *p, char *out) {
  strcpy_P(out, (PGM_P)pgm_read_word(&s_names[p->kind >> 1]));
  if (p->kind >= PAT_RND && p->seed > 1) utoa(p->seed, out + strlen(out), 10);
  if (p->kind & 1) strcat(out, "~");
}
