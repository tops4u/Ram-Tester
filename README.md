# Ram-Tester
Ram Tester for vintage CBM Computer RAM Chips 

## Introduction
This project was started because I had bought a Commodore A2630 card with 2MB Ram at a flea market and wanted to upgrade it with 2MB Ram. On the internet I found either very expensive offers or cheap ones from a Chinese dealer. I tried my luck, but of course the card refused to work with the additional RAM. So I wanted a tester for the required RAM. On the one hand, there were very simple projects, which probably didn't test very well, or semi-professional testers with >1000U$. 

I decided to build a tester myself with the aim of being able to test some of the common DRAM chips of the CBM computers 1980-1990. 

The project should also be able to be built by inexperienced people, which is why I decided on a solution with ATMEGA 328 processors - known from the Arduino UNO. 

The processor can be programmed and taken from an Arduino UNO, programmed with a programmer or programmed via the existing ICSP. People who want to remove the processor for programming should find enough space on the PCB for a ZIF socket. 

Operation is child's play. Insert the RAM, switch on the power and observe the LED. if it flashes green at the end, everything is ok, if it flashes red, something is broken. 

Currently the software only works with 256k x 4 DRAM (e.g. TC514256-80) because I needed this for my card. However, the following DRAM components should also be testable (as soon as the software is able to do so): 4164 (64k x 1), 4416 (16k x 4), 4464 (64k x 4), 41256/57 (256k x 1). The prerequisite is that GND is on the last pin and VCC on the diagonally opposite pin as well as the IC size of 16, 18 or 20 pins. 

![Ram-Tester PCB](https://github.com/tops4u/Ram-Tester/blob/main/RamTester.png?raw=true)

## Operation
TBD

## Build
TBD
