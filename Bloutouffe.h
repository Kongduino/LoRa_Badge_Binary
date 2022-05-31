#include <bluefruit.h>

bool bleConnected = false;
/**
   @brief  BLE UART service
   @note   Used for BLE UART communication
*/
BLEUart g_BleUart;

void ble_connect_callback(uint16_t);
void ble_disconnect_callback(uint16_t, uint8_t);

void setupBLE() {
  // Config the peripheral connection with maximum bandwidth
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  if (DEBUG > 0) Serial.println(F("====================================="));
  if (DEBUG > 0) Serial.println(F("             BLE Setup"));
  if (DEBUG > 0) Serial.println(F("====================================="));
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.configPrphConn(92, BLE_GAP_EVENT_LENGTH_MIN, 16, 16);
  Bluefruit.begin(1, 0);
  // Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(4);
  // Set the BLE device name
  char bleName[33] = {0};
  sprintf(bleName, "Badge_%s", myPlainTextUUID);
  if (DEBUG > 0) Serial.printf("Setting name as %s\n", bleName);
  Bluefruit.setName(bleName);
  Bluefruit.Periph.setConnectCallback(ble_connect_callback);
  Bluefruit.Periph.setDisconnectCallback(ble_disconnect_callback);
  // Configure and Start BLE Uart Service
  g_BleUart.begin();
  // Set up and start advertising
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addName();
  /* Start Advertising
    - Enable auto advertising if disconnected
    - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
    - Timeout for fast mode is 30 seconds
    - Start(timeout) with timeout = 0 will advertise forever (until connected)
    For recommended advertising interval
    https://developer.apple.com/library/content/qa/qa1931/_index.html
  */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30); // number of seconds in fast mode
  Bluefruit.Advertising.start(0); // 0 = Don't stop advertising after n seconds
  Bluefruit.autoConnLed(false);
  Bluefruit._stopConnLed();
}

/**
   @brief  Callback when client connects
   @param  conn_handle: Connection handle id
*/
void ble_connect_callback(uint16_t conn_handle) {
  (void)conn_handle;
  bleConnected = true;
  if (DEBUG > 0) Serial.println("BLE client connected");
}

/**
   @brief  Callback invoked when a connection is dropped
   @param  conn_handle: connection handle id
   @param  reason: disconnect reason
*/
void ble_disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  (void)reason;
  bleConnected = false;
  if (DEBUG > 0) Serial.println("BLE client disconnected");
}
