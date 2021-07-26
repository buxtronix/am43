# AM43 blind controller library for ESP32

A library and example sketch that allows you to control AM43 style blind controllers
using an ESP32 device's built-in Bluetooth radio.

Included is an example sketch to perform control and monitoring via MQTT, including
auto-discovery for Home Assistant.

These blind and rollershade controllers are sold under various names, including
Zemismart, A-OK and all use the "Blind Engine" app.

Feel free to send PRs.

Looking for native ESPHome support? [See this repo](https://github.com/buxtronix/esphome-am43)

## MQTTBlinds example overview
 
This sketch will scan for and auto-connect to any AM43 devices in range, then provide
MQTT topics to control and get status from them.

For Home Assisant, auto-discovery is supported so connected AM43 devices will
show up as entities. See below for simple setup.

Note that the Arduino ESP32 integration only supports connecting to a maximum of 3
bluetooth devices. See below if you have more.

The following MQTT topis are published to:

| Topic | Description | Values |
| ----- | ----------- | ------ |
| am43/&lt;device>/available | Connection status of the blind controller | Either 'offline' or 'online' |
| am43/&lt;device>/position  | The current blind position | between 0 and 100 |
| am43/&lt;device>/battery   | The current battery level | between 0 and 100 |
| am43/&lt;device>/light     | The current light level | between 0 and 100 |
| am43/enabled            | Whether the BLE client is enabled | Either "on" or "off" |
| am43/LWT                | MQTT connection status | Either 'Online' or 'Offline' |

The following MQTT topics are subscribed to:

| Topic | Description | Values |
| ----- | ----------- | ------ |
| am43/&lt;device>/set          | Set the blind position | 'OPEN', 'STOP' or 'CLOSE' |
| am43/&lt;device>/set_position | Set the blind % position | between 0 and 100. |
| am43/&lt;device>/status | Get device status on demand (position/light/battery) | Ignored. |
| am43/enable               | Enable or disable BLE connections | 'off' or 'on'.
| am43/restart               | Reboot this service | Ignored.
| am43/cmnd/#                | Only on bleOnDemand mode.  This topic process commands with on demand connections. ex. am43/cmnd/&lt;device>/set ; am32/cmnd/&lt;device>/set_position  | Depends on every command

&lt;device> is the bluetooth mac address of the device, eg 02:69:32:f0:c5:1d

If you enable AM43_USE_NAME_FOR_TOPIC in config.h, then the device name configured is used
in the topic instead of the mac.

For the position set commands, you can use name 'all' to change all devices.

## Getting started

Download this archive and unzip it to your Arduino installation's *libraries*
folder, it should extract as its own sub-folder. Then (re)start the IDE.

Open the example Sketch under *File -> Examples -> AM43Client -> MQTTBlinds*

In the file tab *config.h*, configure your Wifi credentials and your MQTT server
details. If your AM43 devices are not using the default pin (8888) also set it
there.

### Components needed

- ESP-32  --> v1.0.6 (https://github.com/espressif/arduino-esp32)
- NimBLE-Arduino --> v1.2.0 (https://github.com/h2zero/NimBLE-Arduino)

### Installation with NimBLE

As of Version 0.5.0, the library uses the [NimBLE bluetooth stack](https://github.com/h2zero/NimBLE-Arduino/),
rather than the legacy Arduino stack. This is signficantly smaller in both flash and
RAM usage, so should increase stability. You can download the NimBLE library directly
within the Arduino Library Manager (*Sketch->Include Library->Manage Libraries*).

### Legacy (non-NimBLE only) installation requirements

It's recommended to use the NimBLE library, but if you are having issues with
this, you can use the legacy stack (please also raise a bug).

To have the AM43 library use the legacy BLE stack, edit the file *AM43Client.h* in your
installation and comment out the line ```#define USE_NIMBLE``` near the top.

The sketch takes up a lot of space on flash thanks to the use of Wifi and BLE - you
will need to increase the available space by changing the board options in the
Arduino IDE. Once you have selected your ESP32 board in *Tools*, select 
*Tools -> Partition Scheme -> Minimal SPIFFS* to enable the larger program space.

#### Patch BLE library

Whilst developing this, I found bugs in the ESP32 Arduino BLE libraries
which cause significant instability issues. A patch will been submitted to
the BLE maintainers, though if it's not yet in your release, you will need
to make the following changes, which result in a massive stability gain:

Find <b>BLEClient.cpp</b> in your installation, and make the following changes.

```
// Search for the following block, around line 180 and add the line.

case ESP_GATTC_DISCONNECT_EVT: {
  if (evtParam->disconnect.conn_id != getConnId()) break;  // <- ADD THIS LINE
  // If we receive a disconnect event, set the class flag that indicates that we are
  // no longer connected.
  m_isConnected = false;

// Also two changes around line 238.

case ESP_GATTC_CONNECT_EVT: {
  if (evtParam->connect.conn_id != getConnId()) break;  // <- ADD THIS LINE
  BLEDevice::updatePeerDevice(this, true, m_appId);  
//                                        ^^^^^^^ CHANGE THIS PARAMETER.

```

Next, find BLEScan.cpp, and add the following on line ~28 in the Constructor:

```
    m_scan_params.scan_duplicate     = BLE_SCAN_DUPLICATE_DISABLE;
```

Finally, you need to find the file <b>esp32-hal-bt.c</b> and make the following changes:

```
// Change the mode to BLE only on the following line:

bool btStart(){
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    cfg.mode = ESP_BT_MODE_BLE;  // <- ADD THIS LINE

// Then around 10 lines below this, replace these lines:
//       if (esp_bt_controller_enable(BT_MODE)) {
//            log_e("BT Enable failed");
//            return false;
//        }
// with:
        auto err_p = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (err_p) {
            log_e("BT Enable failed err=%d", err_p);
            return false;
        }
```

## Testing

Note that only one BLE client can connect to the AM43 at a time. So you cannot
have multiple ESP controllers or have the device app (Blind Engine) running at
the same time.

You should perform initial setup of your AM43 devices with the native app
before running this gateway. This includes setting the device name and motor
direction.

Lots of information is printed over the serial console. Connect to your ESP32 device
at 115200 baud and there should be plenty of chatter.

Once you have your device ready, you can monitor and control it using an MQTT
client. For example, using mosquitto_sub, you can watch activity with:

```
$ mosquitto_sub -h <mqtt_server> -p <mqtt_port> -v -t am43/#
am43/LWT Online
am43/026932f2c41d/available online
am43/026932f2c41d/position 0
am43/026932f2c41d/battery 70
am43/026932f2c41d/light 49
am43/024d45f05b2e/available online
am43/024d45f05b2e/battery 100
am43/024d45f05b2e/position 0
am43/024d45f05b2e/light 68
```

It's trivial to then control the shades similarly:

```
$ mosquitto_pub -h <mqtt_server> -p <mqtt_port> -t am43/026932f2c41d/set -m OPEN
```

You can also control all in unison:

```
$ mosquitto_pub -h <mqtt_server> -p <mqtt_port> -t am43/all/set -m CLOSE
```

## Multiple devices

If you have up to three AM43 devices connecting to the one ESP32, then it should
just work out of the box.

However if you have more devices, or just want to limit which AM43's are
controlled by a given ESP32, then you'll need to set up as follows.

### Multiple ESP32 proxy devices

This might be for a setup where you have AM43s in different parts of the house
and you'd like to ensure each ESP32 only controls specific motor(s) such
as the closest.

For each ESP32, you should edit *config.h* and add the MAC address of the
desired AM43(s) to DEVICE_ALLOWLIST. Separate each address by a comma. You
can find the addresses either in the Arduino serial console or the Blind
Engine app. Then that ESP will not connect to any other AM43.

### More than three AM43's

Arduino BLE can only connect to at most three bluetooth devices (unless
you custom compile it). If you have more devices you will need to have
additional ESP32 proxies with this code, each setup with a separate allow
list as above.

## Home Assistant configuration

### Native with ESPHome

There is now a repo which allows you to integrate AM43 support directly
into ESPHome without downloading or installing anything beyond your standard
ESPHome installation.

See [https://github.com/buxtronix/esphome-am43](https://github.com/buxtronix/esphome-am43)

### HA with auto-discovery

Ref: [Home Assistant Auto Discovery](https://www.home-assistant.io/docs/mqtt/discovery/)

If you have enabled auto-discovery in *config.h* as well as in Home Assistant,
then once your blinds are setup and the sketch has connected, you will see them
appear in the entity list under *Configuration->Entities* either by the device name
or mac address.

It's easy to add them to the Lovelace dashboard - create a new Entities Card and
find the entities matching your device names (this is the name configured in the
Blinds Engine app), or the MAC address. There will be three entities per AM43
device - one cover and two sensors for the battery and light levels.

There will also be a switch entity created, used to enable and disable the BLE
connections. This can be useful to save battery power by only establishing the
BLE connection when moving the cover. Note that it may take up to a minute to
establish the connection so automations should add a delay between enabling the
switch and sending a command.

Example automation with only enabling BLE when changing the cover:

```
id: 'Sunrise blinds'
  trigger:
    - event: sunrise
      platform: sun
  action:
    - entity_id: switch.am43_ble
      service: switch.turn_on
    - delay: 00:01
    - entity_id: cover.bedroom
      service: cover.open_cover
    - delay: 00:02
    - entity_id: switch.am43_ble
      service: switch.turn_off
```

Check the *config.h* file for other related options.

### Without auto-discovery

The MQTT topics are set to integrate natively with Home Assistant. Once both
are talking to the MQTT server, add the following configuration for each
blind:

```
cover:
  - platform: mqtt
    name: "Bedroom right"
    device_class: "shade"
    command_topic: "am43/026932f2c41d/set"
    position_topic: "am43/026932f2c41d/position"
    set_position_topic: "am43/026932f2c41d/set_position"
    availability_topic: "am43/026932f2c41d/available"
# Devices dont always report 0, open might be 0, 1 or 2.
    position_open: 2
# Devices dont always report 100, closed might be 99 or 100.
    position_closed: 99

sensor:
  - platform: mqtt
    name: "Bedroom right blind battery"
    availability_topic: "am43/026932f2c41d/available"
    state_topic: "am43/026932f2c41d/battery"
    unit_of_measurement: "%"
    device_class: battery

  - platform: mqtt
    name: "Bedroom right blind light"
    state_topic: "am43/026932f2c41d/light"
    availability_topic: "am43/026932f2c41d/available"
    unit_of_measurement: "%"
    device_class: illuminance

switch:
  - platform: mqtt
    name: "Bedroom BLE enable"
    command_topic: "am43/enable"
    state_topic: "am43/enabled"
    availability_topic: "am43/LWT"
    icon: "mdi:bluetooth"

```

### Building with [PlatformIO](https://platformio.org/)

Building with PlatformIO is simple, it will manage dependencies automatically.

#### To build with PlatformIO

1. Copy examples/MQTTBlinds/MQTTBlinds.ino and config.h to the src/ directory.

2. Edit `config.h`

3. Compile and upload via USB
```
pio run -t upload
```

4. Optionally monitor the serial port
```
pio run -t monitor
```

## TODO

 - Consider more functionality such as device configuration.
 - Allow buttons on the ESP32 for control?
 - <s>On-demand BLE connect to save AM43 device battery.</s>

## Copyright

Copyright 2020-2021, Ben Buxton. Licenced under the MIT licence, see LICENSE.
