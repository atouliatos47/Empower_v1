#ifndef BUTTONS_LEDS_H
#define BUTTONS_LEDS_H

#include "config.h"
#include "state_machine.h"

extern unsigned long lastBlinkTime;
extern bool ledState;

void handleStartStopButton();
void handleMaintenanceButton();
void handleQualityButton();
void handleMaterialButton();
void handleToolChangeButton();
void updateLEDs();
void initializePins();

#endif