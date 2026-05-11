#pragma once

#include <Arduino.h>

void cleanupBLEDevices(unsigned long nowMs);
void cleanupWiFiNetworks(unsigned long nowMs);
void serviceStaleScanData();
