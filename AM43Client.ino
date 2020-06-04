#include "AM43Client.h"
#include "BLEDevice.h"

uint8_t startPacket[5] = {0x00, 0xff, 0x00, 0x00, 0x9a};
std::vector<uint8_t> startPkt(startPacket, startPacket + sizeof(startPacket)/sizeof(startPacket[0]));

AM43Client::AM43Client(BLEAdvertisedDevice *d)
  :AM43Client(d, AM43_DEFAULT_PIN)
{}

AM43Client::AM43Client(BLEAdvertisedDevice *d, uint16_t pin) {
  this->pDevice = d;
  this->connected = false;
  this->disconnected = false;
  this->loggedIn = false;
  this->pin = pin;
  this->batteryPercent = 0xff;
  this->m_ClientCallbacks = nullptr;
}

void AM43Client::onConnect(BLEClient* pclient) {
  this->connected = true;
  if (this->m_ClientCallbacks != nullptr)
    this->m_ClientCallbacks->onConnect(this);
}

void AM43Client::onDisconnect(BLEClient* pclient) {
  this->connected = false;
  this->disconnected = true;
  if (this->m_ClientCallbacks != nullptr)
    this->m_ClientCallbacks->onDisconnect(this);
}

void AM43Client::setClientCallbacks(AM43Callbacks *callbacks) {
  m_ClientCallbacks = callbacks;
}

String AM43Client::deviceString() {
  return String(pDevice->getName().c_str()) + " " + String(pDevice->getAddress().toString().c_str());
}

void AM43Client::myNotifyCallback(
BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (length  < 1 || pData[0] != 0x9a) return;

#if 0
  Serial.printf("%s: 0x", deviceString().c_str());
  for (int i = 0; i < length ; i++) {
    if (pData[i]<0x10) Serial.print("0");
    Serial.print(pData[i], HEX);
  }
  Serial.println();
#endif 
  switch (pData[1]) {
    case AM43_COMMAND_GET_BATTERY: {
      batteryPercent = pData[7];
      if (this->m_ClientCallbacks != nullptr)
        this->m_ClientCallbacks->onBatteryLevel(batteryPercent);
      break;
    }
    case AM43_COMMAND_SET_POSITION: {
      if (pData[3] != AM43_RESPONSE_ACK) {
        Serial.printf("[%s] Position command nack! (%d)\r\n", deviceString().c_str(), pData[3]);
      }
      break;
    }
    case AM43_NOTIFY_POSITION: {
      openLevel = pData[4];
      if (this->m_ClientCallbacks != nullptr)
        this->m_ClientCallbacks->onPosition(openLevel);
      break;
    }
    case AM43_COMMAND_GET_POSITION: {
      openLevel = pData[5];
      if (this->m_ClientCallbacks != nullptr)
        this->m_ClientCallbacks->onPosition(openLevel);
      break;
    }

    case AM43_COMMAND_GET_LIGHT: {
      lightLevel = pData[5];
      if (this->m_ClientCallbacks != nullptr)
        this->m_ClientCallbacks->onLightLevel(lightLevel);
      break;
    }

    case AM43_COMMAND_LOGIN: {
      if (pData[3] == AM43_RESPONSE_ACK) {
        this->loggedIn = true;
        Serial.printf("[%s] Pin ok\r\n", deviceString().c_str());
      } else if (pData[3] == AM43_RESPONSE_NACK) {
        Serial.printf("[%s] Pin incorrect\r\n", deviceString().c_str());
        this->loggedIn = false;
      }
      break;
    }

    case AM43_COMMAND_MOVE: {
      if (pData[3] == AM43_RESPONSE_ACK) {
        Serial.printf("[%s] Move ok\r\n", deviceString().c_str());
        break;
      } else if (pData[3] == AM43_RESPONSE_NACK) {
        Serial.printf("[%s] Move nack\r\n", deviceString().c_str());
        break;
      }
    }

    case AM43_REPLY_UNKNOWN2:
    case AM43_REPLY_UNKNOWN1: {
      Serial.printf("[%s] Unknown reply: ", deviceString().c_str());
      for (int i = 0; i < length ; i++) {
          if (pData[i]<0x10) Serial.print("0");
          Serial.print(pData[i], HEX);
      }
      Serial.println();
      break;
    }

    default: {
      Serial.printf("[%s] Unknown notify data for characteristic %s: 0x", deviceString().c_str(), pBLERemoteCharacteristic->getUUID().toString().c_str());
      for (int i = 0; i < length ; i++) {
        if (pData[i]<0x10) Serial.print("0");
        Serial.print(pData[i], HEX);
      }
      Serial.println();
    }
  }
}

