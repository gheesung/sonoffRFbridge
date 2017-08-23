/*
  esp01_sonoff_rfbridge_uno.ino - RF Bridge for Tasmota and Arduino

  Copyright (C) 2017  Ghee Sung

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Acknowledgement to 
 *  Theo Arends for Tasmota
 *  Suat Özgür for RCSwitch
 *  SlashDevin for NeoSWSerial
 */
#include <RCSwitch.h>
#include <NeoSWSerial.h>
#define INPUT_BUFFER_SIZE      250          // Max number of characters in (serial) command buffer
#define rxPin 4
#define txPin 5
#define LED_BUILTIN1 7

unsigned int  byteRecv =0;
byte startText = 0;
byte endText = 0;
int firstbyte = 0;
int stackcounter = 0;
unsigned long recvData = 0;

RCSwitch mySwitch = RCSwitch(); 

NeoSWSerial mySerial(rxPin, txPin);

// array to store the message
char svalue[90];
char stack[90];

void setup() {
  Serial.begin(19200);
  Serial.println("DIY Sonoff RF Bridge");
  mySerial.begin(19200);
  mySwitch.enableReceive(0);  
  mySwitch.enableTransmit(3);  
  pinMode(LED_BUILTIN1, OUTPUT);
}

void loop(){
  // this loop is for Tasmota to talk to RF bridge
  espArduinoBridge();
  
  // This loop is for the RF receiver
  // As long as the RF receiver cna decode the data, it will be send to Tasmota.
  recvRFSwitch();
  if (0 != recvData){
    processRecvData();
    recvData = 0;
  }
}

void espArduinoBridge(){
  if (mySerial.available()){
    
    Serial.println("received triggered");
    while(mySerial.available()){

      svalue[byteRecv] = (byte)mySerial.read();;
      Serial.print((byte) svalue[byteRecv]);
      Serial.print(" ");
      byteRecv++;

      // prevent buffer overflow. 
      // when ESP boot, it sends the log thru' UART
      if (byteRecv>= 90) { 
        byteRecv = 0; 
        break; 
      }
    } 

    svalue[byteRecv] = '\0';
    mySerial.flush();
    Serial.print(" receive byte:");
    Serial.println(byteRecv);
    if (byteRecv){
      
      // start of text message 
      if ((byte)svalue[0] == 170){
        startText=1;
        stack[0] = svalue[0];
      }
      // the rest
      if ( startText){
        for(int i=0;i<byteRecv;i++){
          stack[stackcounter] = svalue[i];
          stackcounter++;

          // end byte (85)
          if ((byte)svalue[i] == 85){  endText =1;  stack[stackcounter]=0;  }
        }
      }
      // detect end text
      if(endText){
        // dump out the stack
        dumpStack();
        stackcounter = 0;
        endText = 0;

        // end of command character detected
        Serial.println("Processing Commands");
        processCommand();
      }
      // 
      byteRecv =0;   
    }
  }
}

// process the command from Tasmota
void processCommand(){
  // Learn Mode
  if (((byte)stack[0] == 170) && ((byte)stack[1] == 161) && ((byte)stack[2] == 85)){
    Serial.println("Learn Mode ");
    processLearnMode();
  }

  // normal mode
  if (((byte)stack[0] == 170) && ((byte)stack[1] == 165) && ((byte)stack[2] == 32)){
    Serial.println("Command --> ");
    sendRFSwitch();
    recvData=0;
    stack[0] = 0;
  }

}

// Learn mode
void processLearnMode(){
    Serial.println("Command --> Start Learning");
    delay(1000);
    recvRFSwitch();
    int i = 0;
    while (recvData == 0){
      recvRFSwitch();

      if (recvData != 0){ break;}
      // no signal received, send learn fail
      if (i > 200) {
        Serial.println("Learned code fail");
        mySerial.write(0xAA);
        mySerial.write(0xA2);
        mySerial.write(0x55);
        digitalWrite(LED_BUILTIN1,LOW);
        return;
      }
      //flash the LED for 20s when learn mode is enabled.
      digitalWrite(LED_BUILTIN1,HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN1,LOW);
      i++;
    }

    // some preamble from SONOFF bridge
    // may make use of this in future.
    mySerial.write(0xAA);
    mySerial.write(0xA3);
    mySerial.write(0x20);
    mySerial.write(0xF8);
    mySerial.write(0x01);
    mySerial.write(0x18);
    mySerial.write(0x03);
    mySerial.write(0x3E);

    // actual data
    mySerial.write((byte)((unsigned long)recvData>>16));
    mySerial.write((byte)((unsigned long)recvData>>8));
    mySerial.write((byte)recvData);

    mySerial.write(0x55);
    stack[0] = 0;
    Serial.println("Command --> Learned code sent");
    recvData =0;
    delay(1000);

}

