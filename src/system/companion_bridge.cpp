#include <Arduino.h>

#include "companion_bridge.h"
#include "system.h"
#include "system_types.h"
#include "orion_tools.h"
#define LOG_SCOPE "CMP"
#include "logging.h"

extern ScreenState currentScreen;
extern RadioMode currentRadioMode;
extern String currentTitle;

extern bool sdMounted;
extern bool sdError;
extern bool radio1Ok;
extern bool cc1101Ok;
extern bool radio3Ok;
extern bool diagnosticsOk;
extern uint8_t batteryPercent;
extern bool chargerConnected;

extern int totalItems;
extern int selectedIndex;

extern const char** currentMenu;
extern int currentMenuSize;
extern int currentMenuIndex;
extern int currentMenuScrollOffset;
extern const char* screenSaverMenu[];
extern const char* settingsMenu[];
extern const char* brightnessMenu[];

extern int bleDeviceCount;
extern int bleSelectedIndex;
extern int bleScrollOffset;
extern bool bleScanRunning;
extern String bleNames[];
extern String bleMACs[];
extern int bleRSSI[];
extern String bleLoggerTargetName;
extern String bleLoggerTargetMAC;
extern int bleLoggerCurrentRSSI;
extern int bleLoggerSampleIntervalMs;
extern int bleLoggerSamples[];
extern int bleLoggerSampleCount;
extern int bleLoggerWriteIndex;
extern int bleDetailIndex;

extern int wifiNetworkCount;
extern int wifiSelectedIndex;
extern int wifiScrollOffset;
extern bool wifiScanRunning;
extern int wifiDetailIndex;
extern String wifiSSIDs[];
extern int wifiRSSI[];
extern int wifiChannel[];
extern String wifiEncStr[];
extern String wifiBSSID[];

extern String fileManagerEntries[];
extern bool fileManagerIsDir[];
extern bool fileManagerMarked[];
extern uint32_t fileManagerSizes[];
extern int fileManagerCount;
extern int fileManagerSelectedIndex;
extern int fileManagerScrollOffset;
extern String fileManagerPath;
extern String fileDetailPath;
extern String fileDetailSavedText;
extern String fileRenameDraft;
extern int fileRenameCursor;
extern bool fileRenameUppercase;

extern float rfLockedFrequencyMHz;
extern bool rfSweepPaused;
extern float rfSweepStrongestMHz;
extern int rfSweepStrongestRSSI;
extern uint8_t rfSweepLevels[];
extern float rfDisplayRSSI;
extern float rfDisplayAverage;
extern float rfDisplayPeak;
extern int rfCurrentRSSI;
extern int rfMonitorHistory[];
extern int rfMonitorHistoryIndex;
extern int rfMonitorHistoryCount;
extern bool rfCaptureRecording;
extern bool rfCaptureHasRecording;
extern int rfCaptureThreshold;
extern int rfCapturePeakHits;
extern int rfCaptureHistory[];
extern int rfCaptureHistoryIndex;
extern int rfCaptureHistoryCount;
extern bool rfTransmitActive;
extern uint32_t rfTransmitCount;
extern bool rfSubLoaded;
extern String rfSubLoadedName;
extern String rfTransmitStatus;
extern uint32_t rfSubFrequencyHz;

extern String wirelessUpdateSSID;
extern String wirelessUpdateIP;
extern String wirelessUpdateStatus;

extern const char* getScreenSaverStyleLabel();
extern String getScreenSaverTimeoutLabel();
String rfSweepBandLabel();
String rfBandLabel();

