/********************************************************
                   Thermometer Module
                      By: Ryan Wade
 *******************************************************/

// WIFI stuffs
#include <ESP8266WiFi.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"

Adafruit_SHT31 sht31 = Adafruit_SHT31();

const bool debug = true;

const int WIFI_STATUS_LIGHT = 2;

/* HOME CONFIG 
const char* ssid     = "2Girls1SlowRouter";
const char* password = "onthefridge";
*/

//School Config
const char* ssid = "RyansIOTDevicePlayground";
const IPAddress ip(10,0,0,3);
const IPAddress subnet(255, 255, 255, 0);
const IPAddress gateway(10, 0, 0, 1);

// Web Control Server on port 80
WiFiServer server(80);


void setup() {

  pinMode(WIFI_STATUS_LIGHT, OUTPUT);

  // Serial begin if debugging
  /*if (debug)*/ Serial.begin(115200);
  if (debug) Serial.println();

  // WIFI Setup
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid/* SCHOOL CONFIG, password */);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(WIFI_STATUS_LIGHT, LOW);
    if (debug) Serial.print(".");
    delay(500);
  }
  digitalWrite(WIFI_STATUS_LIGHT, HIGH);
  if (debug) Serial.println("\nWifi Connected to " + String(ssid));
  server.begin();
  if (debug) Serial.printf("on IP: %s", WiFi.localIP().toString().c_str());

  Serial.println();
  Serial.println("SHT31 starting");
  if (! sht31.begin(0x44)) {
    if (debug) Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }

  if (debug) Serial.println("SHT31 Connected");


}


void loop() {
  /****** MAIN SYSTEM FUNCTINOS ***********/

  /*************************************/

  WiFiClient client = server.available();
  if (client) {
    if (debug) Serial.println("\n[SOMEONE CONNECTED]");
    // we have a new client sending some request
    while (!client.available()) {
      delay(1);
    }
    String req = client.readStringUntil('\r');
    if (debug) Serial.println(req);
    client.flush();

    if (req.indexOf("?tmp") != -1) {
      // TODO: Read temp and return to client
      if (debug) Serial.println("Temp is: " + String(getTemp()) + "* F");
      client.print(getTemp());
      client.stop();
    }
    else if (req.indexOf("?hmd") != -1) {
      if (debug) Serial.println("Humidity is: " + String(sht31.readHumidity()) + "* F");
      client.print(sht31.readHumidity());
      client.stop();
    }
    else if (req.indexOf("?info") != -1) {
      if (debug) Serial.println("Info requested");
      client.print("Temperature sensor V0.1a");
      client.stop();
    }
    else if (req.indexOf("?mac") != -1) {
      if (debug) Serial.println("MAC requested");
      client.print(WiFi.macAddress());
      client.stop();
    }
    delay(1);
    if (debug) Serial.println("Disconnected");
  }
}

float getTemp() {
  return ((9.0 / 5.0) * sht31.readTemperature()) + 32.0;
}


