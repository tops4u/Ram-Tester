# Vintage DRAM Tester  
Fast – Precise – Open for all 8-/16-bit systems

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Media/IMG_3591.jpeg" width="400px" align="center"/><br/>

---

## Why this tester?

Most Arduino-based DRAM testers need 40 s or more per IC and check only basic functions.  
This project completes a full memory, address and data-line test **in 12 s or less**.  
It is one of the fastest Arduino solutions and also covers **static-column DRAMs, cell retention time and 20-pin ZIP packages**.

---

## Key features

| Feature | Benefit |
|---------|---------|
| Test time <= 12 s | Rapid diagnosis on the workbench or at retro repair events |
| Retention-time measurement | Detects weak chips by extending the refresh pause for every cell |
| Static-column support | Reliable testing of 44258, 514402 and other SC devices |
| 20-pin ZIP socket | Direct test of 20-pin ZIP DRAMs without an adapter |
| Optional OLED display or LED blink codes | Full text feedback or minimal hardware setup |
| Open hardware and firmware | KiCad, Gerber files and Arduino source under an open licence |

---

## Supported DRAM types (selection)

| Capacity | DIP | 20-pin ZIP | Static column |
|----------|-----|-----------|---------------|
| 16 K × 4 | 4416 | – | – |
| 64 K × 1 | 4164 | - | – |
| 64 K × 4 | 4464 | – | – |
| 256 K × 1 | 41256 | - | – |
| 256 K × 4 | 44256 | both | 44258 |
| 1 M × 4 | 514256, 514400 | both | 514402 |

Most pin-compatible variants are also detected.

---

## Test procedure

1. Insert the device (18, 20 or 28 pins, DIP or ZIP).  
2. Connect 5 V via USB.  
3. Press the Start button.  
4. Read the result  
   * OLED version: plain-text report on the display  
   * LED-only version: green = pass, red = fail

There is a short YouTube video demonstrating the tester in action. <br/>
[![YouTube Demonstrator Video](https://img.youtube.com/vi/9TBlnfiTfQk/0.jpg)](https://www.youtube.com/watch?v=9TBlnfiTfQk "Demonstration")<br/>
*(Note: this video shows a prototype, the current SMD Design can be seen above. Handling without Display remains identical):*  

---

## Build or buy
Salles thread on Amibay - Sell Hw - Commodore Section [https://www.amibay.com](https://www.amibay.com/threads/memory-tester.2450230/)<br/>
DIY PCBWay for Thru-Hole Version : https://www.pcbway.com/project/shareproject/Ram_Tester_ThruHole_Version_93863356.html

---

## Documentation

* **Wiki** – assembly and operating guide  
* **Software** – source code and pre-compiled HEX files  
* **Schematic** – KiCad project and Gerber files  
* **Changelog**

---

## Contributing

Pull requests, issues and forks are welcome.  
Questions: GitHub Discussions or contact **tops4u** on AmiBay.

Open-source hardware under GPL v3 – use at your own risk.
