/*
  Kombinierter Testcode:
  - SD-Karte wird nur initialisiert, wenn sie eingesteckt ist (Card Detect an Pin 4)
  - RTC (DS1307/DS3231) zeigt aktuelle Zeit an
*/

#include <SPI.h>
#include <SD.h>
#include <RTClib.h>

// ---------- SD-Karte ----------
Sd2Card card;
SdVolume volume;
SdFile root;

// Pins
const int chipSelect = 10;   // CS für SD-Karte
const int cardDetectPin = 4; // Card-Detection (LOW = Karte vorhanden)

// ---------- RTC ----------
RTC_DS1307 rtc;

char daysOfTheWeek[7][12] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

// ---------- Variablen ----------
bool sdInitialized = false;

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  while (!Serial) ; // Warten bei USB

  Serial.println("\n=== SD + RTC TEST mit Card Detect ===");

  // --- Card Detect Pin konfigurieren ---
  pinMode(cardDetectPin, INPUT_PULLUP); // intern Pullup aktivieren
  delay(50);

  // --- RTC initialisieren ---
  Serial.print("[1] Initialisiere RTC... ");
  if (!rtc.begin()) {
    Serial.println("FEHLER: RTC nicht gefunden!");
    while (1);
  }
  Serial.println("OK");

  if (!rtc.isrunning()) {
    Serial.println("RTC läuft nicht – stelle Zeit auf Kompilierungszeit...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // --- SD Karte prüfen ---
  checkSDCard();
}

// ---------- HAUPTSCHLEIFE ----------
void loop() {
  // Prüfe, ob sich der SD-Card-Status geändert hat
  static bool lastCardState = HIGH;
  bool currentCardState = digitalRead(cardDetectPin);

  if (currentCardState != lastCardState) {
    delay(50); // kleine Entprellung
    currentCardState = digitalRead(cardDetectPin);

    if (currentCardState == LOW && !sdInitialized) {
      Serial.println("\nSD-Karte erkannt → Initialisiere...");
      checkSDCard();
    } else if (currentCardState == HIGH && sdInitialized) {
      Serial.println("\nSD-Karte entfernt!");
      sdInitialized = false;
    }

    lastCardState = currentCardState;
  }

  // --- RTC Zeit anzeigen ---
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

// ---------- SD Initialisierung ----------
void checkSDCard() {
  if (digitalRead(cardDetectPin) == HIGH) {
    Serial.println("Keine SD-Karte eingesteckt.");
    sdInitialized = false;
    return;
  }

  Serial.print("Initialisiere SD-Karte... ");
  if (!card.init(SPI_HALF_SPEED, chipSelect)) {
    Serial.println("FEHLER!");
    Serial.println("* Karte eingesetzt?");
    Serial.println("* Verkabelung korrekt?");
    Serial.println("* Richtiger CS-Pin?");
    sdInitialized = false;
    return;
  }

  if (!volume.init(card)) {
    Serial.println("Keine FAT16/FAT32 Partition gefunden.");
    sdInitialized = false;
    return;
  }

  Serial.println("OK – SD-Karte erkannt und initialisiert!");

  // Kartentyp anzeigen
  Serial.print("Kartentyp: ");
  switch (card.type()) {
    case SD_CARD_TYPE_SD1: Serial.println("SD1"); break;
    case SD_CARD_TYPE_SD2: Serial.println("SD2"); break;
    case SD_CARD_TYPE_SDHC: Serial.println("SDHC"); break;
    default: Serial.println("Unbekannt");
  }

  Serial.print("Dateisystem: FAT");
  Serial.println(volume.fatType());

  sdInitialized = true;
}
