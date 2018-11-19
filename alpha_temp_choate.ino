/*
   FALL 2018 (GRE, PACEPRINT, ESCOLAB, CERN, theworkharder.com)
   Developed for ESCOLAB Pace University

  One arduino is on the pod (transmitter) and the other connected to a computer (receiver).
  THE TRANSMITTER:
  +takes readings every 15 minutes using CHRONOGRAPH library.
  +has two DS18B20 Waterproof sensors connected to it and communicating via one-wire (rig developed by Dr.Ganis)
  +Stores readings into EEPROM (for now) (SD CARD LATER) as two floats:
  ++One for the temperature sensor and the other for the time (millis) after the arduino starts
  +is connected to a xbee S2, and set as coordinator with PanID = 1214
  +Listens for the router xbee S2 (receiver) and if asked for sends all the data it has


  THE RECEIVER:
  +XBEE S2 setup as ROUTER (AT MODE) with PANID 1214
  +gets started up whenever I want to collect data from the TRANSMITTER
  +Contacts the Transmitter and asks for data
  +Pastes the data into the serial logger as a csv.

  REFERENCE:
  Start another instance of Arduino on mac in terminal: /Applications/Arduino.app/Contents/MacOS/Arduino ;


*/

//START INCLUDE STATEMENTS

//Libraries for connecting to the two DS18B20 Temperature Sensors
#include <OneWire.h>
#include <DallasTemperature.h>
//Library for communicating serially through XBEE S2 (AT MODE)
#include <SoftwareSerial.h>
//Library for CHRONO timekeeper that fires every 15 minutes
#include <Chrono.h>
//Library for writing to EEPROM
//    #include <EEPROM.h>
//Library for Read/Write Sd Card
#include <SPI.h>
#include <SD.h>
//END INCLUDE STATEMENTS


// START DEFINE STATEMENTS
//define pins for xbee and two temperature sensors
#define rxPin 2
#define txPin 3
#define upperTemperatureSensor 6
#define lowerTemperatureSensor 7
//array to store values of time, upperTemp_value, lowerTemp_value into values array
#define time_value 0
#define upperTemp_value 1
#define lowerTemp_value 2
//time to wait to take each measurement (900 seconds = 15 minutes)
#define measurementTime 90
//time to wait between each transmission of data to xbee in ms
#define transmitWait 500
//names of the data files and system log files
#define dataLog "dataLog.txt"
#define sysLog "sysLog.txt"

// END DEFINE STATEMENTS

//START GLOBAL VARIABLES
//remember to cross xbee! Xbee Rx -> Arduino Tx & Xbee Tx -> Arduino Rx
SoftwareSerial xbeeSerial(rxPin, txPin);
Chrono timeKeeper(Chrono::SECONDS);
//temporary array that hold the time, upper temp, and lower temp.
float values[3];
//hold the array of time, lower temp, upper temp.. should just hold 25 records (300 bytes) for three*sizeFloat values.. added 4 extra bytes just in case

File sdFile;
//END GLOBAL VARIABLES