namespace {

constexpr uint8_t kButtonCount = 5;
constexpr unsigned long kPulseMs = 140UL;
constexpr unsigned long kSnapshotIntervalMs = 180UL;
constexpr int kVisibleRows = 6;

constexpr uint8_t kButtonPins[kButtonCount] = {32, 33, 26, 27, 25};
constexpr const char* const kButtonNames[kButtonCount] = {"UP", "DOWN", "LEFT", "RIGHT", "SELECT"};

String* g_inputLine = nullptr;
bool g_companionActive = false;
unsigned long g_lastSnapshotMs = 0;
uint8_t g_buttonHeldMask = 0;
unsigned long g_buttonPulseUntil[kButtonCount] = {0};

constexpr const char* const mainLabels[] = {
    "WiFi",
    "Bluetooth",
    "2.4GHz",
    "RF",
    "Settings",
    "Files"
};

int findButtonIndexByPin(int pin) {
    for (int i = 0; i < kButtonCount; i++) {
        if (kButtonPins[i] == pin) {
            return i;
        }
    }
    return -1;
}

int findButtonIndexByName(const String& rawName) {
    String name = rawName;
    name.trim();
    name.toUpperCase();
    for (int i = 0; i < kButtonCount; i++) {
        if (name == kButtonNames[i]) {
            return i;
        }
    }
    return -1;
}

bool virtualButtonDown(int pin) {
    int index = findButtonIndexByPin(pin);
    if (index < 0) {
        return false;
    }
    const unsigned long now = millis();
    if ((g_buttonHeldMask & (1U << index)) != 0) {
        return true;
    }
    if (g_buttonPulseUntil[index] != 0 && now < g_buttonPulseUntil[index]) {
        return true;
    }
    if (g_buttonPulseUntil[index] != 0 && now >= g_buttonPulseUntil[index]) {
        g_buttonPulseUntil[index] = 0;
    }
    return false;
}

const char* screenLabel(ScreenState screen) {
    switch (screen) {
        case SCREEN_MAIN: return "MAIN";
        case SCREEN_SUBMENU: return "SUBMENU";
        case SCREEN_TOOL: return "TOOL";
        case SCREEN_SYSTEM_INFO: return "SYSTEM_INFO";
        case SCREEN_WIRELESS_UPDATE: return "WIRELESS_UPDATE";
        case SCREEN_SD_FORMAT_CONFIRM: return "SD_FORMAT";
        case SCREEN_FILE_MANAGER: return "FILE_MANAGER";
        case SCREEN_FILE_DETAIL: return "FILE_DETAIL";
        case SCREEN_FILE_DELETE_CONFIRM: return "FILE_DELETE";
        case SCREEN_FILE_RENAME: return "FILE_RENAME";
        case SCREEN_WIFI_DETAIL: return "WIFI_DETAIL";
        case SCREEN_BLE_DETAIL: return "BLE_DETAIL";
        default: return "UNKNOWN";
    }
}

const char* modeLabel(RadioMode mode) {
    switch (mode) {
        case RADIO_IDLE: return "IDLE";
        case RADIO_24_SCAN: return "2.4GHz Scanner";
        case RADIO_NOISE_ANALYZER: return "Noise Analyzer";
        case RADIO_24_ACTIVE: return "2.4GHz Jammer";
        case RF_SCANNER: return "RF Scanner";
        case RF_MONITOR: return "RF Monitor";
        case RF_JAMMER: return "RF Jammer";
        case RF_SQUELCH_ACTIVATE: return "Squelch Open";
        case RF_SIGNAL_CAPTURE: return "Signal Capture";
        case RF_FREQUENCY_SWEEP: return "Frequency Sweep";
        case RF_TRANSMIT: return "RF Transmit";
        case WIFI_SCAN: return "WiFi Scanner";
        case WIFI_PACKET_MONITOR: return "Packet Monitor";
        case WIFI_BEACON: return "Beacon Spammer";
        case WIFI_DETECTOR: return "WiFi Detector";
        case BLE_SCAN: return "BLE Scanner";
        case BLE_BEACON_SPAM: return "BLE Swarm";
        case BLE_BEACON_TEST: return "BLE Beacon Test";
        case BLE_DEVICE_STABLE: return "BLE Device";
        case BLE_RADAR: return "BLE Radar";
        case BLE_SIGNAL_LOGGER_PICK: return "Signal Logger";
        case BLE_SIGNAL_LOGGER: return "Signal Logger";
        default: return "Unknown";
    }
}

const char* sdStateLabel() {
    if (sdError) return "error";
    return sdMounted ? "mounted" : "idle";
}

void appendEscaped(String& out, const String& value) {
    for (size_t i = 0; i < value.length(); i++) {
        char c = value[i];
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': break;
            case '\t': out += " "; break;
            default:
                if (static_cast<unsigned char>(c) < 32) {
                    out += ' ';
                } else {
                    out += c;
                }
                break;
        }
    }
}

