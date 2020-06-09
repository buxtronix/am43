#include <Arduino.h>
#include "AM43Client.h"
#include "BLEDevice.h"

BLEUUID serviceUUID(AM43_SERVICE_UUID);
BLEUUID    charUUID(AM43_CHAR_UUID);


uint8_t startPacket[5] = {0x00, 0xff, 0x00, 0x00, 0x9a};
std::vector<uint8_t> startPkt(startPacket, startPacket + sizeof(startPacket)/sizeof(startPacket[0]));

AM43Client::AM43Client(BLEAdvertisedDevice *d)
  :AM43Client(d, AM43_DEFAULT_PIN)
{}

AM43Client::AM43Client(BLEAdvertisedDevice *d, uint16_t pin) {
  this->m_Device = d;
  this->m_Connected = false;
  this->m_Disconnected = false;
  this->m_LoggedIn = false;
  this->m_Pin = pin;
  this->m_BatteryPercent = 0xff;
  this->m_ClientCallbacks = nullptr;
}

void AM43Client::onConnect(BLEClient* pclient) {
  this->m_Connected = true;
  if (this->m_ClientCallbacks != nullptr)
    this->m_ClientCallbacks->onConnect(this);
}

void AM43Client::onDisconnect(BLEClient* pclient) {
  this->m_Connected = false;
  this->m_Disconnected = true;
  if (this->m_ClientCallbacks != nullptr)
    this->m_ClientCallbacks->onDisconnect(this);
}

void AM43Client::setClientCallbacks(AM43Callbacks *callbacks) {
  this->m_ClientCallbacks = callbacks;
}

String AM43Client::deviceString() {
  return String(this->m_Device->getName().c_str()) + " " + String(this->m_Device->getAddress().toString().c_str());
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
      this->m_BatteryPercent = pData[7];
      if (this->m_ClientCallbacks != nullptr)
        this->m_ClientCallbacks->onBatteryLevel(this->m_BatteryPercent);
      break;
    }
    case AM43_COMMAND_SET_POSITION: {
      if (pData[3] != AM43_RESPONSE_ACK) {
        Serial.printf("[%s] Position command nack! (%d)\r\n", deviceString().c_str(), pData[3]);
      }
      break;
    }
    case AM43_NOTIFY_POSITION: {
      this->m_OpenLevel = pData[4];
      if (this->m_ClientCallbacks != nullptr)
        this->m_ClientCallbacks->onPosition(this->m_OpenLevel);
      break;
    }
    case AM43_COMMAND_GET_POSITION: {
      this->m_OpenLevel = pData[5];
      if (this->m_ClientCallbacks != nullptr)
        this->m_ClientCallbacks->onPosition(this->m_OpenLevel);
      break;
    }

    case AM43_COMMAND_GET_LIGHT: {
      this->m_LightLevel = pData[5];
      if (this->m_ClientCallbacks != nullptr)
        this->m_ClientCallbacks->onLightLevel(this->m_LightLevel);
      break;
    }

    case AM43_COMMAND_LOGIN: {
      if (pData[3] == AM43_RESPONSE_ACK) {
        this->m_LoggedIn = true;
        Serial.printf("[%s] Pin ok\r\n", deviceString().c_str());
      } else if (pData[3] == AM43_RESPONSE_NACK) {
        Serial.printf("[%s] Pin incorrect\r\n", deviceString().c_str());
        this->m_LoggedIn = false;
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
  this->sendCommand(AM43_COMMAND_GET_BATTERY, data);
}

void AM43Client::sendGetLightRequest() {
  std::vector<uint8_t> data{0x1};
  this->sendCommand(AM43_COMMAND_GET_LIGHT, data);
}

void AM43Client::sendGetPositionRequest() {
  std::vector<uint8_t> data{0x1};
  this->sendCommand(AM43_COMMAND_GET_POSITION, data);
}

void AM43Client::sendPin() {
  std::vector<uint8_t> data{((uint16_t)this->m_Pin & 0xff00)>>8, ((uint16_t)this->m_Pin & 0xff)};
  this->sendCommand(AM43_COMMAND_LOGIN, data);
}

void AM43Client::open() {
  std::vector<uint8_t> data{0xDD};
  this->sendCommand(AM43_COMMAND_MOVE, data);
}

void AM43Client::stop() {
  std::vector<uint8_t> data{0xCC};
  this->sendCommand(AM43_COMMAND_MOVE, data);
}

void AM43Client::close() {
  std::vector<uint8_t> data{0xEE};
  this->sendCommand(AM43_COMMAND_MOVE, data);
}

void AM43Client::setPosition(uint8_t pos) {
  std::vector<uint8_t> data{pos};
  this->sendCommand(AM43_COMMAND_SET_POSITION, data);
}

void AM43Client::update() {
  if (millis() - this->m_LastUpdate > AM43_UPDATE_INTERVAL) {
    if (!this->m_LoggedIn) {
      this->sendPin();
      return;
    }
    switch(this->m_CurrentQuery++) {
      case 0:
      this->sendGetBatteryRequest();
      break;
      case 1:
      this->sendGetPositionRequest();
      break;
      case 2:
      this->sendGetLightRequest();
      break;
    }
    if (this->m_CurrentQuery > 2) this->m_CurrentQuery = 0;

    this->m_LastUpdate = millis();
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
  m_Char->writeValue(&sendData[0], sendData.size());
}

boolean AM43Client::connectToServer(notify_callback callback) {
  Serial.printf("Attempting to connect to: %s ", this->m_Device->getAddress().toString().c_str());
  unsigned long connectStart = millis();
  this->m_DoConnect = false;

  this->m_Client  = BLEDevice::createClient();
  this->m_Client->setClientCallbacks(this);

  this->m_Connected = false;
  // Connect to the remote BLE Server.
  Serial.print("...");
  if (this->m_Client->connect(this->m_Device)) {   // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");
  } else {
    Serial.println(" - Failed to connect.");
    this->m_Disconnected = true;
    this->m_Connected = false;
    return false;
  }

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = m_Client->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    this->m_Client->disconnect();
    return false;
  }

  // Obtain a reference to the characteristic in the service of the remote BLE server.
  m_Char = pRemoteService->getCharacteristic(charUUID);
  if (m_Char == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    this->m_Client->disconnect();
    return false;
  }
    
  if(this->m_Char->canNotify())
    this->m_Char->registerForNotify(callback);

  this->m_Connected = true;
  Serial.printf("Connect took %dms\r\n", millis()-connectStart);
  return true;
}
