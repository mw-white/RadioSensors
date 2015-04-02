#include <elapsedMillis.h>
#include <RFM69.h>
#include <SPI.h>
#include <SPIFlash.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "settings.h"

#define NODEID    2   // Every node must have a unique ID
#define ACK_TIME  30

/* Arduino pin assignments */
const int nGarageSensorPin = 14;
const int sGarageSensorPin = 15;
const int nGarageOpenerPin = 16;
const int sGarageOpenerPin = 17;

// State variables
int nGarageState = 0;
int nGarageLastState = -1;
int sGarageState = 0;
int sGarageLastState = -1;
int nGarageOpenerState = 0;
int sGarageOpenerState = 0;
float insideTemp = -1;
float outsideTemp = 4.5;

// How long (ms) does a "press" last to open the garage doors?
unsigned int garageCycleLength = 700;
// How often (ms) to send status updates to gateway
unsigned long updateInterval = 30000;
// How often (ms) to update temperature readings
unsigned long tempUpdateInterval = 20000;

// Set up 1 wire bus
OneWire  ds(4);  // on pin 4 (a 4.7K resistor is necessary)
DallasTemperature sensors(&ds);

// DS18B20 sensor addresses
DeviceAddress insideThermometer = { 0x28, 0xFF, 0x01, 0x67, 0x65, 0x14, 0x01, 0x8A };
DeviceAddress outsideThermometer = { 0x28, 0xff, 0x83, 0x64, 0x65, 0x14, 0x01, 0x4A };

// Timers
elapsedMillis nGarageOpenerElapsed;
elapsedMillis sGarageOpenerElapsed;
elapsedMillis updateElapsed;
elapsedMillis tempUpdate = 30000;

boolean requestACK = false;
RFM69 radio;

void setup()
{
  // Set up the serial port and radio
  Serial.begin(115200);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  radio.setHighPower();
  radio.encrypt(ENCRYPTKEY);
  
  // Set up the IO pins
  pinMode(nGarageSensorPin,INPUT);
  pinMode(sGarageSensorPin,INPUT);
  pinMode(nGarageOpenerPin,OUTPUT);
  pinMode(sGarageOpenerPin,OUTPUT);
  sensors.begin();
  // set the resolution to 10 bit (good enough?)
  sensors.setResolution(insideThermometer, 10);
}

void loop()
{
  // Read the various door sensors
  nGarageState = digitalRead(nGarageSensorPin);
  sGarageState = digitalRead(sGarageSensorPin);

  // Serial for debugging - take this out later
  if (Serial.available() > 0) {
    char input = Serial.read();
    if (input == 'q') // q - query current values
      outputStatus();        
    if (input == 'n') { // n - trigger north door
      if (nGarageOpenerState == 0) {  // ignore if we're already opening
        nGarageOpenerState = 1;
        digitalWrite(nGarageOpenerPin,HIGH);
        nGarageOpenerElapsed = 0;
      }
    }
    if (input == 's') { // s - trigger south door
      if (sGarageOpenerState == 0) {  // ignore if we're already opening
        sGarageOpenerState = 1;
        digitalWrite(sGarageOpenerPin,HIGH);
        sGarageOpenerElapsed = 0;
      }
    }
  }
  
  /*
   * See if there's data in from the radio
   */
  if (radio.receiveDone()) {
    if (radio.SENDERID == GATEWAYID) {
      for (byte i=0; i<radio.DATALEN; i++) {
        char radiocmd = (char)radio.DATA[i];
        Serial.print("Recv:");
        Serial.println(radiocmd);
        if (radiocmd == 'q') // q - query current values
          outputStatus();        
        if (radiocmd == 'n') { // n - trigger north door
          if (nGarageOpenerState == 0) {  // ignore if we're already opening
            nGarageOpenerState = 1;
            digitalWrite(nGarageOpenerPin,HIGH);
            nGarageOpenerElapsed = 0;
          }
        }
        if (radiocmd == 's') { // s - trigger south door
          if (sGarageOpenerState == 0) {  // ignore if we're already opening
            sGarageOpenerState = 1;
            digitalWrite(sGarageOpenerPin,HIGH);
            sGarageOpenerElapsed = 0;
          }
        }
      }
    }
    if (radio.ACKRequested()) {
      radio.sendACK();
    }
  }

  /* 
   * 1-wire temperature sensors take a while to read.  We don't need to
   * refresh the temperatures every time through the loop - only check
   * every 'tempUpdateInterval' ms.
   */
  if (tempUpdate > tempUpdateInterval) {
    updateTemperatures();
  }
  
  /*
   * If the garage sensors have change, send an update immediately.
   * If 'updateInterval' ms have elapsed since last update, send an
   * update anyway.
   */
  if ((nGarageState != nGarageLastState) || 
      (sGarageState != sGarageLastState) ||
      (updateElapsed > updateInterval)) {
    outputStatus();
    nGarageLastState = nGarageState;
    sGarageLastState = sGarageState;
  }

  /*
   * If one of the garage door opener relays is "pressed", check how
   * long it's been, and turn them back off when 'garageCycleLength' ms
   * has passed.
   */
  if ((nGarageOpenerState == 1) && (nGarageOpenerElapsed > garageCycleLength)) {
    nGarageOpenerState = 0;
    digitalWrite(nGarageOpenerPin,LOW);
  }
  if ((sGarageOpenerState == 1) && (sGarageOpenerElapsed > garageCycleLength)) {
    sGarageOpenerState = 0;
    digitalWrite(sGarageOpenerPin,LOW);
  }
    
  delay(10);
}

/*
 * Poll the DS18B20 sensors on the 1wire bus and update the local
 * temperature variables.
 */
void updateTemperatures() {
  tempUpdate = 0;
  sensors.requestTemperatures();
  float tempC = sensors.getTempC(insideThermometer);
  insideTemp = tempC;
  tempC = sensors.getTempC(outsideThermometer);
  outsideTemp = tempC;
}

/*
 * Send the current state to the gateway node
 */
void outputStatus() {
  char buffer[60];
  char insideStr[10];
  char outsideStr[10];

  updateElapsed = 0;
  dtostrf(insideTemp,3,1,insideStr);
  dtostrf(outsideTemp,3,1,outsideStr);
  sprintf(buffer, "%d|1=%d&2=%d&3=%s&4=%s",
      NODEID, nGarageState, sGarageState,
      insideStr, outsideStr);
  Serial.println(buffer);
  if (!radio.sendWithRetry(GATEWAYID, buffer, strlen(buffer))) {
    Serial.println("Send error");
  }
}