void appendQuoted(String& out, const String& value) {
    out += '"';
    appendEscaped(out, value);
    out += '"';
}

void appendQuoted(String& out, const char* value) {
    out += '"';
    if (value != nullptr) {
        while (*value != '\0') {
            char c = *value++;
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': break;
                case '\t': out += " "; break;
                default:
                    if (static_cast<unsigned char>(c) < 32) {
                        out += ' ';
                    } else {
                        out += c;
                    }
                    break;
            }
        }
    }
    out += '"';
}

void appendVisibleMenu(String& out) {
    out += ",\"menu\":{";
    out += "\"selected\":";
    out += String(currentMenuIndex);
    out += ",\"scroll\":";
    out += String(currentMenuScrollOffset);
    out += ",\"items\":[";
    for (int i = 0; i < currentMenuSize; i++) {
        if (i > 0) out += ',';
        appendQuoted(out, currentMenu[i]);
    }
    out += "]";

    if (currentMenu == screenSaverMenu) {
        out += ",\"values\":[";
        appendQuoted(out, getScreenSaverTimeoutLabel());
        out += ",";
        appendQuoted(out, getScreenSaverStyleLabel());
        out += ",\"\"";
        out += "]";
    } else if (currentMenu == brightnessMenu) {
        out += ",\"values\":[";
        appendQuoted(out, getTftBrightnessLabel());
        out += ",\"\"";
        out += "]";
    } else if (currentMenu == settingsMenu) {
        out += ",\"values\":[\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"]";
    }

    out += "}";
}

void appendVisibleFileList(String& out) {
    out += ",\"files\":{";
    out += "\"path\":";
    appendQuoted(out, fileManagerPath);
    out += ",\"count\":";
    out += String(fileManagerCount);
    out += ",\"total\":";
    out += String(fileManagerCount);
    out += ",\"selected\":";
    out += String(fileManagerSelectedIndex);
    out += ",\"scroll\":";
    out += String(fileManagerScrollOffset);
    out += ",\"items\":[";

    int start = max(0, fileManagerScrollOffset);
    int end = min(fileManagerCount, start + kVisibleRows);
    for (int i = start; i < end; i++) {
        if (i > start) out += ',';
        out += "{\"name\":";
        appendQuoted(out, fileManagerEntries[i]);
        out += ",\"dir\":";
        out += (fileManagerIsDir[i] ? "true" : "false");
        out += ",\"marked\":";
        out += (fileManagerMarked[i] ? "true" : "false");
        out += ",\"size\":";
        out += String(fileManagerSizes[i]);
        out += "}";
    }
    out += "]}";
}

