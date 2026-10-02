# Ram-Tester
**Schneller Open-Source-DIY-Tester für Vintage-DRAM/SRAM** — erkennt den
Chiptyp automatisch, führt alle Tests durch und liefert in Sekunden ein klares Gut/Schlecht.

[GPL v3] [Firmware 5.1.2] · C64 · Amiga · Atari · ST · Apple II · Spectrum

## Worum es geht
Ein schneller Open-Source-RAM-Tester zum Selbstbauen für Vintage-RAM-Chips aus C64, Amiga, Atari und anderen Retro-Computern. Er basiert auf einem ATmega328P mit 16 MHz auf einer eigenen Platine, testet Speicher gründlich in wenigen Sekunden und unterstützt eine breite Palette von Chiptypen, optional mit OLED-Anzeige.

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Media/IMGP7463 (1).jpeg" width="400px" align="center"/><br/>

**Bekannt aus:**

- [***Jan Beta***](https://www.youtube.com/watch?v=iQYF2-FwuoI) (Aug. 2026): "The best Arduino Tester (so far)"
- [***Adrians Digital Basement II***](https://youtu.be/9QQ8ZqHPRVQ?&t=2573) (Mai 2026): "This is freaking awesome"
- [***Hackaday***](https://hackaday.com/2025/12/08/cheap-and-aggressive-dram-chip-tester/) (Dez. 2025): "Cheap and Aggressive DRAM Chip Tester"
- [***Elektor Magazine***](https://www.elektormagazine.com/news/open-source-diy-ram-tester) (Dez. 2025): "Ram-Tester Is an Open-Source DIY Solution for Retro Computer RAM"

---

## Warum dieser Tester?
- **Kein Chip-Wissen nötig** – Pinanzahl per DIP-Schalter einstellen — der Tester erkennt den Chiptyp selbst und führt alle Tests automatisch aus. Keine Algorithmen auswählen, keinen passenden Sockel suchen, keine Menüs, kein Nachschlagen im Datenblatt. Chip nehmen, einstecken, Ergebnis ablesen.
- **Schnell** – Kompletter Test eines 41256 in unter 8 Sekunden. Ein ganzes Tray voller Chips ist in Minuten durchgetestet.
- **Gründlich** – Wo andere Tester eine Wahl zwischen Testmodi verlangen, führt dieser alle aus: March-B-Adressdecodertest, Speichermuster, Crosstalk, Retention Time, CAS-before-RAS-Refresh, Fast Page Mode, Static Column Mode, Kurzschlusserkennung (VCC, GND und Pin-zu-Pin).
- **Praxisnah** – Kaputt ist kaputt. Das Ergebnis ist ein klares Gut/Schlecht, denn ein DRAM-Chip lässt sich ohnehin nicht reparieren.
- **Sicher** – Kurzschlussschutz, Strombegrenzung durch rückstellbare Sicherung, Kurzschlusserkennung gegen VCC, GND und zwischen den Pins. Inklusive Selbsttest.
- **Vollständig Open Source** – Hardware, Firmware, Schaltpläne. Keine Blackbox.

---

## Wichtigste Merkmale
| Merkmal | Nutzen |
|---------|---------|
| 20-poliger ZIP-Sockel auf der Platine | Direkter Test von 256K × 4 und 1M × 4 ZIP-DRAMs ohne Adapter |
| Optionaler ZIP/SOJ-Adapter | Ergänzt 1M × 1 ZIP und 41256 ZIP sowie SOJ-Sockel für 18- und 20-polige SOJ-RAMs <sup>1)</sup> |
| Optionales OLED-Display oder LED-Blinkcodes | Ausführliche Textausgabe oder minimaler Hardwareaufwand |
| Selbsttest-Modus | Prüft die Hardware auf Defekte wie Kurzschlüsse oder schlechte Lötstellen |

<sup>1)</sup> Das ZIP-Pinout der 1M × 1 unterscheidet sich vom ZIP-Pinout der 256K × 4 / 1M × 4, deshalb können diese Chips nicht im ZIP-Sockel auf der Platine getestet werden. Siehe [Unterstützte DRAM-Typen](#unterstützte-dram-typen-geschwindigkeit-mit-aktueller-firmware).

---

## Unterstützte DRAM-Typen (Geschwindigkeit mit aktueller Firmware)

| Kapazität | DIP | 20-pol. ZIP | Static Column | Nibble Mode | Retention Time | Testdauer |
|----------|-----|------------|---------------|-------------|----------------|-----------|
| 4 K × 1 | 4027 <sup>1)</sup> | – | – | – | 2 ms | 0.2 s |
| 16 K × 1 | 4816 | – | – | – | 2 ms | 0.7 s |
| 16 K × 1 | 4116 <sup>1)</sup> | – | – | – | 2 ms | 0.5 s |
| 16 K × 4 | 4416 | – | – | – | 4 ms | 1.7 s |
| 32 K × 1 | 3732 <sup>2)</sup> | – | – | – | 2/4 ms | 1.8 s |
| 32 K × 1 | 4532 <sup>2)</sup> | – | – | – | 2/4 ms | 1.8 s |
| 64 K × 1 | 4164 | – | – | – | 2/4 ms | 1.8 s |
| 64 K × 4 | 4464 | – | – | – | 4 ms | 5.2 s |
| 256 K × 1 | 41256 | Adapter <sup>3)</sup> | – | 41257 | 4 ms | 7.5 s |
| 256 K × 4 | 44256 | auf Platine | 44258 | – | 8 ms | 3.5 s |
| 1 M × 1 | 411000 / 511000 | Adapter <sup>3)</sup> | – | – | 8 ms | 19.4 s |
| 1 M × 4 | 514400 | auf Platine | 514402 | – | 16 ms | 12.0 s |

