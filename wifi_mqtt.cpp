#include "wifi_mqtt.h"
#include "alphabase.h"
#include "buttons_leds.h"
#include <ArduinoJson.h>

// External variables from main file
extern PressState pressState;
extern unsigned long pressStartTime;
extern unsigned long pressStopTime;

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n✅ WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  Serial.print("Connecting to MQTT");
  while (!mqttClient.connected()) {
    if (mqttClient.connect(deviceID)) {
      Serial.println("\n✅ MQTT Connected!");
      
      // Subscribe to commands topic
      mqttClient.subscribe(mqttTopicCommands);
      Serial.print("📥 Subscribed to: ");
      Serial.println(mqttTopicCommands);
      
      // Publish initial status
      publishStatusMQTT();
    } else {
      Serial.print(".");
      delay(500);
    }
  }
}

void publishStatusMQTT() {
  StaticJsonDocument<256> doc;
  doc["device_id"] = deviceID;
  
  String stateStr;
  if (pressState == IDLE) stateStr = "IDLE";
  else if (pressState == RUNNING) stateStr = "RUNNING";
  else if (pressState == WAITING_FOR_REASON) stateStr = "WAITING_FOR_REASON";
  
  doc["press1"] = stateStr;
  doc["timestamp"] = millis();
  doc["ip"] = WiFi.localIP().toString();
  
  String jsonData;
  serializeJson(doc, jsonData);
  
  mqttClient.publish(mqttTopicStatus, jsonData.c_str());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("\n⬇️  MQTT Command Received!");
  
  // Create a buffer to hold the message
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0'; // Null-terminate the string
  
  Serial.println(message);

  // Only process commands if we are waiting for a reason
  if (pressState != WAITING_FOR_REASON) {
    Serial.println("⚠️  Not in WAITING_FOR_REASON state. Ignoring command.");
    return;
  }

  // Parse the JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.print("❌ Failed to parse JSON: ");
    Serial.println(error.c_str());
    return;
  }

  const char* command = doc["command"]; // "select_reason"
  const char* reason = doc["reason"];   // The reason text

  if (strcmp(command, "select_reason") == 0) {
    // Handle all four possible reasons from the app
    if (strcmp(reason, "Maintenance Required") == 0 || strcmp(reason, "Maintenance") == 0) {
        Serial.println("\n🔧 MAINTENANCE Reason Selected (from App)!");
        sendStopNotifications("Maintenance Required");
        logEventToAlphaBase("REASON_SELECTED", "Maintenance Required");
        pressState = IDLE;
        publishStatusMQTT();
        Serial.println("✅ Back to IDLE state\n");
    
    } else if (strcmp(reason, "Quality Issue") == 0) {
        Serial.println("\n⚠️  QUALITY Reason Selected (from App)!");
        sendStopNotifications("Quality Issue");
        logEventToAlphaBase("REASON_SELECTED", "Quality Issue");
        pressState = IDLE;
        publishStatusMQTT();
        Serial.println("✅ Back to IDLE state\n");
    
    } else if (strcmp(reason, "Material Issue") == 0) {
        Serial.println("\n📦 MATERIAL ISSUE Reason Selected (from App)!");
        sendStopNotifications("Material Issue");
        logEventToAlphaBase("REASON_SELECTED", "Material Issue");
        pressState = IDLE;
        publishStatusMQTT();
        Serial.println("✅ Back to IDLE state\n");
    
    } else if (strcmp(reason, "Tool Change") == 0) {
        Serial.println("\n🔨 TOOL CHANGE Reason Selected (from App)!");
        sendStopNotifications("Tool Change");
        logEventToAlphaBase("REASON_SELECTED", "Tool Change");
        pressState = IDLE;
        publishStatusMQTT();
        Serial.println("✅ Back to IDLE state\n");
    
    } else {
      Serial.print("Unknown reason: ");
      Serial.println(reason);
    }
  } else {
    Serial.print("Unknown command: ");
    Serial.println(command);
  }
}