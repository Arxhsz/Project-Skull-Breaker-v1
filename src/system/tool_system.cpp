#include <Arduino.h>

#include "tool_system.h"

void run24GHzScannerStep();
void runNoiseAnalyzerStep();
void handleRFProtocols();
void runRFScanner();
void runRFMonitor();
void runRFJammer();
void runRFSignalCapture();
void runRFSweep();
void runRFTransmit();
void runRFReplay();
void runWiFiScanner();
void runPacketMonitor();
void runWiFiBeacon();
void runBLEScanner();
void runBLEBeaconSpammer();
void runBLEBeaconTest();
void runBLEStableDevice();
void runBLERadar();
void runBLELoggerPicker();
void runBLELogger();
void handleWiFiScannerInput();
void handleBLEScannerInput();
void handleBLELoggerPickerInput();

namespace {
const ToolDescriptor TOOL_REGISTRY[] = {
    {"2.4GHz Scanner", RADIO_24_SCAN, run24GHzScannerStep, nullptr},
    {"Noise Analyzer", RADIO_NOISE_ANALYZER, runNoiseAnalyzerStep, nullptr},
    {"2.4GHz Jammer", RADIO_24_ACTIVE, handleRFProtocols, nullptr},
    {"RF Scanner", RF_SCANNER, runRFScanner, nullptr},
    {"RF Monitor", RF_MONITOR, runRFMonitor, nullptr},
    {"RF Jammer", RF_JAMMER, runRFJammer, nullptr},
    {"Signal Capture", RF_SIGNAL_CAPTURE, runRFSignalCapture, nullptr},
    {"Frequency Sweep", RF_FREQUENCY_SWEEP, runRFSweep, nullptr},
    {"RF Transmit", RF_TRANSMIT, runRFTransmit, nullptr},
    {"RF Replay", RF_REPLAY, runRFReplay, nullptr},    {"WiFi Scanner", WIFI_SCAN, runWiFiScanner, handleWiFiScannerInput},
    {"Packet Monitor", WIFI_PACKET_MONITOR, runPacketMonitor, nullptr},
    {"WiFi Beacon", WIFI_BEACON, runWiFiBeacon, nullptr},
    {"BLE Scanner", BLE_SCAN, runBLEScanner, handleBLEScannerInput},
    {"BLE Beacon Spam", BLE_BEACON_SPAM, runBLEBeaconSpammer, nullptr},
    {"BLE Beacon Test", BLE_BEACON_TEST, runBLEBeaconTest, nullptr},
    {"BLE Device", BLE_DEVICE_STABLE, runBLEStableDevice, nullptr},
    {"BLE Radar", BLE_RADAR, runBLERadar, nullptr},
    {"BLE Logger Pick", BLE_SIGNAL_LOGGER_PICK, runBLELoggerPicker, handleBLELoggerPickerInput},
    {"BLE Signal Logger", BLE_SIGNAL_LOGGER, runBLELogger, nullptr},
};
}

const ToolDescriptor* findToolByMode(RadioMode mode) {
    for (const ToolDescriptor& tool : TOOL_REGISTRY) {
        if (tool.mode == mode) {
            return &tool;
        }
    }
    return nullptr;
}
