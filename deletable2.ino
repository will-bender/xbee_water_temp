
//plug in my arduino chinese for this program
//this is the receiver program

#include <SoftwareSerial.h>

SoftwareSerial mySerial(2, 3); //RX, TX
//the tx and rx must be criss crossed

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

}

void loop() {
if(Serial.available() > 0) {
  String mymessage = Serial.readString();
  mySerial.print(mymessage);
  Serial.println("Receiver says: "+mymessage);
}

  if(mySerial.available() > 0) {
    String message = mySerial.readString();
    Serial.println("Received message from Arduino Pod ");
    Serial.println(message);
//    mySerial.println("OK..from receiver");
  delay (200);
  }
