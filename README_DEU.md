# Ram-Tester
**Schneller, quelloffener DIY-Tester für Vintage-DRAM/SRAM** — erkennt den
Chip-Typ automatisch, führt alle Tests durch und liefert in Sekunden ein klares Gut/Schlecht.

[GPL v3] [Firmware 5.0.5] · C64 · Amiga · Atari · ST · Apple II · Spectrum

## Worum es geht
Ein schneller, quelloffener DIY-RAM-Tester für Vintage-RAM-Chips aus C64, Amiga, Atari und anderen Retro-Computern. Aufgebaut um einen 16-MHz-ATmega328P auf einer eigenen Platine, testet er den Speicher in Sekunden gründlich und unterstützt eine breite Palette von Chip-Typen mit optionaler OLED-Anzeige.

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Media/IMGP7463 (1).jpeg" width="400px" align="center"/><br/>

**Bekannt aus:**

- [***Adrians Digital Basement II***](https://youtu.be/9QQ8ZqHPRVQ?&t=2573) (Mai 2026): "This is freaking awesome"
- [***Hackaday***](https://hackaday.com/2025/12/08/cheap-and-aggressive-dram-chip-tester/) (Dez. 2025): "Cheap and Aggressive DRAM Chip Tester"
- [***Elektor Magazine***](https://www.elektormagazine.com/news/open-source-diy-ram-tester) (Dez. 2025): "Ram-Tester Is an Open-Source DIY Solution for Retro Computer RAM"

---

## Warum dieser Tester?
- **Kein Chip-Wissen nötig** – Pin-Anzahl am DIP-Schalter einstellen — der Tester erkennt den Chip-Typ selbst und führt alle Tests automatisch durch. Keine Algorithmen-Auswahl, kein Suchen des richtigen Sockels, keine Menüs, kein Datenblatt-Wälzen. Chip einstecken, Resultat ablesen.
- **Schnell** – Vollständiger Test in unter 8 Sekunden für ein 41256. Ein ganzes Tray Chips in Minuten.
- **Gründlich** – Während andere Tester eine Auswahl zwischen Testmodi verlangen, führt dieser alle aus: Speichermuster, Übersprechen (Crosstalk), Adressleitungs-Prüfung, Retention-Zeit, CAS-before-RAS-Refresh, Fast Page Mode, Static Column Mode, GND-Kurzschluss-Erkennung.
- **Praktisch** – Kaputt ist kaputt. Du bekommst ein klares Gut/Schlecht, denn ein DRAM-Chip lässt sich ohnehin nicht reparieren.
- **Sicher** – Kurzschluss-Schutz, Strombegrenzung, GND-Kurzschluss-Erkennung. Selbsttest-Modus inklusive.
- **Komplett Open Source** – Hardware, Firmware, Schaltpläne. Keine Blackbox.
---
## Kernmerkmale
| Merkmal | Nutzen |
|---------|---------|
| 20-Pin-ZIP-Sockel | Direkter Test von 20-Pin-ZIP-DRAMs ohne Adapter |
| Optionale OLED-Anzeige oder LED-Blinkcodes | Volltext-Rückmeldung oder minimaler Hardware-Aufwand |
| Selbsttest-Modus | Hardware auf Defekte wie Kurzschlüsse oder kalte Lötstellen prüfen |

---

## Unterstützte DRAM-Typen (Geschwindigkeit mit aktueller Firmware)

| Kapazität | DIP | 20-Pin-ZIP | Static Column | Nibble Mode | Retention-Zeit | Testzeit |
|----------|-----|------------|---------------|-------------|----------------|-----------|
| 4 K × 1 | 4027 <sup>1)</sup> | – | – | – | 2 ms | 0.2 s |
| 16 K × 1 | 4816 | – | – | – | 2 ms | 0.7 s |
| 16 K × 1 | 4116 <sup>1)</sup> | – | – | – | 2 ms | 0.5 s |
| 16 K × 4 | 4416 | – | – | – | 4 ms | 1.7 s |
| 32 K × 1 | 3732 <sup>2)</sup> | – | – | – | 2/4 ms | 1.8 s |
| 32 K × 1 | 4532 <sup>2)</sup> | – | – | – | 2/4 ms | 1.8 s |
| 64 K × 1 | 4164 | – | – | – | 2/4 ms | 1.8 s |
| 64 K × 4 | 4464 | – | – | – | 4 ms | 5.2 s |
| 256 K × 1 | 41256 | – | – | 41257 | 4 ms | 7.5 s |
| 256 K × 4 | 44256 | both | 44258 | – | 8 ms | 3.5 s |
| 1 M × 1 | 411000 | **✗**<sup>3)</sup> | – | – | 8 ms | 23.8 s |
| 1 M × 4 | 514400 | both | 514402 | – | 16 ms | 12.0 s |

