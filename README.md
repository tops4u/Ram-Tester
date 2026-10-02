# Ram-Tester
**Fast, open-source DIY tester for vintage DRAM/SRAM** — auto-detects the
chip type, runs every test, gives a clear good/bad in seconds.

[GPL v3] [Firmware 5.1.2] · C64 · Amiga · Atari · ST · Apple II · Spectrum

## What this project is
A fast, open-source DIY RAM tester for vintage RAM chips used in C64, Amiga, Atari, and other retro computers. Built around a 16MHz ATmega328P on a custom PCB, it tests memory thoroughly in seconds and supports a wide range of chip types with optional OLED feedback.

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Media/IMGP7463 (1).jpeg" width="400px" align="center"/><br/>

**As featured on:**

- [***Jan Beta***](https://www.youtube.com/watch?v=iQYF2-FwuoI) (Aug 2026): "The best Arduino Tester (so far)"
- [***Adrians Digital Basement II***](https://youtu.be/9QQ8ZqHPRVQ?&t=2573) (May 2026): "This is freaking awesome"
- [***Hackaday***](https://hackaday.com/2025/12/08/cheap-and-aggressive-dram-chip-tester/) (Dec 2025): "Cheap and Aggressive DRAM Chip Tester"
- [***Elektor Magazine***](https://www.elektormagazine.com/news/open-source-diy-ram-tester) (Dec 2025): "Ram-Tester Is an Open-Source DIY Solution for Retro Computer RAM"

---

## Why this tester?
- **No chip knowledge needed** – Set the pin count via DIP switch — the tester auto-detects the chip type and runs all tests automatically. No selecting algorithms, no choosing the right socket, no menus, no datasheet lookups. Grab a chip, plug it in, get a result.
- **Fast** – Full test in under 8 seconds for a 41256. Test a whole tray of chips in minutes.
- **Thorough** – While other testers require you to choose between test modes, this one runs them all: March-B address decoder test, memory patterns, crosstalk, retention time, CAS-before-RAS refresh, fast page mode, static column mode, short detection (VCC, GND and pin-to-pin).
- **Practical** – Broken is broken. You get a clear good/bad result because a DRAM chip can't be repaired anyway.
- **Safe** – Short-circuit protection, current limiting via resettable fuse, short detection against VCC, GND and between pins. Self-test mode included.
- **Fully Open Source** – Hardware, firmware, schematics. No black box.

---

## Key features
| Feature | Benefit |
|---------|---------|
| 20-pin ZIP socket on board | Direct test of 256K × 4 and 1M × 4 ZIP DRAMs without an adapter |
| Optional ZIP/SOJ adapter | Adds 1M × 1 ZIP and 41256 ZIP support, plus SOJ sockets for 18- and 20-pin SOJ RAMs <sup>1)</sup> |
| Optional OLED display or LED blink codes | Full text feedback or minimal hardware setup |
| Self-test mode | Verify the hardware for defects like short circuits or broken solder joints |

<sup>1)</sup> The 1M × 1 ZIP pinout differs from the 256K × 4 / 1M × 4 ZIP pinout, so these chips cannot use the on-board ZIP socket. See [Supported DRAM types](#supported-dram-types-speed-with-current-firmware-version).

---

## Supported DRAM types (speed with current firmware version)

| Capacity | DIP | 20-pin ZIP | Static Column | Nibble Mode | Retention Time | Test Time |
|----------|-----|------------|---------------|-------------|----------------|-----------|
| 4 K × 1 | 4027 <sup>1)</sup> | – | – | – | 2 ms | 0.2 s |
| 16 K × 1 | 4816 | – | – | – | 2 ms | 0.7 s |
| 16 K × 1 | 4116 <sup>1)</sup> | – | – | – | 2 ms | 0.5 s |
| 16 K × 4 | 4416 | – | – | – | 4 ms | 1.7 s |
| 32 K × 1 | 3732 <sup>2)</sup> | – | – | – | 2/4 ms | 1.8 s |
| 32 K × 1 | 4532 <sup>2)</sup> | – | – | – | 2/4 ms | 1.8 s |
| 64 K × 1 | 4164 | – | – | – | 2/4 ms | 1.8 s |
| 64 K × 4 | 4464 | – | – | – | 4 ms | 5.2 s |
| 256 K × 1 | 41256 | adapter <sup>3)</sup> | – | 41257 | 4 ms | 7.5 s |
| 256 K × 4 | 44256 | on board | 44258 | – | 8 ms | 3.5 s |
| 1 M × 1 | 411000 / 511000 | adapter <sup>3)</sup> | – | – | 8 ms | 19.4 s |
| 1 M × 4 | 514400 | on board | 514402 | – | 16 ms | 12.0 s |

