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
#define AM43_COMMAND_GET_LIGHT 0xAA
#define AM43_COMMAND_LOGIN 0x17
#define AM43_RESPONSE_ACK 0x5A
#define AM43_RESPONSE_NACK 0xA5
#define AM43_DEFAULT_PIN 8888

#define AM43_UPDATE_INTERVAL 30000  // Frequency to poll battery/position.

extern BLEUUID serviceUUID;
extern BLEUUID    charUUID;

class AM43Callbacks;

class AM43Client : public BLEClientCallbacks {
  public:
    AM43Client(BLEAdvertisedDevice*);
    AM43Client(BLEAdvertisedDevice *d, uint16_t pin);

    BLEAdvertisedDevice* m_Device;
    BLEClient* m_Client;
    BLERemoteCharacteristic* m_Char;
    std::string m_Name;
    // We are connected.
    boolean m_Connected;
    // Previously (attempted) connected, now disconnected.
    boolean m_Disconnected;
    // True to start a connection.
    boolean m_DoConnect;
    // We're logged in (correct pin)
    boolean m_LoggedIn;

    // Latest battery level (percent)
    unsigned char m_BatteryPercent;
    // Latest closed amount (0=open, 100=closed)
    unsigned char m_OpenLevel;
    unsigned char m_LightLevel;
    int m_Rssi;
  
    void onConnect(BLEClient* pclient);
    void onDisconnect(BLEClient* pclient);
  
    void setClientCallbacks(AM43Callbacks *callbacks);
    void myNotifyCallback(BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  
    boolean connectToServer(notify_callback);

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
      uint16_t m_Pin;
      AM43Callbacks *m_ClientCallbacks;
      unsigned long m_LastUpdate;
      String deviceString();
      uint8_t m_CurrentQuery;
};


class AM43Callbacks {
  public:
    virtual ~AM43Callbacks() {};
    virtual void onPosition(uint8_t position) = 0;
    virtual void onBatteryLevel(uint8_t level) = 0;
    virtual void onLightLevel(uint8_t level) = 0;
    virtual void onConnect(AM43Client*) = 0;
    virtual void onDisconnect(AM43Client*) = 0;
};

#endif
