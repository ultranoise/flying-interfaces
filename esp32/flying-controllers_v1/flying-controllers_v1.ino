/////////////////////////////////////////////////////////////////////////
/// ESP32 Tangible Core  . OSC Client Generic Solution                 //
///                                                                    //
/// This sends OSC data to a host                                      //
/// configured as access point or connected to a wifi network          //
//                                                                     //
//  It uses pin 15 (LOW) to hardreset the board in access point mode   //
//  so you need to connect pin 15 to HIGH to make it work.             //
/// enrique.tomas@ufg.at may 2019                                      //
/////////////////////////////////////////////////////////////////////////

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <WiFiAP.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include "esp_wifi.h"

#include "EEPROM.h"
#include <Wire.h>

#include "lwip/inet.h"
#include "lwip/dns.h"

#include <DNSServer.h>
#include "RemoteDebug.h"        //https://github.com/JoaoLopesF/RemoteDebug

#include <Arduino.h>
#include <Ewma.h>

#include <Update.h> //to update firmware
#include <Wire.h>
#include <math.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define SEALEVELPRESSURE_HPA (1013.25)

///////////////////////
//global variables
///////////////////////

//int LED_BUILTIN = 2;    //LED BUILT_IN pin in DOIT DEV KIT is 2

// I2C
Adafruit_LIS3DH lis = Adafruit_LIS3DH();
sensors_event_t event;
Adafruit_BME280 bme; // I2C

//Using Wifi UDP for OSC and Server to configure the board with a browser
WiFiUDP udp;
WebServer server(80);

// WiFi network name and password:
String network;
String password;

// Name of your default access point.
const char *APssid = "kike-AP5";

//first IP address to send UDP data to:
// either use the ip address of the server or a network broadcast address
IPAddress udpAddress(192, 168, 0, 2);  //just a dummy, it can be configured via browser
int udpPort = 54322; //33333;                   //just a dummy, it can be configured via browser


//A few flags to know the status of our connection
boolean connected = false;     //wifi connection
bool accesspoint = true;       //acccess point mode
boolean APconnected = false;   //access point connected
bool registered = false;       //wifi handler registration
bool shouldReboot = false;     //flag to use from web update to reboot the ESP

// Set your Static IP address (dummy values initialization)
IPAddress staIP(192,168,0,129);         //Board static IP
IPAddress staGateway(192,168,0,1);      //Gateway IP
IPAddress staSubnet(255,255,255,0);     //Subnet range
IPAddress primaryDNS(192, 168, 0, 1);   //optional
IPAddress secondaryDNS(8, 8, 4, 4);     //optional

IPAddress clientsAddress[10];           //ten clients can be connected in access point mode
int numClients = 0;


//for mdns discovery
bool hostSelected = false;    //mDNS status flag
String hostSelectedName;      //mDNS host found
int nServices = 0;            //mDNS services count

//OSC message types (create yours)
OSCMessage msg_test("/esp/test");
OSCMessage msg_pot1("/accelx");
OSCMessage msg_pot2("/accely");
OSCMessage msg_pot3("/accelz");
OSCMessage msg_temp("/temp");
OSCMessage msg_press("/pressure");
OSCMessage msg_alt("/altitude");
OSCMessage msg_hum("/humidity");


//overall delay added to loop() to avoid overflow of OSC messages
float delayProgram = 0;

//EEPROM global vars
#define EEPROM_SIZE 256
int addr = 0; // the current address in the EEPROM (i.e. which byte we're going to write to next)

//Sensor vars
const int numReadings = 20;
float input0[numReadings];    //arrays for iterated readings and posterior smooth
float input1[numReadings]; 
float input2[numReadings]; 

float inputVal =0;
float inputVal2 =0;
float inputVal3 =0;
float prev_inputVal = 0;
float prev_inputVal2 = 0;
float prev_inputVal3 = 0;

