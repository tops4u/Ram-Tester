# Vintage DRAM Tester for 15 different RAM Types
Fast – Precise – Open for many 8-/16-bit systems like C64, C128, Amigas, Atari 800XL, 1040ST and others, Apple IIe, Spectrum and many more

## What this project is
A fast, open-source DIY DRAM tester for vintage RAM chips used in C64, Amiga, Atari, and other retro computers. Built around an ATmega328P on a custom PCB, it tests memory thoroughly in seconds and supports a wide range of chip types with optional OLED feedback.

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Media/IMGP7463 (1).jpeg" width="400px" align="center"/><br/>

**As featured on:**

- [***Hackaday***](https://hackaday.com/2025/12/08/cheap-and-aggressive-dram-chip-tester/) (Dec 2025): "Cheap and Aggressive DRAM Chip Tester"
- [***Elektor Magazine***](https://www.elektormagazine.com/news/open-source-diy-ram-tester) (Dec 2025): "Ram-Tester Is an Open-Source DIY Solution for Retro Computer RAM"

---

## Why this tester?

Most Arduino-based DRAM testers need up to 2 mins or even more for a single 41256 RAM and check only basic functions.
This project completes a full memory, address, data-line and retention time test **in 15 s or less** (or even 2.5 s faster without the display).
It is probably the fastest Arduino solution which also covers **static-column DRAMs, cell retention time and 20-pin ZIP packages**.

---

## Key features

| Feature | Benefit |
|---------|---------|
| Most tests are ≤ 10 s | Rapid diagnosis on the workbench or at retro repair events |
| Retention-time measurement | Detects weak chips by checking they meet the minimum retention times |
| Static-column support | Reliable testing of 44258, 514402 static-column functions |
| Nibble Mode support | Testing 41257 RAM with nibble mode access |
| 20-pin ZIP socket | Direct test of 20-pin ZIP DRAMs without an adapter |
| Optional OLED display or LED blink codes | Full text feedback or minimal hardware setup |
| Open hardware and firmware | KiCad, Gerber files and Arduino source under an open licence |
| Self-test mode | Check the hardware for defects like short circuits, broken solder joints, etc. |
| Automatic detection | Automatically identifies the chip type and checks for reusability of broken parts |

---

## Supported DRAM types (speed with current firmware version)

| Capacity | DIP | 20-pin ZIP | Static Column | Nibble Mode | Retention Time | Test Time |
|----------|-----|------------|---------------|-------------|----------------|-----------|
| 4 K × 1 | 4027 <sup>1)</sup> | – | – | – | 2 ms | 1.3 s |
| 16 K × 1 | 4816 | – | – | – | 2 ms | 1.6 s |
| 16 K × 1 | 4116 <sup>1)</sup> | – | – | – | 2 ms | 1.6 s |
| 16 K × 4 | 4416 | – | – | – | 4 ms | 4.2 s |
| 32 K × 1 | 3732 <sup>2)</sup> | – | – | – | 2/4 ms | 2.8 s |
| 32 K × 1 | 4532 <sup>2)</sup> | – | – | – | 2/4 ms | 2.8 s |
| 64 K × 1 | 4164 | – | – | – | 2/4 ms | 2.8 s |
| 64 K × 4 | 4464 | – | – | – | 4 ms | 6.4 s |
| 256 K × 1 | 41256 | – | – | 41257 | 4 ms | 7.4 s |
| 256 K × 4 | 44256 | both | 44258 | – | 8 ms | 4.1 s |
| 1 M × 1 | 411000 | **!NO!** | – | – | 8 ms | 25.9 s |
| 1 M × 4 | 514400 | both | 514402 | – | 16 ms | 12.8 s |

<sup>1)</sup> Requires the [4116 adapter board](Schematic/4116).<br/>
<sup>2)</sup> Half-good 4164 chips sold as 32K × 1 (OKI MSM3732 / TI TMS4532). Enabled by default since firmware 4.2.3. See [32K documentation](Docs/32K-Option) for details.

