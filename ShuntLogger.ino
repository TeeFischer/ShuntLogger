#include <SPI.h>
#include <SD.h>
#include <RTClib.h>

// Pins
#define chipSelect     10   // CS für SD-Karte
#define cardDetectPin   4   // Card-Detection (LOW = Karte vorhanden)

// ---------- SD-Karte ----------
Sd2Card card;
SdVolume volume;
SdFile root;
File logFile;

// ---------- RealTimeClock ----------
RTC_DS1307 rtc;
bool rtcInitialized = false;

char daysOfTheWeek[7][12] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

// ---------- Variablen ----------
bool sdInitialized = false;
bool lastCardState = HIGH;
#define resistance 10 // in ohms

// ---------- Timer ----------
unsigned long lastTimeUpdate = 0;
const unsigned long timeInterval = 1000; // in ms
// ---------- Takt-Variablen ----------
// (not used) unsigned long lastWriteTime removed
const unsigned long writeInterval = 1000; // in ms

// ---------- Messtakt-Variablen ----------
unsigned long lastMeasurementTime = 0;
#define numMeasurementsPerWrite 10
#define measurementInterval writeInterval / numMeasurementsPerWrite

// Puffer für Messungen (wird gruppenweise auf die SD geschrieben)
String logBuffer = "";
unsigned int measurementsInBuffer = 0;
float totalVoltage = 0;
float totalCurrent = 0;

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  while (!Serial); // Warten bei USB

  Serial.println("\n=== SD + RTC + Analog-Logger ===");

  // RTC initialisieren
  Serial.print("[1] Initialisiere RTC.");
  for (uint8_t i = 0; i < 5; i++) {
    delay(200);
    Serial.print(".");
  }

  if (!rtc.begin()) {
    Serial.println("FEHLER: RTC nicht gefunden! Nutze Systemzeit als Fallback.");
    rtcInitialized = false;
  } else {
    rtcInitialized = true;
    Serial.println("OK - RTC gefunden.");
  }

  // Initialisiere SD-Karte und andere Komponenten
  checkSDCard();
}

void loop() {
  unsigned long currentMillis = millis();

  // --- RTC-Zeit ausgeben oder Systemzeit verwenden ---
  if (currentMillis - lastTimeUpdate >= timeInterval) {
    lastTimeUpdate = currentMillis;

    if (rtcInitialized) {
      // RTC-Zeit verwenden, wenn RTC initialisiert ist
      printDateTime();
    } else {
      // Systemzeit verwenden, wenn RTC nicht gefunden wurde
      printSystemTime();
    }
  }
}

// ---------- RTC-Zeit ausgeben ----------
void printDateTime() {
  DateTime now = rtc.now();

  Serial.print("Datum & Zeit (RTC): ");
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
}

// ---------- Systemzeit ausgeben (Fallback, wenn RTC nicht verfügbar) ----------
void printSystemTime() {
  unsigned long currentMillis = millis();
  
  // Zeit als Stunden, Minuten und Sekunden aus der Systemzeit berechnen
  unsigned long seconds = currentMillis / 1000;
  unsigned long hours = (seconds / 3600) % 24;
  unsigned long minutes = (seconds / 60) % 60;
  unsigned long secondsRem = seconds % 60;

  Serial.print("Datum & Zeit (Systemzeit): ");
  Serial.print("2025/11/20 "); // Beispiel-Datum (als Fallback)
  Serial.print("(");
  Serial.print(daysOfTheWeek[0]);  // Beispiel Wochentag (Sunday)
  Serial.print(") ");
  Serial.print(hours);
  Serial.print(':');
  Serial.print(minutes);
  Serial.print(':');
  Serial.println(secondsRem);
}

// ---------- SD-Initialisierung ----------
void checkSDCard() {
  Serial.print("[2] Initialisiere SD-Karte... ");
  if (digitalRead(cardDetectPin) == HIGH) {
    Serial.println("Keine SD-Karte eingesteckt.");
    sdInitialized = false;
    return;
  }

  delay(200);

  if (!card.init(SPI_HALF_SPEED, chipSelect)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card inserted?");
    Serial.println("* is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
    sdInitialized = false;
    return;
  }

  Serial.println("OK - SD-Karte erkannt und initialisiert!");
  sdInitialized = true;
}

// ---------- SD-Karte schreiben ----------
void writeToSD() {
  if (!sdInitialized) {
    Serial.println("⚠️ Keine SD-Karte initialisiert.");
    return;
  }
  
  // Mittelwerte berechnen
  float averageVoltage = totalVoltage / measurementsInBuffer;
  float averageCurrent = totalCurrent / measurementsInBuffer;

  // Schreibe Puffer in eine Datei (einmalig pro Gruppe)
  Serial.println("Schreibe Puffer auf SD-Karte...");
  logFile = SD.open("log.txt", FILE_WRITE);
  if (!logFile) {
    Serial.println("⚠️  Fehler beim Öffnen von log.txt");
//    return;
  }

  // Optional: Schreibe einen kurzen Header mit Schreibzeit
  DateTime now = rtc.now();
  char header[64];
  snprintf(header, sizeof(header), "=== Batch write: %04d-%02d-%02d %02d:%02d:%02d ===\n",
           now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  logFile.print(header);

  // Ganze Puffer-Inhalte schreiben
  logFile.print(logBuffer);

  // Schreibe den Mittelwert
  logFile.print("Mittelwert: ");
  logFile.print("Spannung: ");
  logFile.print(averageVoltage, 3);
  logFile.print(" V, Strom: ");
  logFile.print(averageCurrent, 4);
  logFile.println(" A");

  logFile.close(); 

  Serial.print("Geschriebene Einträge: ");
  Serial.println(measurementsInBuffer);

  // Puffer leeren
  logBuffer = "";
  measurementsInBuffer = 0;
  totalCurrent = 0;
  totalVoltage = 0;
  
  // Schreibe den Mittelwert
  Serial.print("Mittelwert: ");
  Serial.print("Spannung: ");
  Serial.print(averageVoltage, 3);
  Serial.print(" V, Strom: ");
  Serial.print(averageCurrent, 4);
  Serial.println(" A");
}
