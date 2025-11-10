# Vintage DRAM Tester  
Schnell – Präzise – Kompatibel mit vielen 8-/16-Bit-Systemen wie C64, C128, Amigas, Atari 800XL, 1040ST und anderen, Apple IIe, Spectrum und vielen weiteren

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Media/Tester.jpeg" width="400px" align="center"/><br/>

---

## Warum dieser Tester?

Die meisten Arduino-basierten DRAM-Tester benötigen bis zu 2 Minuten oder mehr für einen einzelnen 41256 RAM und prüfen nur Grundfunktionen.  
Dieses Projekt führt einen vollständigen Speicher-, Adress-, Datenleitungs- und Retentionszeittest **in 15 Sekunden oder weniger** durch (oder sogar 2,5 Sekunden schneller ohne Display).  
Es ist wahrscheinlich die schnellste Arduino-Lösung, die zudem **Static-Column-DRAMs, Zell-Retentionszeit und 20-polige ZIP-Gehäuse** unterstützt.

---

## Hauptfunktionen

| Funktion | Vorteil |
|-----------|----------|
| Die meisten Tests dauern ≤ 10s | Schnelle Diagnose auf der Werkbank oder bei Retro-Reparatur-Events |
| Messung der Retentionszeit | Erkennt schwache Chips, indem überprüft wird, ob die minimale Haltezeit eingehalten wird |
| Unterstützung für Static Column | Zuverlässige Prüfung von 44258-, 514402-Static-Column-Funktionen |
| 20-poliger ZIP-Sockel | Direkter Test von 20-poligen ZIP-DRAMs ohne Adapter |
| Optionales OLED-Display oder LED-Blinkcodes | Vollständige Textausgabe oder minimale Hardwarekonfiguration |
| Offene Hardware und Firmware | KiCad-, Gerber-Dateien und Arduino-Quellcode unter offener Lizenz |

---

## Unterstützte DRAM-Typen 

| Kapazität | DIP | 20-pol. ZIP | Static Column | Retentionszeit | Testzeit |
|------------|-----|-------------|----------------|----------------|-----------|
| 16 K x 1 | 4816 | - | - | 2ms | 2,9s |
| 16 K x 1 | 4116 1) | - | - | 2ms | 2,9s |
| 16 K × 4 | 4416 | – | – | 4ms | 3,9s |
| 64 K × 1 | 4164 | - | – | 2ms | 4,6s |
| 64 K × 4 | 4464 | – | – | 4ms | 6,6s |
| 256 K × 1 | 41256 | - | – | 4ms | 12s |
| 256 K × 4 | 44256 | beide | 44258 | 4ms | 6s |
| 1 M x 1 | 411000 | **!NEIN!** | - | 8ms | 37s |
| 1 M × 4 | 514400 | beide | 514402 | 16ms | 16s |

1) 4116 benötigt eine Adapterplatine.

Hinweis: Die obigen Testzeiten beinhalten das OLED-Display. Ohne Display sind die Testzeiten etwa 1,7 Sekunden kürzer.

---

## Testablauf

1. Das Bauteil einstecken (16, 18 oder 20 Pins, DIP oder ZIP).  
2. USB-Stromversorgung anschließen.  
3. Ergebnis ablesen  
   * OLED-Version: Textausgabe auf dem Display  
   * LED-Version: grün = bestanden, rot = Fehler

Ein kurzes YouTube-Video zeigt den Tester in Aktion. <br/>
[![YouTube Demonstrator Video](https://img.youtube.com/vi/vsYpcPfiFhY/0.jpg)](https://youtu.be/vsYpcPfiFhY "Demonstration")<br/>

---

## Was wird getestet?
1. Kurzschluss gegen GND, falls dein RAM einen Kurzschluss auf einen Pin hat  
2. Kurzschluss in der Stromversorgung – es gibt eine rücksetzbare Sicherung zum Schutz der Platine  
3. Adressleitungs- oder Decoderfehler  
4. Festsitzende Zellen oder Übersprechen durch verschiedene Testmuster  
5. Zufällige Muster kombiniert mit Retentionszeitprüfungen  
6. All dies erfolgt mit Fast Page Mode oder Static Column Steuerung – je nach RAM-Typ

### Warum kein MARCH-B?
Hier eine Analyse dieses Algorithmus im Vergleich zu March-B:

| Aspekt | Aktueller Code | MARCH-B |
| --- | --- | --- |
| Musterabdeckung     | ✅ 0x00, 0xFF, 0xAA, 0x55 | ✅ |
| 0/1-Übergangstests | ✅ Durch Muster-Sequenz | ✅ Durch R0W1, R1W0 |
| Adressreihenfolge | ⚠️ Nur aufsteigend | ✅ Auf-/Absteigend |
| Kopplungserkennung | ✅ Durch Retentionsverzögerung | ✅ Systematisch |
| Echte Retention | ✅ 2–16 ms Tests | ❌ Nur µs-Bereich |

### Warum ist er wahrscheinlich besser als viele andere Arduino-basierte RAM-Tester?
Im Vergleich zu vielen anderen Open-Source-Projekten:
- Sie testen normalerweise keine Adressleitungsfehler. Tests bestehen sogar, wenn ein Adresspin hochgebogen ist.  
- Sie prüfen selten mehrere Reihen gleichzeitig, da sie langsame Arduino-Lese-/Schreibbefehle verwenden.  
- Viele verwenden nur einfache Tests mit allen 0 oder allen 1, keine Muster- oder Zufallsdaten.  
- Keine Retentionstests möglich, da das Schreiben und Lesen einer einzelnen Reihe mit Arduino-I/O länger als eine Sekunde dauert – typische Retentionszeiten liegen im Millisekundenbereich.  
- Keine Prüfung auf defekte Chips (z. B. Kurzschluss nach GND).  
- Kein Schutz vor Kurzschlüssen auf Versorgungsleitungen. Einige nutzen DC-DC-Wandler von der Stange, die bei Kurzschluss hohe Ströme liefern.  
- Keine Prüfung von Refresh- oder Static-Column-Funktionalität.  
- Sehr oft nur auf 1-Bit- oder 4-Bit-Schaltungen begrenzt und keine ZIP-Sockel.  
- Sie sind in der Regel **VIEL** langsamer.

---

## Bauen oder kaufen – du hast die Wahl
Verkaufs-Thread auf Amibay: [https://www.amibay.com](https://www.amibay.com/threads/memory-tester.2450230/)<br/>
Produktseite auf Tindie: [https://www.tindie.com](https://www.tindie.com/products/38927/)<br/>
Produktseite auf EBAY: [https://www.ebay.ch](https://www.ebay.ch/itm/136500192190)<br/>
DIY-Bestellung bei PCBWay für die Through-Hole-Version: [https://www.pcbway.com](https://www.pcbway.com/project/shareproject/Ram_Tester_ThruHole_Version_93863356.html)<br/>
Verwende die bereitgestellten Gerber-Dateien aus diesem Repository.

---

## Dokumentation

* **Wiki** – Montage- und Bedienungsanleitung  
* **Software** – Quellcode  
* **Schaltplan** – KiCad-Projekt und Gerber-Dateien  
* **Änderungsprotokoll**

---

## Mitwirken

Pull Requests, Issues und Forks sind willkommen.  
Fragen: GitHub Discussions oder Kontakt **tops4u** auf AmiBay.

Open-Source-Hardware unter GPL v3 – Nutzung auf eigenes Risiko.
