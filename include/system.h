#pragma once

#include <Arduino.h>
#include "system_types.h"

void system_init();
void system_update();
void system_handleInput();

void resetAllToolAndUIState(bool clearLists = true);
void beginSPIOperation(bool suspendDisplay = true);
void endSPIOperation(bool restoreDisplay = true, bool heavyRestore = false);
void stopCurrentRadioMode(bool clearMode = true);
void enterMode(RadioMode mode);
void navigateBack();
void markCurrentScreenForRedraw();
bool serviceScreenSaver();
bool startWirelessUpdate();
void stopWirelessUpdate();
void serviceWirelessUpdateScreen();
String getScreenSaverTimeoutLabel();
const char* getScreenSaverStyleLabel();
String getTftBrightnessLabel();
