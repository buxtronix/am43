# AM43  blind controller for ESP32

A proxy that allows you to control AM43 style blind controllers via MQTT,
using an ESP32 device's built-in Bluetooth radio.

These blind and rollershade controllers are sold under various names, including
Zemismart, A-OK and all use the "Blind Engine" app.

Feel free to send PRs.

## Overview
 
This sketch will scan for and auto-connect to any AM43 devices in range, then provide
MQTT topics to control and get status from them.

The following MQTT topis are published to:

- am43/&lt;device>/available - Either 'offline' or 'online'
- am43/&lt;device>/position  - The current blind position, between 0 and 100
- am43/&lt;device>/battery   - The current battery level, between 0 and 100
- am43/&lt;device>/light     - The current light level, between 0 and 100
- am43/LWT                - Either 'Online' or 'Offline', MQTT status of this service.

The following MQTT topics are subscribed to:

- am43/&lt;device>/set          - Set the blind to 'OPEN', 'STOP' or 'CLOSE'
- am43/&lt;device>/set_position - Set the blind position, between 0 and 100.
- am43/restart               - Reboot this service.

&lt;device> is the bluetooth mac address of the device, eg 02:69:32:f0:c5:1d

If you enable AM43_USE_NAME_FOR_TOPIC then the device name configured is used
in the topic instead of the mac.

For the position set commands, you can use name 'all' to change all devices.

## Getting started

In the file *config.h*, configure your Wifi credentials and your MQTT server
details. If your AM43 devices are not using the default pin (8888) also set it
there.

The sketch takes up a lot of space on flash thanks to the use of Wifi and BLE - you
will need to increase the available space by changing the board options in the
Arduino IDE. Once you have selected your ESP32 board in *Tools*, select 
*Tools -> Partition Scheme -> Minimal SPIFFS* to enable the larger program space.

### Patch BLE library

Whilst developing this, I found a bug in the ESP32 Arduino BLE library
which causes problems when connecting to multiple devices. A patch
will been submitted to the BLE maintainers, though if it's not yet in
your release, find BLEClient.cpp in your installation and change:

```
// Search for the following block, around line 180 and add the line.

case ESP_GATTC_DISCONNECT_EVT: {
  if (evtParam->disconnect.conn_id != getConnId()) break;  // <- ADD THIS LINE
  // If we receive a disconnect event, set the class flag that indicates that we are
  // no longer connected.
  m_isConnected = false;

// Also add another line around line 238.

case ESP_GATTC_CONNECT_EVT: {
  if (evtParam->connect.conn_id != getConnId()) break;  // <- ADD THIS LINE
  BLEDevice::updatePeerDevice(this, true, m_gattc_if);

```

## Testing

Lots of information is printed over the serial console. Connect to your ESP32 device
at 115200 baud and there should be plenty of chatter.

Once you have your device ready, you can monitor and control it using an MQTT
client. For example, using mosquitto_sub, you can watch activity with:

```
$ mosquitto_sub -h <mqtt_server> -v -t am43/#
am43/LWT Online
am43/02:69:32:f2:c4:1d/available offline
am43/02:69:32:f2:c4:1d/position 0
am43/02:69:32:f2:c4:1d/battery 70
am43/02:69:32:f2:c4:1d/light 49
am43/02:4d:45:f0:5b:2e/available offline
am43/02:4d:45:f0:5b:2e/battery 100
am43/02:4d:45:f0:5b:2e/position 0
am43/02:4d:45:f0:5b:2e/light 68
```

It's trivial to then control the shades similarly:

```
$ mosquitto_sub -h <mqtt_server> -t am43/02:69:32:f2:c4:1d/set -m OPEN
```

You can also control all in unison:

```
$ mosquitto_sub -h <mqtt_server> -t am43/all/set -m CLOSE
```

Note that only one BLE client can connect to the AM43 at a time. So you cannot
have multiple controllers or have the device app (Blind Engine) running at
the same time.

It's recommended that you configure your AM43 devices with the native app first.

## Home Assistant configuration

The MQTT topics are set to integrate natively with Home Assistant. Once both
are talking to the MQTT server, add the following configuration for each
blind:

```
cover:
  - platform: mqtt
    name: "Bedroom right"
    device_class: "shade"
    command_topic: "am43/02:69:32:f2:c4:1d/set"
    position_topic: "am43/02:69:32:f2:c4:1d/position"
    set_position_topic: "am43/02:69:32:f2:c4:1d/set_position"
    availability_topic: "am43/02:69:32:f2:c4:1d/available"
    position_open: 0
    position_closed: 100

sensor:
  - platform: mqtt
    name: "Bedroom right blind battery"
    state_topic: "am43/02:69:32:f2:c4:1d/battery"
    unit_of_measurement: "%"
    device_class: battery

  - platform: mqtt
    name: "Bedroom left blind light"
    state_topic: "am43/02:69:32:f2:c4:1d/light"
    unit_of_measurement: "%"
    device_class: illuminance

```

## TODO

 - Expose the configured device name, perhaps use it for MQTT topic name.
 - Consider more functionality such as device configuration.
 - Allow buttons on the ESP32 for control?
 - Port this to native ESPHome

## Copyright

Copyright 2020, Ben Buxton. Licenced under the MIT licence, see LICENSE.
