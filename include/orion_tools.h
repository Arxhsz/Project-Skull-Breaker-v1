#pragma once

#include <Arduino.h>

void orionToolsLoadPrefs();
void orionToolsPersistPrefs();

bool orionToolsWifiOpenOnly();
int8_t orionToolsBleMinRssi();
bool orionToolsRfCsvEnabled();

void orionToolsCycleWifiFilter();
void orionToolsCycleBleFilter();
void orionToolsCycleRfCsv();
void orionToolsCycleRssiAlert();

const char* orionToolsWifiFilterLabel();
const char* orionToolsBleFilterLabel();
const char* orionToolsRfCsvLabel();
const char* orionToolsRssiAlertLabel();

void orionToolsBookmarkSaveCurrent(float mhz);
bool orionToolsBookmarkApplyNext(float* outMhz);

bool orionToolsExportWiFiCsv();
bool orionToolsExportBleCsv();

void orionToolsServiceRfCsv(const char* modeTag, float mhz, int rssiDbm);
void orionToolsRssiToneTick(int rssiDbm);

void orionToolsDecayRfPeakHold(uint8_t* peakHold, const uint8_t* live, int count, uint8_t decayPerSweep);
uint8_t orionToolsCombinedLevel(uint8_t live, uint8_t peak);

void orionToolsLoadBleProfileFromSd();
bool orionToolsAppendFavoriteSubPath(const char* path);

bool orionToolsSdLowSpace();

void orionToolsFactoryResetConfirm();

const char* orionToolsResetReasonStr();
String orionToolsBuildInfoLine();
String orionToolsFlashLine();

bool orionToolsFactoryResetArmed();
