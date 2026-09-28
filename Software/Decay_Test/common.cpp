#line 1 "C:\\Users\\Andreas\\Documents\\DRAM_Decay_Test\\common.cpp"
// common.cpp - Display screens, formatting and LED for the DRAM Decay Test
//=======================================================================================

#include "common.h"
#include <stdlib.h>

// SW-I2C on the LED pins (clock = PB5 / D13, data = PB4 / D12), 2-page buffer (256 B).
// The bundled driver hard-codes those pins and runs the bus at ~600 kHz.
U8G2_SSD1306_128X64_NONAME_2_SW_I2C display(U8G2_R0, /*clock=*/13, /*data=*/12, U8X8_PIN_NONE);

//=======================================================================================
// HELPERS
//=======================================================================================

// value/div with `dec` decimals, e.g. (2350, 1000, 2) -> "2.35"
static void fmtFixed(char *out, uint32_t value, uint32_t div, uint8_t dec) {
  ultoa(value / div, out, 10);
  if (!dec) return;
  char *p = out + strlen(out);
  *p++ = '.';
  uint32_t rem = value % div;
  for (; dec; dec--) {
    div /= 10;
    *p++ = '0' + (uint8_t)(rem / div);
    rem %= div;
  }
  *p = 0;
}

void fmtTime(char *out, uint32_t us) {
  if (us < 10000UL)          fmtFixed(out, us, 1000UL, 2);        // 2.35ms
  else if (us < 100000UL)    fmtFixed(out, us, 1000UL, 1);        // 12.3ms
  else if (us < 1000000UL)   fmtFixed(out, us, 1000UL, 0);        // 123ms
  else if (us < 10000000UL)  fmtFixed(out, us, 1000000UL, 2);     // 1.23s
  else if (us < 100000000UL) fmtFixed(out, us, 1000000UL, 1);     // 12.3s
  else                       fmtFixed(out, us, 1000000UL, 0);     // 123s .. 1800s
  strcat(out, us < 1000000UL ? "ms" : "s");
}

void ledRed(bool on) {
  DDRB |= (1 << 5);
  if (on) SBI(PORTB, 5);
  else CBI(PORTB, 5);
}