<sup>1)</sup> Benötigt das [4116-Adapterboard](Schematic/4116).<br/>
<sup>2)</sup> Halb-gute 4164-Chips, verkauft als 32K × 1 (OKI MSM3732 / TI TMS4532). Seit Firmware 4.2.3 standardmässig aktiviert. Details siehe [32K-Dokumentation](Docs/32K-Option).<br/>
<sup>3)</sup> Das ZIP-Pinout der 1M × 1 unterscheidet sich vom ZIP-Pinout der 256K × 4 / 1M × 4, deshalb können 411000 / 511000 im ZIP-Gehäuse nicht im ZIP-Sockel auf der Platine getestet werden. Der separate ZIP/SOJ-Adapter bietet den passenden ZIP-Sockel für 1M × 1 und 41256 sowie zwei SOJ-Sockel für 20- und 18-polige SOJ-RAMs.

**Static Column** bedeutet, dass das RAM Spaltenwechsel zulässt, während CAS auf Low bleibt — schneller als der normale Page Mode. **Nibble Mode** liefert vier aufeinanderfolgende Bits aus einer einzigen Spaltenadresse.

## Unterstützte SRAM-Typen (Geschwindigkeit mit aktueller Firmware)

| Kapazität | DIP | Testdauer |
|----------|-----|-----------|
| 1 K × 4 | 2114 <sup>1)</sup> | 0.4 s |

<sup>1)</sup> **ACHTUNG:** Das 2114-SRAM muss um 180° gedreht eingesetzt werden, mit Pin 10 des SRAM auf der Pin-1-Markierung des ZIF-Sockels.

---

## Testablauf

1. Chip einsetzen (16, 18 oder 20 Pins, DIP oder ZIP). 1M × 1 ZIP, 41256 ZIP und SOJ-Chips kommen in den ZIP/SOJ-Adapter.
2. DIP-Schalter auf die Pinanzahl des RAMs einstellen. Die Schalterstellungen stehen in der [Bedienungsanleitung](Docs).
3. USB-Netzteil anschliessen (oder RESET drücken, falls bereits eingeschaltet).
4. Ergebnis ablesen
   * OLED-Version: Klartext-Bericht auf dem Display
   * Reine LED-Version: grün = bestanden, rot = fehlerhaft

