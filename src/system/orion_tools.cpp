#include <Arduino.h>
#include <SD.h>
#include <Preferences.h>
#include <esp_system.h>

#include "orion_tools.h"
#define LOG_SCOPE "SKULL"
#include "logging.h"

extern Preferences prefs;
extern bool sdMounted;
extern bool sdError;

extern char wifiSSIDs[][33];
extern char wifiBSSID[][18];
extern char wifiEncStr[][8];
extern int wifiRSSI[];
extern int wifiChannel[];
extern int wifiNetworkCount;

extern char bleNames[][32];
extern char bleMACs[][18];
extern int bleRSSI[];
extern int bleDeviceCount;

extern String bleBeaconName;

namespace {

bool g_wifiOpenOnly = false;
int8_t g_bleMinRssi = -100;
bool g_rfCsv = false;
uint8_t g_rssiAlert = 0;
uint32_t g_rfBkKhz[4] = {0};

uint32_t g_rfCsvLastMs = 0;
uint8_t g_rfBkSlot = 0;
uint8_t g_rfBkApply = 0;

unsigned long g_factoryArmUntil = 0;

#ifndef ORION_RSSI_TONE_PIN
#define ORION_RSSI_TONE_PIN (-1)
#endif

bool g_toneActive = false;
unsigned long g_toneUntil = 0;

bool ensureOrionDir() {
    if (!sdMounted || sdError) {
        return false;
    }
    if (!SD.exists("/skullbreaker")) {
        return SD.mkdir("/skullbreaker");
    }
    return true;
}

int rssiAlertThresholdDb() {
    switch (g_rssiAlert) {
        case 1:
            return -85;
        case 2:
            return -75;
        case 3:
            return -65;
        default:
            return 0;
    }
}

}  // namespace

void orionToolsLoadPrefs() {
    g_wifiOpenOnly = prefs.getBool("otWiOpen", false);
    g_bleMinRssi = (int8_t)prefs.getChar("otBleRssi", -100);
    g_rfCsv = prefs.getBool("otRfCsv", false);
    g_rssiAlert = prefs.getUChar("otRssiAl", 0);
    if (g_rssiAlert > 3) {
        g_rssiAlert = 0;
    }
    for (int i = 0; i < 4; i++) {
        char key[8];
        snprintf(key, sizeof(key), "otBk%d", i);
        g_rfBkKhz[i] = prefs.getUInt(key, 0);
    }
}

void orionToolsPersistPrefs() {
    prefs.putBool("otWiOpen", g_wifiOpenOnly);
    prefs.putChar("otBleRssi", (char)g_bleMinRssi);
    prefs.putBool("otRfCsv", g_rfCsv);
    prefs.putUChar("otRssiAl", g_rssiAlert);
    for (int i = 0; i < 4; i++) {
        char key[8];
        snprintf(key, sizeof(key), "otBk%d", i);
        prefs.putUInt(key, g_rfBkKhz[i]);
    }
}

bool orionToolsWifiOpenOnly() {
    return g_wifiOpenOnly;
}

int8_t orionToolsBleMinRssi() {
    return g_bleMinRssi;
}

bool orionToolsRfCsvEnabled() {
    return g_rfCsv;
}

void orionToolsCycleWifiFilter() {
    g_wifiOpenOnly = !g_wifiOpenOnly;
    orionToolsPersistPrefs();
}

void orionToolsCycleBleFilter() {
    if (g_bleMinRssi <= -100) {
        g_bleMinRssi = -80;
    } else if (g_bleMinRssi == -80) {
        g_bleMinRssi = -70;
    } else {
        g_bleMinRssi = -100;
    }
    orionToolsPersistPrefs();
}

void orionToolsCycleRfCsv() {
    g_rfCsv = !g_rfCsv;
    orionToolsPersistPrefs();
}

void orionToolsCycleRssiAlert() {
    g_rssiAlert = (g_rssiAlert + 1) % 4;
    orionToolsPersistPrefs();
}

const char* orionToolsWifiFilterLabel() {
    return g_wifiOpenOnly ? "Open" : "All";
}

