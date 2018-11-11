/* 
 * FALL 2018 (GRE, PACEPRINT, ESCOLAB, CERN, theworkharder.com)
 * Developed for ESCOLAB Pace University
 * 
 *One arduino is on the pod (transmitter) and the other connected to a computer (receiver).  
 *THE TRANSMITTER:
 *+takes readings every 15 minutes using CHRONOGRAPH library.
 *+has two DS18B20 Waterproof sensors connected to it and communicating via one-wire (rig developed by Dr.Ganis)
 *+Stores readings into EEPROM (for now) (SD CARD LATER) as two floats: 
 *++One for the temperature sensor and the other for the time (millis) after the arduino starts
 *+is connected to a xbee S2, and set as coordinator with PanID = 1214
 *+Listens for the router xbee S2 (receiver) and if asked for sends all the data it has
 *
 *
 *THE RECEIVER:
 *+XBEE S2 setup as ROUTER (AT MODE) with PANID 1214
 *+gets started up whenever I want to collect data from the TRANSMITTER
 *+Contacts the Transmitter and asks for data
 *+Pastes the data into the serial logger as a csv.
 *
 *REFERENCE:
 *
 * 
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
    #include <EEPROM.h>

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
  #define measurementTime 15
//time to wait between each transmission of data to xbee in ms
  #define transmitWait 200
// END DEFINE STATEMENTS

//START GLOBAL VARIABLES
  //remember to cross xbee! Xbee Rx -> Arduino Tx & Xbee Tx -> Arduino Rx
  SoftwareSerial xbeeSerial(rxPin,txPin);
  Chrono timeKeeper(Chrono::SECONDS);
  //temporary array that hold the time, upper temp, and lower temp.
  float values[3];
  //hold the array of time, lower temp, upper temp.. should just hold 25 records (300 bytes) for three*sizeFloat values.. added 4 extra bytes just in case
  float storedValues[180];
  OneWire probeArray[] = {upperTemperatureSensor,lowerTemperatureSensor};
  const int wireCount = sizeof(probeArray)/sizeof(OneWire);
  DallasTemperature sensor[wireCount];
  DeviceAddress deviceAddress;
//END GLOBAL VARIABLES

void setup() {
startSerial();
printStartingTime();
initializeTemperatureProbes();
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

void printStartingTime() {
  int seconds = millis()/1000;
  Serial.print("Starting Time: ");
  Serial.print(seconds);
  Serial.println();
}

void initializeTemperatureProbes(){
  Serial.println("Starting Temperature Probes");  
  //start up the library on all defined bus-wires
  for (int i = 0; i < wireCount; i++) {;
    sensor[i].setOneWire(&probeArray[i]);
    sensor[i].begin();
    if (sensor[i].getAddress(deviceAddress, 0)){
    sensor[i].setResolution(deviceAddress, 12);
    }
  }
}

float getTimestamp(){
  float timestamp_seconds = millis()/1000;
  float timestamp_minutes = timestamp_seconds/60;
  return timestamp_minutes;
}

void didTimeKeeperFire(){
  if(timeKeeper.hasPassed(measurementTime)){
    timeKeeper.restart();
    values[time_value] = getTimestamp();
    takeTemperatures();
    printCurrentData();
    saveCurrentData();
  }
}

void takeTemperatures(){
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
    int adjustmentForValuesArray = i+1;
    values[adjustmentForValuesArray] = temperature;
  }
}

void printCurrentData(){
Serial.print("Current Time is: ");
Serial.print(values[0]);
Serial.print(" / Upper Sensor / ");
Serial.print(values[1]);
Serial.print(" / Lower Sensor / ");
Serial.print(values[2]);
Serial.println();
Serial.println();
}

String returnAppendedCurrentData() {
  String appendedString = "";
  appendedString = "Current Time is: " + String(values[0],2) + " / Upper Sensor / " + String(values[1],2) + " / Lower Sensor / " + String(values[2],2);
  return appendedString;
}

void checkForReceiver(){
  if (xbeeSerial.available() > 0){
    String message = " ";
    message = xbeeSerial.readString();
    message.trim();
    Serial.println("Receiver says: " + message);
    if(message.equals("send")){
      xbeeSerial.println("Transmitter says: OK Sending");
      delay(transmitWait);
//      message = returnAppendedCurrentData();
      transmitData();
    }
    else if(message.equals("current")){
      Serial.println("Receiver says: "+ message);
      delay(transmitWait);
      xbeeSerial.println("Transmitter Says: Current Data is " + returnAppendedCurrentData());
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

void transmitData(){
  
  delay(transmitWait);
}

void saveCurrentData(){
  int mailbox = 0;
  while(storedValues[mailbox] > 0.00){
    Serial.println(storedValues[mailbox]);
    if(((mailbox +1) % 3 ) == 0){
      Serial.println();
    }
    mailbox++;
  }
  storedValues[mailbox++] = values[0];
  Serial.println(values[0]);
  storedValues[mailbox++] = values[1];
  Serial.println(values[1]) ;
  storedValues[mailbox++] = values[2];
  Serial.println(values[2]);
  
}


//END TRANSMITTER METHODS




//START RECEIVER METHODS

//END RECEIVER METHODS
