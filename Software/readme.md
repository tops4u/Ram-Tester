# Software / Firmware

## Which firmware do I need?

There are **two versions** of the firmware:

| Firmware | File | 32K Testing | Best for |
|---|---|---|---|
| **Standard + 32K** | `Ram_Tester_4.2.3_32k.hex` | ✅ Enabled | Users who test 3732/4532 or want full diagnostics on 4164 |
| **Standard** | `Ram_Tester_4.2.3.hex` | ❌ Disabled | Users who only test 4164 and other standard types |

**Not sure which one to pick?** If you repair ZX Spectrums or work with 32K RAM, use the 32K version. If you never touch 3732 or 4532 chips, use the standard version — it keeps things simpler.

> **Users without a display should use the standard version** (without 32K), since the 32K results rely on on-screen text to distinguish chip types.

## What's the difference?

With **32K enabled**, the tester can identify MSM3732 and TMS4532 chips. But this changes how defective 4164 chips are reported: instead of simply showing "defective", the tester will check whether the chip still works as a 3732 or 4532 and report that instead. Only chips that have no usable 32K half are reported as defective. See the [32K documentation](../Docs/32K-Option) for details on how this works.

With **32K disabled**, any defective 4164 is simply reported as defective. Any 3732 or 4532 inserted will also be reported as a bad 4164, since the tester doesn't know to look for a working half.

## How to tell which version is on your tester

Select an invalid DIP switch combination to show the firmware version on the display:

- **Standard firmware**: white text on black background, e.g. `Ver.:4.2.3`
- **32K firmware**: black text on white background (inverted), with suffix "32", e.g. `Ver.:4.2.3 32`

When testing a 4164 with 32K firmware enabled, any result showing a 3732 or 4532 type will also be displayed with **inverted text** — this is a visual hint that the chip was inserted as a 4164 but is only partially functional.

## How to flash the firmware

There are two ways to update:

### Option 1: Flash the .hex file directly (recommended)

Use any ISP programmer (e.g. USBasp, T48, or an Arduino as ISP) connected to the **ICSP header** on the tester board.

**Fuse settings** (ATmega328P):
| Fuse | Value |
|---|---|
| Low Byte | `0xFF` |
| High Byte | `0xDF` |
| Extended | `0xFF` |
| Lock Bit | `0xFF` |

Example using `avrdude` with a USBasp:
```
avrdude -c usbasp -p m328p -U flash:w:Ram_Tester_4.2.3_32k.hex:i
```

### Option 2: Compile from source using Arduino IDE

1. Open the `.ino` file in Arduino IDE
2. To **disable 32K logic**: open `common.h` and change the line `#define ENABLE_32K` to `// #define ENABLE_32K` (add `//` in front)
3. Upload via any ICSP method supported by the Arduino IDE

> **Note for source builders:** Changing the code or using a different compiler version may affect retention timing calibration due to compiler optimizations. If you only want to toggle 32K support, use the `#define` switch and avoid other changes.

### Not sure how to update? 
Check the Documentation on the Update procedure if you have never done this before and only have an Arduino UNO to use as programmer. Available in [English](../Docs/Update_EN.pdf) or [German](../Docs/Update_DE.pdf).

## Firmware history

| Version | Notes |
|---|---|
| **4.2.x** | Current release. Verified 3732/4532 quadrant logic. Two .hex variants (with/without 32K). |
| **2.1.x** | Legacy firmware, before display support was added. |
