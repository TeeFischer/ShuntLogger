/*
  Kombinierter Testcode:
  - SD-Karte wird geprüft und Infos werden ausgegeben
  - RTC (DS1307/DS3231) wird geprüft und aktuelle Zeit angezeigt

  Abhängigkeiten:
   - SD (Standardbibliothek)
   - SPI (Standardbibliothek)
   - RTClib (von Adafruit)
*/

#include <SPI.h>
#include <SD.h>
#include <RTClib.h>

// ---------- SD-Karte ----------
Sd2Card card;
SdVolume volume;
SdFile root;

// Chip-Select-Pin für SD-Karte (anpassen je nach Shield)
const int chipSelect = 10;

// ---------- RTC ----------
RTC_DS1307 rtc;

char daysOfTheWeek[7][12] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Warte, bis serielle Verbindung hergestellt ist (nur bei USB)
  }

  Serial.println("\n=== SD + RTC TEST ===");

  // --- SD-KARTE INITIALISIEREN ---
  Serial.print("\n[1] Initialisiere SD-Karte... ");
  if (!card.init(SPI_HALF_SPEED, chipSelect)) {
    Serial.println("FEHLER!");
    Serial.println("* Karte eingesetzt?");
    Serial.println("* Verkabelung korrekt?");
    Serial.println("* Richtiger CS-Pin?");
    while (1);
  } else {
    Serial.println("OK – Karte erkannt!");
  }

  // Kartentyp anzeigen
  Serial.print("Kartentyp: ");
  switch (card.type()) {
    case SD_CARD_TYPE_SD1: Serial.println("SD1"); break;
    case SD_CARD_TYPE_SD2: Serial.println("SD2"); break;
    case SD_CARD_TYPE_SDHC: Serial.println("SDHC"); break;
    default: Serial.println("Unbekannt");
  }

  if (!volume.init(card)) {
    Serial.println("Keine FAT16/FAT32 Partition gefunden.");
    while (1);
  }

  Serial.print("Dateisystem: FAT");
  Serial.println(volume.fatType());

  // --- RTC INITIALISIEREN ---
  Serial.print("\n[2] Initialisiere RTC... ");
  if (!rtc.begin()) {
    Serial.println("FEHLER: RTC nicht gefunden!");
    while (1);
  }
  Serial.println("OK");

  if (!rtc.isrunning()) {
    Serial.println("RTC läuft nicht, stelle Zeit auf Kompilierungszeit...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.println("\nSetup abgeschlossen!\n");
}

// ---------- LOOP ----------
void loop() {
  // Aktuelle Zeit abrufen
  DateTime now = rtc.now();

  Serial.print("Datum & Zeit: ");
  Serial.print(now.year());
  Serial.print('/');
  Serial.print(now.month());
  Serial.print('/');
  Serial.print(now.day());
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour());
  Serial.print(':');
  Serial.print(now.minute());
  Serial.print(':');
  Serial.println(now.second());

  delay(1000);
}