void appendBleList(String& out, bool includeScanAgain) {
    out += ",\"tool\":{";
    out += "\"type\":\"device_list\",";
    out += "\"name\":";
    appendQuoted(out, modeLabel(currentRadioMode));
    out += ",\"scanning\":";
    out += (bleScanRunning ? "true" : "false");
    out += ",\"total\":";
    out += String(bleDeviceCount + (includeScanAgain ? 1 : 0));
    out += ",\"selected\":";
    out += String(bleSelectedIndex);
    out += ",\"scroll\":";
    out += String(bleScrollOffset);
    out += ",\"items\":[";

    int total = bleDeviceCount + (includeScanAgain ? 1 : 0);
    int start = max(0, bleScrollOffset);
    int end = min(total, start + kVisibleRows);
    for (int i = start; i < end; i++) {
        if (i > start) out += ',';
        out += "{\"name\":";
        if (includeScanAgain && i == bleDeviceCount) {
            appendQuoted(out, "Scan Again");
            out += ",\"rssi\":0,\"meta\":\"action\"";
        } else {
            appendQuoted(out, bleNames[i].length() > 0 ? bleNames[i] : bleMACs[i]);
            out += ",\"rssi\":";
            out += String(bleRSSI[i]);
            out += ",\"meta\":";
            appendQuoted(out, bleMACs[i]);
        }
        out += "}";
    }
    out += "]}";
}

void appendWiFiList(String& out) {
    out += ",\"tool\":{";
    out += "\"type\":\"wifi_list\",";
    out += "\"name\":";
    appendQuoted(out, modeLabel(currentRadioMode));
    out += ",\"scanning\":";
    out += (wifiScanRunning ? "true" : "false");
    out += ",\"total\":";
    out += String(wifiNetworkCount + 1);
    out += ",\"selected\":";
    out += String(wifiSelectedIndex);
    out += ",\"scroll\":";
    out += String(wifiScrollOffset);
    out += ",\"items\":[";

    int total = wifiNetworkCount + 1;
    int start = max(0, wifiScrollOffset);
    int end = min(total, start + kVisibleRows);
    for (int i = start; i < end; i++) {
        if (i > start) out += ',';
        out += "{\"name\":";
        if (i == wifiNetworkCount) {
            appendQuoted(out, "Scan Again");
            out += ",\"rssi\":0,\"meta\":\"action\"";
        } else {
            appendQuoted(out, wifiSSIDs[i].length() > 0 ? wifiSSIDs[i] : "<hidden>");
            out += ",\"rssi\":";
            out += String(wifiRSSI[i]);
            out += ",\"meta\":";
            appendQuoted(out, String("CH ") + String(wifiChannel[i]));
        }
        out += "}";
    }
    out += "]}";
}

void appendBleLogger(String& out) {
    out += ",\"tool\":{";
    out += "\"type\":\"logger\",";
    out += "\"name\":\"Signal Logger\",";
    out += "\"target\":";
    appendQuoted(out, bleLoggerTargetName.length() > 0 ? bleLoggerTargetName : "No target");
    out += ",\"mac\":";
    appendQuoted(out, bleLoggerTargetMAC);
    out += ",\"rssi\":";
    out += String(bleLoggerCurrentRSSI);
    out += ",\"rateMs\":";
    out += String(bleLoggerSampleIntervalMs);
    out += ",\"graph\":[";
    if (bleLoggerSampleCount > 0) {
        int oldest = (bleLoggerWriteIndex - bleLoggerSampleCount + 120) % 120;
        for (int i = 0; i < bleLoggerSampleCount; i++) {
            if (i > 0) out += ',';
            int idx = (oldest + i) % 120;
            out += String(bleLoggerSamples[idx]);
        }
    }
    out += "]}";
}

void appendWiFiDetail(String& out) {
    out += ",\"detailInfo\":{";
    if (wifiDetailIndex >= 0 && wifiDetailIndex < wifiNetworkCount) {
        out += "\"name\":";
        appendQuoted(out, wifiSSIDs[wifiDetailIndex].length() > 0 ? wifiSSIDs[wifiDetailIndex] : "<hidden>");
        out += ",\"rssi\":";
        out += String(wifiRSSI[wifiDetailIndex]);
        out += ",\"channel\":";
        out += String(wifiChannel[wifiDetailIndex]);
        out += ",\"security\":";
        appendQuoted(out, wifiEncStr[wifiDetailIndex]);
        out += ",\"mac\":";
        appendQuoted(out, wifiBSSID[wifiDetailIndex]);
    }
    out += "}";
}

