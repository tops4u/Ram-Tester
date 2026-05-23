# Vintage DRAM Tester for 15 different RAM Types
Fast – Precise – Open for many 8-/16-bit systems like C64, C128, Amigas, Atari 800XL, 1040ST and others, Apple IIe, Spectrum and many more

## What this project is
A fast, open-source DIY DRAM tester for vintage RAM chips used in C64, Amiga, Atari, and other retro computers. Built around an ATmega328P on a custom PCB, it tests memory thoroughly in seconds and supports a wide range of chip types with optional OLED feedback.

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Media/IMGP7463 (1).jpeg" width="400px" align="center"/><br/>

**As featured on:**

- [***Adrians Digital Basement II***](https://youtu.be/9QQ8ZqHPRVQ?&t=2573) (May 2026): "This is freaking awesome"
- [***Hackaday***](https://hackaday.com/2025/12/08/cheap-and-aggressive-dram-chip-tester/) (Dec 2025): "Cheap and Aggressive DRAM Chip Tester"
- [***Elektor Magazine***](https://www.elektormagazine.com/news/open-source-diy-ram-tester) (Dec 2025): "Ram-Tester Is an Open-Source DIY Solution for Retro Computer RAM"

---

## Why this tester?
- **No chip knowledge needed** – Set the pin count via DIP switch, the tester auto-detects the chip type. No chip marking deciphering, no menus, no datasheet lookups. Grab a chip from the parts bin, plug it in, get a result.
- **Fast** – Most tests complete in under 10 seconds. Full 41256 test in 7.4s — up to 60× faster than typical Arduino-based testers that rely on `digitalWrite()`. Direct port manipulation at 62.5 ns per operation makes the difference.
- **Thorough** – Six test patterns per cell, crosstalk analysis, address line verification, retention time measurement, CAS-Before-RAS refresh, fast page mode, static column, nibble mode and ground short detection.
- **Practical** – Clear good/bad result. A DRAM chip can't be repaired, so detailed fault classification is academic. This tester tells you what you need to know to get your machine running.
- **Safe** – Short-circuit protection, current limiting, ground short detection, resettable fuse. Self-test mode to verify your build.
- **Fully open** – KiCad schematics, Gerber files and Arduino source under GPL v3. No black box.

---
## Key features
| Feature | Benefit |
|---------|---------|
| 20-pin ZIP socket | Direct test of 20-pin ZIP DRAMs without an adapter |
| Optional OLED display or LED blink codes | Full text feedback or minimal hardware setup |
| Self-test mode | Verify the hardware for defects like short circuits or broken solder joints |

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

| Aspect | This Tester | MARCH-B |
| --- | --- | --- |
| Pattern Coverage     | ✅ 0x00, 0xFF, 0xAA, 0x55 | ✅                   |
| 0→1 / 1→0 Transition | ✅ Via pattern sequence  | ✅ Via R0W1, R1W0   |
| Address Sequence     | ⚠️ Ascending only         | ✅ Asc + Descending   |
| Coupling Detection   | ✅ Via retention delay     | ✅ Systematic    |
| Real Retention       | ✅ 2–16 ms per chip spec  | ❌ Not covered <sup>1)</sup>   |
| GND Short Detection  | ✅ Before power-up        | ⚠️ Implicit only <sup>1)</sup>      |
| Address Line Faults  | ✅ Bit-independence check  | ⚠️ Implicit only <sup>1)</sup>    |
| CBR Refresh          | ✅ CAS-before-RAS test    | ❌ Not covered <sup>1)</sup>      |

<sup>1)</sup>unless implemented outside of MARCH-B

March-B is thorough for coupling and decoder faults but operates in the microsecond range — it cannot catch chips with weak retention that meet spec on paper but fail under real-world refresh timing. This tester trades systematic address ordering for real-time retention stress, which is where most age-related failures actually occur.

### A note on CMOS vs TTL voltage levels
Vintage DRAM chips were designed for TTL signal levels. The ATmega328P drives CMOS levels at 5 V — logic high is close to V<sub>CC</sub>, which is above what the original systems delivered. This means marginal chips that fail at true TTL thresholds may still pass on this tester (and most other microcontroller-based testers).
Some designs use 3.3 V controllers with 5 V-tolerant I/O to get closer to TTL levels, but the ATmega328P does not support this operating mode. In practice, virtually no consumer DRAM tester on the market operates at true TTL levels — this is a fundamental limitation of the approach, not specific to this project. For definitive TTL-level testing, dedicated vintage test equipment (e.g. Advantest, Agilent) operating at calibrated thresholds would be needed.

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