<sup>1)</sup> Benötigt die [4116-Adapterplatine](Schematic/4116).<br/>
<sup>2)</sup> Half-Good-4164-Chips, verkauft als 32K × 1 (OKI MSM3732 / TI TMS4532). Seit Firmware 4.2.3 standardmässig aktiviert. Details in der [32K-Dokumentation](Docs/32K-Option).<br/>
<sup>3)</sup> Der ZIP-Pinout unterscheidet sich vom 20-Pin-Typ — für die ZIP-Version brauchst du einen Adapter.

**Static Column** bedeutet, dass der RAM Spaltenwechsel erlaubt, während CAS low gehalten wird — schneller als der Standard-Page-Mode. **Nibble Mode** liefert vier aufeinanderfolgende Bits aus einer einzigen Spaltenadresse.

## Unterstützte SRAM-Typen (Geschwindigkeit mit aktueller Firmware)

| Kapazität | DIP | Testzeit |
|----------|-----|-----------|
| 1 K × 4 | 2114 <sup>1)</sup> | 0.4 s |

 <sup>1)</sup> **WARNUNG:** 2114-SRAM muss um 180° gedreht eingesteckt werden — Pin 10 des SRAM auf die ZIF-Pin-1-Markierung.

---

## Testablauf

1. Bauteil einstecken (16, 18 oder 20 Pins, DIP oder ZIP).
2. DIP-Schalter auf die Pin-Anzahl deines RAM einstellen. Schalterstellungen siehe [Bedienungsanleitung](Docs).
3. USB-Stromversorgung anschliessen (oder RESET drücken, falls bereits versorgt).
4. Resultat ablesen
   * OLED-Version: Klartext-Bericht auf dem Display
   * LED-Version: grün = bestanden, rot = durchgefallen

Ein kurzes YouTube-Video zeigt den Tester in Aktion. <br/>
[![YouTube Demonstrator Video](https://img.youtube.com/vi/vsYpcPfiFhY/0.jpg)](https://youtu.be/vsYpcPfiFhY "Demonstration")<br/>

---

## Was wird getestet?
1. GND-Kurzschlüsse — prüft, ob ein Pin gegen Masse kurzgeschlossen ist
2. Versorgungs-Kurzschlüsse — eine rückstellbare Sicherung schützt die Platine
3. Adressleitungs- und Dekoder-Fehler
4. Festsitzende Zellen oder Übersprechen via bidirektionale Checkerboards
5. Zufallsmuster kombiniert mit Retention-Zeit-Prüfungen
6. CAS-before-RAS-Refresh-Timer-Funktion
7. Alle oben genannten nutzen den passenden Zugriffsmodus für den Chip-Typ: Fast Page Mode, Static Column oder Nibble Mode

### Warum kein MARCH-B?
Hier der Vergleich dieses Algorithmus mit MARCH-B:

| Aspekt | Dieser Tester | MARCH-B |
| --- | --- | --- |
| Muster-Abdeckung       | ✅ Checkerboard            | ✅                  |
| 0→1- / 1→0-Übergang    | ✅ Über Mustersequenz      | ✅ Über R0W1, R1W0  |
| Adress-Reihenfolge     | ✅ Auf- + absteigend       | ✅ Auf- + absteigend |
| Kopplungs-Erkennung    | ✅ Über Retention-Delay    | ✅ Systematisch     |
| Echte Retention        | ✅ 2–16 ms je Chip-Spec    | ❌ Nicht abgedeckt <sup>1)</sup> |
| GND-Kurzschluss-Erkennung | ✅ Vor dem Test         | ⚠️ Nur implizit <sup>1)</sup> |
| Adressleitungs-Fehler  | ✅ Bit-Unabhängigkeits-Prüfung | ⚠️ Nur implizit <sup>1)</sup> |
| CBR-Refresh            | ✅ CAS-before-RAS-Test<sup>2)</sup> | ❌ Nicht abgedeckt <sup>1)</sup> |

<sup>1)</sup> sofern nicht ausserhalb von MARCH-B implementiert<br>
<sup>2)</sup> Für RAM mit Refresh-Zähler (41256 und neuer)

