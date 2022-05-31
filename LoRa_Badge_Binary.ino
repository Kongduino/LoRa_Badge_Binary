#undef max
#undef min
#include <string>
#include <vector>

using namespace std;
template class basic_string<char>; // https://github.com/esp8266/Arduino/issues/1136
namespace std _GLIBCXX_VISIBILITY(default) {
_GLIBCXX_BEGIN_NAMESPACE_VERSION
void __throw_length_error(char const*) {
}
void __throw_out_of_range(char const*) {
}
void __throw_logic_error(char const*) {
}
void __throw_out_of_range_fmt(char const*, ...) {
}
}

#include <Arduino.h>
//#include <Adafruit_TinyUSB.h>
#include <SX126x-RAK4630.h>
#include <qrcode.h>
// https://github.com/ricmoo/qrcode/

void showQRCode(char *, bool, bool);
void hexDump(char *, uint16_t);
void readEEPROM();
void showData();
void initEEPROM(char *);
void initReadVBAT(void);
uint8_t readBatt(void);
uint8_t lorawanBattLevel(void);

#include <Adafruit_GFX.h>
#include <Adafruit_EPD.h>
#include "Format.h"
#include "Bloutouffe.h"
#include "images.h"
#include "LoRa_Related.h"
#include "Battery.h"

QRCode qrcode;
uint8_t version = 3;
uint32_t lastBatteryCheck = millis();
uint32_t BatteryCheckDelay = 60000;
#define DEBUG 1
#define SLEEP_MODE 1
#ifdef SLEEP_MODE
// Time the device is sleeping in milliseconds = 10 * 1000 milliseconds
#define SLEEP_TIME 10000
#define TIMER_WAKEUP 1
// Semaphore used by events to wake up loop task
SemaphoreHandle_t taskEvent = NULL;
// Timer to wakeup task frequently and send message
SoftwareTimer taskWakeupTimer;
uint8_t eventType = -1;
/**
   @brief Timer event that wakes up the loop task frequently
   @param unused
*/
void periodicWakeup(TimerHandle_t unused) {
  // Switch on blue LED to show we are awake
  digitalWrite(LED_BLUE, HIGH);
  eventType = TIMER_WAKEUP;
  // Give the semaphore, so the loop task will wake up
  xSemaphoreGiveFromISR(taskEvent, pdFALSE);
}
#endif


void showQRCode(char *msg, bool showASCII = true, bool refresh = false) {
  display.clearBuffer();
  uint16_t sz = qrcode_getBufferSize(version);
  uint8_t qrcodeData[sz];
  qrcode_initText(&qrcode, qrcodeData, version, 0, msg);
  uint8_t myWidth = qrcode.size;
  uint16_t qrc_wd = myWidth / 2;
  if (myWidth * 2 != qrc_wd) qrc_wd += 1;
  uint16_t qrc_hg = myWidth * 4;
  uint16_t qrc_sz = qrc_wd * qrc_hg, ix = 0;
  unsigned char qrc[qrc_sz];
  // Text version: look at the Serial Monitor :-)
  if (showASCII) {
    if (DEBUG > 0) Serial.print("\n\n\n\n");
  }
  for (uint8_t y = 0; y < myWidth; y++) {
    // Left quiet zone
    if (DEBUG > 0) Serial.print("        ");
    // Each horizontal module
    uint16_t px = ix;
    for (uint8_t x = 0; x < myWidth; x += 2) {
      // Print each module (UTF-8 \u2588 is a solid block)
      if (showASCII) {
        if (DEBUG > 0) Serial.print(qrcode_getModule(&qrcode, x, y) ? "\u2588\u2588" : "  ");
      }
      uint8_t c = 0;
      if (qrcode_getModule(&qrcode, x, y)) c = 0xF0;
      if (x + 1 < myWidth && qrcode_getModule(&qrcode, x + 1, y)) {
        c += 0x0F;
        if (showASCII) {
          if (DEBUG > 0) Serial.print("\u2588\u2588");
        }
      } else {
        if (showASCII) {
          if (DEBUG > 0) Serial.print("  ");
        }
      }
      qrc[ix++] = c;
    }
    memcpy(qrc + ix, qrc + px, qrc_wd);
    px = ix;
    ix += qrc_wd;
    memcpy(qrc + ix, qrc + px, qrc_wd);
    px = ix;
    ix += qrc_wd;
    memcpy(qrc + ix, qrc + px, qrc_wd);
    px = ix;
    ix += qrc_wd;
    if (showASCII) {
      if (DEBUG > 0) Serial.print("\n");
    }
  }
  // Bottom quiet zone
  if (showASCII) {
    if (DEBUG > 0) Serial.print("\n\n\n\n");
  }
  display.drawBitmap(0, 0, qrc, qrc_wd * 8, qrc_hg, EPD_BLACK);
  if (refresh) display.display(true);
}