float temp, pressure, altitude, humidity;
float prev_temp, prev_pressure, prev_altitude, prev_humidity;

//timming control
unsigned long t_int;
unsigned long t_micros_int;
unsigned long previous_int;
unsigned long t_osc;
unsigned long previous_osc;

bool ok = false; //bool to check stuff and blink the led

////// REMOTE DEBUG Defines
String HOST_NAME = "kike2-oficina-483"; // Host name (please change it)

RemoteDebug Debug;


//smoothing
Ewma adcFilter1(0.5);   // Less smoothing - faster to detect changes, but more prone to noise
Ewma adcFilter2(0.5);  // More smoothing - less prone to noise, but slower to detect changes
Ewma adcFilter3(0.5);
//Ewma adcFilter4(0.5);
//Ewma adcFilter5(0.5);
Ewma adcFilter6(0.1);  //altitude
//Ewma adcFilter7(0.5);

int ledState = LOW;
unsigned long previousMillis2;

extern String updateform;
extern String loginIndex;
extern String style;


///////////////////////////
///An interrupt function
///////////////////////////

//void IRAM_ATTR isr() {
  //do something
  //t_int = millis();
//}

//////////////////////
// setup
//////////////////////
void setup(){
  // Initialize hardware serial:
  Serial.begin(115200);
  Serial.println();
  Serial.println("/////////////////////////////");
  Serial.println("Generic ESP32 solution");
  Serial.println("Tangible Music Lab 2019 (enrique.tomas@ufg.at)");
  Serial.println("/////////////////////////////");
    
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(15, INPUT);             //hardreset pin

  //interrupts example on a particular pin
  //pinMode(23, INPUT_PULLUP);
  //attachInterrupt(23, isr, RISING);
  
  ///eeprom init
  EEPROM.begin(EEPROM_SIZE);
  
  // First check hardreset pin
  // A hardreset pin at pin 15 will save us when a network configuration fails!!
  // It resets the board to access point, stores this mode in the eeprom and restarts
  bool j = false;
  if(j) { //digitalRead(15) == LOW){
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println();
    Serial.println("RESET pin 15 is low!!");
    
    resetBoard();   // reset board, fix Access Point mode and save to EEPROM

    Serial.println("Rebooting in 2 secs...");
    delay(2000);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(10000);
    ESP.restart();
    
  } else {
    Serial.println();
    Serial.println("RESET pin 15 is high!!");
  }
  //lets read the settings anyway
  confSettings();  //read configuration from eeprom in both AP (access point) or STA (Station network) modes 


  if (! lis.begin(0x18)) {   // change this to 0x19 for alternative i2c address
    Serial.println("Couldnt start");
    while (1);
  }
  Serial.println("LIS3DH found!");
  
  lis.setRange(LIS3DH_RANGE_4_G);   // 2, 4, 8 or 16 G!
  
  Serial.print("Range = "); Serial.print(2 << lis.getRange());  
  Serial.println("G");

  Serial.println(F("BME280 test"));

    unsigned status;
    
    // default settings
    status = bme.begin(0x76);  
    // You can also pass in a Wire library object like &Wire2
    //status = bme.begin(0x76, &Wire2)
    if (!status) {
        Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
        Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
        Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
        Serial.print("        ID of 0x60 represents a BME 280.\n");
        Serial.print("        ID of 0x61 represents a BME 680.\n");
        while (1) delay(10);
    }
    
    Serial.println("-- Default Test --");
  
  ///Create WIFI for AP or STA modes
  if(accesspoint == false){
    // No access point, connect to STA
    Serial.println();
    Serial.print("Connecting to WIFI... ");
    Serial.println(network);
    
    //Connect to the WiFi network
    connectToWiFi(network, password);
    
  } else { 
    Serial.println();
    Serial.println("Configuring access point...");

    createAccessPoint();
  }



  // A simple web server will allow us configuring a bit the board 
  // start web server
  server.on("/", handleRoot);       //handleRoot is a function dealing with the actions on the server via browser
  server.onNotFound(handleNotFound);// a default function dealing with errors during browsing 
  
  //additional pages for uploading code by users
  
  server.on("/updatecode", HTTP_GET, []() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", updateform);
  });
  
  //server.on("/serverIndex", handleUpdateCode);
  
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        shouldReboot = true;
      } else {
        Update.printError(Serial);
      }
    }
  });
  
  //finally begin server
  server.begin();
  Serial.println();
  Serial.println("Web Server started");
  Serial.println();


  //OSC Info
  Serial.println("OSC port: " + String(udpPort)); //inform about the port
  Serial.println();

  //setup mDNS for collecting the IPs of other machines in the network, only in STA Mode
  discoverMDNShosts();

  //REMOTE DEBUG SETUP
  configureDebugService();

  //init input reading arrays
  /*
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    input0[thisReading] = 0;
    input1[thisReading] = 0;
    input2[thisReading] = 0;
  }
  */  
  //init interruptions timing
  //previous_int=millis();
}



