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

#include <Adafruit_GFX.h>
#include <Adafruit_EPD.h>
#include "images.h"
#include "Format.h"

void showQRCode(char *, bool, bool);

// LoRa
static RadioEvents_t RadioEvents;
// Define LoRa parameters
#define RF_FREQUENCY 868300000 // Hz
#define TX_OUTPUT_POWER 22 // dBm
#define LORA_BANDWIDTH 0 // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved]
#define LORA_SPREADING_FACTOR 10 // [SF7..SF12]
#define LORA_CODINGRATE 1 // [1: 4/5, 2: 4/6,  3: 4/7,  4: 4/8]
#define LORA_PREAMBLE_LENGTH 8 // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0 // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE 100000
#define TX_TIMEOUT_VALUE 3000

#define RSSI_THRESHOLD -85
#define SNR_THRESHOLD 0
#define RESEND_LIMIT 3

char buffer[256];
// #define BUZZER_CONTROL WB_IO3
char TxdBuffer[256];
uint8_t TxdLength;

void OnTxDone(void) {
  Serial.println("OnTxDone");
  Radio.Rx(RX_TIMEOUT_VALUE);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_GREEN, HIGH);
}

void OnTxTimeout(void) {
  Serial.println("OnTxTimeout");
  Radio.Rx(RX_TIMEOUT_VALUE);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_GREEN, HIGH);
}

void OnCadDone(bool cadResult) {
  if (cadResult) {
    Serial.printf("CAD returned channel busy!\n");
    Radio.Rx(RX_TIMEOUT_VALUE);
  } else {
    Serial.printf("CAD returned channel free: Sending...");
    Radio.Send((uint8_t*)TxdBuffer, TxdLength);
    Serial.println(" done!");
  }
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_GREEN, HIGH);
}

void OnRxTimeout(void) {
  digitalWrite(LED_BLUE, HIGH);
  Serial.println("\nRx Timeout!");
  delay(500);
  digitalWrite(LED_BLUE, LOW);
  Radio.Rx(RX_TIMEOUT_VALUE);
}