<sup>1)</sup> Requires the [4116 adapter board](Schematic/4116).<br/>
<sup>2)</sup> Half-good 4164 chips sold as 32K × 1 (OKI MSM3732 / TI TMS4532). Enabled by default since firmware 4.2.3. See [32K documentation](Docs/32K-Option) for details.<br/>
<sup>3)</sup> The 1M × 1 ZIP pinout differs from the 256K × 4 / 1M × 4 ZIP pinout, so 411000 / 511000 ZIP chips cannot be tested in the on-board ZIP socket. The separate ZIP/SOJ adapter provides the matching ZIP socket for 1M × 1 and 41256, plus two SOJ sockets for 20-pin and 18-pin SOJ RAMs.

**Static Column** means the RAM allows column changes while CAS is held low — faster than standard page mode. **Nibble Mode** delivers four consecutive bits from one column address.

## Supported SRAM types (speed with current firmware version)

| Capacity | DIP | Test Time |
|----------|-----|-----------|
| 1 K × 4 | 2114 <sup>1)</sup> | 0.4 s |

<sup>1)</sup> **WARNING:** 2114 SRAM needs to be inserted 180° rotated, with Pin 10 of the SRAM on the ZIF Pin 1 marking.

---

## Test procedure

1. Insert the device (16, 18 or 20 pins, DIP or ZIP). 1M × 1 ZIP, 41256 ZIP and SOJ chips go into the ZIP/SOJ adapter.
2. Set the DIP switch to match the pin count of your RAM. See the [operation manual](Docs) for the switch settings.
3. Connect USB power supply (or press RESET if already powered).
4. Read the result
   * OLED version: plain-text report on the display
   * LED-only version: green = pass, red = fail

There is a short YouTube video demonstrating the tester in action. <br/>
[![YouTube Demonstrator Video](https://img.youtube.com/vi/vsYpcPfiFhY/0.jpg)](https://youtu.be/vsYpcPfiFhY "Demonstration")<br/>

---

## How does it test?
1. Shorts — checks whether any pin is shorted to GND, to VCC (5 V) or to another pin
2. Chip detection — detects whether a chip is present and identifies its type
3. Address line and decoder faults — March-B sequence applied to the address lines
4. Stuck cells and crosstalk — bidirectional checkerboards
5. Random patterns combined with retention time checks
6. CAS-before-RAS refresh counter function (if supported by the RAM)
7. All of the above use the appropriate access mode for the chip type: Fast Page Mode, Static Column, or Nibble Mode

### Test algorithm and March-B
The tester combines a March-B sequence with chip-specific stress tests.

**March-B for address decoding.** Address line and decoder faults are detected with a March-B sequence (R0W1, R1W0, ascending and descending) applied to a reduced address set that exercises every row and column address line individually. This catches shorted, open or stuck address lines and decoder aliasing systematically, without running a full cell-level march.

**Why not a full cell-level March-B?** For the fault classes that matter in practice, the combination above offers comparable coverage, and it adds what a full March-B leaves out:

- **Access modes:** A classic March-B accesses every cell individually (direct single-cell read/write). It does not exercise the full feature set of a RAM chip, such as Fast Page Mode, Static Column, Nibble Mode or CAS-before-RAS refresh. This tester tests every feature the chip supports.
- **Retention:** March-B runs in the microsecond range per access, so it cannot catch chips with weak retention that pass on paper yet fail under real refresh timing, which is where most age-related failures occur. Cell-level coverage here comes from bidirectional checkerboards and random patterns combined with real retention delays.
- **Test time:** A full cell-level March-B on large chips such as 1M × 1 or 1M × 4 is not feasible on an ATmega328P within a practical test time. The goal is a reliable result in seconds, not minutes.

| Aspect | This Tester | Full cell-level March-B |
| --- | --- | --- |
| Address line / decoder faults | ✅ March-B on address lines | ✅ |
| Stuck cells, 0→1 / 1→0 transitions | ✅ Checkerboard + random patterns | ✅ |
| Address sequence | ✅ Ascending + descending | ✅ Ascending + descending |
| Crosstalk / coupling | ✅ Bidirectional checkerboards | ✅ Systematic |
| VCC/GND short detection | ✅ Explicit, before test | ⚠️ Implicit only <sup>1)</sup> |
| Pin-to-pin short detection | ✅ Explicit, before test | ⚠️ Implicit only <sup>1)</sup> |
| Access modes | ✅ FPM, Static Column, Nibble Mode | ❌ Direct single-cell access only |
| CBR refresh | ✅ CAS-before-RAS test <sup>2)</sup> | ❌ Not covered <sup>3)</sup> |
| Real retention | ✅ 2–16 ms per chip spec | ❌ Not covered <sup>3)</sup> |
| Test time on ATmega328P | ✅ Seconds | ❌ Not practical for large chips |

