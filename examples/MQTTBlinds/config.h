// Your Wifi SSID.
#define WIFI_SSID "wifi_ssid"

// Wifi password.
#define WIFI_PASSWORD "wifi_password"

// MQTT server details.
#define MQTT_ADDRESS "192.168.0.88"
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""

// Comment below to use the device mac address in the topic instead of the name. Ignored
// if autodiscovery is enabled (will always use the mac address).
#define AM43_USE_NAME_FOR_TOPIC

// Prefix for MQTT topics, you can change this if you have multiple ESP devices, etc.
#define MQTT_TOPIC_PREFIX "am43"

// Enable MQTT auto-discovery for Home Assistant.
#define AM43_ENABLE_MQTT_DISCOVERY

// Device class to report to HomeAssistant. Must be one of the values at
// https://www.home-assistant.io/integrations/cover/
// Currently don't support per-device class.
#define AM43_MQTT_DEVICE_CLASS "shade"

// PIN for the AM43 device (printed on it, default is 8888)
#define AM43_PIN 8888

// Comma separated list of MAC addresses to allow for control. Useful if you
// have multiple ESP controllers. Leave empty to allow all devices.
//
// Note that the default Arduino SDK only allows connecting to three devices
// maximum. For more devices, you should have multiple ESP32's, ensure they have
// a different MQTT_TOPIC_PREFIX, and use the DEVICE_ALLOWLIST to control up
// to 3 am43's per ESP.
//
#define DEVICE_ALLOWLIST ""
//#define DEVICE_ALLOWLIST "11:22:33:44:55:66"
//#define DEVICE_ALLOWLIST "11:22:33:44:55:66,AA:BB:CC:DD:EE:FF"

// Comment out below to disable the OTA feature, especially if you have
// stability problems.
#define ENABLE_ARDUINO_OTA

// LEAVE BELOW UNTOUCHED
#ifdef AM43_ENABLE_MQTT_DISCOVERY
#undef AM43_USE_NAME_FOR_TOPIC
#endif

// Maximum number of connected devices. This should not be raised unless the Arduino
// BLE stack is also modified as it has this limit also.
#define BLE_MAX_CONN 3
