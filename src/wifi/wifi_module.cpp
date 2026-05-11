#include <Arduino.h>

#include "wifi_module.h"
#include "system_types.h"

extern RadioMode currentRadioMode;

void handleWiFiScannerInput();
void runWiFiScanner();
void runPacketMonitor();
void runWiFiBeacon();
void runProbeSniff();
void runBeaconSniff();
void runDeauthSniff();
void runEapolScan();
void runRawCapture();
void runStationSniff();
void runSignalMonitor();
void runChannelAnalyzer();
void runPacketCount();
void runScanAll();
void runEvilPortal();
void runRickRoll();
void runProbeFlood();
void runDeauthFlood();
void runBadMsg();
void runChannelSwitch();
void runQuiet();
void runAssocSleep();
void runAPCloneSpam();

void runHiddenSSIDReveal();

void wifi_serviceMode() {
    if (currentRadioMode == WIFI_SCAN) { handleWiFiScannerInput(); runWiFiScanner(); return; }
    if (currentRadioMode == WIFI_PACKET_MONITOR) { runPacketMonitor(); return; }
    if (currentRadioMode == WIFI_BEACON)          { runWiFiBeacon(); return; }
    if (currentRadioMode == WIFI_PROBE_SNIFF)     { runProbeSniff(); return; }
    if (currentRadioMode == WIFI_BEACON_SNIFF)    { runBeaconSniff(); return; }
    if (currentRadioMode == WIFI_DEAUTH_SNIFF)    { runDeauthSniff(); return; }
    if (currentRadioMode == WIFI_EAPOL_SCAN)      { runEapolScan(); return; }
    if (currentRadioMode == WIFI_RAW_CAPTURE)     { runRawCapture(); return; }
    if (currentRadioMode == WIFI_STATION_SNIFF)   { runStationSniff(); return; }
    if (currentRadioMode == WIFI_SIGNAL_MONITOR)  { runSignalMonitor(); return; }
    if (currentRadioMode == WIFI_CHANNEL_ANALYZER){ runChannelAnalyzer(); return; }
    if (currentRadioMode == WIFI_PACKET_COUNT)    { runPacketCount(); return; }
    if (currentRadioMode == WIFI_SCAN_ALL)        { runScanAll(); return; }
    if (currentRadioMode == WIFI_ATTACK_EVIL_PORTAL) { runEvilPortal(); return; }
    if (currentRadioMode == WIFI_ATTACK_RICKROLL)    { runRickRoll(); return; }
    if (currentRadioMode == WIFI_ATTACK_PROBE_FLOOD) { runProbeFlood(); return; }
    if (currentRadioMode == WIFI_ATTACK_DEAUTH_FLOOD){ runDeauthFlood(); return; }
    if (currentRadioMode == WIFI_ATTACK_BAD_MSG)     { runBadMsg(); return; }
    if (currentRadioMode == WIFI_ATTACK_CHANNEL_SWITCH) { runChannelSwitch(); return; }
    if (currentRadioMode == WIFI_ATTACK_QUIET)       { runQuiet(); return; }
    if (currentRadioMode == WIFI_ATTACK_ASSOC_SLEEP) { runAssocSleep(); return; }
    if (currentRadioMode == WIFI_ATTACK_AP_CLONE)    { runAPCloneSpam(); return; }
    if (currentRadioMode == WIFI_HIDDEN_SSID_REVEAL) { runHiddenSSIDReveal(); return; }
}
