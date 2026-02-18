# Vintage DRAM Tester  
Fast – Precise – Open for many 8-/16-bit systems like C64, C128, Amigas, Atari 800XL, 1040ST and others, Apple IIe, Spektrum and many more

## What this project is
A fast, open-source DIY DRAM tester for vintage RAM chips used in C64, Amiga, Atari, and other retro computers. Tests memory thoroughly in seconds and supports a wide range of chip types with optional OLED feedback.

**As featured on:** 

- ***Hackaday*** (Dec 2025): "Cheap and Aggressive DRAM Chip Tester"
- ***Elektor Magazine*** (Dez 2025): "Ram-Tester Is an Open-Source DIY Solution for Retro Computer RAM"

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Media/IMGP7463 (1).jpeg" width="400px" align="center"/><br/>

---

## Why this tester?

Most Arduino-based DRAM testers need up to 2 mins or even more for a single 41256 RAM and check only basic functions.  
This project completes a full memory, address, data-line and retention time test **in 15 s or less**. (or even 2.5sec faster without the Display).
It is probably the fastest Arduino solution which also covers **static-column DRAMs, cell retention time and 20-pin ZIP packages**.

---

## Key features

| Feature | Benefit |
|---------|---------|
| Most tests are <= 10s | Rapid diagnosis on the workbench or at retro repair events |
| Retention-time measurement | Detects weak chips by checking it meets the min retention times |
| Static-column support | Reliable testing of 44258, 514402 static column functions|
| Nibble Mode support | Testing 41257 RAM with nibble Mode access |
| 20-pin ZIP socket | Direct test of 20-pin ZIP DRAMs without an adapter |
| Optional OLED display or LED blink codes | Full text feedback or minimal hardware setup |
| Open hardware and firmware | KiCad, Gerber files and Arduino source under an open licence |
| Self Test Mode | Check the HW for any defects like short circuit, broken soldering, etc |

---

## Supported DRAM types (Speed with current Firmware Version)

| Capacity | DIP | 20-pin ZIP | Static column | Nibble Mode | Retention Time | Test Time |
|----------|-----|-----------|---------------|----------------|-----------|
| 4 K x 1 | 4027 <sup>1)</sup>| - | - | - | 2ms  | 1.3sec |
| 16 K x 1 | 4816 | - | - | - | 2ms | 1.6sec |
| 16 K x 1 | 4116 <sup>1)</sup> | - | - | - | 2ms | 1.6sec |
| 16 K × 4 | 4416 | – | – | - | 4ms | 4.2sec |
| 64 K × 1 | 4164 | - | – | - | 4ms | 2.8sec |
| 64 K × 4 | 4464 | – | – | - | 4ms | 6.4sec |
| 256 K × 1 | 41256 | - | – | 41257 | 4ms | 7.9sec |
| 256 K × 4 | 44256 | both | 44258 | - | 4ms | 4.5sec|
| 1 M x 1 | 411000 | **!NO!** | - | - | 8ms | 26.1sec |
| 1 M × 4 | 514400 | both | 514402 | - | 16ms | 12.8sec |

 <sup>1)</sup>requires the 4116 adapter board.

Note: Above test times include the OLED Display. Without Display, the test durations are approx 1.7 sec shorter.

---

## Test procedure

1. Insert the device (16, 18 or 20 pins, DIP or ZIP).
2. Dial the needed DIP Switch to ON - depending on the number of Pins of your RAM
3. Connect USB power supply.  
4. Read the result  
   * OLED version: plain-text report on the display  
   * LED-only version: green = pass, red = fail

There is a short YouTube video demonstrating the tester in action. <br/>
[![YouTube Demonstrator Video](https://img.youtube.com/vi/vsYpcPfiFhY/0.jpg)](https://youtu.be/vsYpcPfiFhY "Demonstration")<br/>

---

## What does it test?
1. GND Shorts, if you RAM has a Short to GND on any Pin
2. Power Supply Shorts - well there is a resetable fuse to protect the board
3. Addressline or decoder faults
4. Stuck Cells or Crosstalk by using various patterns
5. Random patterns combined with retention time checks
6. CAS-before-RAS Refresh Timer Function
7. All of the above uses Fast Page Mode or Static Column control depending on RAM Chip type

### So why no MARCH-B?
Here is the analytics of this algorithm vs. March-B

| Aspect | Actual Code | MARCH-B |
| --- | --- | --- |
| Pattern Coverage     | ✅ 0x00, 0xFF, 0xAA, 0x55 | ✅                   | 
| 0, 1Transition Tests | ✅ Thru Pattern-Sequence  | ✅ Thru R0W1, R1W0   |
| Address Sequence     | ⚠️ Ascending only         | ✅ Asc-/Descending   |
| Coupling Detection   | ✅ By Retention-Delay     | ✅ Systematically    |
| Real Retention       | ✅ 2-16ms Tests           | ❌ Only µs-Range     |

### Why is it probably better than many other Arduino Based Ram-Testers?
Compared with many other open source projects:
- They usually don't test for address line faults. Tests will pass even if you bend one address pin up.
- They usually don't check multiple rows at a time, since they use slow Arduino read and write commands.
- Many use simple all 0 and all 1 tests, no patterns or random data tests.
- No retention tests possible since writing and reading one row alone with Arduino I/O takes more than one second - usual retention times are within a few milliseconds.
- No checks for broken chips (i.e. shorts to GND).
- No protection against short circuits on supply lines. Some use off-the-shelf DC-DC converters which supply high currents in short circuit situations.
- No tests of refresh or static column functionality.
- Very often limited to 1-bit or 4-bit circuits and no ZIP sockets.
- They are usually **MUCH** slower.

---
## Build or buy - the choice is yours
Sales thread on Amibay: [https://www.amibay.com](https://www.amibay.com/threads/memory-tester.2450230/)<br/>
Product Listing on Tindie: [https://www.tindie.com](https://www.tindie.com/products/38927/)<br/>
Product Listing on EBAY: [https://www.ebay.ch](https://www.ebay.ch/itm/136743995188)<br/>
DIY order at PCBWay for Thru-Hole Version: [https://www.pcbway.com](https://www.pcbway.com/project/shareproject/Ram_Tester_ThruHole_Version_93863356.html)<br/>
Use the provided Gerber Files from this Repo

---

## Documentation

* **Wiki** – assembly and operating guide  
* **Software** – source code 
* **Schematic** – KiCad project and Gerber files  
* **Changelog**

---

## Contributing

Pull requests, issues and forks are welcome.  
Questions: GitHub Discussions or contact **tops4u** on AmiBay.

Open-source hardware under GPL v3 – use at your own risk.