const char* orionToolsBleFilterLabel() {
    if (g_bleMinRssi <= -100) {
        return "All";
    }
    if (g_bleMinRssi == -80) {
        return ">-80";
    }
    return ">-70";
}

const char* orionToolsRfCsvLabel() {
    return g_rfCsv ? "On" : "Off";
}

const char* orionToolsRssiAlertLabel() {
    switch (g_rssiAlert) {
        case 1:
            return "-85";
        case 2:
            return "-75";
        case 3:
            return "-65";
        default:
            return "Off";
    }
}

void orionToolsBookmarkSaveCurrent(float mhz) {
    if (mhz < 1.0f || mhz > 1000.0f) {
        return;
    }
    uint32_t khz = (uint32_t)(mhz * 1000.0f + 0.5f);
    g_rfBkKhz[g_rfBkSlot % 4] = khz;
    g_rfBkSlot = (g_rfBkSlot + 1) % 4;
    orionToolsPersistPrefs();
}

bool orionToolsBookmarkApplyNext(float* outMhz) {
    for (int tries = 0; tries < 4; tries++) {
        uint32_t k = g_rfBkKhz[g_rfBkApply % 4];
        g_rfBkApply = (g_rfBkApply + 1) % 4;
        if (k > 0) {
            *outMhz = k / 1000.0f;
            return true;
        }
    }
    return false;
}

static void appendCsvField(File& f, const String& s, bool last) {
    f.print('"');
    for (unsigned i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '"') {
            f.print("\"\"");
        } else {
            f.print(c);
        }
    }
    f.print('"');
    if (!last) {
        f.print(',');
    }
}

bool orionToolsExportWiFiCsv() {
    if (!sdMounted || sdError || wifiNetworkCount <= 0) {
        return false;
    }
    if (!ensureOrionDir()) {
        return false;
    }
    File f = SD.open("/skullbreaker/wifi_scan.csv", FILE_WRITE);
    if (!f) {
        return false;
    }
    f.println("ssid,bssid,channel,rssi_dbm,encryption");
    for (int i = 0; i < wifiNetworkCount; i++) {
        appendCsvField(f, String(wifiSSIDs[i]), false);
        appendCsvField(f, String(wifiBSSID[i]), false);
        f.print(wifiChannel[i]);
        f.print(',');
        f.print(wifiRSSI[i]);
        f.print(',');
        appendCsvField(f, String(wifiEncStr[i]), true);
        f.println();
    }
    f.close();
    return true;
}

bool orionToolsExportBleCsv() {
    if (!sdMounted || sdError || bleDeviceCount <= 0) {
        return false;
    }
    if (!ensureOrionDir()) {
        return false;
    }
    File f = SD.open("/skullbreaker/ble_scan.csv", FILE_WRITE);
    if (!f) {
        return false;
    }
    f.println("name,mac,rssi_dbm");
    for (int i = 0; i < bleDeviceCount; i++) {
        appendCsvField(f, String(bleNames[i]), false);
        appendCsvField(f, String(bleMACs[i]), false);
        f.println(bleRSSI[i]);
    }
    f.close();
    return true;
}

void orionToolsServiceRfCsv(const char* modeTag, float mhz, int rssiDbm) {
    if (!g_rfCsv || !sdMounted || sdError) {
        return;
    }
    uint32_t now = millis();
    if (g_rfCsvLastMs != 0 && (now - g_rfCsvLastMs) < 1000UL) {
        return;
    }
    g_rfCsvLastMs = now;
    if (!ensureOrionDir()) {
        return;
    }
    File f = SD.open("/skullbreaker/rf_trace.csv", FILE_APPEND);
    if (!f) {
        return;
    }
    if (f.size() == 0) {
        f.println("ms,mhz,rssi_dbm,mode");
    }
    f.print(now);
    f.print(',');
    f.print(mhz, 3);
    f.print(',');
    f.print(rssiDbm);
    f.print(',');
    f.println(modeTag);
    f.close();
}

