#include <vector>
#include <string>
using namespace std;

#include "Adafruit_nRFCrypto.h"
#include <Wire.h>
#include "Adafruit_EEPROM_I2C.h" // Click here to get the library: http://librarymanager/All#Adafruit_EEPROM_I2C

#define EEPROM_ADDR 0x50 // the default address
#define MAXADD 262143 // max address in byte
Adafruit_EEPROM_I2C i2ceeprom;

#define EPD_MOSI MOSI
#define EPD_MISO -1 // not used
#define EPD_SCK SCK
#define EPD_CS SS
#define EPD_DC WB_IO1
#define SRAM_CS -1 // not used
#define EPD_RESET -1 // not used
#define EPD_BUSY WB_IO4
Adafruit_SSD1680 display(250, 122, EPD_MOSI, EPD_SCK, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_MISO, EPD_BUSY);

char MAGIC[4] = {0xde, 0xca, 0xfb, 0xad};
#define UUIDlen 2
char myUUID[UUIDlen];
uint16_t myIntUUID;
char myPlainTextUUID[UUIDlen * 2 + 2];
#define NAMElen 16
char myName[NAMElen + 1];
#define fullSetLen (NAMElen + UUIDlen + 4)
char fullSet[fullSetLen]; // MAGIC = 4 bytes
char buffer[256];
#define MAX_UUID_NUMBER 1024

void showData() {
  if (DEBUG > 0) Serial.printf(" . Name: %s\n . UUID: %s vs %04x\n", myName, myPlainTextUUID, myIntUUID);
}

void readEEPROM() {
  memset(fullSet, 0, fullSetLen);
  uint16_t addr = 0x0000;
  bool rslt = i2ceeprom.read(addr, (uint8_t*)fullSet, fullSetLen);
  if (DEBUG > 0) Serial.printf(" . Read %d bytes\n", fullSetLen);
  memset(myName, 0, NAMElen + 1);
  memcpy(myName, fullSet + 4 + UUIDlen, NAMElen);
  memset(myPlainTextUUID, 0, UUIDlen);
  myIntUUID = 0;
  for (uint8_t ix = 0; ix < UUIDlen; ix++) {
    sprintf(myPlainTextUUID + ix * 2, "%02x", fullSet[ix + 4]);
    myUUID[ix] = fullSet[ix + 4];
    myIntUUID = myIntUUID << 8 | fullSet[ix + 4];
  }
  bool correct = true;
  for (uint8_t ix = 0; ix < 4; ix++) {
    if (fullSet[ix] != MAGIC[ix]) {
      correct = false;
      break;
    }
  }
  if (!correct) {
    if (DEBUG > 0) Serial.println(" . Incorrect format! Please init EEPROM with \"name xxxxx\"!");
    return;
  } else if (DEBUG > 0) Serial.println(" . MAGIC checks out.");
}


/**
   @brief Write a text on the display
   @param x x position to start
   @param y y position to start
   @param text text to write
   @param text_color color of text
   @param text_size size of text
*/
void drawTextXY(int16_t x, int16_t y, char *text, uint16_t text_color, uint32_t text_size) {
  display.setCursor(x, y);
  display.setTextColor(text_color);
  display.setTextSize(text_size);
  display.setTextWrap(true);
  display.print(text);
}

void hexDump(char *buf, uint16_t len) {
  char alphabet[17] = "0123456789abcdef";
  Serial.print(F("   +------------------------------------------------+ +----------------+\n"));
  Serial.print(F("   |.0 .1 .2 .3 .4 .5 .6 .7 .8 .9 .a .b .c .d .e .f | |      ASCII     |\n"));
  for (uint16_t i = 0; i < len; i += 16) {
    if (i % 128 == 0)
      Serial.print(F("   +------------------------------------------------+ +----------------+\n"));
    char s[] = "|                                                | |                |\n";
    uint8_t ix = 1, iy = 52;
    for (uint8_t j = 0; j < 16; j++) {
      if (i + j < len) {
        uint8_t c = buf[i + j];
        s[ix++] = alphabet[(c >> 4) & 0x0F];
        s[ix++] = alphabet[c & 0x0F];
        ix++;
        if (c > 31 && c < 128) s[iy++] = c;
        else s[iy++] = '.';
      }
    }
    uint8_t index = i / 16;
    if (i < 256) Serial.write(' ');
    Serial.print(index, HEX); Serial.write('.');
    Serial.print(s);
  }
  Serial.print(F("   +------------------------------------------------+ +----------------+\n"));
}

vector<string> precanned;
vector<string> transmitters;
vector<uint8_t> transmittersIndex;
vector<uint32_t> receivedMessageUUID;

string lookupTransmitters(uint8_t number) {
  // Check whether we have seen this  transmitter ID (WE SHOULD!)
  for (unsigned i = 0; i < transmittersIndex.size(); i++) {
    if (transmittersIndex[i] == number) return transmitters[i];
    // return the name
  }
  return (string)"?"; // Ouatte Zeu Fouc
}

bool lookupMessageUUID(uint32_t number) {
  // Check whether we have seen this message UUID before
  for (unsigned i = 0; i < receivedMessageUUID.size(); i++) {
    if (receivedMessageUUID[i] == number) return true;
  }
  // No? Cool. Now let's add it to the list.
  receivedMessageUUID.push_back(number);
  if (DEBUG > 0) Serial.printf("Adding %08x to the list\n", number);
  // Do we have too many? If so, let's prune a little...
  if (receivedMessageUUID.size() > MAX_UUID_NUMBER) {
    // let's remove the 16 oldest message UUIDs
    receivedMessageUUID.erase(receivedMessageUUID.begin(), receivedMessageUUID.begin() + 16);
    if (DEBUG > 0) Serial.printf("Pruning oldest 16 message UUIDs. We now have %d.\n", receivedMessageUUID.size());
  }
  // And return false to say it's a new one.
  return false;
}

void initVectors() {
  precanned.push_back("Call the office");
  precanned.push_back("Call this number");
  precanned.push_back("Come back to the office");
  transmitters.push_back("Yannick");
  transmitters.push_back("Albert");
  transmitters.push_back("Toto");
  transmittersIndex.push_back(0x01);
  transmittersIndex.push_back(0x02);
  transmittersIndex.push_back(0x03);
}
