/*
   FALL 2018 - SPRING 2019 (GRE, PACEPRINT, ESCOLAB, CERN, theworkharder.com)
   Developed for ESCOLAB Pace University

This code is for the transmitter!

  One arduino is the (transmitter) and the other connected to a computer (receiver).
  THE TRANSMITTER:
  +takes readings every 15 minutes using DS1307 RTC which wakes the Arduino up every second using the square wave output.
  +has two DS18B20 Waterproof sensors connected to it and communicating via one-wire (rig developed by Dr.Ganis)
  +Stores readings and timestamp into an SD card as three floats:
  ++Two floats for the temperature sensors and the third for the timestamp in the format (YYYY.MM.DD.HH.MM.SS) 
  +is connected to an xbee S2, and set as coordinator with PanID = 1214
  +Listens for the router xbee S2 (receiver) and if asked for, either sends all the data it has or the current data


  THE RECEIVER:
  +XBEE S2 setup as ROUTER (AT MODE) with PANID 1214
  +gets started up whenever I want to collect data from the TRANSMITTER
  +Contacts the Transmitter and can ask for either all the data or the most current data
  +Pastes the data into the serial logger as a csv with headers for easy import into excel or parsing to DB

  REFERENCE:
  Start another instance of Arduino on mac in terminal: /Applications/Arduino.app/Contents/MacOS/Arduino ;

*/

//START INCLUDE STATEMENTS

//Libraries for connecting to the two DS18B20 Temperature Sensors
#include <OneWire.h>
#include <DallasTemperature.h>
//Library for communicating serially through XBEE S2 (AT MODE)
#include <SoftwareSerial.h>
//Library for writing to EEPROM
//    #include <EEPROM.h>
//Library for Read/Write Sd Card
#include <SPI.h>
#include <SD.h>
//END INCLUDE STATEMENTS

//APPENDED TIME STATEMENTS
#include <Wire.h>
#include "uRTCLib.h"
//APPENDED TIME STATEMENTS

// START DEFINE STATEMENTS
//define pins for xbee and two temperature sensors
#define rxPin 3
#define txPin 2
#define upperTemperatureSensor 8
#define lowerTemperatureSensor 9
//array to store values of time, upperTemp_value, lowerTemp_value into values array
#define time_value 0
#define upperTemp_value 1
#define lowerTemp_value 2
//time to wait to take each measurement (900 seconds = 15 minutes)
#define measurementTime 15
//time to wait between each transmission of data to xbee in ms
#define transmitWait 500
//names of the data files and system log files
#define datalog "datalog.txt"
#define sysLog "sysLog.txt"

// END DEFINE STATEMENTS

//START GLOBAL VARIABLES
//remember to cross xbee! Xbee Rx -> Arduino Tx & Xbee Tx -> Arduino Rx
SoftwareSerial xbeeSerial(rxPin, txPin);
//temporary array that hold the time, upper temp, and lower temp.
float values[2];
String time = "";
const byte quartlyTimes[4] = {0, 15, 30, 45};
//initialize the last time so we know that it's our first run
byte lastTimeFired = 1;
byte nextTimeFired = 60;

File sdFile;
//END GLOBAL VARIABLES

void setup() {
  startSerial();
  //  removeFile(datalog);
  createFiles(doFilesExist(SDinit()));

}

void loop() {
  checkForReceiver();
  didTimeKeeperFire();
}

//START GENERIC METHODS

void startSerial() {
  Serial.begin(9600);
  xbeeSerial.begin(9600);
  delay(100);
}

//END GENERIC METHODS

//START TRANSMITTER METHODS



void initializeTemperatureProbes(DallasTemperature sensor[], const int wireCount, OneWire probeArray[]) {
  DeviceAddress deviceAddress;
  Serial.println(F("Starting Temperature Probes"));
  //start up the library on all defined bus-wires
  for (int i = 0; i < wireCount; i++) {
    sensor[i].setOneWire(&probeArray[i]);
    sensor[i].begin();
    if (sensor[i].getAddress(deviceAddress, 0)) {
      sensor[i].setResolution(deviceAddress, 12);
    }
  }
}


void didTimeKeeperFire() {
  if (checkTime()) {
    OneWire probeArray[] = {upperTemperatureSensor, lowerTemperatureSensor};
    const int wireCount = sizeof(probeArray) / sizeof(OneWire);
    DallasTemperature sensor[wireCount];
    initializeTemperatureProbes(sensor, wireCount, probeArray);
    time = getTimestamp();
    takeTemperatures(sensor, wireCount);
    printCurrentData();
    saveCurrentData();
  }
}

void takeTemperatures(DallasTemperature sensor[], const int wireCount) {
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  Serial.print(F("Requesting temperatures..."));
  for (int i = 0; i < wireCount; i++) {
    sensor[i].requestTemperatures();
  }
  Serial.println(F("DONE"));


  for (int i = 0; i < wireCount; i++) {
    float temperature = sensor[i].getTempCByIndex(0);
    values[i] = temperature;
  }
}

void printCurrentData() {
  Serial.print(F("Current Time is: "));
  Serial.print(time);
  Serial.print(F(" / Upper Sensor / "));
  Serial.print(values[0]);
  Serial.print(F(" / Lower Sensor / "));
  Serial.print(values[1]);
  Serial.println();
}
//this method returns the current data for sending to the receiver
String returnAppendedCurrentData() {
  String appendedString = "";
  appendedString = "F(Current Time is: )" + time + "F( / Upper Sensor / )" + String(values[0], 2) + "F( / Lower Sensor / " + String(values[1], 2);
  return appendedString;
}