MARCH-B ist gründlich bei Kopplungs- und Dekoder-Fehlern, arbeitet aber im Mikrosekunden-Bereich — es erkennt keine Chips mit schwacher Retention, die auf dem Papier der Spezifikation entsprechen, unter realem Refresh-Timing aber versagen. Dieser Tester tauscht die systematische Adress-Reihenfolge gegen Echtzeit-Retention-Stress — genau dort treten die meisten altersbedingten Ausfälle tatsächlich auf.

### Hinweis zu CMOS- vs. TTL-Pegeln
Vintage-DRAM-Chips wurden für TTL-Signalpegel entwickelt. Der ATmega328P treibt bei 5 V CMOS-Pegel — ein logisches High liegt nahe an V<sub>CC</sub> und damit höher, als die Originalsysteme lieferten. Das bedeutet: Grenzwertige Chips, die an echten TTL-Schwellen versagen, können auf diesem Tester (und den meisten anderen mikrocontroller-basierten Testern) trotzdem bestehen.
Manche Designs verwenden 3.3-V-Controller mit 5-V-toleranten I/Os, um näher an TTL-Pegel zu kommen, doch der ATmega328P unterstützt diesen Betriebsmodus nicht. In der Praxis arbeitet praktisch kein Consumer-DRAM-Tester am Markt mit echten TTL-Pegeln — das ist eine grundsätzliche Einschränkung des Ansatzes, nicht spezifisch für dieses Projekt. Für definitive Tests auf TTL-Niveau wäre dediziertes Vintage-Messequipment (z. B. Advantest, Agilent) mit kalibrierten Schwellen nötig.

---
## Bauen oder kaufen — du hast die Wahl

**Fertig kaufen:** [Amibay](https://www.amibay.com/threads/memory-tester.2450230/) · [Lectronz](https://lectronz.com/products/ram-tester) · [eBay](https://www.ebay.ch/itm/136743995188)

**Selber bauen (DIY):** Erhältlich als einsteigerfreundliche Through-Hole-Version (THT) oder kompakte SMD-Version. Platinen bei [PCBWay (THT)](https://www.pcbway.com/project/shareproject/Ram_Tester_ThruHole_Version_93863356.html) bestellen oder die mitgelieferten Gerber-Dateien aus dem [Schematic](Schematic)-Ordner verwenden.

---

## Dokumentation

* [**Bedienungsanleitung**](Docs) – Testablauf, Fehlercodes, Selbsttest und Fehlersuche
* [**Software / Firmware**](Software) – Firmware-Varianten, Flash-Anleitung
* [**Schaltplan**](Schematic) – KiCad-Projekt und Gerber-Dateien (THT und SMD)
* [**Changelog**](changelog.md) – Firmware-Versionsverlauf
* [**Kompatibilität**](compatibility.md) – Hersteller-Querverweis und DRAM-/SRAM-Verwendung nach System

---

## Eigene Geräte bauen oder verkaufen

Du darfst den Ram-Tester nachbauen, anpassen und auch verkaufen — genau dafür
ist Open Hardware da. Im Gegenzug verlangt die Lizenz, dass es offen bleibt:

- **Bleibt Open Source.** Boards, die du weitergibst oder verkaufst, stehen
  weiter unter CERN-OHL-S v2 (Hardware) und GPL v3 (Firmware). Kein Schliessen,
  kein proprietärer Fork.
- **Quelle mitliefern.** Wenn du Boards verkaufst oder weitergibst, mach die
  vollständigen Designdateien *deiner* Version — inkl. deiner Änderungen — unter
  derselben Lizenz verfügbar.
- **Urheber und Link nennen.** Copyright- und Lizenzhinweise intakt lassen
  und die Quelle — https://github.com/tops4u/Ram-Tester/ — sichtbar halten, wo
  praktikabel im Bestückungsdruck der Platine (CERN-OHL-S §4).
- **Keine Attribution entfernen.** Als eigenes, geschlossenes Produkt verkaufen
  oder die Hinweise entfernen ist nicht erlaubt.
- **Artikelbeschreibung.** Beim Verkauf auf Online-Marktplätzen/im Shop müssen
  der Urheber, ein Link zu den Quelldaten und die Lizenz enthalten sein.

Kurz: bauen, verbessern, verkaufen — aber offen halten und auf die Quelle
verweisen. Im Zweifel einfach in den Discussions fragen.

---

## Mitmachen

Pull Requests, Issues und Forks sind willkommen.
Fragen: GitHub Discussions oder **tops4u** auf AmiBay kontaktieren.

Open-Source-Hardware (CERN-OHL-S v2) und Firmware (GPL v3) – Benutzung auf eigenes Risiko.
