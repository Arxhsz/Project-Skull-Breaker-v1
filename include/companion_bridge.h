#pragma once

#include <Arduino.h>

void companion_setup();
void companion_service();
int companion_readButtonState(int pin);
