/*
  Test Code für den Shunt Logger:
  - RTC (DS1307/DS3231) zeigt aktuelle Zeit an (1x pro Sekunde)
  - SD-Karte wird erkannt und initialisiert, wenn Card-Detect-Pin LOW ist
  - Analoge Spannung (A0) wird regelmäßig gelesen und auf SD-Karte geloggt
*/

#include <SPI.h>
#include <SD.h>
#include <RTClib.h>

// ---------- SD-Karte ----------
Sd2Card card;
SdVolume volume;
SdFile root;
File logFile;

// Pins
#define chipSelect     10   // CS für SD-Karte
#define cardDetectPin   4   // Card-Detection (LOW = Karte vorhanden)
#define analogPinPlus  A0   // Analogeingang der positiven Seite des Widerstands und dem Pluspol
#define analogPinGND   A1   // Analogeingang des Minuspols
#define analogPinMinus A2   // Analogeingang der negativen Seite des Widerstands

// ---------- RealTimeClock ----------
RTC_DS1307 rtc;

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
const unsigned long writeInterval = 5000; // in ms

// ---------- Messtakt-Variablen ----------
unsigned long lastMeasurementTime = 0;
#define numMeasurementsPerWrite 10
#define measurementInterval writeInterval / numMeasurementsPerWrite

// Puffer für Messungen (wird gruppenweise auf die SD geschrieben)
String logBuffer = "";
unsigned int measurementsInBuffer = 0;

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  while (!Serial) ; // Warten bei USB

  Serial.println("\n=== SD + RTC + Analog-Logger ===");

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

  // Erstmaliger SD-Check
  checkSDCard();
}

// ---------- HAUPTSCHLEIFE ----------
void loop() {
  unsigned long currentMillis = millis();

  // --- SD-Karte überwachen ---
  bool currentCardState = digitalRead(cardDetectPin);
  if (currentCardState != lastCardState) {
    static unsigned long debounceStart = 0;
    if (debounceStart == 0) debounceStart = currentMillis;

    if (currentMillis - debounceStart > 50) {
      if (currentCardState == LOW && !sdInitialized) {
        Serial.println("\nSD-Karte erkannt → Initialisiere...");
        checkSDCard();
      } else if (currentCardState == HIGH && sdInitialized) {
        Serial.println("\nSD-Karte entfernt!");
        sdInitialized = false;
      }

      lastCardState = currentCardState;
      debounceStart = 0;
    }
  }

  // // --- RTC-Zeit jede Sekunde anzeigen ---
  // if (currentMillis - lastTimeUpdate >= timeInterval) {
  //   lastTimeUpdate = currentMillis;
  //   printDateTime();
  // }

  // --- Messung einmal pro measurementInterval ---
  if (currentMillis - lastMeasurementTime >= measurementInterval) {
    lastMeasurementTime = currentMillis;
    // Eine Messung durchführen und in den Puffer schreiben
    readAndLogAnalog();

    // Wenn Puffer voll ist, schreibe alle Messungen zusammen auf die SD-Karte
    if (measurementsInBuffer >= numMeasurementsPerWrite) {
      writeToSD();
    }
  }
}

// ---------- SD Initialisierung ----------
void checkSDCard() {
  if (digitalRead(cardDetectPin) == HIGH) {
    Serial.println("Keine SD-Karte eingesteckt.");
    sdInitialized = false;
    return;
  }

  Serial.print("Initialisiere SD-Karte... ");
  if (!SD.begin(chipSelect)) {
    Serial.println("FEHLER!");
    sdInitialized = false;
    return;
  }

  Serial.println("OK - SD-Karte erkannt und initialisiert!");
  sdInitialized = true;
}

// ---------- RTC-Zeit ausgeben ----------
void printDateTime() {
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
}

// ---------- Analoge Spannung lesen & loggen ----------
void readAndLogAnalog() {
  // Lese Sensor
  int plusValue = analogRead(analogPinPlus);
  int minusValue = analogRead(analogPinMinus);
  int gndValue = analogRead(analogPinGND);
  int loadValue = plusValue - minusValue;
  int totalValue = plusValue - gndValue;
  float load = loadValue * (5.0 / 1023.0);
  float totalVoltage = totalValue * (5.0 / 1023.0);

  DateTime now = rtc.now();

  // Datensatz zusammensetzen (Timestamp + Spannung)
  char line[100];
  snprintf(line, sizeof(line),
           "%04d-%02d-%02d %02d:%02d:%02d, %.3f V\n",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second(),
           load);

  float current = load/ resistance;

  Serial.print("Voltage: ");
  Serial.print(totalVoltage, 3);
  Serial.print(" V, Last:");
  Serial.print(load, 3);
  Serial.print(" V, Strom: ");
  Serial.print(current, 4);
  Serial.println(" A");

  // In Puffer anhängen (wird später gruppenweise auf die SD geschrieben)
  logBuffer += String(line);
  measurementsInBuffer++;
}

// ---------- SD-Karte schreiben ----------
void writeToSD() {
  if (!sdInitialized) {
    Serial.println("⚠️ Keine SD-Karte initialisiert.");
    return;
  }
  // Schreibe Puffer in eine Datei (einmalig pro Gruppe)
  Serial.println("Schreibe Puffer auf SD-Karte...");
  logFile = SD.open("log.txt", FILE_WRITE);
  if (!logFile) {
    Serial.println("⚠️  Fehler beim Öffnen von log.txt");
    return;
  }

  // Optional: Schreibe einen kurzen Header mit Schreibzeit
  DateTime now = rtc.now();
  char header[64];
  snprintf(header, sizeof(header), "=== Batch write: %04d-%02d-%02d %02d:%02d:%02d ===\n",
           now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  logFile.print(header);

  // Ganze Puffer-Inhalte schreiben
  logFile.print(logBuffer);
  logFile.close();

  Serial.print("Geschriebene Einträge: ");
  Serial.println(measurementsInBuffer);

  // Puffer leeren
  logBuffer = "";
  measurementsInBuffer = 0;
}