**Static Column** means the RAM allows column changes while CAS is held low — faster than standard page mode. **Nibble Mode** delivers four consecutive bits from one column address.

Note: Above test times include the OLED display. Without display, test durations are approx. 1 s shorter.

---

## Test procedure

1. Insert the device (16, 18 or 20 pins, DIP or ZIP).
2. Set the DIP switch to match the pin count of your RAM. See the [operation manual](Docs) for the switch settings.
3. Connect USB power supply (or press RESET if already powered).
4. Read the result
   * OLED version: plain-text report on the display
   * LED-only version: green = pass, red = fail

There is a short YouTube video demonstrating the tester in action. <br/>
[![YouTube Demonstrator Video](https://img.youtube.com/vi/vsYpcPfiFhY/0.jpg)](https://youtu.be/vsYpcPfiFhY "Demonstration")<br/>

---

## What does it test?
1. GND shorts — checks if any pin is shorted to ground
2. Power supply shorts — a resettable fuse protects the board
3. Address-line and decoder faults
4. Stuck cells or crosstalk using various patterns
5. Random patterns combined with retention time checks
6. CAS-before-RAS refresh timer function
7. All of the above use the appropriate access mode for the chip type: Fast Page Mode, Static Column, or Nibble Mode

### So why no MARCH-B?
Here is the analytics of this algorithm vs. March-B

| Aspect | Actual Code | MARCH-B |
| --- | --- | --- |
| Pattern Coverage     | ✅ 0x00, 0xFF, 0xAA, 0x55 | ✅                   |
| 0, 1Transition Tests | ✅ Thru Pattern-Sequence  | ✅ Thru R0W1, R1W0   |
| Address Sequence     | ⚠️ Ascending only         | ✅ Asc-/Descending   |
| Coupling Detection   | ✅ By Retention-Delay     | ✅ Systematically    |
| Real Retention       | ✅ 2–16 ms Tests          | ❌ Only µs-Range     |

### Why is it probably better than many other Arduino-based RAM testers?
Compared with many other open source projects:
- They usually don't test for address line faults. Tests will pass even if you bend one address pin up.
- They usually don't check multiple rows at a time, since they use slow Arduino read and write commands.
- Many use simple all-0 and all-1 tests, no patterns or random data tests.
- No retention tests possible since writing and reading one row alone with Arduino I/O takes more than one second — usual retention times are within a few milliseconds.
- No checks for broken chips (i.e. shorts to GND).
- No protection against short circuits on supply lines. Some use off-the-shelf DC-DC converters which supply high currents in short circuit situations.
- No tests of refresh or static column functionality.
- Very often limited to 1-bit or 4-bit circuits and no ZIP sockets.
- They are usually **MUCH** slower.

---
## Build or buy — the choice is yours

**Buy ready-made:** [Amibay](https://www.amibay.com/threads/memory-tester.2450230/) · [Tindie](https://www.tindie.com/products/38927/) · [eBay](https://www.ebay.ch/itm/136743995188)

**Build it yourself (DIY):** Available as a beginner-friendly through-hole (THT) version or a compact SMD version. Order PCBs at [PCBWay (THT)](https://www.pcbway.com/project/shareproject/Ram_Tester_ThruHole_Version_93863356.html) or use the provided Gerber files from the [Schematic](Schematic) folder.

---

## Documentation

* [**Operation manual**](Docs) – test procedure, error codes, self-test and troubleshooting
* [**Software / Firmware**](Software) – firmware variants, flashing instructions
* [**Schematic**](Schematic) – KiCad project and Gerber files (THT and SMD)
* [**Changelog**](changelog.md) – firmware version history
* [**Compatibility**](compatibility.md) – manufacturer cross-reference and system DRAM usage

---

## Contributing

Pull requests, issues and forks are welcome.
Questions: GitHub Discussions or contact **tops4u** on AmiBay.

Open-source hardware under GPL v3 – use at your own risk.