void appendBleDetail(String& out) {
    out += ",\"detailInfo\":{";
    if (bleDetailIndex >= 0 && bleDetailIndex < bleDeviceCount) {
        out += "\"name\":";
        appendQuoted(out, bleNames[bleDetailIndex].length() > 0 ? bleNames[bleDetailIndex] : "Unknown");
        out += ",\"rssi\":";
        out += String(bleRSSI[bleDetailIndex]);
        out += ",\"mac\":";
        appendQuoted(out, bleMACs[bleDetailIndex]);
    }
    out += "}";
}

void appendRfSweep(String& out) {
    out += ",\"tool\":{";
    out += "\"type\":\"rf_sweep\",";
    out += "\"name\":\"Frequency Sweep\",";
    out += "\"band\":";
    appendQuoted(out, rfSweepBandLabel());
    out += ",\"paused\":";
    out += (rfSweepPaused ? "true" : "false");
    out += ",\"tunedMHz\":";
    out += String(rfLockedFrequencyMHz, 2);
    out += ",\"peakMHz\":";
    out += String(rfSweepStrongestMHz, 2);
    out += ",\"peakRssi\":";
    out += String(rfSweepStrongestRSSI);
    out += ",\"levels\":[";
    for (int i = 0; i < 96; i++) {
        if (i > 0) out += ',';
        out += String(rfSweepLevels[i]);
    }
    out += "]}";
}

void appendHistorySamples(String& out, const int* values, int count, int writeIndex, int bufferSize, int outputCount, int minValue, int maxValue) {
    out += '[';
    int actualCount = min(count, outputCount);
    for (int i = 0; i < actualCount; i++) {
        if (i > 0) out += ',';
        int historyIndex = (writeIndex - actualCount + i);
        while (historyIndex < 0) historyIndex += bufferSize;
        historyIndex %= bufferSize;
        int value = values[historyIndex];
        value = constrain(value, minValue, maxValue);
        out += String(value);
    }
    out += ']';
}

void appendRfMonitor(String& out) {
    out += ",\"tool\":{";
    out += "\"type\":\"rf_monitor\",";
    out += "\"band\":";
    appendQuoted(out, rfBandLabel());
    out += ",\"freqMHz\":";
    out += String(rfLockedFrequencyMHz, 2);
    out += ",\"rssi\":";
    out += String((int)roundf(rfDisplayRSSI));
    out += ",\"avg\":";
    out += String((int)roundf(rfDisplayAverage));
    out += ",\"peak\":";
    out += String((int)roundf(rfDisplayPeak));
    out += ",\"graph\":";
    appendHistorySamples(out, rfMonitorHistory, rfMonitorHistoryCount, rfMonitorHistoryIndex, 96, 64, -120, -20);
    out += "}";
}

void appendRfCapture(String& out) {
    out += ",\"tool\":{";
    out += "\"type\":\"rf_capture\",";
    out += "\"band\":";
    appendQuoted(out, rfBandLabel());
    out += ",\"freqMHz\":";
    out += String(rfLockedFrequencyMHz, 2);
    out += ",\"threshold\":";
    out += String(rfCaptureThreshold);
    out += ",\"rssi\":";
    out += String(rfCurrentRSSI);
    out += ",\"recording\":";
    out += (rfCaptureRecording ? "true" : "false");
    out += ",\"hasRecording\":";
    out += (rfCaptureHasRecording ? "true" : "false");
    out += ",\"peakPerSec\":";
    out += String(rfCapturePeakHits);
    out += ",\"graph\":";
    appendHistorySamples(out, rfCaptureHistory, rfCaptureHistoryCount, rfCaptureHistoryIndex, 96, 64, 0, 1);
    out += "}";
}