void OnRxError(void) {
  Serial.println("Rx Error!");
  digitalWrite(LED_BLUE, HIGH);
  delay(200);
  digitalWrite(LED_BLUE, LOW);
  delay(200);
  digitalWrite(LED_BLUE, HIGH);
  delay(200);
  digitalWrite(LED_BLUE, LOW);
  delay(200);
  digitalWrite(LED_BLUE, HIGH);
  delay(200);
  digitalWrite(LED_BLUE, LOW);
  delay(200);
  Radio.Rx(RX_TIMEOUT_VALUE);
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  // Read fixed data
  uint32_t msgUUID = payload[0] << 24 | payload[1] << 16 | payload[2] << 8 | payload[3];
  char From = payload[4];
  uint16_t To = payload[5] << 8 | payload[6];
  char ResendCount = payload[7];
  char MessageType = payload[8];
  uint8_t precanNum = payload[9];

  // Display useful info
  Serial.printf(buffer, "Message RSSI: %-d and SNR: %-d", rssi, snr);
  Serial.printf("Message UUID: %08x\n", msgUUID);
  Serial.printf(
    "From: %02x to %04x\nResend count %02x\nMessage type: %s\n",
    From, To, ResendCount, MessageType == 0 ? "Precanned" : "Custom"
  );

  if (To == 0) To = myIntUUID; // 0 = for everybody
  if (myIntUUID == To) {
    // You have a message! Blinky blinky!
    for (uint8_t i = 0; i < 5; i++) {
      digitalWrite(LED_BLUE, HIGH);
      digitalWrite(LED_GREEN, LOW);
      delay(200);
      digitalWrite(LED_BLUE, LOW);
      digitalWrite(LED_GREEN, HIGH);
      delay(200);
    }
    if (MessageType == 0) {
      Serial.printf("Canned message #%d\n", precanNum);
      uint16_t py = 45, msgLen = strlen((char*)precanned.at(precanNum).c_str());
      char precannedText[256]; // this could and should be smaller. Make sure in Xojo we keep that short.
      memset(precannedText, 0, 256); // This will ensure we don't display stray characters
      strcpy(precannedText, (char*)precanned.at(precanNum).c_str());
      // precannedText contains now our canned message
      if (precanNum == 1) {
        // Special case: call this number
        // Need to add the next 5 bytes as a French phone number
        sprintf(
          buffer, "From: %02x: %s: %02d.%02d%02d.%02d%02d",
          From, precannedText, payload[10], payload[11], payload[12], payload[13], payload[14]
        );
      } else {
        sprintf(buffer, "From: %02x: %s", From, precannedText);
      }
      // show the QR code. Start with that: it erases the screen
      // do not update the display
      showQRCode(buffer, false, false);
      // Show the logo
      display.drawBitmap(192, 0, rak_img, 150, 56, EPD_BLACK);
      Serial.printf("Canned message: %d\n", precanNum);
      sprintf(buffer, "From: %02x", From);
      drawTextXY(125, py, (char*)buffer, EPD_BLACK, 1);
      py += 15;
      Serial.println(precannedText);
      // Display the text 20 chars a line, removing spaces at the beginning of the line.
      for (uint16_t x = 0; x < msgLen; x += 20) {
        // Display 20 chars mac per line
        char c = precannedText[x + 20];
        precannedText[x + 20] = 0;
        uint8_t offset = 0;
        while (precannedText[x + offset] == ' ') offset += 1;
        drawTextXY(125, py, precannedText + x + offset, EPD_BLACK, 1);
        precannedText[x + 20] = c;
        py += 15;
      }
      if (precanNum == 1) {
        // Special case: call this number. Display the phone number
        sprintf(buffer, "%02d.%02d%02d.%02d%02d", payload[10], payload[11], payload[12], payload[13], payload[14]);
        drawTextXY(125, py, buffer, EPD_BLACK, 1);
      }
    } else {
      // Custom message
      // MessageType serves as a Pascal String length
      char msg[MessageType + 1] = {0};
      uint16_t py = 45;
      // Copy the message to msg
      memcpy(msg, payload + 9, MessageType);
      sprintf(buffer, "From: %02x: %s", From, msg);
      Serial.println(buffer);
      showQRCode(buffer, false, false);
      display.drawBitmap(192, 0, rak_img, 150, 56, EPD_BLACK);
      sprintf(buffer, "From: 0x%02x:", From);
      drawTextXY(125, py, (char*)buffer, EPD_BLACK, 1);
      py += 15;
      // Display the text 20 chars a line, removing spaces at the beginning of the line.
      for (uint16_t x = 0; x < MessageType; x += 20) {
        // Display 20 chars mac per line
        char c = msg[x + 20];
        msg[x + 20] = 0;
        uint8_t offset = 0;
        while (msg[x + offset] == ' ') offset += 1;
        drawTextXY(125, py, msg + x + offset, EPD_BLACK, 1);
        msg[x + 20] = c;
        py += 15;
      }
    }
    // Finally update the EPD
    display.display(true);
  } else {
    // Not for us!
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_GREEN, LOW);
    delay(500);
    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_GREEN, HIGH);
    Serial.printf(" --> Not for me! [%04x vs %04x]", myIntUUID, To);
    if (ResendCount < RESEND_LIMIT) {
      // Should we repeat the message?
      // was the message forwarded less than RESEND_LIMIT times?
      if (rssi < RSSI_THRESHOLD || snr < SNR_THRESHOLD) {
        // Is the message signal weak? Yes --> forward
        digitalWrite(LED_BLUE, HIGH);
        digitalWrite(LED_GREEN, LOW);
        memcpy(TxdBuffer, payload, size);
        TxdLength = size;
        Serial.printf("Forwarding: RSSI is too low\n");
        Radio.Standby();
        delay(500);
        Radio.SetCadParams(LORA_CAD_08_SYMBOL, LORA_SPREADING_FACTOR + 13, 10, LORA_CAD_ONLY, 0);
        Radio.StartCad();
      }
    }
  }
  digitalWrite(LED_GREEN, LOW); // Turn off Green LED
  Radio.Rx(RX_TIMEOUT_VALUE);
}

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
  //  pinMode(WB_IO6, INPUT_PULLUP);
  //  digitalWrite(WB_IO6, HIGH);
  //  delay(300);
  // pinMode(BUZZER_CONTROL, OUTPUT);
  // delay(300);
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
  Serial.println(F("====================================="));
  Serial.println("             LoRa Setup");
  Serial.println(F("====================================="));
  // Initialize the Radio callbacks
#if defined NRF52_SERIES
  lora_rak4630_init();
#else
  lora_rak11300_init();
#endif
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError = OnRxError;
  RadioEvents.CadDone = OnCadDone;
  // Initialize the Radio
  Radio.Init(&RadioEvents);
  // Set Radio channel
  Radio.SetChannel(RF_FREQUENCY);
  Serial.println("Freq = " + String(RF_FREQUENCY / 1e6) + " MHz");
  Serial.println("SF = " + String(LORA_SPREADING_FACTOR));
  Serial.println("BW = " + String(LORA_BANDWIDTH) + " KHz");
  Serial.println("CR = 4/" + String(LORA_CODINGRATE));
  Serial.println("Tx Power = " + String(TX_OUTPUT_POWER));
  // Set Radio TX configuration
  Radio.SetTxConfig(
    MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
    true, 0, 0, LORA_IQ_INVERSION_ON, TX_TIMEOUT_VALUE);
  SX126xSetTxParams(TX_OUTPUT_POWER, RADIO_RAMP_40_US);
  Radio.SetRxConfig(
    MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
  Serial.println("Starting Radio.Rx");
  Radio.Rx(RX_TIMEOUT_VALUE);
}

void loop() {
}
