# Ram-Tester
Ram-Tester für alte CBM Computer RAM Chips - **BETA** noch nicht bauen/forken.

Ich habe mich entschlossen, selbst einen Tester zu bauen mit dem Ziel, einige der gängigen DRAM-Chips der CBM-Computer 1980-1990 testen zu können.

## Einleitung
Dieses Projekt wurde gestartet, weil ich eine Commodore A2630 Karte mit 2MB Ram auf einem Flohmarkt gekauft hatte und diese mit 2MB Ram aufrüsten wollte. Im Internet fand ich entweder sehr teure Angebote oder billige von chinesischen Händlern. Ich versuchte mein Glück, aber natürlich weigerte sich die Karte, mit dem zusätzlichen RAM zu arbeiten. Also wollte ich ein Testgerät für den benötigten Arbeitsspeicher. Einerseits gab es sehr einfache Projekte, die wahrscheinlich nicht sehr gut getestet haben, oder semiprofessionelle Tester mit >1000U$. In der Zwischenzeit konnte ich die defekten RAMs aussortieren, indem ich eine bootfähige Konfiguration hatte und mit dem Amiga Test Kit (ATK v1.22) herausfand, welche BITs fehlerhaft sind und mögliche Kandidaten durch Überprüfung des Schaltplans ausfindig machte. 

Dieses Projekt wurde/ist inspiriert von:
- Projekt DRAM (https://github.com/ProjectDRAM/514256B)
- DRAMTESTER (https://github.com/zeus074/dramtester) 
- DRAMARDUINO (https://forum.defence-force.org/viewtopic.php?t=1699)

-> Kein Code oder Schaltplan wurde von einem dieser Projekte übernommen.

Warum also noch ein weiteres Projekt? 
1. Einfach eine billige und einfache Lösung. Kein schickes LCD, das keinen echten Mehrwert bietet. Wenn ein Chip defekt ist, ist es Ihnen wahrscheinlich egal, an welcher Adresse.
2. Eine schnelle Lösung. Einige andere RAM-Tester brauchen mehr als 1 Minute, um nur einen 64kb (8kB) Chip wie den 4164 zu testen. Dieses Projekt braucht weniger als 3 Sekunden, um einen 256x4 (128kB) Chip zu testen.
3. Einfacher Aufbau mit wenigen Bauteilen - ZIF-Sockel, wenn man sie oft braucht, ansonsten genügen wahrscheinlich normale DIP-Sockel.
4. Kleiner Footprint PCB um Kosten zu sparen.
5. Einen Tester haben, der auch größere RAMs unterstützt, wie sie z.B. im Amiga verwendet werden - insb auch ZIP RAM. 

Das Projekt sollte auch von unerfahrenen Leuten gebaut werden können, deshalb habe ich mich für eine Lösung mit ATMEGA 328 Prozessoren entschieden - bekannt vom Arduino UNO. Aber einfach nur ein weiteres Shield für Arduino zu haben, erschien mir unpraktisch, da man durch den Arduino selbst einige Einschränkungen hat.

Der Prozessor kann immer noch programmiert und von einem [Arduino UNO](https://store.arduino.cc/products/arduino-uno-rev3) übernommen werden, mit einem Programmer programmiert werden oder über die vorhandene ICSP programmiert werden (z.B. mit einem AVRISP MKII). Wer den Prozessor zum Programmieren ausbauen möchte, kann ihn zwischen einem Arduino UNO und diesem Board austauschen. 

Die Bedienung ist kinderleicht. RAM einstecken, Strom einschalten und die LED beobachten. Blinkt sie am Ende grün, ist alles in Ordnung, blinkt sie rot, ist etwas kaputt. 

Die folgenden DRAM-Bausteine sollten testbar sein (sobald die Software dazu in der Lage ist): **4164** (64k x 1), **4416** (16k x 4), **4464** (64k x 4), **41256/57** (256k x 1), **514256** (256k x 4) und **514400** (1M x 4). Bitte das Changelog File anschauen um zu sehen, welche DRAM aktuell unterstützt sind. Voraussetzung ist, dass GND auf dem letzten Pin und VCC auf dem schräg gegenüberliegenden Pin liegt, sowie die IC-Größe von 16, 18 oder 20 Pins. 

*->Daher wird dieses Projekt die folgenden RAM-Typen **nicht** unterstützen können: **2144** (Vcc auf Pin 18), **6116** (24 Pin IC), **4116** (benötigt negative und 12V Spannung).

Um die Baukosten niedrig zu halten, habe ich nur eine Platine entworfen. Wenn Sie den ZIP-Adapter verwenden möchten, müssen Sie die Platine entlang der dicken weißen Linie ausschneiden. Die Baukosten mit Platine betragen weniger als 10U$, wenn man das Material bereit hat. Wenn Sie alles bestellen müssen, wird es eher 20-30U$ sein.

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Schematic/IMG_2988.jpeg" width="400px" align="center"/>
Pre v1.0 PCB build

## Betrieb
TBD

## Build
**BOM**
- Y1 1x 16 MHz Quarz [AliExpress](https://aliexpress.com/item/1005006119798769.html)
- C1/C2/C6/C7 4x SMD 0805 100nF Kondensator 
- C2/C3 2x 10-20pf Scheibenkondensator [AliExpress](https://aliexpress.com/item/1005003167676803.html)
- R3, R4, R5 3x SMD 0603 1M Widerstand
- R2 1x SMD 0603 1k Widerstand
- R1 1x SMD 0603 100R Widerstand
- R6 1x SMD 0603 150R Widerstand
- R7, R8 2x SMD 0603 47R Widerstand
- RNx 4x SMD 0603 47R 4xAnordnung
- Q1, Q2, SMD IRL 6401 SOJ P-FET
- U1 DIP-Sockel 28 Pin - Schmal
- U2 1x ZIF Sockel / DIP Sockel 20 Pin [AliExpress](https://aliexpress.com/item/1005007205054381.html)
- D1 1x BiColor LED 5mm (Rot/Grün) Gemeinsame Mittelkathode [AliExpress](https://aliexpress.com/item/1005006014283662.html)
- SW1 1x 3 Wege Dip-Schalter [AliExpress](https://aliexpress.com/item/4001205849246.html)
- SW2 Druckknopf [AliExpress](https://aliexpress.com/item/4000555847543.html)
- J1 MicroUSB (oder jeder andere Stromanschluss mit 2,54mm Abstand [AliExpress](https://aliexpress.com/item/1005001515820458.html)
  
Wenn Sie ICSP verwenden möchten:
- ICSP 1x PinHeader 2x3 [AliExpress](https://aliexpress.com/item/4000303366348.html)
  
Wenn Sie den ZIP-Adapter verwenden wollen
- U3 2x 10Pin Stiftleiste Buchse [AliExpress](https://aliexpress.com/item/32717301965.html)
- U4 2x 10Pin Stiftleiste Stecker [AliExpress](https://aliexpress.com/item/1005005390193356.html)
