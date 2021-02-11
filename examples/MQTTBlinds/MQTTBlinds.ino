/**
 * A proxy that allows you to control AM43 style blind controllers via MQTT.
 * 
 * It will scan for and auto-connect to any AM43 devices in range, then provide
 * MQTT topics to control and get status from them.
 *
 * The following MQTT topis are published to:
 *
 * - <prefix>/<device>/available - Either 'offline' or 'online'
 * - <prefix>/<device>/position  - The current blind position, between 0 and 100
 * - <prefix>/<device>/battery   - The current battery level, between 0 and 100
 * - <prefix>/<device>rssi       - The current RSSI reported by the device.
 * - <prefix>/LWT                - Either 'Online' or 'Offline', MQTT status of this service.
 *
 * The following MQTT topics are subscribed to:
 *
 * - <prefix>/<device>/set          - Set the blind to 'OPEN', 'STOP' or 'CLOSE'
 * - <prefix>/<device>/set_position - Set the blind position, between 0 and 100.
 * - <prefix>/restart               - Reboot this service.
 *
 * <device> is the bluetooth mac address of the device, eg 026932f0c51d
 *
 * For the position set commands, you can use name 'all' to change all devices.
 * 
 * Arduino OTA update is supported.
 */

#include "config.h"
#ifdef ENABLE_ARDUINO_OTA
#include <ArduinoOTA.h>
#endif
#include <WiFi.h>
#include <WiFiClient.h>

#include <PubSubClient.h>
#include <AM43Client.h>
#ifdef USE_NIMBLE
#include <NimBLEDevice.h>
#else
#include <BLEDevice.h>
#endif

#ifdef WDT_TIMEOUT
#include <esp_task_wdt.h>
#endif


const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *mqtt_server = MQTT_ADDRESS;
const uint16_t am43Pin = AM43_PIN;

WiFiClient espClient;
PubSubClient pubSubClient(espClient);

#ifdef USE_NIMBLE
struct ble_npl_sem clientListSemaphore;
#else
FreeRTOS::Semaphore clientListSem = FreeRTOS::Semaphore("clients");
#endif

void mqtt_callback(char* top, byte* pay, unsigned int length);

unsigned long lastScan = 0;
boolean scanning = false;
boolean bleEnabled = true; // Controlled via MQTT.
BLEScan* pBLEScan;

const char discTopicTmpl[] PROGMEM = "homeassistant/cover/%s/config";
const char discPayloadTmpl[] PROGMEM = " { \"cmd_t\": \"~/set\", \"pos_t\": \"~/position\","
    "\"pos_open\": 2, \"pos_clsd\": 99, \"set_pos_t\": \"~/set_position\","
    "\"avty_t\": \"~/available\", \"pl_avail\": \"online\", \"pl_not_avail\": \"offline\","
    "\"dev_cla\": \"%s\", \"name\": \"%s\", \"uniq_id\": \"%s_am43_cover\", \"~\": \"%s/%s\"}";

const char discBattTopicTmpl[] PROGMEM = "homeassistant/sensor/%s_Battery/config";
const char discBattPayloadTmpl[] PROGMEM = " { \"device_class\": \"battery\","
    "\"name\":\"%s Battery\", \"stat_t\": \"~/battery\", \"unit_of_meas\": \"%%\","
    "\"avty_t\": \"~/available\", \"uniq_id\": \"%s_am43_battery\", \"~\": \"%s/%s\"}";

const char discLightTopicTmpl[] PROGMEM = "homeassistant/sensor/%s_Light/config";
const char discLightPayloadTmpl[] PROGMEM = " { \"device_class\": \"illuminance\","
    "\"name\":\"%s Light\", \"stat_t\": \"~/light\", \"unit_of_meas\": \"%%\","
    "\"avty_t\": \"~/available\", \"uniq_id\": \"%s_am43_light\", \"~\": \"%s/%s\"}";