void AM43Client::sendGetBatteryRequest() {
  std::vector<uint8_t> data{0x1};
  sendCommand(AM43_COMMAND_GET_BATTERY, data);
}

void AM43Client::sendGetLightRequest() {
  std::vector<uint8_t> data{0x1};
  sendCommand(AM43_COMMAND_GET_LIGHT, data);
}

void AM43Client::sendGetPositionRequest() {
  std::vector<uint8_t> data{0x1};
  sendCommand(AM43_COMMAND_GET_POSITION, data);
}

void AM43Client::sendPin() {
  std::vector<uint8_t> data{((uint16_t)this->pin & 0xff00)>>8, ((uint16_t)this->pin & 0xff)};
  sendCommand(AM43_COMMAND_LOGIN, data);
}

void AM43Client::open() {
  std::vector<uint8_t> data{0xDD};
  sendCommand(AM43_COMMAND_MOVE, data);
}

void AM43Client::stop() {
  std::vector<uint8_t> data{0xCC};
  sendCommand(AM43_COMMAND_MOVE, data);
}

void AM43Client::close() {
  std::vector<uint8_t> data{0xEE};
  sendCommand(AM43_COMMAND_MOVE, data);
}

void AM43Client::setPosition(uint8_t pos) {
  std::vector<uint8_t> data{pos};
  sendCommand(AM43_COMMAND_SET_POSITION, data);
}

void AM43Client::update() {
  if (millis() - lastUpdate > AM43_UPDATE_INTERVAL) {
    if (!loggedIn) {
      sendPin();
      return;
    }
    switch(currentQuery++) {
      case 0:
      sendGetBatteryRequest();
      break;
      case 1:
      sendGetPositionRequest();
      break;
      case 2:
      sendGetLightRequest();
      break;
    }
    if (currentQuery > 2) currentQuery = 0;

    lastUpdate = millis();
  }
}

byte checksum(std::vector<uint8_t> data) {
  uint8_t checksum = 0;
  for (int i = 0; i < data.size() ; i++)
    checksum = checksum ^ data[i];
  checksum = checksum ^ 0xff;
  return checksum;
}

void AM43Client::sendCommand(uint8_t command, std::vector<uint8_t> data) {
  std::vector<uint8_t> sendData;
  for (int i=0; i < startPkt.size(); i++)
    sendData.push_back(startPkt[i]);
  sendData.push_back(command);
  sendData.push_back((char)data.size());
  sendData.insert(sendData.end(), data.begin(), data.end());
  sendData.push_back(checksum(sendData));
      Serial.printf("[%s] AM43 Send: ", deviceString().c_str());
      for (int i = 0; i < sendData.size() ; i++) {
        if (sendData[i]<0x10) Serial.print("0");
        Serial.print(sendData[i], HEX);
      }
      Serial.println();
  pChar->writeValue(&sendData[0], sendData.size());
}

boolean AM43Client::connectToServer() {
  Serial.printf("Attempting to connect to: %s ", pDevice->getAddress().toString().c_str());
  unsigned long connectStart = millis();
  this->doConnect = false;

  pClient  = BLEDevice::createClient();
  pClient->setClientCallbacks(this);

  this->connected = false;
  // Connect to the remote BLE Server.
  Serial.print("...");
  if (pClient->connect(pDevice)) {   // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");
  } else {
    Serial.println(" - Failed to connect.");
    this->disconnected = true;
    this->connected = false;
    return false;
  }

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }

  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pChar = pRemoteService->getCharacteristic(charUUID);
  if (pChar == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
    
  if(pChar->canNotify())
    pChar->registerForNotify(notifyCallback);

  this->connected = true;
  Serial.printf("Connect took %dms\r\n", millis()-connectStart);
  //this->sendPin();
  return true;
}