void setup() {
  startSerial();
  removeFile(dataLog);
//  printStartingTime();
//  initializeTemperatureProbes();
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

//void printStartingTime() {
//  int seconds = millis()/1000;
//  Serial.print("Starting Time: ");
//  Serial.print(seconds);
//  Serial.println();
//}

void initializeTemperatureProbes(DallasTemperature sensor[],const int wireCount,OneWire probeArray[]) {
 DeviceAddress deviceAddress;
  Serial.println("Starting Temperature Probes");
  //start up the library on all defined bus-wires
  for (int i = 0; i < wireCount; i++) {
    ;
    sensor[i].setOneWire(&probeArray[i]);
    sensor[i].begin();
    if (sensor[i].getAddress(deviceAddress, 0)) {
      sensor[i].setResolution(deviceAddress, 12);
    }
  }
}

float getTimestamp() {
  float timestamp_seconds = millis() / 1000;
  float timestamp_minutes = timestamp_seconds / 60;
  return timestamp_minutes;
}

void didTimeKeeperFire() {
  
  if (timeKeeper.hasPassed(measurementTime)) {
    OneWire probeArray[] = {upperTemperatureSensor, lowerTemperatureSensor};
    const int wireCount = sizeof(probeArray) / sizeof(OneWire);
    DallasTemperature sensor[wireCount];
    initializeTemperatureProbes(sensor,wireCount,probeArray);
    timeKeeper.restart();
    values[time_value] = getTimestamp();
    takeTemperatures(sensor,wireCount);
    printCurrentData();
    saveCurrentData();
  }
}

void takeTemperatures(DallasTemperature sensor[],const int wireCount) {
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  Serial.print("Requesting temperatures...");
  for (int i = 0; i < wireCount; i++) {
    sensor[i].requestTemperatures();
  }
  Serial.println("DONE");


  for (int i = 0; i < wireCount; i++) {
    float temperature = sensor[i].getTempCByIndex(0);
    //    Serial.print("Temperature for the sensor ");
    //    Serial.print(i);
    //    Serial.print(" is ");
    //    Serial.println(temperature);
    //this is dependent on having two sensors
    int adjustmentForValuesArray = i + 1;
    values[adjustmentForValuesArray] = temperature;
  }
}

void printCurrentData() {
  Serial.print("Current Time is: ");
  Serial.print(values[0]);
  Serial.print(" / Upper Sensor / ");
  Serial.print(values[1]);
  Serial.print(" / Lower Sensor / ");
  Serial.print(values[2]);
  Serial.println();
  Serial.println();
}
//this method returns the current data for sending to the receiver
String returnAppendedCurrentData() {
  String appendedString = "";
  appendedString = "Current Time is: " + String(values[0], 2) + " / Upper Sensor / " + String(values[1], 2) + " / Lower Sensor / " + String(values[2], 2);
  return appendedString;
}

//this method returns a CSV file friendly version of the three values (timestamp, upper temp, lower temp)
String returnAppendedData() {
  String appendedString = "";
  appendedString = String(values[0], 2) + "," + String(values[1], 2) + "," + String(values[2], 2);
  return appendedString;
}

void checkForReceiver() {
  if (xbeeSerial.available() > 0) {
    String message = " ";
    message = xbeeSerial.readString();
    message.trim();
    Serial.println("Receiver says: " + message);
    if (message.equals("send")) {
      xbeeSerial.println("Transmitter says: OK Sending");
      delay(transmitWait);
      //      message = returnAppendedCurrentData();
      readFromDataTransmit(dataLog);
    }
    else if (message.equals("current")) {
      //      Serial.println("Receiver says: Received message: "+ message);
      delay(transmitWait);
//      xbeeSerial.print("Transmitter Says: Current Data is " + returnAppendedCurrentData());
      xbeeSerial.print("Transmitter Says: Current Data is ");
      xbeeSerial.print(values[0]);
      xbeeSerial.print(",");
      xbeeSerial.print(values[1]);
      xbeeSerial.print(",");
      xbeeSerial.print(values[2]);
      xbeeSerial.println();
      delay(transmitWait);
    }
    else {
      Serial.println(message);
      xbeeSerial.println("Transmitter says: Invalid message ");
      delay(transmitWait);
      xbeeSerial.println("Transmitter says: I received: ");
      delay(transmitWait);
      xbeeSerial.println(message);
    }
    xbeeSerial.println("Transmitter Says: Done Transmitting");
  }
}

void readFromDataTransmit(String fileToRead) {
  char byteRead = 'k';
  sdFile = SD.open(fileToRead);

  Serial.print("Reading from ");
  Serial.println(sdFile.name());

  while (sdFile.available()) {
    byteRead = sdFile.read();
    xbeeSerial.write(byteRead);
    if (byteRead == '\n' || byteRead == '\r') {
//      Serial.println("Breaks");
      delay(transmitWait);
    }
  }
  sdFile.close();

}

void saveCurrentData() {

  writeToFile(dataLog, returnAppendedData());

  //  int mailbox = 0;
  //  while(storedValues[mailbox] > 0.00){
  //    Serial.println(storedValues[mailbox]);
  //    if(((mailbox +1) % 3 ) == 0){
  //      Serial.println();
  //    }
  //    mailbox++;
  //  }
  //  storedValues[mailbox++] = values[0];
  //  Serial.println(values[0]);
  //  storedValues[mailbox++] = values[1];
  //  Serial.println(values[1]) ;
  //  storedValues[mailbox++] = values[2];
  //  Serial.println(values[2]);

}


bool SDinit() {
  if (!SD.begin(4)) {
    Serial.println("initialization failed!");
    return false;
  }
  else {
    Serial.println("Initialization done");
    //  removeFile(dataLog);
    return true;
  }
}

void removeFile(String fileToRemove) {
  SD.remove(fileToRemove);
  sdFile.close();
}

void readFromFile(String fileToRead) {
  char byteRead = 'k';
  sdFile = SD.open(fileToRead);

  Serial.print("Reading from ");
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
  Serial.println("Writing to files");
  sdFile.println(dataToWrite);
  sdFile.close();
}

void createFiles(bool createFile) {
  if (!createFile) {
    //if there is no text file on the sd card named data.txt then this is a fresh sd card
    Serial.println("Creating file called data.txt");
    sdFile = SD.open("datalog.txt", FILE_WRITE);
    sdFile.println("TimeStamp,Upper Temperature,Lower Temperature");
    sdFile.close();
  }
}

bool doFilesExist(bool SDInitialized) {

  if (!SD.exists("dataLog.txt") || SDInitialized == false) {
    Serial.println("dataLog.txt does not exist");
    //there are no files on the sd card this must mean that the sd card is fresh
    return false;
  }
  else {
    Serial.println("dataLog.txt does exist");
    //there is a file named data.txt
    return true;
  }

}


//END TRANSMITTER METHODS




//START RECEIVER METHODS

//END RECEIVER METHODS
