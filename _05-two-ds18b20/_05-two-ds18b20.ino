#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

extern "C" {
#include "user_interface.h"
}

extern "C" uint16_t readvdd33(void);

#define dsout 5

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 4
#define TEMPERATURE_PRECISION 12

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
DeviceAddress insideThermometer, outsideThermometer;

// wifi
#ifdef __IS_MY_HOME
  #include "/usr/local/src/ap_setting.h"
#else
  #include "ap_setting.h"
#endif

char* topic = "esp8266/arduino/s04";
char* hellotopic = "HELLO";
IPAddress server(192, 168, 10, 10);

String clientName;
WiFiClient wifiClient;

void callback(const MQTT::Publish& pub) {
  // handle message arrived
}

PubSubClient client(wifiClient, server);

long startMills;

int vdd; 

void setup(void)
{
  startMills = millis();
  // start serial port
  Serial.begin(38400);
  Serial.println("Dallas Temperature IC Control Library Demo");

  vdd = readvdd33();
//-------------------

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

 // client.set_callback(callback);

  WiFi.mode(WIFI_STA);

  #ifdef __IS_MY_HOME
  WiFi.begin(ssid, password, channel, bssid);
  WiFi.config(IPAddress(192, 168, 10, 13), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
  #else
  WiFi.begin(ssid, password); 
  #endif

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(millis() - startMills);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  Serial.print("Connecting to ");
  Serial.print(server);
  Serial.print(" as ");
  Serial.println(clientName);

  delay(200);
  
  if (client.connect((char*) clientName.c_str())) {
    Serial.println("Connected to MQTT broker");
    Serial.print("Topic is: ");
    Serial.println(topic);
    
    if (client.publish(hellotopic, "hello from ESP8266")) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
    }
  }
  else {
    Serial.println("MQTT connect failed");
    Serial.println("Will reset and try again...");
    abort();
  }
  
 Serial.println(millis() - startMills);
 //------- 
  
  pinMode(dsout, OUTPUT);
  digitalWrite(dsout, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(20);
    
  // Start up the library
  sensors.begin();


  // 
  // method 1: by index
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0"); 
  if (!sensors.getAddress(outsideThermometer, 1)) Serial.println("Unable to find address for Device 1"); 

  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
  sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION);

  Serial.println(millis() - startMills);
  
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  Serial.print("Temp C: ");
  Serial.print(tempC);
  Serial.print(" Temp F: ");
  Serial.print(DallasTemperature::toFahrenheit(tempC));
}

// function to print a device's resolution
void printResolution(DeviceAddress deviceAddress)
{
  Serial.print("Resolution: ");
  Serial.print(sensors.getResolution(deviceAddress));
  Serial.println();    
}

// main function to print information about a device
void printData(DeviceAddress deviceAddress)
{
  Serial.print("Device Address: ");
  printAddress(deviceAddress);
  Serial.print(" ");
  printTemperature(deviceAddress);
  Serial.println();
}

void loop(void)
{ 

  // original loop
  
  Serial.println(millis() - startMills);
  // call sensors.requestTemperatures() to issue a global temperature 
  // request to all devices on the bus
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures();
  Serial.println("DONE");
  Serial.println(millis() - startMills);
  // print the device information
  printData(insideThermometer);
  printData(outsideThermometer);
  
  Serial.println(millis() - startMills);
  float tempCinside  = sensors.getTempC(outsideThermometer);
  float tempCoutside = sensors.getTempC(insideThermometer);

  if ( isnan(tempCinside) || isnan(tempCoutside) || isnan(vdd) ) {
    Serial.println("Failed to read from sensor!");
    return;
  }
  
  Serial.println(millis() - startMills);
  String payload = "{\"INSIDE\":";
  payload += tempCinside;
  payload += ",\"OUTSIDE\":";
  payload += tempCoutside;
  payload += ",\"vdd\":";
  payload += vdd;
  payload += "}";

  Serial.println(payload);

  sendTemperature(payload);
  
  Serial.println(millis() - startMills);
  Serial.println("Going to sleep"); 
  delay(250);
  ESP.deepSleep(300000000);
  delay(250);
}



void sendTemperature(String payload) {
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker again OUTTEMP");
      Serial.print("Topic is: ");
      Serial.println(topic);
    }
    else {
      Serial.println("MQTT connect failed");
      Serial.println("Will reset and try again...");
      abort();
    }
  }

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.println(payload);

    if (client.publish(topic, (char*) payload.c_str())) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
    }
  }

}


String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

//---------------