void orionToolsRssiToneTick(int rssiDbm) {
#if ORION_RSSI_TONE_PIN >= 0
    int th = rssiAlertThresholdDb();
    if (th == 0) {
        if (g_toneActive) {
            noTone(ORION_RSSI_TONE_PIN);
            g_toneActive = false;
        }
        return;
    }
    bool above = rssiDbm >= th;
    uint32_t now = millis();
    if (above) {
        if (!g_toneActive && now >= g_toneUntil) {
            tone(ORION_RSSI_TONE_PIN, 2000, 60);
            g_toneActive = true;
            g_toneUntil = now + 200;
        }
    } else {
        if (g_toneActive && now >= g_toneUntil) {
            noTone(ORION_RSSI_TONE_PIN);
            g_toneActive = false;
        }
    }
#else
    (void)rssiDbm;
#endif
}

void orionToolsDecayRfPeakHold(uint8_t* peakHold, const uint8_t* live, int count, uint8_t decayPerSweep) {
    for (int i = 0; i < count; i++) {
        if (peakHold[i] > live[i]) {
            uint8_t d = peakHold[i] - decayPerSweep;
            peakHold[i] = (d > live[i]) ? d : live[i];
        } else {
            peakHold[i] = live[i];
        }
    }
}

uint8_t orionToolsCombinedLevel(uint8_t live, uint8_t peak) {
    return live > peak ? live : peak;
}

void orionToolsLoadBleProfileFromSd() {
    if (!sdMounted || sdError) {
        return;
    }
    File f = SD.open("/skullbreaker/beacon_profile.txt", FILE_READ);
    if (!f) {
        return;
    }
    String line = f.readStringUntil('\n');
    f.close();
    line.trim();
    if (line.length() > 0 && line.length() < 48) {
        bleBeaconName = line;
        LOG(String("BLE profile name from SD: ") + line);
    }
}

bool orionToolsAppendFavoriteSubPath(const char* path) {
    if (!path || !sdMounted || sdError) {
        return false;
    }
    if (!ensureOrionDir()) {
        return false;
    }
    String p(path);
    if (!p.endsWith(".sub") && !p.endsWith(".SUB")) {
        return false;
    }
    File f = SD.open("/skullbreaker/favorites.txt", FILE_APPEND);
    if (!f) {
        return false;
    }
    f.println(p);
    f.close();
    return true;
}

bool orionToolsSdLowSpace() {
    if (!sdMounted || sdError) {
        return false;
    }
    uint64_t tot = SD.totalBytes();
    uint64_t used = SD.usedBytes();
    if (tot == 0) {
        return false;
    }
    uint64_t freeB = tot - used;
    return freeB < (tot / 10);
}

void orionToolsFactoryResetConfirm() {
    uint32_t now = millis();
    if (g_factoryArmUntil != 0 && now < g_factoryArmUntil) {
        prefs.clear();
        g_factoryArmUntil = 0;
        delay(200);
        ESP.restart();
    } else {
        g_factoryArmUntil = now + 5000UL;
    }
}

bool orionToolsFactoryResetArmed() {
    return g_factoryArmUntil != 0 && millis() < g_factoryArmUntil;
}

const char* orionToolsResetReasonStr() {
    esp_reset_reason_t r = esp_reset_reason();
    switch (r) {
        case ESP_RST_POWERON:
            return "Power-on";
        case ESP_RST_SW:
            return "SW reset";
        case ESP_RST_PANIC:
            return "Panic/crash";
        case ESP_RST_INT_WDT:
            return "Int WDT";
        case ESP_RST_TASK_WDT:
            return "Task WDT";
        case ESP_RST_WDT:
            return "WDT";
        case ESP_RST_DEEPSLEEP:
            return "Deep sleep";
        case ESP_RST_BROWNOUT:
            return "Brownout";
        case ESP_RST_SDIO:
            return "SDIO";
        default:
            return "Unknown";
    }
}

#ifndef ORION_GIT_HASH
#define ORION_GIT_HASH unknown
#endif
#define ORION_STR2(x) #x
#define ORION_STR(x) ORION_STR2(x)

String orionToolsBuildInfoLine() {
    return String("Git ") + ORION_STR(ORION_GIT_HASH);
}

String orionToolsFlashLine() {
    return String("Flash ") + String(ESP.getFlashChipSize() / 1024) + " KiB  App " +
           String(ESP.getSketchSize() / 1024) + "/" + String(ESP.getFreeSketchSpace() / 1024) + " KiB";
}