Ein kurzes YouTube-Video zeigt den Tester im Einsatz. <br/>
[![YouTube-Demovideo](https://img.youtube.com/vi/vsYpcPfiFhY/0.jpg)](https://youtu.be/vsYpcPfiFhY "Demonstration")<br/>

---

## Wie wird getestet?
1. Kurzschlüsse — prüft, ob ein Pin mit GND, mit VCC (5 V) oder mit einem anderen Pin kurzgeschlossen ist
2. Chiperkennung — erkennt, ob ein Chip eingesetzt ist, und bestimmt seinen Typ
3. Adressleitungs- und Decoderfehler — March-B-Sequenz auf die Adressleitungen angewendet
4. Hängende Zellen und Crosstalk — bidirektionale Schachbrettmuster
5. Zufallsmuster kombiniert mit Retention-Time-Prüfung
6. Funktion des CAS-before-RAS-Refreshzählers (sofern vom RAM unterstützt)
7. Alle obigen Tests verwenden den passenden Zugriffsmodus des Chips: Fast Page Mode, Static Column oder Nibble Mode

### Testalgorithmus und March-B
Der Tester kombiniert eine March-B-Sequenz mit chipspezifischen Stresstests.

**March-B für die Adressdecodierung.** Adressleitungs- und Decoderfehler werden mit einer March-B-Sequenz (R0W1, R1W0, aufsteigend und absteigend) erkannt, angewendet auf eine reduzierte Adressmenge, die jede Zeilen- und Spaltenadressleitung einzeln anspricht. So werden kurzgeschlossene, offene oder hängende Adressleitungen sowie Decoder-Aliasing systematisch gefunden, ohne einen vollständigen March auf Zellebene durchzuführen.

**Warum kein vollständiger March-B auf Zellebene?** Für die in der Praxis relevanten Fehlerklassen bietet die obige Kombination eine vergleichbare Abdeckung und ergänzt, was ein vollständiger March-B auslässt:

- **Zugriffsmodi:** Ein klassischer March-B greift auf jede Zelle einzeln zu (direkter Einzelzellzugriff). Er prüft nicht den vollen Funktionsumfang eines RAM-Chips wie Fast Page Mode, Static Column, Nibble Mode oder CAS-before-RAS-Refresh. Dieser Tester prüft jede Funktion, die der Chip unterstützt.
- **Retention:** March-B arbeitet im Mikrosekundenbereich pro Zugriff und kann daher keine Chips mit schwacher Retention erkennen, die auf dem Papier bestehen, unter realem Refresh-Timing aber versagen — und genau dort treten die meisten alterungsbedingten Ausfälle auf. Die Abdeckung auf Zellebene kommt hier von bidirektionalen Schachbrettmustern und Zufallsmustern in Kombination mit realen Retention-Wartezeiten.
- **Testdauer:** Ein vollständiger March-B auf Zellebene ist bei grossen Chips wie 1M × 1 oder 1M × 4 auf einem ATmega328P nicht in praxistauglicher Zeit machbar. Ziel ist ein zuverlässiges Ergebnis in Sekunden, nicht in Minuten.

| Aspekt | Dieser Tester | Vollständiger March-B auf Zellebene |
| --- | --- | --- |
| Adressleitungs- / Decoderfehler | ✅ March-B auf Adressleitungen | ✅ |
| Hängende Zellen, 0→1 / 1→0-Übergänge | ✅ Schachbrett- + Zufallsmuster | ✅ |
| Adressreihenfolge | ✅ Auf- + absteigend | ✅ Auf- + absteigend |
| Crosstalk / Kopplung | ✅ Bidirektionale Schachbrettmuster | ✅ Systematisch |
| VCC/GND-Kurzschlusserkennung | ✅ Explizit, vor dem Test | ⚠️ Nur implizit <sup>1)</sup> |
| Pin-zu-Pin-Kurzschlusserkennung | ✅ Explizit, vor dem Test | ⚠️ Nur implizit <sup>1)</sup> |
| Zugriffsmodi | ✅ FPM, Static Column, Nibble Mode | ❌ Nur direkter Einzelzellzugriff |
| CBR-Refresh | ✅ CAS-before-RAS-Test <sup>2)</sup> | ❌ Nicht abgedeckt <sup>3)</sup> |
| Reale Retention | ✅ 2–16 ms gemäss Chipspezifikation | ❌ Nicht abgedeckt <sup>3)</sup> |
| Testdauer auf ATmega328P | ✅ Sekunden | ❌ Bei grossen Chips nicht praktikabel |

<sup>1)</sup> Ein Kurzschluss lässt den Chip im March durchfallen, wird aber nicht als Kurzschluss erkannt<br>
<sup>2)</sup> Für RAMs mit Refreshzähler (41256 und neuer)<br>
<sup>3)</sup> Sofern nicht ausserhalb von March-B implementiert