void appendRfTransmit(String& out) {
    out += ",\"tool\":{";
    out += "\"type\":\"rf_transmit\",";
    out += "\"band\":";
    appendQuoted(out, rfBandLabel());
    out += ",\"freqMHz\":";
    out += String(rfLockedFrequencyMHz, 2);
    out += ",\"active\":";
    out += (rfTransmitActive ? "true" : "false");
    out += ",\"loaded\":";
    out += (rfSubLoaded ? "true" : "false");
    out += ",\"count\":";
    out += String(rfTransmitCount);
    out += ",\"status\":";
    appendQuoted(out, rfTransmitStatus);
    out += ",\"source\":";
    appendQuoted(out, rfSubLoaded ? rfSubLoadedName : String("CC1101 burst packet output"));
    out += ",\"fileFreqMHz\":";
    out += String(rfSubFrequencyHz / 1000000.0f, 2);
    out += "}";
}

void appendWirelessUpdate(String& out) {
    out += ",\"wireless\":{";
    out += "\"status\":";
    appendQuoted(out, wirelessUpdateStatus);
    out += ",\"ssid\":";
    appendQuoted(out, wirelessUpdateSSID);
    out += ",\"ip\":";
    appendQuoted(out, wirelessUpdateIP);
    out += "}";
}

void appendToolSummary(String& out) {
    out += ",\"tool\":{";
    out += "\"type\":\"mode\",";
    out += "\"name\":";
    appendQuoted(out, modeLabel(currentRadioMode));
    out += ",\"summary\":";
    appendQuoted(out, String("Mode ") + modeLabel(currentRadioMode));
    out += "}";
}

void sendSnapshot() {
    if (!g_companionActive) {
        return;
    }

    String out;
    out.reserve(4200);
    out = "@CMP {\"type\":\"snapshot\",\"screen\":";
    appendQuoted(out, screenLabel(currentScreen));
    out += ",\"mode\":";
    appendQuoted(out, modeLabel(currentRadioMode));
    out += ",\"title\":";
    appendQuoted(out, currentTitle);
    out += ",\"status\":{";
    out += "\"sd\":";
    appendQuoted(out, sdStateLabel());
    out += ",\"r1\":";
    out += (radio1Ok ? "true" : "false");
    out += ",\"r2\":";
    out += (cc1101Ok ? "true" : "false");
    out += ",\"r3\":";
    out += (radio3Ok ? "true" : "false");
    out += ",\"diag\":";
    out += (diagnosticsOk ? "true" : "false");
    out += ",\"batteryPct\":";
    out += String(batteryPercent);
    out += ",\"batteryCharging\":";
    out += (chargerConnected ? "true" : "false");
    out += "}";

    out += ",\"main\":{\"selected\":";
    out += String(selectedIndex);
    out += ",\"items\":[";
    for (int i = 0; i < totalItems; i++) {
        if (i > 0) out += ',';
        appendQuoted(out, mainLabels[i]);
    }
    out += "]}";

    if (currentMenu != nullptr && currentMenuSize > 0) {
        appendVisibleMenu(out);
    }

    if (currentScreen == SCREEN_WIRELESS_UPDATE) {
        appendWirelessUpdate(out);
    }

    if (currentScreen == SCREEN_WIFI_DETAIL) {
        appendWiFiDetail(out);
    } else if (currentScreen == SCREEN_BLE_DETAIL) {
        appendBleDetail(out);
    }

    if (currentScreen == SCREEN_FILE_MANAGER ||
        currentScreen == SCREEN_FILE_DETAIL ||
        currentScreen == SCREEN_FILE_DELETE_CONFIRM ||
        currentScreen == SCREEN_FILE_RENAME) {
        appendVisibleFileList(out);
    }

    if (currentScreen == SCREEN_FILE_DETAIL) {
        out += ",\"detail\":{\"path\":";
        appendQuoted(out, fileDetailPath);
        out += ",\"saved\":";
        appendQuoted(out, fileDetailSavedText);
        out += "}";
    } else if (currentScreen == SCREEN_FILE_RENAME) {
        out += ",\"rename\":{\"draft\":";
        appendQuoted(out, fileRenameDraft);
        out += ",\"cursor\":";
        out += String(fileRenameCursor);
        out += ",\"uppercase\":";
        out += (fileRenameUppercase ? "true" : "false");
        out += "}";
    }

    if (currentScreen == SCREEN_TOOL) {
        switch (currentRadioMode) {
            case BLE_SCAN:
                appendBleList(out, false);
                break;
            case BLE_SIGNAL_LOGGER_PICK:
                appendBleList(out, true);
                break;
            case BLE_SIGNAL_LOGGER:
                appendBleLogger(out);
                break;
            case WIFI_SCAN:
                appendWiFiList(out);
                break;
            case RF_FREQUENCY_SWEEP:
                appendRfSweep(out);
                break;
            case RF_MONITOR:
                appendRfMonitor(out);
                break;
            case RF_SIGNAL_CAPTURE:
                appendRfCapture(out);
                break;
            case RF_TRANSMIT:
                appendRfTransmit(out);
                break;
            default:
                appendToolSummary(out);
                break;
        }
    }

    out += "}";
    Serial.println(out);
}

