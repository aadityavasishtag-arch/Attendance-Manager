/*
  RFID Attendance Manager - Arduino Uno + MFRC522 (RC522)
  Components:
    - Arduino Uno
    - RC522 RFID reader
    - 16x2 LCD with I2C backpack (optional)
    - Active buzzer (optional)
    - Green LED + 220 ohm resistor (optional)
    - Red LED + 220 ohm resistor (optional)

  Libraries (Arduino IDE > Library Manager):
    - MFRC522 by GithubCommunity
    - LiquidCrystal I2C (only if using the LCD)

  Wiring (Arduino Uno):
    RC522 SDA/SS -> D10
    RC522 SCK    -> D13
    RC522 MOSI   -> D11
    RC522 MISO   -> D12
    RC522 RST    -> D9
    RC522 3.3V   -> 3.3V  (IMPORTANT: do not connect RC522 to 5V)
    RC522 GND    -> GND
    Buzzer +     -> D6, Buzzer - -> GND
    Green LED    -> D4 through 220 ohm resistor to GND
    Red LED      -> D5 through 220 ohm resistor to GND
    I2C LCD SDA  -> A4, SCL -> A5, VCC/GND as required by your LCD module

  Notes:
    - Edit the sample UID values below to match your own cards.
    - UID is an identifier, not secure authentication; RFID cards can be cloned.
    - This sketch emits CSV lines to Serial: ATTENDANCE,UID,STATUS,MILLIS
      It does not itself save records permanently after power loss.
*/

#include <SPI.h>
#include <MFRC522.h>

// Uncomment the next two lines if you have an I2C LCD and installed its library.
// #include <Wire.h>
// #include <LiquidCrystal_I2C.h>

#define SS_PIN 10
#define RST_PIN 9
#define BUZZER_PIN 6
#define GREEN_LED_PIN 4
#define RED_LED_PIN 5

MFRC522 rfid(SS_PIN, RST_PIN);

// Set this to true only if you installed an LCD and uncommented its library above.
// const bool USE_LCD = true;
// LiquidCrystal_I2C lcd(0x27, 16, 2);

struct Student {
  const char* uid;
  const char* name;
};

// REPLACE these sample UIDs with the UIDs printed in the Serial Monitor for your cards.
// UIDs are compared in uppercase, with no spaces.
Student students[] = {
  {"A1B2C3D4", "Student One"},
  {"11223344", "Student Two"},
  {"DEADBEEF", "Student Three"}
};
const int STUDENT_COUNT = sizeof(students) / sizeof(students[0]);

// Simple in-session duplicate prevention. It resets when Arduino restarts.
bool marked[STUDENT_COUNT] = {false};

String uidToString(MFRC522::Uid *uid) {
  String result = "";
  for (byte i = 0; i < uid->size; i++) {
    if (uid->uidByte[i] < 0x10) result += "0";
    result += String(uid->uidByte[i], HEX);
  }
  result.toUpperCase();
  return result;
}

int findStudentIndex(const String& uid) {
  for (int i = 0; i < STUDENT_COUNT; i++) {
    if (uid.equals(students[i].uid)) return i;
  }
  return -1;
}

void beep(int frequency, int durationMs) {
  tone(BUZZER_PIN, frequency, durationMs);
  delay(durationMs);
  noTone(BUZZER_PIN);
}

void setup() {
  Serial.begin(9600);
  while (!Serial) { }

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  SPI.begin();
  rfid.PCD_Init();

  // If using LCD, uncomment and configure this block:
  // lcd.init();
  // lcd.backlight();
  // lcd.setCursor(0, 0);
  // lcd.print("RFID Attendance");
  // lcd.setCursor(0, 1);
  // lcd.print("Scan your card");

  Serial.println(F("RFID Attendance Manager ready."));
  Serial.println(F("Scan a card to display its UID."));
  Serial.println(F("For unknown cards, copy the UID and add it to the students[] list."));
  Serial.println(F("CSV_HEADER,UID,STATUS,MILLIS"));
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uid = uidToString(&rfid.uid);
  int index = findStudentIndex(uid);

  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  if (index < 0) {
    Serial.print(F("UNKNOWN,"));
    Serial.println(uid);
    digitalWrite(RED_LED_PIN, HIGH);
    beep(500, 250);
    digitalWrite(RED_LED_PIN, LOW);
    // If using LCD:
    // lcd.clear(); lcd.print("Unknown card");
    // lcd.setCursor(0,1); lcd.print(uid.substring(0, 16));
  } else if (marked[index]) {
    Serial.print(F("ATTENDANCE,"));
    Serial.print(uid);
    Serial.println(F(",DUPLICATE"));
    digitalWrite(RED_LED_PIN, HIGH);
    beep(700, 120);
    delay(80);
    beep(700, 120);
    digitalWrite(RED_LED_PIN, LOW);
    // If using LCD:
    // lcd.clear(); lcd.print("Already marked");
    // lcd.setCursor(0,1); lcd.print(students[index].name);
  } else {
    marked[index] = true;
    Serial.print(F("ATTENDANCE,"));
    Serial.print(uid);
    Serial.print(F(",PRESENT,"));
    Serial.println(millis());

    digitalWrite(GREEN_LED_PIN, HIGH);
    beep(1200, 180);
    delay(250);
    digitalWrite(GREEN_LED_PIN, LOW);
    // If using LCD:
    // lcd.clear(); lcd.print("Attendance OK");
    // lcd.setCursor(0,1); lcd.print(students[index].name);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(500); // prevents rapid repeat reads of the same card
}
