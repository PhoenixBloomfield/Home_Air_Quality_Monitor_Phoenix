#include "BLEServer.h"

NimBLECharacteristic *pSensorCharacteristic;

class MyCallbacks : public NimBLECharacteristicCallbacks {

  void onConnect(NimBLECharacteristic *pCharacteristic){
    Serial.println("Client connected...");
    xEventGroupSetBits(BLEStateFG, BLE_FLAG_CLIENT_CONNECTED);
  }

  void onSubscribe(NimBLECharacteristic *pCharacteristic){
    Serial.println("Client subscribed...");
    xEventGroupSetBits(BLEStateFG, BLE_FLAG_CLIENT_SUBSCRIBED);
  }

  void onRead(NimBLECharacteristic *pCharacteristic) {
    Serial.println("Read request serviced");
    xEventGroupSetBits(BLEStateFG, BLE_FLAG_READ_COMPLETE);


    // std::string message = std::to_string(rtc.getSecond());
    // pCharacteristic->setValue(BLEMessageBuffer);
  }

  void onWrite(NimBLECharacteristic *pCharacteristic) {
    std::string value = pSensorCharacteristic->getValue();

    if (value.length() > 0) {
      Serial.print("BLE from core ");
      Serial.println(xPortGetCoreID());
      Serial.println("*********");
      Serial.print("New value: ");
      for (int i = 0; i < value.length(); i++)
        Serial.print(value[i]);
      Serial.println();
      Serial.println("*********");
      // Example : TIME=1699143542
      // Epoch time is guaranteed to be 10 digits
      std::string BLEMessageType = value.substr(0, 5);
      if (BLEMessageType == "TIME=") {
        rtc.setTime(stoi(value.substr(5, 10)));
        Serial.print("Current epoch time: ");
        Serial.print(rtc.getDateTime());
      } else if (BLEMessageType == "TGMT=") {
        // Given +/- GMT offset
        // Ex: -06 = MST | +13 = Tonga | +9.5 = Adelaide
        float GMTOffset = stof(value.substr(5, value.length() - 5));
        long epoch_prev = rtc.getEpoch();
        dateConfigured = true;

        // Change RTC offset
        rtc.setTime(epoch_prev + (3600 * GMTOffset));

      } else if (BLEMessageType == "READ!") {
        // String currDateAndTime = rtc.getDateTime();
        // std::string myString = "This is new!";
        // pCharacteristic->setValue("HI Aziz!");
        // pCharacteristic->notify();
        // Serial.println("Updated value...");
        xEventGroupSetBits(appStateFG, APP_FLAG_TRANSMITTING);
        xEventGroupClearBits(appStateFG, APP_FLAG_RUNNING);
      }
    }
  }
};

void BLEServer_comm_task(void *pvParameter) {
  setupBLE();
  while (1) {
    while (xEventGroupGetBits(appStateFG) & APP_FLAG_TRANSMITTING) {
      Serial.println("Inside BLE comm task");
      xEventGroupSetBits(appStateFG, APP_FLAG_PUSH_BUFFER);
      Serial.println("Waiting for buffer to be ready...");
      xEventGroupWaitBits(BLEStateFG, BLE_FLAG_BUFFER_READY, BLE_FLAG_BUFFER_READY, false, 5000);
      Serial.println("Buffer ready...");
      pSensorCharacteristic->setValue(BLEMessageBuffer);
      // Notify
      uint8_t message[1] = { 65 };
      pSensorCharacteristic->notify(&message[0], 1, true);
      Serial.println("Notification sent!");
      xEventGroupWaitBits(BLEStateFG, BLE_FLAG_READ_COMPLETE, BLE_FLAG_READ_COMPLETE, false, 60000);
      Serial.println("Buffer read!");
    }
    vTaskDelay(1000 / portTICK_RATE_MS);
  }
}

void setupBLE() {
  NimBLEDevice::init("NimBLE Test");
  NimBLEServer *pServer = NimBLEDevice::createServer();
  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  pSensorCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);

  pSensorCharacteristic->setCallbacks(new MyCallbacks());
  // pSensorCharacteristic->setValue("Hallo");
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  // pAdvertising->setScanResponse(true);
  // pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  // pAdvertising->setMinPreferred(0x12);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();
}