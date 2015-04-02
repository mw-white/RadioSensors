# RadioSensors

This is a framework I'm using to integrate sensors around my house.  Each sensor node is based around an Arduino microcontroller and an RFM69 radio transceiever.  Specifically, I'm using the Moteino from LowPowerLab (http://lowpowerlab.com/moteino/), which is an Arduino-compatible with the RFM69 transceiver built in.

One node is the gateway.  It is connected to a computer (a Raspberry Pi in my case) via USB.  The computer runs the included node.js script, which interprets the incoming sensor messages from the nodes and sends them out via MQTT to the HA system (I use OpenHAB).  It also can subscribe to MQTT topics and send commands back to the nodes.

The Arduino sketches are based heavily on the Moteino example code.

## Included Programs

All of the Arduino sketches require a 'settings.h' file customized for your network.  The settings.h file must be the same for all sketches.  Make a copy of the settings.h-dist file called settings.h, edit it to your specifications, and copy it into each of the sketch directories.  

Here are the configurable items:

  * NETWORKID - network identifier (1-255) - all nodes must have the same network id
  * FREQUENCY - frequency your radio module uses
  * ENCRYPTKEY - a 16 character (MUST BE 16 EXACTLY!) key used to encrypt traffic
  * GATEWAYID - the node ID of the gateway node (will usually be 1)

### Libraries Needed

The following libraries must be added to your Arduino environment to be able to compile these sketches.  Note that these are dependant on the sensors you use - for example, if you don't use the temperature sensors, you won't need the 1wire and dallastemperature libraries.

RFM69:
https://github.com/LowPowerLab/RFM69

SPIFlash:
https://github.com/LowPowerLab/SPIFlash

OneWire.h
http://www.pjrc.com/teensy/arduino_libraries/OneWire.zip
(activate in arduino app)

DallasTemperature:
https://github.com/milesburton/Arduino-Temperature-Control-Library

elapsedMillis:
https://github.com/pfeerick/elapsedMillis


### Gateway

The gateway sketch is pretty simple - it listens for messages from other nodes and forwards them on to the computer via USB->serial.  Messages from nodes are in the format:

```
nodeid|sensorid=sensorval[&sensorid=sensorval[&sensorid=sensorval...]]
```

The gateway also listens for incoming messages on the serial (USB) port from the computer.  Messages from the computer are in the format:

```
nodeid|command;
```

Commands are generally 1 character values sent to the nodes to trigger actions.  It's up to the node to parse the command when it's received from the gateway.

### Garage

This is a fairly complex node - I use it in my detached garage.  Here are the sensors configured on the node (out means information going from the sensor to the gateway/computer, and in means information coming from the gateway to the node):

  * 1 - out - north garage door state (digital input connected to magnetic sensor to detect door state)
  * 2 - out - south garage door state
  * 3 - out - inside temperature (one of two DS18b20 temperature sensors)
  * 4 - out - outside temperature
  * 5 - in - north garage door opener (digital output to trigger relay to simulate pressing the opener)
  * 6 - in - south garage door opener

