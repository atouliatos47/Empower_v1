#include "config.h"

// WiFi Credentials
const char* ssid = "SKYPL2JH";
const char* password = "zNeUN3iQa2AbCJ";

// AlphaBase Configuration
const char* alphabaseURL = "http://192.168.0.52:8000";
const char* alphabaseUsername = "atoul";
const char* alphabasePassword = "password123";

// MQTT Configuration
const char* mqttServer = "192.168.0.52";
const int mqttPort = 1883;
const char* mqttTopicStatus = "alphabase/presses/status";
const char* mqttTopicCommands = "alphabase/presses/commands";

// Device Info
const char* deviceID = "Press-Simulator-01";

// Pin Definitions
const int PIN_BUTTON_START_STOP = 15;
const int PIN_RED_LED = 2;
const int PIN_GREEN_LED = 4;
const int PIN_BUTTON_MAINTENANCE = 5;
const int PIN_BUTTON_QUALITY = 21;
const int PIN_BUTTON_MATERIAL = 12;
const int PIN_BUTTON_TOOL_CHANGE = 13;

// Timing Constants
const long blinkInterval = 500;
const long mqttPublishInterval = 5000;
const unsigned long debounceDelay = 50;