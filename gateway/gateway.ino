#include <RFM69.h>
#include <SPI.h>
#include <SPIFlash.h>
#include "settings.h"

#define ACK_TIME      30
#define SERIAL_BAUD   115200

RFM69 radio;
char serialBuf[60];
int serialIdx = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(10);
  memset(serialBuf, 0, sizeof(serialBuf));
  serialIdx = 0;
  radio.initialize(FREQUENCY, GATEWAYID, NETWORKID);
  radio.setHighPower(); //only for RFM69HW!
  radio.encrypt(ENCRYPTKEY);
  Serial.println("STARTUP:Gateway-0.1");
}

void loop() {
  /*
   * Deal with any incoming data on serial (from computer).
   * Incoming 'commands' must be separated by semicolan
   */
  if (Serial.available() > 0) {
    char input = Serial.read();
    if ((input == ';') || (serialIdx == 79)) {
      processSerial();
    } else {
      serialBuf[serialIdx++] = input;
    }
  }

  /* 
   * Deal with incoming data on the radio (from other nodes)
   */
  if (radio.receiveDone()) {
    // When receive finishes, output ID and data
    Serial.print(radio.SENDERID, DEC);
    Serial.print(":");
    for (byte i = 0; i < radio.DATALEN; i++)
      Serial.print((char)radio.DATA[i]);

    if (radio.ACKRequested()) {
      byte theNodeID = radio.SENDERID;
      radio.sendACK();
    }
    Serial.println();
  }
}

/*
 * Process a serial command
 */
void processSerial() {
  if (strlen(serialBuf) == 0)
    return;

  // Parse out the destination node id and send the data to that node
  char *separator = strchr(serialBuf, '|');
  if (separator != 0) {
    *separator = 0;
    int destNode = atoi(serialBuf);
    separator++;
    if (destNode != GATEWAYID) {
      radio.sendWithRetry(destNode, separator, strlen(separator), 0);
    } else {
      Serial.println("ERR:Not sending to self");
    }
  }
  // Zero the buffer back out
  memset(serialBuf, 0, sizeof(serialBuf));
  serialIdx = 0;
}

