#include <elapsedMillis.h>
#include <RFM69.h>
#include <SPI.h>
#include <SPIFlash.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "settings.h"

#define NODEID    2
#define ACK_TIME  30

const int nGarageSensorPin = 14; /* 4pin 1/2 */
const int sGarageSensorPin = 15; /* 4pin 3/4 */
const int nGarageOpenerPin = 16; /* Yellow */
const int sGarageOpenerPin = 17; /* Green */

int nGarageState = 0;
int nGarageLastState = -1;
int sGarageState = 0;
int sGarageLastState = -1;
unsigned int garageCycleLength = 700;
int nGarageOpenerState = 0;
int sGarageOpenerState = 0;
unsigned long updateInterval = 30000;
unsigned long tempUpdateInterval = 20000;
float insideTemp = -1;
float outsideTemp = 4.5;
OneWire  ds(4);  // on pin 4 (a 4.7K resistor is necessary)
DallasTemperature sensors(&ds);

DeviceAddress insideThermometer = { 0x28, 0xFF, 0x01, 0x67, 0x65, 0x14, 0x01, 0x8A };
DeviceAddress outsideThermometer = { 0x28, 0xff, 0x83, 0x64, 0x65, 0x14, 0x01, 0x4A };


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

  // See if there's an incoming command
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
    if (input == 's') { // n - trigger north door
      if (sGarageOpenerState == 0) {  // ignore if we're already opening
        sGarageOpenerState = 1;
        digitalWrite(sGarageOpenerPin,HIGH);
        sGarageOpenerElapsed = 0;
      }
    }
  }
  
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

  if (tempUpdate > tempUpdateInterval) {
    updateTemperatures();
  }
  
  // If anything has changed, send the status (add a status update every x seconds)
  if ((nGarageState != nGarageLastState) || 
      (sGarageState != sGarageLastState) ||
      (updateElapsed > updateInterval)) {
    outputStatus();
    nGarageLastState = nGarageState;
    sGarageLastState = sGarageState;
  }

  // If we're pressing the door opener, check timeouts
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

void updateTemperatures() {
  tempUpdate = 0;
  sensors.requestTemperatures();
  float tempC = sensors.getTempC(insideThermometer);
  insideTemp = tempC;
  tempC = sensors.getTempC(outsideThermometer);
  outsideTemp = tempC;
}

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
