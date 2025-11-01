#ifndef CONFIG_H
#define CONFIG_H

// WiFi Credentials
extern const char* ssid;
extern const char* password;

// AlphaBase Configuration
extern const char* alphabaseURL;
extern const char* alphabaseUsername;
extern const char* alphabasePassword;

// MQTT Configuration
extern const char* mqttServer;
extern const int mqttPort;
extern const char* mqttTopicStatus;
extern const char* mqttTopicCommands;

// Device Info
extern const char* deviceID;

// Pin Definitions
extern const int PIN_BUTTON_START_STOP;
extern const int PIN_RED_LED;
extern const int PIN_GREEN_LED;
extern const int PIN_BUTTON_MAINTENANCE;
extern const int PIN_BUTTON_QUALITY;
extern const int PIN_BUTTON_MATERIAL;
extern const int PIN_BUTTON_TOOL_CHANGE;

// Timing Constants
extern const long blinkInterval;
extern const long mqttPublishInterval;
extern const unsigned long debounceDelay;

#endif