const char discSwitchTopic[] PROGMEM = "homeassistant/switch/%s_Switch/config";
const char discSwitchPayload[] PROGMEM = " { \"icon\": \"mdi:bluetooth\","
    "\"name\":\"%s Switch\", \"stat_t\": \"~/enabled\", \"cmd_t\": \"~/enable\","
    "\"uniq_id\": \"%s_enabled\", \"~\": \"%s\"}";

class MyAM43Callbacks: public AM43Callbacks {
  public:
    AM43Client *client;
    WiFiClient wifiClient;
    PubSubClient *mqtt;
    unsigned long nextMqttAttempt;
    String rmtAddress;
    String deviceName;
    String mqttName;
    unsigned long lastBatteryMessage;
    ~MyAM43Callbacks() {
      delete this->client;
      delete this->mqtt;
    }

    String topic(char *t) {
      char top[64];
      sprintf(top, "%s/%s/%s", MQTT_TOPIC_PREFIX, this->mqttName.c_str(), t);
      String ret = String(top);
      return ret;
    }

    void onPosition(uint8_t pos) {
      Serial.printf("[%s] Got position: %d\r\n", this->rmtAddress.c_str(), pos);
      this->mqtt->publish(topic("position").c_str(), String(pos).c_str(), false);
    }
    void onBatteryLevel(uint8_t level) {
      // Ignore overly frequent battery messages. AM43s are known to spam them at times.
      if (this->lastBatteryMessage == 0 || millis() - this->lastBatteryMessage > 30000)  {
        Serial.printf("[%s] Got battery: %d\r\n", this->rmtAddress.c_str(), level);
        this->mqtt->publish(topic("battery").c_str(), String(level).c_str(), false);
        this->lastBatteryMessage = millis();
      }
    }
    void onLightLevel(uint8_t level) {
      Serial.printf("[%s] Got light: %d\r\n", this->rmtAddress.c_str(), level);
      this->mqtt->publish(topic("light").c_str(), String(level).c_str(), false);
    }
    void onConnect(AM43Client *c) {
      this->mqtt = new PubSubClient(this->wifiClient);
      this->nextMqttAttempt = 0;
      this->mqtt->setServer(mqtt_server, 1883);
      this->mqtt->setCallback(mqtt_callback);
      this->mqtt->setBufferSize(512);
      this->rmtAddress = String(c->m_Device->getAddress().toString().c_str());
      this->rmtAddress.replace(":", "");
      this->deviceName = String(client->m_Name.c_str());
#ifdef AM43_USE_NAME_FOR_TOPIC
      this->mqttName = this->deviceName;
#else
      this->mqttName = this->rmtAddress;
#endif
      lastScan = millis()-58000; // Trigger a new scan shortly after connection.
      Serial.printf("[%s] Connected\r\n", this->rmtAddress.c_str());
    }
    void onDisconnect(AM43Client *c) {
      Serial.printf("[%s] Disconnected\r\n", this->rmtAddress.c_str());
      if (this->mqtt != nullptr && this->mqtt->connected()) {
        // Publish offline availability as LWT is only for ungraceful disconnect.
        this->mqtt->publish(topic("available").c_str(), "offline", true);
        this->mqtt->loop();
        this->mqtt->disconnect();
      }
      delete this->mqtt;
      this->mqtt = nullptr;
    }