// Right-aligned print helpers (current font).
static void printRight(uint8_t y, const char *s) {
  display.setCursor(128 - display.getStrWidth(s), y);
  display.print(s);
}
static void printRightP(uint8_t y, const __FlashStringHelper *s) {
  char buf[22];
  strncpy_P(buf, (PGM_P)s, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  printRight(y, buf);
}
static void printCentered(uint8_t y, const char *s) {
  int16_t x = (int16_t)(128 - display.getStrWidth(s)) / 2;
  if (x < 0) x = 0;
  display.setCursor(x, y);
  display.print(s);
}

//=======================================================================================
// SCREENS
//=======================================================================================

void dispInit(void) {
  display.begin();
  display.setPowerSave(0);
  display.setContrast(255);
  CBI(PORTB, 5);
}

// Title line of the splash and message screens: "DRAM Decay v1.0" (7x14B, 18 chars max).
static void printTitle(void) {
  display.setFont(FONT_BIG);
  display.setCursor(2, 12);
  display.print(F("DRAM Decay v"));
  display.print(F(DECAY_VERSION));
}

// Two-line message (errors / terminal states) under the title.
void dispMsg(const __FlashStringHelper *l1, const __FlashStringHelper *l2) {
  char buf[22];
  OLED_BEGIN()
  printTitle();
  strncpy_P(buf, (PGM_P)l1, 21); buf[21] = 0;
  printCentered(36, buf);
  if (l2) {
    strncpy_P(buf, (PGM_P)l2, 21); buf[21] = 0;
    printCentered(56, buf);
  }
  OLED_END();
}

// Splash / start-up summary: version, part, geometry, datasheet refresh spec, measured
// row times and the resulting floor.
void dispBoot(const char *chip, uint16_t rows, uint16_t cols, uint16_t specUs,
              uint16_t wUs, uint16_t rUs, uint32_t tminUs) {
  char t[8];
  OLED_BEGIN()
  printTitle();
  display.setFont(FONT_SMALL);
  display.setCursor(2, 23);
  display.print((const __FlashStringHelper *)chip);
  display.setCursor(2, 33);
  display.print(rows); display.print('x'); display.print(cols);
  fmtTime(t, specUs);
  display.print(F("  spec ")); display.print(t);
  display.setCursor(2, 43);
  display.print(F("W ")); display.print(wUs); display.print(F("us  R ")); display.print(rUs); display.print(F("us"));
  display.setCursor(2, 53);
  fmtTime(t, tminUs);
  display.print(F("Tmin ")); display.print(t);
  display.setCursor(2, 63);
  fmtTime(t, START_US > tminUs ? START_US : tminUs);
  display.print(F("start ")); display.print(t); display.print(F(" x2 5%"));
  OLED_END();
}

// Run screen, 6 lines of 6x10:
//   <chip>                 Run <n>
//   <phase [k/N]>          <pattern>
//   Test <t>               <stage [s]>
//   ok <lo>                bad <hi>
//   Last: OK | Last: <n> err @<row>,<col>
//   Ret. <R> (<k> pat) | Ret. >=<R> | Ret. ?
void dispRun(const RunView *v) {
  char buf[22], sec[8];
  OLED_BEGIN()
  display.setFont(FONT_SMALL);

  // line 1: part number only (the name is "<part> <geometry>"), run counter right
  display.setCursor(0, 8);
  strncpy_P(buf, v->chip, 21); buf[21] = 0;
  char *sp = strchr(buf, ' ');
  if (sp) *sp = 0;
  display.print(buf);
  strcpy_P(buf, PSTR("Run "));
  utoa(v->run, buf + 4, 10);
  printRight(8, buf);

  // line 2
  display.setCursor(0, 19);
  display.print((const __FlashStringHelper *)v->phase);
  if (v->phaseNum) {
    display.print(' ');
    display.print(v->phaseNum);
    if (v->phaseDen) { display.print('/'); display.print(v->phaseDen); }
  }
  printRight(19, v->pat);

  // line 3
  display.setCursor(0, 30);
  fmtTime(sec, v->t);
  display.print(F("Test ")); display.print(sec);
  strncpy_P(buf, (PGM_P)v->stage, 8); buf[8] = 0;
  if (v->holdLeft) {
    strcat(buf, " ");
    utoa(v->holdLeft, buf + strlen(buf), 10);
    strcat(buf, "s");
  }
  printRight(30, buf);

  // line 4
  display.setCursor(0, 41);
  display.print(F("ok "));
  if (v->lo) { fmtTime(sec, v->lo); display.print(sec); } else display.print('-');
  strcpy_P(buf, PSTR("bad "));
  if (v->hi != T_INF) fmtTime(buf + 4, v->hi); else strcat(buf, "-");
  printRight(41, buf);

  // line 5
  display.setCursor(0, 52);
  if (v->lastErr == 0xFFFF) {
    display.print(F("Last: -"));
  } else if (v->lastErr == 0) {
    display.print(F("Last: OK  late "));     // worst event lateness = age error bound
    display.print(v->lastLateUs);
    display.print(F("us"));
  } else {
    display.print(F("Last: "));
    display.print(v->lastErr);
    display.print(F(" err @"));
    display.print(v->lastRow); display.print(','); display.print(v->lastCol);
  }

  // line 6
  display.setCursor(0, 63);
  display.print(F("Ret. "));
  if (v->result == T_INF) {
    display.print('?');
  } else {
    if (v->capped) display.print(F(">="));
    fmtTime(sec, v->result);
    display.print(sec);
    display.print(F(" ("));
    display.print(v->npat);
    display.print(F(" pat)"));
  }
  OLED_END();
}

// Big-number result screen. dead = the part failed even at the floor T_MIN.
void dispResult(uint32_t us, uint8_t npat, bool capped, bool dead) {
  char sec[8];
  OLED_BEGIN()
  display.setFont(FONT_BIG);
  display.setCursor(2, 12);
  display.print(F("Retention"));
  printRightP(12, dead ? F("FAIL") : (capped ? F(">= max") : F("weakest")));
  if (dead) {
    display.setFont(FONT_BIG);
    printCentered(40, "< T min");
  } else {
    // number in the big font (digits only), unit in the text font
    fmtTime(sec, us);
    char *unit = sec + strlen(sec) - (us < 1000000UL ? 2 : 1);
    char u[3];
    strcpy(u, unit);
    *unit = 0;
    display.setFont(FONT_NUM);
    uint16_t wn = display.getStrWidth(sec);
    int16_t x = (int16_t)(128 - wn - 20) / 2;
    if (x < 0) x = 0;
    display.setCursor(x, 42);
    display.print(sec);
    display.setFont(FONT_BIG);
    display.setCursor(x + wn + 3, 42);
    display.print(u);
  }
  display.setFont(FONT_SMALL);
  display.setCursor(2, 62);
  if (dead) {
    display.print(F("cell(s) fail at floor"));
  } else {
    display.print(F("confirmed with "));
    display.print(npat);
    display.print(F(" pat."));
  }
  OLED_END();
}
