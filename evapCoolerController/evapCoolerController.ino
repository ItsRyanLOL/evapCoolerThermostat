/********************************************************
                   Cooler Control Module
                      By: Ryan Wade
 *******************************************************/

// WIFI stuffs
#include <ESP8266WiFi.h>

const bool debug = true;

const char* ssid     = "2Girls1SlowRouter";
const char* password = "onthefridge";
// Web Control Server on port 80
WiFiServer server(80);


// Main System Flags
bool pendingFanShutdown = false;
bool pumpPowerPress = false;
bool fanPowerPress = false;
bool secondPress = false;


// Pin Defnitions
const int PUMP_STATUS_PIN = 4;
const int FAN_LOW_PIN = 5;
const int FAN_HIGH_PIN = 12;
const int PUMP_POWER = 13;
const int FAN_POWER = 14;
const int MAIN_POWER = 15;
const int WIFI_STATUS_LIGHT = 2;

// CONSTANTS
const unsigned long FAN_SHUTDOWN_DELAY = 1000 * (10 * 60); // 10 min delay
const unsigned long BUTTON_PRESS_TIME = 1000 * 1.5; // 1.5 second button press
const String FAN_PREFIX = "?fan=";
const String PUMP_PREFIX = "?pmp=";
const String STATUS_IOT = "?status";

// Time Keepers
unsigned long fanShutdownTime;
unsigned long pumpButtonTime;
unsigned long fanButtonTime;
unsigned long secondPressTime;
void setup() {
  // Pin initalization
  pinMode(PUMP_STATUS_PIN, INPUT);
  pinMode(FAN_LOW_PIN, INPUT);
  pinMode(FAN_HIGH_PIN, INPUT);
  pinMode(PUMP_POWER, OUTPUT);
  pinMode(FAN_POWER, OUTPUT);
  pinMode(MAIN_POWER, OUTPUT);
  pinMode(WIFI_STATUS_LIGHT, OUTPUT);

  digitalWrite(PUMP_POWER, LOW);
  digitalWrite(FAN_POWER, LOW);
  digitalWrite(MAIN_POWER, LOW);

  // Serial begin
  /*if (debug)*/ Serial.begin(115200);
  if (debug) Serial.println();

  // WIFI Setup
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(WIFI_STATUS_LIGHT, LOW);
    if (debug) Serial.print(".");
    delay(500);
  }
  digitalWrite(WIFI_STATUS_LIGHT, HIGH);
  if (debug) Serial.println("\nWifi Connected to " + String(ssid));
  server.begin();
  if (debug) Serial.printf("on IP: %s", WiFi.localIP().toString().c_str());




}


void loop() {
  /****** MAIN SYSTEM FUNCTINOS ***********/
  // Update our "button presses"
  updateButtons();
  // Check if we are tying to shutdown fan
  if (pendingFanShutdown) {
    checkShutdown();
  }
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

    // Igonre Index Requests because bots!
    if (req.indexOf("T / H") != -1) {
      if (debug) Serial.println("Index Page");
      client.stop();
    }
    else if (req.indexOf(STATUS_IOT) != -1) {
    // Send formatted Status for IOT Devices
    client.print(String(getFanState()) + "," + String(getPumpState()) + "," + String(pendingFanShutdown));
    if (debug) Serial.print(String(getFanState()) + "," + String(getPumpState()) + "," + String(pendingFanShutdown));
    }
    else if (req.indexOf("/s") != -1) {
    if (debug) Serial.println("Status page req");
      String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\nStatus is now:  \r\n";
      s += getFanState();
      s += ",";
      s += getPumpState();
      s += "</html>\n";
      client.print(s);
    }
    else if (req.indexOf(FAN_PREFIX) != -1) {
    if (debug) Serial.println("Fan State change: " + String(req[req.indexOf(FAN_PREFIX) + 5]));
      String a = String(req[req.indexOf(FAN_PREFIX) + 5]);
      updateFan(a.toInt());
      String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\nStatus is now:<p>";
      s += getFanState();
      s += ",";
      s += getPumpState();
      s += "<p>Setting fan to ";
      s += a;
      s += "</html>\n";
      client.print(s);

    }
    else if (req.indexOf(PUMP_PREFIX) != -1) {
    if (debug) Serial.println("Pump State change: " + String(req[req.indexOf(PUMP_PREFIX) + 5]));
      String a = String(req[req.indexOf(PUMP_PREFIX) + 5]);
      if (a == "1" && !getPumpState()) powerPump();
      else if (a == "0" && getPumpState()) powerPump();
      String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\nStatus is now:<p>";
      s += getFanState();
      s += ",";
      s += getPumpState();
      s += "<p>Setting pump to ";
      s += a;
      s += "</html>\n";
      client.print(s);
    }
    else if (req.indexOf("?shutdown") != -1) {
    if (debug) Serial.println("Cooler Shutdown iniated.");
      shutDownSequence();
      String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\nStatus is now: \r\n";
      s += getFanState();
      s += ",";
      s += getPumpState();
      s += "</html>\n";
      client.print(s);
    }
    delay(1);
    //client.stop();
    if (debug) Serial.println("Disconnected");
  }
}


