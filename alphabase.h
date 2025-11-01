#ifndef ALPHABASE_H
#define ALPHABASE_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

extern String authToken;

bool loginAlphaBase();
bool refreshAlphaBaseToken();
void discoverAlphaBaseCollections();
void sendStopEmailAlert(String reason, unsigned long runtime);
void sendStopTelegramAlert(String reason, unsigned long runtime);
void sendStopNotifications(String reason);
void logEventToAlphaBase(String event, String reason = "", int partCount = 0, String partNumber = "");

#endif