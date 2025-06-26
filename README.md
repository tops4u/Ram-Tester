# Vintage DRAM Tester  
Fast – Precise – Open for many 8-/16-bit systems like C64, C128, Amigas, Atari 800XL, 1040ST and others, Apple IIe, Spektrum and many more

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Media/Tester.jpeg" width="400px" align="center"/><br/>

---

## Why this tester?

Most Arduino-based DRAM testers need up to 2 mins or even more for a single 41256 RAM and check only basic functions.  
This project completes a full memory, address, data-line and retention time test **in 15 s or less**. (or even 2.5sec faster without the Display).
It is probably the fastest Arduino solution which also covers **static-column DRAMs, cell retention time and 20-pin ZIP packages**.

---

## Key features

| Feature | Benefit |
|---------|---------|
| Test time <= 15 s | Rapid diagnosis on the workbench or at retro repair events |
| Retention-time measurement | Detects weak chips by checking it meets the min retention times |
| Static-column support | Reliable testing of 44258, 514402 static column functions|
| 20-pin ZIP socket | Direct test of 20-pin ZIP DRAMs without an adapter |
| Optional OLED display or LED blink codes | Full text feedback or minimal hardware setup |
| Open hardware and firmware | KiCad, Gerber files and Arduino source under an open licence |

---

## Supported DRAM types 

| Capacity | DIP | 20-pin ZIP | Static column | Retention Time | Test Time |
|----------|-----|-----------|---------------|----------------|-----------|
| 16 K × 4 | 4416 | – | – | 4ms | 2.6sec |
| 64 K × 1 | 4164 | - | – | 2ms | 3.5 sec |
| 64 K × 4 | 4464 | – | – | 4ms | 5.2 sec |
| 256 K × 1 | 41256 | - | – | 4ms | 8.9 sec |
| 256 K × 4 | 44256 | both | 44258 | 4ms | 3.8 sec|
| 1 M x 1 | 411000 | NO! | - | 8ms | 28 sec |
| 1 M × 4 | 514400 | both | 514402 | 16ms | 8.9 sec |

Note: Above test times include the OLED Display. Without Display, the test durations are approx 1.7 sec shorter.

---

## Test procedure

1. Insert the device (16, 18 or 20 pins, DIP or ZIP).  
2. Connect USB power supply.  
3. Read the result  
   * OLED version: plain-text report on the display  
   * LED-only version: green = pass, red = fail

There is a short YouTube video demonstrating the tester in action. <br/>
[![YouTube Demonstrator Video](https://img.youtube.com/vi/9TBlnfiTfQk/0.jpg)](https://www.youtube.com/watch?v=9TBlnfiTfQk "Demonstration")<br/>
*(Note: this video shows a prototype, the current SMD Design can be seen above. Handling without Display remains identical):*  

---

## What does it test?
1. GND Shorts, if you RAM has a Short to GND on any Pin
2. Power Supply Shorts - well there is a resetable fuse to protect the board
3. Addressline or decoder faults
4. Stuck Cells or Crosstalk by using various patterns
5. Random patterns combined with retention time checks
6. All of the above uses Fast Page Mode or Static Column control depending on RAM Chip type

### So why no MARCH-B?
Here is the analytics of this algorithm vs. March-B

| Aspect | Actual Code | MARCH-B |
| --- | --- | --- |
| Pattern Coverage     | ✅ 0x00, 0xFF, 0xAA, 0x55 | ✅                   | 
| 0, 1Transition Tests | ✅ Thru Pattern-Sequence  | ✅ Thru R0W1, R1W0   |
| Address Sequence     | ⚠️ Ascending only         | ✅ Asc-/Descending   |
| Coupling Detection   | ✅ By Retention-Delay     | ✅ Systematically    |
| Real Retention       | ✅ 2-16ms Tests           | ❌ Only µs-Range     |

---
## Build or buy
Sales thread on Amibay: [https://www.amibay.com](https://www.amibay.com/threads/memory-tester.2450230/)<br/>
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