// Checks if it is time to turn off the fan.
void checkShutdown() {
  unsigned long timeNow = millis();
  if (timeNow >= fanShutdownTime) {
    if (debug) Serial.println("Shutting down fan now!");
    // Turn off fan
    updateFan(0);
    // Reset our flag
    pendingFanShutdown = false;
  }
}
// Gets the state of the fan. reurns 0 for OFF, 1 for HIGH, 2 for LOW
int getFanState() {
  if (digitalRead(FAN_LOW_PIN) == HIGH) {
    return 2;
  }
  else if (digitalRead(FAN_HIGH_PIN) == HIGH) {
    return 1;
  }
  else {
    return 0;
  }
}


// Returns true if pump is on, false if pump is off.
bool getPumpState() {
  if (digitalRead(PUMP_STATUS_PIN) == HIGH) {
    return true;
  }
  else {
    return false;
  }
}


// Begins shutting down the cooler. Checks if the pump is on, turns it off, waits 10 min then turns off fan.
void shutDownSequence() {
  if (getPumpState()) {
    // Pump is on, lets turn it off
    powerPump();
  }
  // Start a timer to turn off fan. Set a flag for pending fan shutdown
  pendingFanShutdown = true;
  fanShutdownTime = millis() + FAN_SHUTDOWN_DELAY;
  // Set fan to low
  updateFan(2);
}


// Turns on or off the water pump
void powerPump() {
  digitalWrite(PUMP_POWER, HIGH);
  pumpPowerPress = true;
  pumpButtonTime = millis() + BUTTON_PRESS_TIME;
}


// Sets fan to desired power level. 0 = off, 1 = high, 2 = low
void updateFan(int level) {
  // Get current fan state
  int current = getFanState();

  // We need to press the fan button once or twice. INCOMING SLOPPY LOGIC
  // Figure out how many steps we need to make.
  int presses = level - current;
  if (debug) Serial.println("presses: " + String(presses) + " level: " + String(level) + " current: " + String(current));

  if (presses == 1) {
    digitalWrite(FAN_POWER, HIGH);
    //Set time button should be released
    fanButtonTime = millis() + BUTTON_PRESS_TIME;
    // update flag
    fanPowerPress = true;
  }
  else if (presses == 2) {
    digitalWrite(FAN_POWER, HIGH);
    //Set time button should be released
    fanButtonTime = millis() + BUTTON_PRESS_TIME;
    // Set timer for second press
    secondPressTime = millis() + (2 * BUTTON_PRESS_TIME);
    // update flag
    fanPowerPress = true;
    secondPress = true;
  }
  else if (presses == -2) {
    digitalWrite(FAN_POWER, HIGH);
    //Set time button should be released
    fanButtonTime = millis() + BUTTON_PRESS_TIME;
    // update flag
    fanPowerPress = true;
  }
  else if (presses == -1) {
    digitalWrite(FAN_POWER, HIGH);
    //Set time button should be released
    fanButtonTime = millis() + BUTTON_PRESS_TIME;
    // Set timer for second press
    secondPressTime = millis() + (2 * BUTTON_PRESS_TIME);
    // update flag
    fanPowerPress = true;
    secondPress = true;
  }

}
// Updates pin output for simulated button presses. Basically checks our timers and sees if it needs to stop "pressing" a button.
void updateButtons() {
  unsigned long timeNow = millis();
  // pump power button updates
  if (pumpPowerPress && timeNow >= pumpButtonTime) {
    // turn off our pump pin
    digitalWrite(PUMP_POWER, LOW);

    // reset our flag
    pumpPowerPress = false;
  }
  if (!fanPowerPress && secondPress && (secondPressTime <= timeNow)) {
    secondPress = false;
    // Turn on output for second press
    digitalWrite(FAN_POWER, HIGH);
    //Set time button should be released
    fanButtonTime = millis() + BUTTON_PRESS_TIME;
    // update flag
    fanPowerPress = true;

  }

  else if (fanPowerPress && timeNow >= fanButtonTime) {
    //turn off our fan pin
    if (debug) Serial.println("Fan Power Switch Off");
    digitalWrite(FAN_POWER, LOW);
    //reset flag
    fanPowerPress = false;
  }
}

