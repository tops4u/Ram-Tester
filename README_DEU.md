# Vintage DRAM Tester  
Schnell – Präzise – Offen für viele 8-/16-Bit-Systeme wie C64, C128, Amigas, Atari 800XL und andere, Apple IIe, Spektrum und viele mehr

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Media/IMG_3591.jpeg" width="400px" align="center"/><br/>

---

## Warum dieser Tester?

Die meisten Arduino-basierten DRAM-Tester benötigen bis zu 2 Minuten oder mehr für einen einzelnen 41256 RAM und prüfen nur grundlegende Funktionen.  
Dieses Projekt führt einen vollständigen Speicher-, Adress-, Datenleitungs- und Retention-Zeit-Test **in 15 Sekunden oder weniger** durch (oder sogar 2,5 Sekunden schneller ohne Display).  
Es ist wahrscheinlich die schnellste Arduino-Lösung, die außerdem **Static-Column-DRAMs, Zell-Retention-Zeit und 20-polige ZIP-Gehäuse** unterstützt.

---

## Hauptfunktionen

| Funktion | Vorteil |
|----------|---------|
| Testzeit <= 15 s | Schnelle Diagnose auf der Werkbank oder bei Retro-Reparatur-Events |
| Retention-Zeitmessung | Erkennt schwache Chips durch Prüfung auf minimale Retention-Zeiten |
| Static-Column-Unterstützung | Zuverlässige Prüfung von 44258, 514402 Static-Column-Funktionen |
| 20-poliger ZIP-Sockel | Direkter Test von 20-poligen ZIP-DRAMs ohne Adapter |
| Optionales OLED-Display oder LED-Blinkcodes | Vollständige Textanzeige oder minimale Hardwarekonfiguration |
| Offene Hardware und Firmware | KiCad-, Gerber-Dateien und Arduino-Quellcode unter offener Lizenz |

---

## Unterstützte DRAM-Typen

| Kapazität | DIP | 20-pol ZIP | Static Column | Retention-Zeit | Testzeit |
|-----------|-----|------------|----------------|-----------------|----------|
| 16 K × 4 | 4416 | – | – | 4ms | 3,7 Sek. |
| 64 K × 1 | 4164 | - | – | 2ms | 5 Sek. |
| 64 K × 4 | 4464 | – | – | 4ms | 6,5 Sek. |
| 256 K × 1 | 41256 | - | – | 4ms | 12 Sek. |
| 256 K × 4 | 44256 | beide | 44258 | 4ms | 5,2 - 5,5 Sek. |
| 1 M × 4 | 514256, 514400 | beide | 514402 | 16ms | 12,5 - 14,5 Sek. |

Hinweis: Die oben genannten Testzeiten beinhalten das OLED-Display. Ohne Display verkürzen sich die Zeiten um 2,5 Sekunden.

---

## Testablauf

1. DRAM einstecken (16, 18 oder 20 Pins, DIP oder ZIP)  
2. USB-Stromversorgung anschließen  
3. Ergebnis ablesen  
   * OLED-Version: Klartextanzeige auf dem Display  
   * Nur-LED-Version: grün = bestanden, rot = Fehler

Es gibt ein kurzes YouTube-Video, das den Tester in Aktion zeigt. <br/>
[![YouTube Demonstrator Video](https://img.youtube.com/vi/9TBlnfiTfQk/0.jpg)](https://www.youtube.com/watch?v=9TBlnfiTfQk "Demonstration")<br/>
*(Hinweis: Das Video zeigt einen Prototyp; das aktuelle SMD-Design ist oben zu sehen. Die Bedienung ohne Display ist identisch):*  

---

## Selbst bauen oder kaufen  
Verkaufsthread auf Amibay: [https://www.amibay.com](https://www.amibay.com/threads/memory-tester.2450230/)<br/>
DIY-Bestellung bei PCBWay für die THT-Version: [https://www.pcbway.com](https://www.pcbway.com/project/shareproject/Ram_Tester_ThruHole_Version_93863356.html)<br/>
Verwende die bereitgestellten Gerber-Dateien aus diesem Repository

---

## Dokumentation

* **Wiki** – Aufbau- und Bedienungsanleitung  
* **Software** – Quellcode  
* **Schaltplan** – KiCad-Projekt und Gerber-Dateien  
* **Changelog**

---

## Mitmachen

Pull Requests, Issues und Forks sind willkommen.  
Fragen: GitHub Discussions oder Kontakt zu **tops4u** auf AmiBay.

Open-Source-Hardware unter GPL v3 – Nutzung auf eigenes Risiko.