// process recv data from the RF
void processRecvData(){

  Serial.print("Sending Received Data to Tasmota");
  Serial.println((unsigned long)recvData);
  // some preamble from SONOFF bridge
  mySerial.write(0xAA);
  mySerial.write(0xA4);
  mySerial.write(0x20);
  mySerial.write(0xEE);
  mySerial.write(0x01);
  mySerial.write(0x18);
  mySerial.write(0x03);
  mySerial.write(0x3E);

  // actual data
  mySerial.write((byte)((unsigned long)recvData>>16));
  mySerial.write((byte)((unsigned long)recvData>>8));
  mySerial.write((byte)recvData);
  
  // postamble
  mySerial.write(0x55);
  recvData=0;
  stack[0] = 0;
}

// receive the data from the remote RF switch
void recvRFSwitch(){
  if (mySwitch.available()) {
    Serial.println( mySwitch.getReceivedProtocol() );

    int value = mySwitch.getReceivedValue();
    
    if (value == 0) {
      Serial.println("Unknown encoding");
    } else {
      Serial.print("Received ");
      recvData = mySwitch.getReceivedValue();
      Serial.print( recvData );
      Serial.print(" / ");
      Serial.print( mySwitch.getReceivedBitlength() );
      Serial.print("bit ");
      Serial.print( mySwitch.getReceivedDelay());
      Serial.print(" delay ");
      //Serial.print(" Raw:");
      //Serial.print( mySwitch.getReceivedRawdata());
      //Serial.print(" Protocol: ");
      //Serial.println( mySwitch.getReceivedProtocol() );
    }
    mySwitch.resetAvailable();
  }
}

// Send the data out (RF signal out).
void sendRFSwitch(){

  Serial.print("Command header matched :" );

  unsigned long sendData = 0UL;
 
  sendData = (byte)stack[8];
  sendData = sendData<<16 ;
  sendData = sendData | ((byte)stack[9]<<8);
  sendData = sendData | (byte)stack[10];
  Serial.println( sendData );
  mySwitch.send(sendData, 24);
  stack[0] = 0;
  recvData = 0;
}


// for testing
void setuptest(){
  Serial.begin(19200);
  Serial.println("Init");
  //attachInterrupt(digitalPinToInterrupt(rxPin), handleRxChar, CHANGE);
  mySerial.begin(19200);
}

void looptest(){
  if (mySerial.available()) {
    Serial.write(mySerial.read());
  }
  if (Serial.available()) {
    mySerial.write(Serial.read());
  }
  
  //recvRFSwitch();
  mySwitch.send(8991298, 24);
}

void simulateSend(){
     Serial.println("Command --> Simulated signal");
    //A4 20 EE 01 18 03 3E 2E 1A 22 55

      /*mySerial.write(0xAA);
      mySerial.write(0xA4);
      mySerial.write(0x20);
      mySerial.write(0xEE);
      mySerial.write(0x01);
      mySerial.write(0x18);
      mySerial.write(0x03);
      mySerial.write(0x3E);
      mySerial.write(0x2E);
      mySerial.write(0x1A);
      mySerial.write(0x22);
      mySerial.write(0x55);*/

    mySerial.write(0xAA);
    mySerial.write(0xA3);
    mySerial.write(0x20);
    mySerial.write(0xF8);
    mySerial.write(0x01);
    mySerial.write(0x18);
    mySerial.write(0x03);
    mySerial.write(0x3E);
    mySerial.write(0x2E);
    mySerial.write(0x1A);
    mySerial.write(0x22);
    mySerial.write(0x55);      
    delay(5000);
  
}
void dumpStack(){
  // dump out the stack
  int i = 0;
  Serial.println("Stack Dump");
  while (stack[i] != 0){
    Serial.print((byte) stack[i]);
    Serial.print(" ");
    i++;
  }
  Serial.println(" ");
}

