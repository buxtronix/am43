// Your Wifi SSID.
#define WIFI_SSID "...."

// Wifi password.
#define WIFI_PASSWORD "...."

// MQTT server details.
#define MQTT_ADDRESS "192.168.0.88"
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""

// Uncomment below to use the configured device name in the topic instead of the mac.
//#define AM43_USE_NAME_FOR_TOPIC

// Prefix for MQTT topics, you can change this if you have multiple ESP devices, etc.
#define MQTT_TOPIC_PREFIX "am43"

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