### Hinweis zu CMOS- vs. TTL-Pegeln
Vintage-DRAM-Chips wurden für TTL-Signalpegel ausgelegt. Der ATmega328P treibt bei 5 V CMOS-Pegel — logisch High liegt nahe an V<sub>CC</sub> und damit über dem, was die Originalsysteme lieferten. Grenzwertige Chips, die bei echten TTL-Schwellen versagen, können auf diesem Tester (und den meisten anderen mikrocontrollerbasierten Testern) daher trotzdem bestehen.
Manche Designs verwenden 3.3-V-Controller mit 5-V-toleranten I/Os, um näher an TTL-Pegel zu kommen, doch der ATmega328P unterstützt diesen Betriebsmodus nicht. In der Praxis arbeitet kaum ein handelsüblicher DRAM-Tester mit echten TTL-Pegeln — das ist eine grundsätzliche Grenze des Ansatzes und nicht spezifisch für dieses Projekt. Für einen verbindlichen Test auf TTL-Pegel wäre dediziertes Prüfequipment (z. B. Advantest, Agilent) mit kalibrierten Schwellen nötig.

### Experimentell: Firmware zur Messung der Retention Time
Eine alternative, experimentelle Firmware misst, wie lange der Chip seine Daten ohne Refresh tatsächlich hält — bestimmt durch seine schwächste Zelle — statt ein Gut/Schlecht-Ergebnis zu liefern. Sie ist derzeit für 4164, 4464 und 44256/514256 verfügbar. Beim Flashen ersetzt sie vorübergehend die reguläre Tester-Firmware. Details unter [Software](Software).

---

## Selbst bauen oder kaufen — du hast die Wahl

**Fertig kaufen:** [Amibay](https://www.amibay.com/threads/memory-tester.2450230/) · [Lectronz](https://lectronz.com/products/ram-tester) · [eBay](https://www.ebay.ch/itm/136743995188) · [Tindie](https://www.tindie.com/products/reusecircuit/ram-tester-for-2114-4116-4164-41256-441000-514256/)

**Selbst bauen (DIY):** Erhältlich als einsteigerfreundliche Durchsteckversion (THT) oder als kompakte SMD-Version. Platinen bei [PCBWay (TH)](https://www.pcbway.com/project/shareproject/Ram_Tester_ThruHole_Version_93863356.html) bestellen oder die Gerber-Dateien aus dem Ordner [Schematic](Schematic) verwenden.

---

## Dokumentation

* [**Bedienungsanleitung**](Docs) – Testablauf, Fehlercodes, Selbsttest und Fehlersuche
* [**Software / Firmware**](Software) – Firmware-Varianten, Anleitung zum Flashen
* [**Schaltplan**](Schematic) – KiCad-Projekt und Gerber-Dateien (THT und SMD)
* [**Changelog**](changelog.md) – Versionsgeschichte der Firmware
* [**Kompatibilität**](compatibility.md) – Hersteller-Querverweise und DRAM/SRAM-Verwendung in Systemen

---

## Eigene Geräte bauen oder verkaufen

Du darfst eigene Ram-Tester bauen, verändern und sogar verkaufen — genau
darum geht es bei Open Hardware. Im Gegenzug verlangt die Lizenz, dass das Projekt offen bleibt:

- **Es bleibt Open Source.** Weitergegebene oder verkaufte Platinen bleiben unter
  CERN-OHL-S v2 (Hardware) und GPL v3 (Firmware). Kein Schliessen des Designs,
  kein proprietärer Fork.
- **Quellen weitergeben.** Wer Platinen verkauft oder verschenkt, stellt die
  vollständigen Designdateien *seiner* Version — inklusive aller Änderungen —
  unter derselben Lizenz zur Verfügung.
- **Urheberhinweis und Link beibehalten.** Copyright- und Lizenzhinweise bleiben
  unverändert, und die Quelle — https://github.com/tops4u/Ram-Tester/ — bleibt
  sichtbar, wo praktikabel auf dem Bestückungsdruck der Platine (CERN-OHL-S §4).
- **Keine Entfernung der Urheberangaben.** Den Tester als eigenes geschlossenes
  Produkt zu verkaufen oder die Hinweise zu entfernen, ist nicht erlaubt.
- **Artikelbeschreibung.** Auf Online-Plattformen müssen angegeben werden:
  Urheber, Link zu den Quelldateien (dieses GitHub-Repo) und Lizenz.

Kurz gesagt: Bauen, verbessern, verkaufen — aber offen halten und auf dieses
Projekt verweisen. Im Zweifel einfach eine Discussion eröffnen und fragen.

---

## Mitwirken

Pull Requests, Issues und Forks sind willkommen.
Fragen: GitHub Discussions oder **tops4u** auf AmiBay kontaktieren.

Open-Source-Hardware (CERN-OHL-S v2) und Firmware (GPL v3) – Nutzung auf eigene Gefahr.
