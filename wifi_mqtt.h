#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern String authToken;

void connectToWiFi();
void connectMQTT();
void publishStatusMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);

#endif