void loop(){

  if(ok) {  //if connected and ALL ok the LED should be ON
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }

/*
unsigned long currentMillis2 = millis();
 if (currentMillis2 - previousMillis2 >= 1000) {
 // save the last time you blinked the LED
 previousMillis2 = currentMillis2;
 // if the LED is off turn it on and vice-versa:
 ledState = not(ledState);
 // set the LED with the ledState of the variable:
 digitalWrite(LED_BUILTIN,  ledState);
 printValues();
 }
*/


//after a change of mode we should reboot the board
  
  if(shouldReboot){
    Serial.println("Rebooting...");
    shouldReboot = false;
    delay(100);
    ESP.restart();
  }

  //handle wifi server clients connected and configuring via browser (always?)
  server.handleClient();
  
  // OSC data transmission of orientation sensor
  if(connected || APconnected){ //only send OSC data when connected

    if(accesspoint){   // ESP32 AS ACCESS POINT: send to all connected clients
         
      //a control flag
      ok = false;

      lis.getEvent(&event);

       //transform values to MIDI range ( [+-]9.8 to 0 -> 127)
      inputVal = float(map(event.acceleration.x, -9.8, 9.8, 0, 100));
      inputVal2 = float(map(event.acceleration.y,-9.8, 9.8, 0, 100));
      inputVal3 = float(map(event.acceleration.z,-9.8, 9.8, 0, 100));

      temp = bme.readTemperature();
      pressure = bme.readPressure();
      altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
      humidity = bme.readHumidity();
      
      //Filtering
      inputVal = adcFilter1.filter(inputVal);
      inputVal2 = adcFilter2.filter(inputVal2);
      inputVal3 = adcFilter3.filter(inputVal3);

      //temp = adcFilter4.filter(temp);
      //pressure = adcFilter5.filter(pressure);
      altitude = adcFilter6.filter(altitude);
      //humidity = adcFilter7.filter(humidity);
          
      for(int i = 0; i < numClients; i++) {
          ok = true; 
          
          Debug.printf("input1: %s\n", String(inputVal).c_str()); // Note: if type is String need c_str()

          //check if values are different and send them
          if(inputVal != prev_inputVal || inputVal2 != prev_inputVal2 || inputVal3 != prev_inputVal3){
            msg_pot1.add(inputVal);  // some dummy data (milliseconds running this code)
            udp.beginPacket(clientsAddress[i], udpPort);
            msg_pot1.send(udp);
            udp.endPacket();
            msg_pot1.empty();
  
            msg_pot2.add(inputVal2);  // some dummy data (milliseconds running this code)
            udp.beginPacket(clientsAddress[i], udpPort);
            msg_pot2.send(udp);
            udp.endPacket();
            msg_pot2.empty();

            msg_pot3.add(inputVal3);  // some dummy data (milliseconds running this code)
            udp.beginPacket(clientsAddress[i], udpPort);
            msg_pot3.send(udp);
            udp.endPacket();
            msg_pot3.empty();

            msg_temp.add(temp);  // some dummy data (milliseconds running this code)
            udp.beginPacket(clientsAddress[i], udpPort);
            msg_temp.send(udp);
            udp.endPacket();
            msg_temp.empty();

            /*
            msg_press.add(pressure);  // some dummy data (milliseconds running this code)
            udp.beginPacket(clientsAddress[i], udpPort);
            msg_press.send(udp);
            udp.endPacket();
            msg_press.empty();
            */
            
            msg_alt.add(altitude);  // some dummy data (milliseconds running this code)
            udp.beginPacket(clientsAddress[i], udpPort);
            msg_alt.send(udp);
            udp.endPacket();
            msg_alt.empty();

            msg_hum.add(humidity);  // some dummy data (milliseconds running this code)
            udp.beginPacket(clientsAddress[i], udpPort);
            msg_hum.send(udp);
            udp.endPacket();
            msg_hum.empty();
            
          }
          prev_inputVal=inputVal;
          prev_inputVal2=inputVal2;
          prev_inputVal3=inputVal3;
       } 
    } 
    else{ ///CONNECTED TO STA WIFI

     lis.getEvent(&event);

      //transform values to MIDI range ( [+-]9.8 to 0 -> 127)
      inputVal = float(map(event.acceleration.x, -9.8, 9.8, 0, 100));
      inputVal2 = float(map(event.acceleration.y,-9.8, 9.8, 0, 100));
      inputVal3 = float(map(event.acceleration.z,-9.8, 9.8, 0, 100));


      //inputVal = event.acceleration.x;
      //inputVal2 = event.acceleration.y;
      //inputVal3 = event.acceleration.z;
      
      //Filtering
      inputVal = adcFilter1.filter(inputVal);
      inputVal2 = adcFilter2.filter(inputVal2);
      inputVal3 = adcFilter3.filter(inputVal3);
     
     //OSC send
     for (int i = 0; i < nServices; ++i) {
                 
       //Ip address to transmit udp packages
       udpAddress = MDNS.IP(i);

       Debug.printf("input1: %s\n", String(inputVal).c_str()); // Note: if type is String need c_str()

       if(inputVal != prev_inputVal || inputVal2 != prev_inputVal2){
          msg_pot1.add(inputVal);  // some dummy data (milliseconds running this code)
          udp.beginPacket(udpAddress, udpPort);
          msg_pot1.send(udp);
          udp.endPacket();
          msg_pot1.empty();
    
          msg_pot2.add(inputVal2);  // some dummy data (milliseconds running this code)
          udp.beginPacket(udpAddress, udpPort);
          msg_pot2.send(udp);
          udp.endPacket();
          msg_pot2.empty();

          msg_pot3.add(inputVal3);  // some dummy data (milliseconds running this code)
          udp.beginPacket(udpAddress, udpPort);
          msg_pot3.send(udp);
          udp.endPacket();
          msg_pot3.empty();
        }
        
     }
     prev_inputVal=inputVal;
     prev_inputVal2=inputVal2;
     prev_inputVal3=inputVal3;
    
    }
    
  }
  
  //for remote debugger
  //Debug.handle();

  //Wait for some micro or mili-seconds to avoid overflow of OSC messages
  if(delayProgram =! 0) delay(delayProgram);
 
}

void printValues() {
    Serial.print("Temperature = ");
    Serial.print(bme.readTemperature());
    Serial.println(" *C");

    Serial.print("Pressure = ");

    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(" hPa");

    Serial.print("Approx. Altitude = ");
    Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
    Serial.println(" m");

    Serial.print("Humidity = ");
    Serial.print(bme.readHumidity());
    Serial.println(" %");

    Serial.println();
}