    void handle() {
      if (this->mqtt == nullptr) return;
      if (this->mqtt->connected()) {
        this->mqtt->loop();
        return;
      }
      if (WiFi.status() != WL_CONNECTED || millis() < this->nextMqttAttempt) return;
      if (!this->mqtt->connect(topic("").c_str(), MQTT_USERNAME, MQTT_PASSWORD, topic("available").c_str(), 0, false, "offline")) {
        Serial.print("MQTT connect failed, rc=");
        Serial.print(this->mqtt->state());
        Serial.println(" retrying in 5s.");
        this->nextMqttAttempt = millis() + 5000;
        return;
      }
      this->mqtt->publish(topic("available").c_str(), "online", true);
      this->mqtt->subscribe(topic("set").c_str());
      this->mqtt->subscribe(topic("set_position").c_str());

#ifdef AM43_ENABLE_MQTT_DISCOVERY
      char discTopic[128];
      char discPayload[300];

      sprintf(discTopic, discTopicTmpl, this->mqttName.c_str());
      sprintf(discPayload, discPayloadTmpl, AM43_MQTT_DEVICE_CLASS, this->deviceName.c_str(), this->rmtAddress.c_str(), MQTT_TOPIC_PREFIX, this->mqttName.c_str());
      this->mqtt->publish(discTopic, discPayload, true);

      sprintf(discTopic, discBattTopicTmpl, this->mqttName.c_str());
      sprintf(discPayload, discBattPayloadTmpl, this->deviceName.c_str(), this->rmtAddress.c_str(), MQTT_TOPIC_PREFIX, this->mqttName.c_str());
      this->mqtt->publish(discTopic, discPayload, true);

      sprintf(discTopic, discLightTopicTmpl, this->mqttName.c_str());
      sprintf(discPayload, discLightPayloadTmpl, this->deviceName.c_str(), this->rmtAddress.c_str(), MQTT_TOPIC_PREFIX, this->mqttName.c_str());
      this->mqtt->publish(discTopic, discPayload, true);
#endif
      this->mqtt->loop();
    }
};

std::map<std::string, MyAM43Callbacks*> allClients;

std::map<std::string, MyAM43Callbacks*> getClients() {
  std::map<std::string, MyAM43Callbacks*> cls;
#ifdef USE_NIMBLE
  ble_npl_sem_pend(&clientListSemaphore, BLE_NPL_TIME_FOREVER);
#else
  clientListSem.take("clientsAll");
#endif
  for (auto const& c : allClients)
    cls.insert({c.first, c.second});
#ifdef USE_NIMBLE
  ble_npl_sem_release(&clientListSemaphore);
#else
  clientListSem.give();
#endif
  return cls;
}


static void notifyCallback(BLERemoteCharacteristic* rChar, uint8_t* pData, size_t length, bool isNotify) {
  auto cls = getClients();
  for (auto const& c : cls) {
    if (c.second->client->m_Char == rChar) {
      c.second->client->myNotifyCallback(rChar, pData, length, isNotify);
      return;
    }
  }
}


void mqtt_callback(char* top, byte* pay, unsigned int length) {
  pay[length] = '\0';
  String payload = String((char *)pay);
  String topic = String(top);
  Serial.printf("MQTT [%s]%d: %s\r\n", top, length, payload.c_str());

  int i1, i2, i3;
  i1 = topic.indexOf('/');
  i2 = topic.indexOf('/', i1+1);
  String address = topic.substring(i1+1, i2);
  String command = topic.substring(i2+1);
  Serial.printf("Addr: %s Cmd: %s\r\n", address.c_str(), command.c_str());
  payload.toLowerCase();

  auto cls = getClients();
  if (address == "restart") {
    for (auto const& c : cls)
      c.second->onDisconnect(c.second->client);
    delay(200);
    ESP.restart();
  }

  if (address == "enable") {
    if (payload == "off") {
      Serial.println("Disabling BLE Clients");
      bleEnabled = false;
      pubSubClient.publish(topPrefix("/enabled").c_str(), "OFF", true);
      for (auto const& c : cls) {
        c.second->client->disconnectFromServer();
      }
    } else if (payload == "on") {
      Serial.println("Enabling BLE Clients");
      bleEnabled = true;
      pubSubClient.publish(topPrefix("/enabled").c_str(), "ON", true);
      lastScan = 0;
    }
  }

  for (auto const& c : cls) {
    auto cl = c.second->client;
    if (c.second->mqttName == address || address == "all") {
      if (command == "set") {
        if (payload == "open") cl->open();
        if (payload == "close") cl->close();
        if (payload == "stop") cl->stop();
      }
      if (command == "set_position")
        cl->setPosition(payload.toInt());
    }
  }
}

std::vector<BLEAddress> allowList;