void sendHello() {
    Serial.println("@CMP {\"type\":\"hello\",\"device\":\"Skull Breaker\",\"protocol\":1}");
}

void handleCommand(const String& command) {
    if (!command.startsWith("@CMP ")) {
        return;
    }

    String payload = command.substring(5);
    payload.trim();
    if (payload.length() == 0) {
        return;
    }

    g_companionActive = true;

    if (payload == "HELLO") {
        sendHello();
        sendSnapshot();
        return;
    }

    if (payload == "SNAPSHOT" || payload == "PING") {
        sendSnapshot();
        return;
    }

    int firstSpace = payload.indexOf(' ');
    String verb = firstSpace >= 0 ? payload.substring(0, firstSpace) : payload;
    String arg = firstSpace >= 0 ? payload.substring(firstSpace + 1) : "";
    verb.toUpperCase();
    arg.trim();

    int buttonIndex = findButtonIndexByName(arg);
    if (buttonIndex < 0) {
        return;
    }

    if (verb == "TAP") {
        g_buttonPulseUntil[buttonIndex] = millis() + kPulseMs;
        return;
    }
    if (verb == "DOWN") {
        g_buttonHeldMask |= (1U << buttonIndex);
        g_buttonPulseUntil[buttonIndex] = 0;
        return;
    }
    if (verb == "UP") {
        g_buttonHeldMask &= ~(1U << buttonIndex);
        g_buttonPulseUntil[buttonIndex] = 0;
        return;
    }
}

void serviceInput() {
    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (g_inputLine != nullptr && g_inputLine->length() > 0) {
                handleCommand(*g_inputLine);
                *g_inputLine = "";
            }
            continue;
        }
        if (g_inputLine != nullptr && g_inputLine->length() < 96) {
            *g_inputLine += c;
        }
    }
}

}  // namespace

void companion_setup() {
    if (g_inputLine == nullptr) {
        g_inputLine = new String();
        if (g_inputLine != nullptr) {
            g_inputLine->reserve(96);
        }
    }
}

void companion_service() {
    serviceInput();
    if (!g_companionActive) {
        return;
    }
    const unsigned long now = millis();
    if (g_lastSnapshotMs == 0 || now - g_lastSnapshotMs >= kSnapshotIntervalMs) {
        g_lastSnapshotMs = now;
        sendSnapshot();
    }
}

int companion_readButtonState(int pin) {
    if (virtualButtonDown(pin)) {
        return LOW;
    }
    return digitalRead(pin);
}
