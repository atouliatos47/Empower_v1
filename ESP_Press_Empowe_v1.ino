/*
 * Power Press with Reason Codes + Email Alerts
 * Organized Version with Multiple Tabs
 */

#include <WebServer.h>
#include "config.h"
#include "state_machine.h"
#include "wifi_mqtt.h"
#include "alphabase.h"
#include "buttons_leds.h"

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// State Management
PressState pressState = IDLE;
unsigned long pressStartTime = 0;
unsigned long pressStopTime = 0;

// Network Clients
WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Authentication
String authToken = "";

// Button Debouncing
int buttonStartStopState = HIGH;
int lastButtonStartStopState = HIGH;
unsigned long lastDebounceTimeStartStop = 0;

int buttonMaintenanceState = HIGH;
int lastButtonMaintenanceState = HIGH;
unsigned long lastDebounceTimeMaintenance = 0;

int buttonQualityState = HIGH;
int lastButtonQualityState = HIGH;
unsigned long lastDebounceTimeQuality = 0;

int buttonMaterialState = HIGH;
int lastButtonMaterialState = HIGH;
unsigned long lastDebounceTimeMaterial = 0;

int buttonToolChangeState = HIGH;
int lastButtonToolChangeState = HIGH;
unsigned long lastDebounceTimeToolChange = 0;

// LED Blinking
unsigned long lastBlinkTime = 0;
bool ledState = LOW;

// Timing
unsigned long lastMQTTPublish = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  Press Monitor - Organized Version    ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Initialize hardware
  initializePins();
  
  // Connect to networks
  connectToWiFi();
  
  // Login to AlphaBase
  if (!loginAlphaBase()) {
    Serial.println("⚠️  Continuing without AlphaBase...");
  } else {
    discoverAlphaBaseCollections();
  }
  
  // Setup MQTT
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
  connectMQTT();
  
  // Start Web Server
  server.on("/status", HTTP_GET, handleGetStatus);
  server.begin();
  Serial.println("🌐 Web Server Started on /status");
  
  Serial.println("\n✅ Setup Complete!");
  printHelp();
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
  // Maintain connections
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
  
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();
  
  // Handle web server
  server.handleClient();
  
  // Handle all buttons
  handleStartStopButton();
  handleMaintenanceButton();
  handleQualityButton();
  handleMaterialButton();
  handleToolChangeButton();
  
  // Update LEDs
  updateLEDs();
  
  // Periodic status updates
  unsigned long currentMillis = millis();
  if (currentMillis - lastMQTTPublish >= mqttPublishInterval) {
    lastMQTTPublish = currentMillis;
    publishStatusMQTT();
  }

  // AlphaBase token refresh
  static unsigned long lastTokenRefresh = 0;
  const unsigned long tokenRefreshInterval = 15 * 60 * 1000; // 15 minutes

  if (authToken != "" && (currentMillis - lastTokenRefresh >= tokenRefreshInterval)) {
    Serial.println("🔄 Periodic token refresh...");
    if (refreshAlphaBaseToken()) {
      lastTokenRefresh = currentMillis;
      Serial.println("✅ Token refreshed successfully!");
    } else {
      Serial.println("❌ Periodic token refresh failed");
    }
  }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void handleGetStatus() {
  String stateStr;
  if (pressState == IDLE) stateStr = "IDLE";
  else if (pressState == RUNNING) stateStr = "RUNNING";
  else if (pressState == WAITING_FOR_REASON) stateStr = "WAITING_FOR_REASON";
  
  String json = "{";
  json += "\"device_id\": \"" + String(deviceID) + "\",";
  json += "\"press1\": \"" + stateStr + "\",";
  json += "\"uptime\": " + String(millis() / 1000);
  json += "}";
  
  server.send(200, "application/json", json);
}

void printHelp() {
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("PHYSICAL BUTTONS:");
  Serial.println("  Button D15: Start/Stop Press");
  Serial.println("  Button 5:   Maintenance");
  Serial.println("  Button 21:  Quality Issue");
  Serial.println("  Button 12:  Material Issue");
  Serial.println("  Button 13:  Tool Change");
  Serial.println("+ App control via MQTT also available");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}