void parseAllowList() {
  std::string allowListStr = std::string(DEVICE_ALLOWLIST);
  if (allowListStr.length() == 0) return;
  size_t idx1 = 0;

  for(;;) {
    auto idx = allowListStr.find(',', idx1);
    if (idx == std::string::npos) break;
    allowList.push_back(BLEAddress(allowListStr.substr(idx1, idx-idx1)));
    idx1 = idx+1;
  }
  allowList.push_back(BLEAddress(allowListStr.substr(idx1)));
  Serial.println("AllowList contains the following device(s):");
  for (auto dev : allowList)
    Serial.printf(" Mac: %s \n", dev.toString().c_str());
}

bool isAllowed(BLEAddress address) {
  if (allowList.size() < 1) return true;
  for (auto a : allowList) {
    if (a.equals(address)) return true;
  }
  return false;
}

/**
 * Scan for BLE servers and find any that advertise the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
   #ifdef USE_NIMBLE
  void onResult(BLEAdvertisedDevice *advertisedDevice) {
  #else
  void onResult(BLEAdvertisedDevice advDevice) {
    BLEAdvertisedDevice *advertisedDevice = &advDevice;
  #endif
    if (advertisedDevice->getName().length() > 0)
        Serial.printf("BLE Advertised Device found: %s\r\n", advertisedDevice->toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(serviceUUID)) {
      if (!bleEnabled) {
        Serial.printf("BLE connections disabled, ignoring device\r\n");
        return;
      }
      auto cls = getClients();
      for (auto const& c : cls) {
        if (!c.first.compare(advertisedDevice->toString())) {
          Serial.printf("Ignoring advertising device %s, already present\r\n", advertisedDevice->toString().c_str());
          return;
        }
      }
      if (!isAllowed(advertisedDevice->getAddress())) {
        Serial.printf("Ignoring device %s, not in allow list\r\n", advertisedDevice->toString().c_str());
        return;
      }
      if (cls.size() >= BLE_MAX_CONN) {
        Serial.printf("ERROR: Already connected to %d devices, Arduino cannot connect to any more.\r\n", cls.size());
        return;
      }
#ifdef USE_NIMBLE
      //AM43Client* newClient = new AM43Client(advertisedDevice, am43Pin);
      AM43Client* newClient = new AM43Client(new BLEAdvertisedDevice(*advertisedDevice), am43Pin);
#else
      AM43Client* newClient = new AM43Client(new BLEAdvertisedDevice(advDevice), am43Pin);
#endif
      newClient->m_DoConnect = true;
      newClient->m_Name = advertisedDevice->getName();
      MyAM43Callbacks *cbs = new MyAM43Callbacks();
      cbs->client = newClient;
      cbs->lastBatteryMessage = 0;
      newClient->setClientCallbacks(cbs);
#ifdef USE_NIMBLE
      ble_npl_sem_pend(&clientListSemaphore, BLE_NPL_TIME_FOREVER);
#else
      clientListSem.take("clientInsert");
#endif
      allClients.insert({advertisedDevice->toString(), cbs});
#ifdef USE_NIMBLE
      ble_npl_sem_release(&clientListSemaphore);
#else
      clientListSem.give();
#endif
      //pBLEScan->stop();
      //scanning = false;
    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

void bleScanComplete(BLEScanResults r) {
  Serial.println("BLE scan complete.");
  scanning = false;
};

void initBLEScan() {
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
    scanning = true;
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(150);
    pBLEScan->setWindow(99);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(10, bleScanComplete, false);
}

unsigned int wifiDownSince = 0;

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\r\n", event);

  switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      wifiDownSince = millis();
      break;
  }
}

void setup_wifi() {
  WiFi.disconnect(true);
  delay(1000);
  Serial.printf("Wifi connecting to: %s ... \r\n", ssid);
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  wifiDownSince = 0;
}

unsigned long nextMqttAttempt = 0;

String topPrefix(const char *top) {
  String ret = String(MQTT_TOPIC_PREFIX) + top;
  return ret;
}
void reconnect_mqtt() {
  if (WiFi.status() == WL_CONNECTED && millis() > nextMqttAttempt) {
    Serial.print("Attempting MQTT connection...\r\n");
    String mqttPrefix = String(MQTT_TOPIC_PREFIX);
    // Attempt to connect
    if (pubSubClient.connect(topPrefix("-gateway").c_str(), MQTT_USERNAME, MQTT_PASSWORD, topPrefix("/LWT").c_str(), 0, false, "Offline")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      pubSubClient.publish(topPrefix("/LWT").c_str(), "Online", true);
      pubSubClient.subscribe(topPrefix("/restart").c_str());
      pubSubClient.subscribe(topPrefix("/enable").c_str());
      pubSubClient.subscribe(topPrefix("/all/set").c_str());
      pubSubClient.subscribe(topPrefix("/all/set_position").c_str());
      pubSubClient.loop();

      char discTopic[128];
      char discPayload[300];

      sprintf(discTopic, discSwitchTopic, MQTT_TOPIC_PREFIX);
      sprintf(discPayload, discSwitchPayload, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX);
      pubSubClient.publish(discTopic, discPayload, true);
      pubSubClient.publish(topPrefix("/enabled").c_str(), "ON", true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(pubSubClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      nextMqttAttempt = millis() + 5000;
    }
  }
}

bool otaUpdating = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
#ifdef USE_NIMBLE
  Serial.println("Using NimBLE stack.");
#else
  Serial.println("Using legacy stack.");
#endif
  setup_wifi();
  pubSubClient.setServer(mqtt_server, 1883);
  pubSubClient.setCallback(mqtt_callback);

  parseAllowList();
#ifdef USE_NIMBLE
  ble_npl_sem_init(&clientListSemaphore, 1);
#endif
  
#ifdef ENABLE_ARDUINO_OTA
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
      // Stop any active BLEScan during OTA - improves stability.
      otaUpdating = true;
      pBLEScan->stop();
    })
    .onEnd([]() {
      Serial.println("\nEnd");
      pubSubClient.disconnect();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
      otaUpdating = false;
    });

  ArduinoOTA.begin();
#endif
  otaUpdating = false;
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  BLEDevice::init("");
  initBLEScan();
#ifdef WDT_TIMEOUT
  Serial.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
#endif
 } // End of setup.

unsigned long lastAM43update = 0;

// This is the Arduino main loop function.
void loop() {

  if (WiFi.status() != WL_CONNECTED && wifiDownSince > 0 && millis()-wifiDownSince > 20000) {
    setup_wifi();
  }
  if (!pubSubClient.connected()) {
    reconnect_mqtt();
  }
  pubSubClient.loop();
  
  if (millis()-lastAM43update > 500) { // Only process this every 500ms.
    std::vector<std::string> removeList; // Clients will be added to this as they are disconnected.
    auto cls = getClients();
    // Iterate through connected devices, perform any connect/update/etc actions.
    for (auto const &c : cls) {
      if (c.second->client->m_DoConnect && !scanning) {
        c.second->client->connectToServer(notifyCallback);
        break;  // Connect takes some time, so break out to allow other processing.
      }
      if (c.second->client->m_Connected) {
        c.second->client->update();
        c.second->handle();
      }
      if (c.second->client->m_Disconnected) removeList.push_back(c.first);
    }
    // Remove any clients that have been disconnected.
#ifdef USE_NIMBLE
    ble_npl_sem_pend(&clientListSemaphore, BLE_NPL_TIME_FOREVER);
#else
    clientListSem.take("clientRemove");
#endif
    for (auto i : removeList) {
      delete allClients[i];
      allClients.erase(i);
    }
#ifdef USE_NIMBLE
    ble_npl_sem_release(&clientListSemaphore);
#else
   clientListSem.give();
#endif

    lastAM43update = millis();
  }
  // Start a new scan every 60s.
  if (millis() - lastScan > 60000 && !otaUpdating && !scanning) {
    scanning = true;
    pBLEScan->start(10, bleScanComplete, false);
    lastScan = millis();
    Serial.printf("Up for %ds\r\n", millis()/1000);
  }

#ifdef ENABLE_ARDUINO_OTA
  ArduinoOTA.handle();
#endif

#ifdef WDT_TIMEOUT
  esp_task_wdt_reset();
#endif
} // End of loop
