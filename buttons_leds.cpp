#include "buttons_leds.h"
#include "alphabase.h"
#include "wifi_mqtt.h"

// External variables from main file
extern PressState pressState;
extern unsigned long pressStartTime;
extern unsigned long pressStopTime;

// Button states (from main file)
extern int buttonStartStopState;
extern int lastButtonStartStopState;
extern unsigned long lastDebounceTimeStartStop;

extern int buttonMaintenanceState;
extern int lastButtonMaintenanceState;
extern unsigned long lastDebounceTimeMaintenance;

extern int buttonQualityState;
extern int lastButtonQualityState;
extern unsigned long lastDebounceTimeQuality;

extern int buttonMaterialState;
extern int lastButtonMaterialState;
extern unsigned long lastDebounceTimeMaterial;

extern int buttonToolChangeState;
extern int lastButtonToolChangeState;
extern unsigned long lastDebounceTimeToolChange;

// LED states (from main file)
extern unsigned long lastBlinkTime;
extern bool ledState;

void initializePins() {
  pinMode(PIN_RED_LED, OUTPUT);
  pinMode(PIN_GREEN_LED, OUTPUT);
  pinMode(PIN_BUTTON_START_STOP, INPUT_PULLUP);
  pinMode(PIN_BUTTON_MAINTENANCE, INPUT_PULLUP);
  pinMode(PIN_BUTTON_QUALITY, INPUT_PULLUP);
  pinMode(PIN_BUTTON_MATERIAL, INPUT_PULLUP);
  pinMode(PIN_BUTTON_TOOL_CHANGE, INPUT_PULLUP);
  
  digitalWrite(PIN_RED_LED, HIGH);
  digitalWrite(PIN_GREEN_LED, LOW);
}

void handleStartStopButton() {
  int reading = digitalRead(PIN_BUTTON_START_STOP);
  
  if (reading != lastButtonStartStopState) {
    lastDebounceTimeStartStop = millis();
  }
  
  if ((millis() - lastDebounceTimeStartStop) > debounceDelay) {
    if (reading != buttonStartStopState) {
      buttonStartStopState = reading;
      
      if (buttonStartStopState == LOW) {
        Serial.println("\n🔘 START/STOP Button Pressed!");
        
        if (pressState == IDLE) {
          pressState = RUNNING;
          pressStartTime = millis(); 
          Serial.println("✅ Press 1 STARTED"); 
          publishStatusMQTT();
          logEventToAlphaBase("STARTED"); 
        } else if (pressState == RUNNING) {
          pressState = WAITING_FOR_REASON; 
          pressStopTime = millis();
          Serial.println("🛑 Press 1 STOPPED"); 
          publishStatusMQTT();
          logEventToAlphaBase("STOPPED"); 
        }
      }
    }
  }
  
  lastButtonStartStopState = reading;
}

void handleMaintenanceButton() {
  if (pressState != WAITING_FOR_REASON) return;
  
  int reading = digitalRead(PIN_BUTTON_MAINTENANCE);
  
  if (reading != lastButtonMaintenanceState) {
    lastDebounceTimeMaintenance = millis();
  }
  
  if ((millis() - lastDebounceTimeMaintenance) > debounceDelay) {
    if (reading != buttonMaintenanceState) {
      buttonMaintenanceState = reading;
      
      if (buttonMaintenanceState == LOW) {
        Serial.println("\n🔧 MAINTENANCE Button Pressed!");
        sendStopNotifications("Maintenance Required");
        logEventToAlphaBase("REASON_SELECTED", "Maintenance Required");
        pressState = IDLE;
        publishStatusMQTT();
        Serial.println("✅ Back to IDLE state\n");
      }
    }
  }
  
  lastButtonMaintenanceState = reading;
}

void handleQualityButton() {
  if (pressState != WAITING_FOR_REASON) return;
  
  int reading = digitalRead(PIN_BUTTON_QUALITY);
  
  if (reading != lastButtonQualityState) {
    lastDebounceTimeQuality = millis();
  }
  
  if ((millis() - lastDebounceTimeQuality) > debounceDelay) {
    if (reading != buttonQualityState) {
      buttonQualityState = reading;
      
      if (buttonQualityState == LOW) {
        Serial.println("\n⚠️  QUALITY Button Pressed!");
        sendStopNotifications("Quality Issue");
        logEventToAlphaBase("REASON_SELECTED", "Quality Issue");
        pressState = IDLE;
        publishStatusMQTT();
        Serial.println("✅ Back to IDLE state\n");
      }
    }
  }
  
  lastButtonQualityState = reading;
}

void handleMaterialButton() {
  if (pressState != WAITING_FOR_REASON) return;
  
  int reading = digitalRead(PIN_BUTTON_MATERIAL);
  
  if (reading != lastButtonMaterialState) {
    lastDebounceTimeMaterial = millis();
  }
  
  if ((millis() - lastDebounceTimeMaterial) > debounceDelay) {
    if (reading != buttonMaterialState) {
      buttonMaterialState = reading;
      
      if (buttonMaterialState == LOW) {
        Serial.println("\n📦 MATERIAL ISSUE Button Pressed!");
        sendStopNotifications("Material Issue");
        logEventToAlphaBase("REASON_SELECTED", "Material Issue");
        pressState = IDLE;
        publishStatusMQTT();
        Serial.println("✅ Back to IDLE state\n");
      }
    }
  }
  
  lastButtonMaterialState = reading;
}

void handleToolChangeButton() {
  if (pressState != WAITING_FOR_REASON) return;
  
  int reading = digitalRead(PIN_BUTTON_TOOL_CHANGE);
  
  if (reading != lastButtonToolChangeState) {
    lastDebounceTimeToolChange = millis();
  }
  
  if ((millis() - lastDebounceTimeToolChange) > debounceDelay) {
    if (reading != buttonToolChangeState) {
      buttonToolChangeState = reading;
      
      if (buttonToolChangeState == LOW) {
        Serial.println("\n🔨 TOOL CHANGE Button Pressed!");
        sendStopNotifications("Tool Change");
        logEventToAlphaBase("REASON_SELECTED", "Tool Change");
        pressState = IDLE;
        publishStatusMQTT();
        Serial.println("✅ Back to IDLE state\n");
      }
    }
  }
  
  lastButtonToolChangeState = reading;
}

void updateLEDs() {
  switch (pressState) {
    case IDLE:
      digitalWrite(PIN_RED_LED, HIGH);
      digitalWrite(PIN_GREEN_LED, LOW);
      break;
      
    case RUNNING:
      digitalWrite(PIN_RED_LED, LOW);
      if (millis() - lastBlinkTime >= blinkInterval) {
        lastBlinkTime = millis();
        ledState = !ledState;
        digitalWrite(PIN_GREEN_LED, ledState);
      }
      break;
      
    case WAITING_FOR_REASON:
      if (millis() - lastBlinkTime >= blinkInterval) {
        lastBlinkTime = millis();
        ledState = !ledState;
        digitalWrite(PIN_RED_LED, ledState);
        digitalWrite(PIN_GREEN_LED, !ledState);
      }
      break;
  }
}