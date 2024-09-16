# Ram-Tester
Ram Tester for vintage CBM Computer RAM Chips - **BETA** do not build/fork yet.

## Introduction
This project was started because I had bought a Commodore A2630 card with 2MB Ram at a flea market and wanted to upgrade it with 2MB Ram. On the internet I found either very expensive offers or cheap ones from a Chinese dealer. I tried my luck, but of course the card refused to work with the additional RAM. So I wanted a tester for the required RAM. On the one hand, there were very simple projects, which probably didn't test very well, or semi-professional testers with >1000U$. Meanwhile I was able to sort the defect RAM out by have a bootable Configuration and using Amiga Test Kit (ATK v1.22) to figure out which BIT are faulty and pinpoint possible candidates by checking the schematic. 

This Project was/is inspired by:
- DRAMTESTER (https://github.com/zeus074/dramtester) 
- DRAMARDUINO (https://forum.defence-force.org/viewtopic.php?t=1699)

-> No Code or Schematic was taken from any of those Projects.

I decided to build a tester myself with the aim of being able to test some of the common DRAM chips of the CBM computers 1980-1990. 

The project should also be able to be built by inexperienced people, which is why I decided on a solution with ATMEGA 328 processors - known from the Arduino UNO. 

The processor can be programmed and taken from an [Arduino UNO](https://store.arduino.cc/products/arduino-uno-rev3), programmed with a programmer or programmed via the existing ICSP (e.g. AVRISP MKII). People who want to remove the processor for programming should find enough space on the PCB for a ZIF socket. 

Operation is child's play. Insert the RAM, switch on the power and observe the LED. if it flashes green at the end, everything is ok, if it flashes red, something is broken. 

Currently the software only works with 256k x 4 DRAM (e.g. TC514256-80) because I needed this for my card. However, the following DRAM components should also be testable (as soon as the software is able to do so): **4164** (64k x 1), **4416** (16k x 4), **4464** (64k x 4), **41256/57** (256k x 1) and **514400** (1M x 4). The prerequisite is that GND is on the last pin and VCC on the diagonally opposite pin as well as the IC size of 16, 18 or 20 pins. 
As such, this Project will *not* be able to support the following RAM Types 2144 (Vcc on Pin 18), 6116 (24 Pin IC), 4116 (needs negative Voltage).

In order to keep build cost low, I just designed one PCB. If you want to use the ZIP Adapter you need to cut the PCB along the depicted *CUT-HERE* Area. Build cost with PCB actually less than 10U$ if you have the Material ready. If you need to order everything it is more likely to be 20-30U$.

![Ram-Tester PCB](https://github.com/tops4u/Ram-Tester/blob/main/RamTester.png?raw=true)
Render from KiCad - The depicted DIP IC will just be sockets in reality. 

## Operation
TBD

## Build
**BOM**
- 1x 16 MHz Quartz [AliExpress](https://aliexpress.com/item/1005006119798769.html)
- 2x 10-20pf Disc Capacitor [AliExpress](https://aliexpress.com/item/1005003167676803.html)
- 3x SMD 1206 1M Resistor
- 1x SMD 1206 1k Resistor
- 1x SMD 1206 100 Resistor
- 1x SMD 1206 150 Resistor
- 2x SMD 0805 100nF Capacitor 
- 1x PinHeader / PowerPlug 2.54mm [AliExpress](https://aliexpress.com/item/1005003179482974.html)
- 1x ZIF Socket / DIP Socket 28 Pin - Narrow [AliExpress](https://aliexpress.com/item/1005007205054381.html)
- 1x ZIF Socket / DIP Socket 20 Pin [AliExpress](https://aliexpress.com/item/1005007205054381.html)
- 1x BiColor LED 5mm (Red/Green) Common Center Cathode [AliExpress](https://aliexpress.com/item/1005006014283662.html)
- 1x 3 way Dip-Switch [AliExpress](https://aliexpress.com/item/4001205849246.html)
- 2x 10Pin Pin Header Female [AliExpress](https://aliexpress.com/item/32717301965.html)
- 2x 10Pin Pin Header Male [AliExpress](https://aliexpress.com/item/1005005390193356.html)
  
If you want to use ICSP:
- 1x PinHeader 2x3 [AliExpress](https://aliexpress.com/item/4000303366348.html)
  
If you want to use the ZIP Adapter
- 2x 10Pin Pin Header Female [AliExpress](https://aliexpress.com/item/32717301965.html)
- 2x 10Pin Pin Header Male [AliExpress](https://aliexpress.com/item/1005005390193356.html)
