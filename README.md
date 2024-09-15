# Ram-Tester
Ram Tester for vintage CBM Computer RAM Chips 

## Introduction
This project was started because I had bought a Commodore A2630 card with 2MB Ram at a flea market and wanted to upgrade it with 2MB Ram. On the internet I found either very expensive offers or cheap ones from a Chinese dealer. I tried my luck, but of course the card refused to work with the additional RAM. So I wanted a tester for the required RAM. On the one hand, there were very simple projects, which probably didn't test very well, or semi-professional testers with >1000U$. 

I decided to build a tester myself with the aim of being able to test some of the common DRAM chips of the CBM computers 1980-1990. 

The project should also be able to be built by inexperienced people, which is why I decided on a solution with ATMEGA 328 processors - known from the Arduino UNO. 

The processor can be programmed and taken from an Arduino UNO, programmed with a programmer or programmed via the existing ICSP. People who want to remove the processor for programming should find enough space on the PCB for a ZIF socket. 

![Ram-Tester PCB](https://github.com/tops4u/Ram-Tester/blob/main/RamTester.png?raw=true)