<sup>1)</sup> a short makes the chip fail the march, but the fault is not identified as a short<br>
<sup>2)</sup> for RAMs with a refresh counter (41256 and newer)<br>
<sup>3)</sup> unless implemented outside of March-B

### A note on CMOS vs TTL voltage levels
Vintage DRAM chips were designed for TTL signal levels. The ATmega328P drives CMOS levels at 5 V — logic high is close to V<sub>CC</sub>, which is above what the original systems delivered. This means marginal chips that fail at true TTL thresholds may still pass on this tester (and most other microcontroller-based testers).
Some designs use 3.3 V controllers with 5 V-tolerant I/O to get closer to TTL levels, but the ATmega328P does not support this operating mode. In practice, virtually no consumer DRAM tester on the market operates at true TTL levels — this is a fundamental limitation of the approach, not specific to this project. For definitive TTL-level testing, dedicated vintage test equipment (e.g. Advantest, Agilent) operating at calibrated thresholds would be needed.

### Experimental: Retention Time Measurement firmware
An alternative, experimental firmware measures how long the chip actually keeps its data without refresh, determined by its weakest cell, instead of giving a pass/fail result. It is currently available for 4164, 4464 and 44256/514256. Flashing it temporarily replaces the regular tester firmware. See [Software](Software) for details.

---

## Build or buy — the choice is yours

**Buy ready-made:** [Amibay](https://www.amibay.com/threads/memory-tester.2450230/) · [Lectronz](https://lectronz.com/products/ram-tester) · [eBay](https://www.ebay.ch/itm/136743995188) · [Tindie](https://www.tindie.com/products/reusecircuit/ram-tester-for-2114-4116-4164-41256-441000-514256/)

**Build it yourself (DIY):** Available as a beginner-friendly through-hole (THT) version or a compact SMD version. Order PCBs at [PCBWay (TH)](https://www.pcbway.com/project/shareproject/Ram_Tester_ThruHole_Version_93863356.html) or use the provided Gerber files from the [Schematic](Schematic) folder.

---

## Documentation

* [**Operation manual**](Docs) – test procedure, error codes, self-test and troubleshooting
* [**Software / Firmware**](Software) – firmware variants, flashing instructions
* [**Schematic**](Schematic) – KiCad project and Gerber files (THT and SMD)
* [**Changelog**](changelog.md) – firmware version history
* [**Compatibility**](compatibility.md) – manufacturer cross-reference and system DRAM/SRAM usage

---

## Building or selling your own units

You're welcome to build, modify and even sell your own Ram-Testers — that's the
point of open hardware. In return, the license asks you to keep it open:

- **It stays open-source.** Boards you distribute or sell remain under
  CERN-OHL-S v2 (hardware) and GPL v3 (firmware). No closing the design, no
  proprietary fork.
- **Pass on the source.** If you sell or give away boards, make the complete
  design files for *your* version — including any changes you made — available
  under the same license.
- **Keep the credit and the link.** Leave the copyright and license notices
  intact and keep the source location — https://github.com/tops4u/Ram-Tester/ —
  visible, on the board silkscreen where practicable (CERN-OHL-S §4).
- **Don't strip attribution.** Selling it as your own closed product, or
  removing the notices, is not permitted.
- **Article description.** For online platforms you need to include:
  Creator, link to the source files (this GitHub repo) and licence.

In short: build it, improve it, sell it — just keep it open and point people
back here. If unsure, open a Discussion and ask.

---

## Contributing

Pull requests, issues and forks are welcome.
Questions: GitHub Discussions or contact **tops4u** on AmiBay.

Open-source hardware (CERN-OHL-S v2) and firmware (GPL v3) – use at your own risk.