void setup() {
  // Initialize the built in LED
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  time_t timeout = millis();
  while (!Serial) {
    if ((millis() - timeout) < 5000) {
      delay(100);
    } else {
      break;
    }
  }
  Serial.begin(115200);
  delay(300);
  if (DEBUG > 0) Serial.println(F("====================================="));
  if (DEBUG > 0) Serial.println("        QR Code EPD LoRa Test");
  if (DEBUG > 0) Serial.println(F("====================================="));
  if (DEBUG > 0) Serial.println("          Turning on modules");
  pinMode(WB_IO2, INPUT_PULLUP); // EPD
  digitalWrite(WB_IO2, HIGH);
  delay(300);
  if (DEBUG > 0) Serial.println(F("====================================="));
  if (i2ceeprom.begin(EEPROM_ADDR)) {
    // you can put a different I2C address here, e.g. begin(0x51);
    if (DEBUG > 0) Serial.println("Found I2C EEPROM");
  } else {
    if (DEBUG > 0) Serial.println("I2C EEPROM not identified ... check your connections?\r\n");
    while (1) {
      delay(10);
    }
  }
  readEEPROM();
  initVectors();
  if (DEBUG > 0) Serial.println("Epaper-QRCode test\n======================");
  display.begin();
  memset(buffer, 0, 32);
  sprintf(buffer, "UUID: %s, Name: %s", myPlainTextUUID, myName);
  uint8_t ln = strlen(buffer);
  hexDump(buffer, ln);
  showQRCode(buffer);
  display.drawBitmap(196, 0, rak_img, 150, 56, EPD_BLACK);
  sprintf(buffer, "UUID: %s", myPlainTextUUID);
  drawTextXY(125, 60, buffer, EPD_BLACK, 2);
  drawTextXY(125, 80, myName, EPD_BLACK, 2);
  doLoRaSetup();
  setupBLE();
  initReadVBAT();
  uint8_t batlvl, pct;
  batlvl = readBatt();
  delay(100);
  batlvl = readBatt();
  if (batlvl < 20) pct = 0;
  else if (batlvl < 40) pct = 1;
  else if (batlvl < 60) pct = 2;
  else if (batlvl < 80) pct = 3;
  else if (batlvl < 100) pct = 4;
  else pct = 5;
  sprintf(buffer, "LIPO = %02d%%\n", batlvl);
  if (DEBUG > 0) Serial.print(buffer);
  display.drawBitmap(120, 0, epd_bitmap_allArray[pct], 50, 20, EPD_BLACK);
  display.display(true);
#ifdef SLEEP_MODE
  // Create the event semaphore
  taskEvent = xSemaphoreCreateBinary();
  // Initialize semaphore
  xSemaphoreGive(taskEvent);
  taskWakeupTimer.begin(SLEEP_TIME, periodicWakeup);
  taskWakeupTimer.start();
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_GREEN, LOW);
  Bluefruit._stopConnLed();
  xSemaphoreTake(taskEvent, 10);
#endif
}

uint8_t greenStatus = 255, blueStatus = 0;
int8_t greenIncrement = -1, blueIncrement = -1;
void loop() {
#ifdef SLEEP_MODE
  uint32_t t0 = millis();
  if (xSemaphoreTake(taskEvent, portMAX_DELAY) == pdTRUE) {
    // Switch on blue LED to show we are awake
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_BUILTIN, LOW);
    // Check the wake up reason
    switch (eventType) {
      case TIMER_WAKEUP:
        // Wakeup reason is timer
        if (DEBUG > 0) Serial.println("Timer wakeup.");
        /// \todo read sensor or whatever you need to do frequently
        // Send the data package
        break;
      default:
        if (DEBUG > 0) Serial.println("This should never happen ;-)");
        break;
    }
  }
#endif
  if (millis() - lastBatteryCheck > BatteryCheckDelay) {
    sprintf(buffer, "LIPO = %02d%%\n", readBatt());
    if (DEBUG > 0) Serial.print(buffer);
    if (bleConnected) g_BleUart.print(buffer);
    lastBatteryCheck = millis();
  }
  //  analogWrite(LED_GREEN, greenStatus);
  //  analogWrite(LED_BLUE, blueStatus);
  //  if (greenStatus == 0) greenIncrement = 1;
  //  else if (greenStatus == 255) greenIncrement = -1;
  //  greenStatus += greenIncrement;
  //  if (blueStatus == 0) blueIncrement = 1;
  //  else if (blueStatus == 255) blueIncrement = -1;
  //  blueStatus += blueIncrement;
  //  delay(5);
  // Forward anything received from BLE UART to USB Serial
  if (g_BleUart.available()) {
    if (DEBUG > 0) Serial.print(g_BleUart.readString());
  }
#ifdef SLEEP_MODE
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_BUILTIN, LOW);
  delay(5000);
  // Switch off blue LED to show we are going to slep
  digitalWrite(LED_BLUE, LOW);
  if (DEBUG > 0) Serial.print("Going to sleep... ");
  Bluefruit._stopConnLed();
  // Go back to sleep
  xSemaphoreTake(taskEvent, 10);
#endif
}
