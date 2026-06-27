# Software / Firmware

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

***BEWARE***: **Never enable Fuse Bit 7 on on the HighByte (DISBLRST)!** <br/>
This will disable the RESET Function. You will no longer be able to use the Reset Button or to update your Tester! To revert you would need to performa a HighVoltage Programming - which is rather cumbersome to perform!

Example using `avrdude` with a USBasp:
```
avrdude -c usbasp -p m328p -U flash:w:Ram_Tester_5_0_7.hex:i
```

### Option 2: Compile from source using Arduino IDE

1. Open the `.ino` file in Arduino IDE
2. Upload via any ICSP method supported by the Arduino IDE

> **Note for source builders:** Changing the code or using a different compiler version may affect retention timing calibration due to compiler optimizations. 

### Not sure how to update? 
Check the Documentation on the Update procedure if you have never done this before and only have an Arduino UNO to use as programmer. Available in [English](../Docs/Update_EN.pdf) or [German](../Docs/Update_DE.pdf).

### ICSP Pinout for the SMD Version ###
Please note that Pin 1 is not GND! Pin 1 is the marked pin with the square pad on the top-righthand side. 

The following images shows the Pinout of the ICSP on the SMD Board:

![](https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Media/ICSP.jpg) 

## Firmware history

| Version | Notes |
|---|---|
| **5.0.x** | Current active Version with configuration options and adapted Timings / Tests |
| **4.2.x** | Archived Version. Verified 3732/4532 quadrant logic. Two .hex variants (with/without 32K). |
| **2.1.x** | Legacy firmware, before display support was added. |
