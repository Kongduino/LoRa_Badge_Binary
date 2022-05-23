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
  string FromName = lookupTransmitters(From);
  uint16_t To = payload[5] << 8 | payload[6];
  char ResendCount = payload[7];
  char MessageType = payload[8];
  uint8_t precanNum = payload[9];

  // Display useful info
  Serial.printf("Message RSSI: %-d and SNR: %-d\n", rssi, snr);
  Serial.printf("Message UUID: %08x\n", msgUUID);
  Serial.printf(
    "From: %s [%02x] to %04x\nResend count %02x\nMessage type: %s\n",
    FromName.c_str(), From, To, ResendCount, MessageType == 0 ? "Precanned" : "Custom"
  );

  if (To == 0) To = myIntUUID; // 0 = for everybody
  if (myIntUUID == To) {
    // You have a message! Blinky blinky!
    uint16_t py = 50;
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
      uint16_t msgLen = strlen((char*)precanned.at(precanNum).c_str());
      char precannedText[256]; // this could and should be smaller. Make sure in Xojo we keep that short.
      memset(precannedText, 0, 256); // This will ensure we don't display stray characters
      strcpy(precannedText, (char*)precanned.at(precanNum).c_str());
      // precannedText contains now our canned message
      if (precanNum == 1) {
        // Special case: call this number
        // Need to add the next 5 bytes as a French phone number
        sprintf(
          buffer, "From: %s [%02x]: %s: %02d.%02d%02d.%02d%02d",
          FromName.c_str(), From, precannedText, payload[10], payload[11], payload[12], payload[13], payload[14]
        );
      } else {
        sprintf(buffer, "From: %s [%02x]: %s", FromName.c_str(), From, precannedText);
      }
      // show the QR code. Start with that: it erases the screen
      // do not update the display
      showQRCode(buffer, false, false);
      // Show the logo
      display.drawBitmap(192, 0, rak_img, 150, 56, EPD_BLACK);
      Serial.printf("Canned message: %d\n", precanNum);
      sprintf(buffer, "From: %s [%02x]", FromName.c_str(), From);
      drawTextXY(125, py, (char*)buffer, EPD_BLACK, 1);
      py += 12;
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
        py += 12;
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
      // Copy the message to msg
      memcpy(msg, payload + 9, MessageType);
      sprintf(buffer, "From: %s [%02x]: %s", FromName.c_str(), From, msg);
      Serial.println(buffer);
      showQRCode(buffer, false, false);
      display.drawBitmap(192, 0, rak_img, 150, 56, EPD_BLACK);
      sprintf(buffer, "From: %s [%02x]: %s", FromName.c_str(), From);
      drawTextXY(125, py, (char*)buffer, EPD_BLACK, 1);
      py += 12;
      // Display the text 20 chars a line, removing spaces at the beginning of the line.
      for (uint16_t x = 0; x < MessageType; x += 20) {
        // Display 20 chars mac per line
        char c = msg[x + 20];
        msg[x + 20] = 0;
        uint8_t offset = 0;
        while (msg[x + offset] == ' ') offset += 1;
        drawTextXY(125, py, msg + x + offset, EPD_BLACK, 1);
        msg[x + 20] = c;
        py += 12;
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
    // Have we seen this message before?
    if (!lookupMessageUUID(msgUUID)) {
      if (ResendCount < RESEND_LIMIT) {
        // Should we repeat the message?
        // was the message forwarded less than RESEND_LIMIT times?
        if (rssi < RSSI_THRESHOLD || snr < SNR_THRESHOLD) {
          // Is the message signal weak? Yes --> forward
          // Turn on Blue LED. It will be turned off in the Tx callbacks
          digitalWrite(LED_BLUE, HIGH);
          digitalWrite(LED_GREEN, LOW);
          // Copy the payload to TxdBuffer and set the length
          // This will be used by OnCadDone
          memcpy(TxdBuffer, payload, size);
          TxdLength = size;
          // ResendCount = payload[7]; --> Increment the resend count
          TxdBuffer[7] += 1;
          Serial.printf("Forwarding: RSSI and/or SNR too low\n");
          Radio.Standby();
          delay(500);
          Radio.SetCadParams(LORA_CAD_08_SYMBOL, LORA_SPREADING_FACTOR + 13, 10, LORA_CAD_ONLY, 0);
          Radio.StartCad();
        }
      } else {
        Serial.println("Cannot forward any longer, limit reached.");
      }
    } else {
      Serial.println("We've seen this message before!");
    }
  }
  digitalWrite(LED_GREEN, LOW); // Turn off Green LED
  Radio.Rx(RX_TIMEOUT_VALUE);
}

void doLoRaSetup() {
  Serial.println(F("====================================="));
  Serial.println("             LoRa Setup");
  Serial.println(F("====================================="));
  // Initialize the Radio callbacks
  lora_rak4630_init();
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