//this method returns a CSV file friendly version of the three values (timestamp, upper temp, lower temp)
String returnAppendedData() {
  String appendedString = "";
  appendedString = time + "," + String(values[0], 2) + "," + String(values[1], 2);
  return appendedString;
}

void checkForReceiver() {
  if (xbeeSerial.available() > 0) {
    String message = " ";
    message = xbeeSerial.readString();
    message.trim();
    Serial.print(F("Receiver says: ")); Serial.println(message);
    if (message.equals(F("send"))) {
      xbeeSerial.println(F("Transmitter says: OK Sending"));
      delay(transmitWait);
      //      message = returnAppendedCurrentData();
      readFromDataTransmit(datalog);
    }
    else if (message.equals(F("current"))) {
      delay(transmitWait);
      xbeeSerial.print(F("Transmitter Says: Current Data is "));
      xbeeSerial.print(time);
      xbeeSerial.print(F(","));
      xbeeSerial.print(values[0]);
      xbeeSerial.print(F(","));
      xbeeSerial.print(values[1]);
      xbeeSerial.println();
      delay(transmitWait);
    }
    else {
      Serial.println(message);
      xbeeSerial.println(F("Transmitter says: Invalid message "));
      delay(transmitWait);
      xbeeSerial.println(F("Transmitter says: I received: "));
      delay(transmitWait);
      xbeeSerial.println(message);
    }
    xbeeSerial.println(F("Transmitter Says: Done Transmitting"));
  }
}

void readFromDataTransmit(String fileToRead) {
  char byteRead = 'k';
  sdFile = SD.open(fileToRead);

  Serial.print(F("Reading from "));
  Serial.println(sdFile.name());

  while (sdFile.available()) {
    byteRead = sdFile.read();
    xbeeSerial.write(byteRead);
    if (byteRead == '\n' || byteRead == '\r') {
      delay(transmitWait);
    }
  }
  sdFile.close();

}

void saveCurrentData() {

  writeToFile(datalog, returnAppendedData());
}


bool SDinit() {
  if (!SD.begin(4)) {
    Serial.println(F("Initialization failed!"));
    return false;
  }
  else {
    Serial.println(F("Initialization done \n"));
    //  removeFile(datalog);
    return true;
  }
}

void removeFile(String fileToRemove) {
  SDinit();
  SD.remove(fileToRemove);
  sdFile.close();
}

void readFromFile(String fileToRead) {
  char byteRead = 'k';
  sdFile = SD.open(fileToRead);

  Serial.print(F("Reading from "));
  Serial.println(sdFile.name());
  while (sdFile.available()) {
    byteRead = sdFile.read();
    Serial.write(byteRead);
    if (byteRead == '\n' || byteRead == '\r') {
      delay(50);
    }

  }
  sdFile.close();
}

void writeToFile(String fileToWrite, String dataToWrite) {
  sdFile = SD.open(fileToWrite, FILE_WRITE);
  Serial.println(F("Writing to files"));
  sdFile.println(dataToWrite);
  sdFile.close();
}

void createFiles(bool createFile) {
  if (!createFile) {
    //if there is no text file on the sd card named data.txt then this is a fresh sd card
    Serial.println(F("Creating file called datalog.txt"));
    sdFile = SD.open(F("datalog.txt"), FILE_WRITE);
    sdFile.println(F("TimeStamp,Upper Temperature,Lower Temperature"));
    sdFile.close();
  }
}

bool doFilesExist(bool SDInitialized) {

  if (!SD.exists("datalog.txt") || SDInitialized == false) {
    Serial.println(F("datalog.txt does not exist"));
    //there are no files on the sd card this must mean that the sd card is fresh
    return false;
  }
  else {
    Serial.println(F("datalog.txt does exist"));
    //there is a file named data.txt
    return true;
  }

}

String getTimestamp() {
  uRTCLib rtc(0x68);

  Wire.begin();
  rtc.refresh();
  Wire.end();
  return (String(rtc.year()) + "." + String(rtc.month()) + "." + String(rtc.day()) + "." + String(rtc.hour()) + "." + String(rtc.minute()) + "." + String(rtc.second()));
}

bool checkTime() {
  //This is the replacement method for the chronographe function I had
  //returns boolean of whether to fire or not
  uRTCLib rtc(0x68);
  Wire.begin();
  rtc.refresh();
  byte minute = rtc.minute();
  Wire.end();
  
  //if the minute equals any quarter of the hour return true, else return false
  if (minute != lastTimeFired && (minute == quartlyTimes[0] || minute == quartlyTimes[1] || minute == quartlyTimes[2] || minute == quartlyTimes[3]) ) {
    lastTimeFired = minute;
    nextTimeFired  = minute + 15; // plus the timing interval
    return true;
  }
  //check the edge case that we accidentally went over the time interval and need to take the measurement asap. This could happen from data transmission taking a long time
  //for example. lastTime = 0, nextTime = 15, minute = 16
  else if (minute > nextTimeFired){
    lastTimeFired = minute;
    nextTimeFired  = minute + 15;
    return true;
  }
  else {
    return false;
  }
}

//START POWER METHODS




//END POWER METHODS












//END TRANSMITTER METHODS
