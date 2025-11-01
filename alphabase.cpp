#include "alphabase.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// External variables from main file
extern unsigned long pressStartTime;
extern unsigned long pressStopTime;

bool loginAlphaBase() {
  Serial.println("Logging in to AlphaBase...");

  HTTPClient http;
  String url = String(alphabaseURL) + "/auth/login";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;
  doc["username"] = alphabaseUsername;
  doc["password"] = alphabasePassword;

  String jsonData;
  serializeJson(doc, jsonData);

  int httpCode = http.POST(jsonData);

  if (httpCode == 200) {
    String response = http.getString();
    StaticJsonDocument<512> responseDoc;
    deserializeJson(responseDoc, response);

    authToken = responseDoc["access_token"].as<String>();
    Serial.println("✅ AlphaBase Login Successful!");
    http.end();
    return true;
  }

  Serial.print("❌ AlphaBase Login Failed. HTTP Code: ");
  Serial.println(httpCode);
  http.end();
  return false;
}

bool refreshAlphaBaseToken() {
  Serial.println("🔄 Refreshing AlphaBase token...");

  HTTPClient http;
  String url = String(alphabaseURL) + "/auth/refresh";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + authToken);

  int httpCode = http.POST("{}");

  if (httpCode == 200) {
    String response = http.getString();
    StaticJsonDocument<512> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error) {
      Serial.print("❌ JSON parsing failed: ");
      Serial.println(error.c_str());
      http.end();
      return false;
    }

    if (responseDoc.containsKey("access_token")) {
      authToken = responseDoc["access_token"].as<String>();
      Serial.println("✅ Token refreshed successfully!");
      http.end();
      return true;
    } else {
      Serial.println("❌ No access_token in response");
      http.end();
      return false;
    }
  } else {
    Serial.print("❌ Token refresh failed. Code: ");
    Serial.println(httpCode);

    // Print response for debugging
    if (httpCode > 0) {
      String response = http.getString();
      Serial.print("📄 Response: ");
      Serial.println(response);
    }
    http.end();
    return false;
  }
}

void discoverAlphaBaseCollections() {
  Serial.println("\n🔍 DISCOVERING ALPHABASE COLLECTIONS...");

  HTTPClient http;
  String url = String(alphabaseURL) + "/api/collections";

  Serial.print("📡 Checking collections at: ");
  Serial.println(url);

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + authToken);

  int httpCode = http.GET();

  Serial.print("📊 Response Code: ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("✅ Collections found!");
    Serial.print("📦 Collections data: ");
    Serial.println(response);
  } else {
    Serial.println("❌ No collections endpoint or no collections exist");
  }

  http.end();
}

void sendStopEmailAlert(String reason, unsigned long runtime) {
  int minutes = runtime / 60;
  int seconds = runtime % 60;

  Serial.println("📧 Sending EMAIL alert...");

  HTTPClient http;
  String url = String(alphabaseURL) + "/notifications/send-alert";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + authToken);

  StaticJsonDocument<512> doc;
  doc["to_email"] = "atouliatos43@gmail.com";
  doc["alert_title"] = "Press 1 Stopped - " + reason;
  String message = "Press 1 has been stopped.\n\n";
  message += "Reason: " + reason + "\n";
  message += "Runtime: " + String(minutes) + " minutes " + String(seconds) + " seconds\n";
  doc["alert_message"] = message;

  StaticJsonDocument<256> dataDoc;
  dataDoc["press_number"] = 1;
  dataDoc["reason"] = reason;
  dataDoc["runtime_seconds"] = runtime;
  doc["data"] = dataDoc;

  String jsonData;
  serializeJson(doc, jsonData);

  int httpCode = http.POST(jsonData);
  if (httpCode == 200) {
    Serial.println("✅ Email sent successfully!");
  } else {
    Serial.print("❌ Email failed. Code: ");
    Serial.println(httpCode);
  }
  http.end();
}

void sendStopTelegramAlert(String reason, unsigned long runtime) {
  int minutes = runtime / 60;
  int seconds = runtime % 60;

  Serial.println("💬 Sending TELEGRAM alert...");

  HTTPClient http;
  String url = String(alphabaseURL) + "/notifications/send-telegram-alert";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + authToken);

  StaticJsonDocument<512> doc;
  doc["title"] = "Press 1 Stopped - " + reason;
  doc["message"] = "Press 1 has been stopped.";

  StaticJsonDocument<256> dataDoc;
  dataDoc["Reason"] = reason;
  dataDoc["Runtime"] = String(minutes) + " min " + String(seconds) + " sec";
  dataDoc["Press"] = "Press 1";
  doc["data"] = dataDoc;

  String jsonData;
  serializeJson(doc, jsonData);

  int httpCode = http.POST(jsonData);
  if (httpCode == 200) {
    Serial.println("✅ Telegram sent successfully!");
  } else {
    Serial.print("❌ Telegram failed. Code: ");
    Serial.println(httpCode);
  }
  http.end();
}

void sendStopNotifications(String reason) {
  if (authToken == "") {
    Serial.println("⚠️  Not authenticated. Skipping notifications.");
    return;
  }

  unsigned long runtime = (pressStopTime - pressStartTime) / 1000;

  Serial.println("\n🔔 Sending stop notifications...");
  Serial.print("   Reason: ");
  Serial.println(reason);

  sendStopEmailAlert(reason, runtime);
  sendStopTelegramAlert(reason, runtime);
}

bool attemptAlphaBaseLog(String event, String reason, int partCount, String partNumber) {
  if (authToken == "") {
    Serial.println("⚠️  No auth token available");
    return false;
  }

  HTTPClient http;
  String url = String(alphabaseURL) + "/api/collections/press_events/records";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + authToken);

  StaticJsonDocument<512> doc;
  doc["device_id"] = deviceID;
  doc["press_number"] = 1;
  doc["event_type"] = event;
  doc["timestamp"] = millis();

  if (reason != "") {
    doc["downtime_reason"] = reason;
  }

  if (event == "STOPPED" && pressStartTime > 0) {
    unsigned long runtime = (pressStopTime - pressStartTime) / 1000;
    doc["runtime_seconds"] = runtime;
  }

  String jsonData;
  serializeJson(doc, jsonData);

  Serial.print("📊 Logging to AlphaBase: ");
  Serial.println(event);

  int httpCode = http.POST(jsonData);

  if (httpCode == 401) {
    Serial.println("🔑 Token expired (401)");
    http.end();
    return false;
  } else if (httpCode == 200 || httpCode == 201) {
    Serial.println("✅ Data successfully logged to AlphaBase!");
    http.end();
    return true;
  } else {
    Serial.print("❌ AlphaBase log failed. Code: ");
    Serial.println(httpCode);

    // Print response for debugging
    if (httpCode > 0) {
      String response = http.getString();
      Serial.print("📄 Response: ");
      Serial.println(response);
    }
    http.end();
    return false;
  }
}

void logEventToAlphaBase(String event, String reason, int partCount, String partNumber) {
  // Try with current token first
  bool success = attemptAlphaBaseLog(event, reason, partCount, partNumber);

  if (!success) {
    Serial.println("🔄 First attempt failed, trying with token refresh...");

    // Refresh token and try again
    if (refreshAlphaBaseToken()) {
      success = attemptAlphaBaseLog(event, reason, partCount, partNumber);
    } else {
      Serial.println("❌ Token refresh also failed, attempting fresh login...");
      if (loginAlphaBase()) {
        success = attemptAlphaBaseLog(event, reason, partCount, partNumber);
      }
    }
  }

  if (!success) {
    Serial.println("❌ All AlphaBase logging attempts failed");
  }
}