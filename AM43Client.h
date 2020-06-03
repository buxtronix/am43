#ifndef AM43CLIENT_H_
#define AM43CLIENT_H_

#include "BLEDevice.h"

#define AM43_SERVICE_UUID "0000fe50-0000-1000-8000-00805f9b34fb"
#define AM43_CHAR_UUID "0000fe51-0000-1000-8000-00805f9b34fb"

#define AM43_COMMAND_MOVE 0x0A
#define AM43_COMMAND_SET_POSITION 0x0D
#define AM43_NOTIFY_POSITION 0xA1
#define AM43_COMMAND_GET_BATTERY 0xA2
#define AM43_COMMAND_GET_POSITION 0xA7
#define AM43_REPLY_UNKNOWN1 0xA8
#define AM43_REPLY_UNKNOWN2 0xA9
#define AM43_COMMAND_GET_LIGHT 0xA0
#define AM43_COMMAND_LOGIN 0x17
#define AM43_RESPONSE_ACK 0x5A
#define AM43_RESPONSE_NACK 0xA5
#define AM43_DEFAULT_PIN 8888

#define AM43_UPDATE_INTERVAL 120000  // Frequency to poll battery/position.

class AM43Callbacks;

class AM43Client : public BLEClientCallbacks {
  public:
    AM43Client(BLEAdvertisedDevice*);
    AM43Client(BLEAdvertisedDevice *d, uint16_t pin);
    BLEAdvertisedDevice* pDevice;
    BLEClient* pClient;
    BLERemoteCharacteristic* pChar;
    // We are connected.
    boolean connected;
    // Previously (attempted) connected, now disconnected.
    boolean disconnected;
    // True to start a connection.
    boolean doConnect; 
    // We're logged in (correct pin)
    boolean loggedIn;

    // Latest battery level (percent)
    unsigned char batteryPercent;
    // Latest closed amount (0=open, 100=closed)
    unsigned char openLevel;
    unsigned char lightLevel;
    int rssi;
  
    void onConnect(BLEClient* pclient);
    void onDisconnect(BLEClient* pclient);
  
    void setClientCallbacks(AM43Callbacks *callbacks);
    void myNotifyCallback(BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  
    boolean connectToServer();

    void sendGetBatteryRequest();
    void sendGetLightRequest();
    void sendGetPositionRequest();

    void update();

    void open();
    void stop();
    void close();
    void setPosition(uint8_t);

    protected:
      void sendPin();
      void sendCommand(uint8_t, std::vector<uint8_t>);
      uint16_t pin;
      AM43Callbacks *m_ClientCallbacks;
      unsigned long lastUpdate;
      String deviceString();
};


class AM43Callbacks {
  public:
    virtual ~AM43Callbacks() {};
    virtual void onPosition(uint8_t position) = 0;
    virtual void onBatteryLevel(uint8_t level) = 0;
    virtual void onConnect(AM43Client*) = 0;
    virtual void onDisconnect(AM43Client*) = 0;
};

#endif
