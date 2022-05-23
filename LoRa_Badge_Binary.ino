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

#include <Adafruit_GFX.h>
#include <Adafruit_EPD.h>
#include "images.h"
#include "Format.h"
#include "LoRa_Related.h"

QRCode qrcode;
uint8_t version = 3;

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
  if (showASCII) Serial.print("\n\n\n\n");
  for (uint8_t y = 0; y < myWidth; y++) {
    // Left quiet zone
    Serial.print("        ");
    // Each horizontal module
    uint16_t px = ix;
    for (uint8_t x = 0; x < myWidth; x += 2) {
      // Print each module (UTF-8 \u2588 is a solid block)
      if (showASCII) Serial.print(qrcode_getModule(&qrcode, x, y) ? "\u2588\u2588" : "  ");
      uint8_t c = 0;
      if (qrcode_getModule(&qrcode, x, y)) c = 0xF0;
      if (x + 1 < myWidth && qrcode_getModule(&qrcode, x + 1, y)) {
        c += 0x0F;
        if (showASCII) Serial.print("\u2588\u2588");
      } else {
        if (showASCII) Serial.print("  ");
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
    if (showASCII) Serial.print("\n");
  }
  // Bottom quiet zone
  if (showASCII) Serial.print("\n\n\n\n");
  display.drawBitmap(0, 0, qrc, qrc_wd * 8, qrc_hg, EPD_BLACK);
  if (refresh) display.display(true);
}

void setup() {
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
  Serial.println(F("====================================="));
  Serial.println("        QR Code EPD LoRa Test");
  Serial.println(F("====================================="));
  Serial.println("          Turning on modules");
  pinMode(WB_IO2, INPUT_PULLUP); // EPD
  digitalWrite(WB_IO2, HIGH);
  delay(300);
  Serial.println(F("====================================="));
  if (i2ceeprom.begin(EEPROM_ADDR)) {
    // you can put a different I2C address here, e.g. begin(0x51);
    Serial.println("Found I2C EEPROM");
  } else {
    Serial.println("I2C EEPROM not identified ... check your connections?\r\n");
    while (1) {
      delay(10);
    }
  }
  readEEPROM();
  initVectors();
  Serial.println("Epaper-QRCode test\n======================");
  display.begin();
  memset(buffer, 0, 32);
  sprintf(buffer, "UUID: %s, Name: %s", myPlainTextUUID, myName);
  uint8_t ln = strlen(buffer);
  hexDump(buffer, ln);
  showQRCode(buffer);
  display.drawBitmap(192, 0, rak_img, 150, 56, EPD_BLACK);
  sprintf(buffer, "UUID: %s", myPlainTextUUID);
  drawTextXY(125, 60, buffer, EPD_BLACK, 2);
  drawTextXY(125, 80, myName, EPD_BLACK, 2);
  display.display(true);
  doLoRaSetup();
}

void loop() {
}
