#include <Arduino.h>
#include <algorithm>

#include <NimBLEDevice.h>
#include <Preferences.h>
#include <RF24.h>

#include "ble.h"
#include "event_system.h"
#include "rf.h"
#include "scan_cleanup.h"
#include "storage.h"
#include "system.h"
#include "system_types.h"
#include "tool_system.h"
#include "ui.h"
#include "companion_bridge.h"
#include "wifi_module.h"
#define LOG_SCOPE "CTL"
#include "logging.h"

extern ScreenState currentScreen;
extern RadioMode currentRadioMode;
extern unsigned long toolScreenEnteredAt;

extern bool bleNeedsRedraw;
extern bool bleScanRunning;
extern bool bleDetailActive;
extern bool bleLoggerNeedsRedraw;
extern bool bleScannerFirstDraw;
extern int bleSelectedIndex;
extern int bleScrollOffset;
extern int bleLoggerTargetIndex;

extern bool wifiNeedsRedraw;
extern bool wifiDetailActive;

extern bool fileManagerNeedsRedraw;
extern bool rfTransmitReturnToFileManager;

extern TaskHandle_t bleScanTaskHandle;

bool isPressed(int pin);
void handleInput();
void handleSubMenuInput();
void handleToolInput();
void handleSDFormatConfirmInput();
void handleFileManagerInput();
void handleFileDetailInput();
void handleFileDeleteConfirmInput();
void handleFileRenameInput();
void handleEPRenameInput();
void drawStatusBar();
void drawWiFiDetail();
void updateWiFiDetailLive();
void drawBLEDetail();
void drawWirelessUpdateScreen();
void drawStorageSettingsScreen();
void drawFileManager();
void drawSubMenu();
void loadFileManagerEntries();
void resetWiFi();
void restoreDisplayAfterStorageAccess();
void serviceBatteryMonitor();

String getFileManagerParentPath(const String& path);
int findFileManagerIndexByPath(const String& path);

extern String currentTitle;
extern const char** currentMenu;
extern int currentMenuSize;
extern int currentMenuIndex;
extern String fileManagerPath;
extern String rfSubLoadedPath;

extern bool shouldRedraw;
extern bool bleScannerFirstDraw;

static bool toolBackAwaitRelease = false;

void system_init() {
    shouldRedraw = true;
}

void system_handleInput() {
    if (currentScreen == SCREEN_MAIN) {
        handleInput();
        return;
    }

    if (currentScreen == SCREEN_SUBMENU) {
        handleSubMenuInput();
        if (currentScreen == SCREEN_TOOL && toolScreenEnteredAt == 0) {
            toolScreenEnteredAt = millis();
        }
        return;
    }

    if (currentScreen == SCREEN_WIFI_DETAIL) {
        if (isPressed(26)) {
            navigateBack();
            delay(150);
            return;
        }
        // Draw once on entry, then only update live RSSI every 500ms
        extern bool wifiDetailDrawn;        static unsigned long wifiDetailLastUpdate = 0;
        if (!wifiDetailDrawn) {
            drawWiFiDetail();
            wifiDetailDrawn = true;
            wifiDetailLastUpdate = millis();
        } else if (millis() - wifiDetailLastUpdate > 500) {
            wifiDetailLastUpdate = millis();
            updateWiFiDetailLive();
        }
        return;
    }

    if (currentScreen == SCREEN_BLE_DETAIL) {
        if (isPressed(26)) {
            navigateBack();
            delay(150);
            return;
        }
        drawBLEDetail();
        return;
    }

    if (currentScreen == SCREEN_SYSTEM_INFO) {
        if (isPressed(26)) {
            navigateBack();
            delay(150);
        }
        return;
    }

    if (currentScreen == SCREEN_STORAGE_SETTINGS) {
        if (isPressed(26)) {
            navigateBack();
            delay(150);
            return;
        }
        drawStorageSettingsScreen();
        return;
    }

    if (currentScreen == SCREEN_WIRELESS_UPDATE) {
        if (isPressed(26)) {
            navigateBack();
            delay(150);
            return;
        }
        serviceWirelessUpdateScreen();
        return;
    }

    if (currentScreen == SCREEN_SD_FORMAT_CONFIRM) {
        handleSDFormatConfirmInput();
        return;
    }

    if (currentScreen == SCREEN_FILE_MANAGER) {
        handleFileManagerInput();
        if (currentScreen == SCREEN_FILE_MANAGER) {
            drawFileManager();
        }
        return;
    }

    if (currentScreen == SCREEN_FILE_DETAIL) {
        handleFileDetailInput();
        return;
    }

    if (currentScreen == SCREEN_FILE_DELETE_CONFIRM) {
        handleFileDeleteConfirmInput();
        return;
    }

    if (currentScreen == SCREEN_FILE_RENAME) {
        handleFileRenameInput();
        return;
    }

    if (currentScreen == SCREEN_EP_RENAME) {
        handleEPRenameInput();
        return;
    }

    if (currentScreen != SCREEN_TOOL) {
        toolScreenEnteredAt = 0;
        toolBackAwaitRelease = false;
    }
}

static void serviceToolScreen() {
    if (toolScreenEnteredAt == 0) {
        toolScreenEnteredAt = millis();
    }

    bool rawBackDown = (companion_readButtonState(26) == LOW);
    if (toolBackAwaitRelease) {
        if (!rawBackDown) {
            toolBackAwaitRelease = false;
        }
    }

    bool backPressed = (currentRadioMode == RADIO_24_ACTIVE ||
                        currentRadioMode == BLE_RADAR ||
                        currentRadioMode == BLE_SIGNAL_LOGGER ||
                        currentRadioMode == BLE_SIGNAL_LOGGER_PICK)
        ? rawBackDown
        : isPressed(26);

    if (toolBackAwaitRelease) {
        backPressed = false;
    }

    if (millis() - toolScreenEnteredAt > 140 &&
        currentRadioMode == BLE_SIGNAL_LOGGER &&
        backPressed) {
        currentRadioMode = BLE_SIGNAL_LOGGER_PICK;
        bleScannerFirstDraw = true;
        bleNeedsRedraw = true;
        bleSelectedIndex = std::max(0, bleLoggerTargetIndex);
        bleScrollOffset = 0;
        toolScreenEnteredAt = millis();
        toolBackAwaitRelease = true;
        drawStatusBar();
        return;
    }

    if (millis() - toolScreenEnteredAt > 180 && backPressed) {
        // Suppress global back when RIGHT is held (hold-mode tuning)
        // companion_readButtonState index for RIGHT button
        if (currentRadioMode == RF_ROLLING_CAPTURE) {
            // Check if RIGHT is held — if so, LEFT is for tuning not back
            extern bool rfRollingHoldMode;
            if (rfRollingHoldMode) backPressed = false;
        }
    }

    if (millis() - toolScreenEnteredAt > 180 && backPressed) {
        LOG("GLOBAL BACK");
        navigateBack();
        toolScreenEnteredAt = 0;
        delay(150);
        return;
    }

    handleToolInput();

    rf_serviceMode();
    wifi_serviceMode();
    ble_serviceMode();

    if (currentRadioMode == RF_FREQUENCY_SWEEP) {
        delay(2);
    } else if (currentRadioMode == RF_TRANSMIT) {
        delay(4);
    } else if (currentRadioMode == WIFI_SCAN ||
               currentRadioMode == WIFI_PACKET_MONITOR ||
               currentRadioMode == WIFI_BEACON ||
               currentRadioMode == WIFI_PROBE_SNIFF ||
               currentRadioMode == WIFI_BEACON_SNIFF ||
               currentRadioMode == WIFI_DEAUTH_SNIFF ||
               currentRadioMode == WIFI_EAPOL_SCAN ||
               currentRadioMode == WIFI_RAW_CAPTURE ||
               currentRadioMode == WIFI_STATION_SNIFF ||
               currentRadioMode == WIFI_SIGNAL_MONITOR ||
               currentRadioMode == WIFI_CHANNEL_ANALYZER ||
               currentRadioMode == WIFI_PACKET_COUNT ||
               currentRadioMode == WIFI_SCAN_ALL ||
               currentRadioMode == WIFI_ATTACK_EVIL_PORTAL ||
               currentRadioMode == WIFI_ATTACK_RICKROLL ||
               currentRadioMode == WIFI_ATTACK_PROBE_FLOOD ||
               currentRadioMode == WIFI_ATTACK_DEAUTH_FLOOD ||
               currentRadioMode == WIFI_ATTACK_BAD_MSG ||
               currentRadioMode == WIFI_ATTACK_CHANNEL_SWITCH ||
               currentRadioMode == WIFI_ATTACK_QUIET ||
               currentRadioMode == WIFI_ATTACK_ASSOC_SLEEP ||
               currentRadioMode == WIFI_ATTACK_AP_CLONE) {
        return;
    } else if (currentRadioMode == RADIO_24_ACTIVE) {
        delay(10);
    } else if (currentRadioMode == RADIO_24_SCAN ||
               currentRadioMode == RADIO_NOISE_ANALYZER ||
               currentRadioMode == BLE_BEACON_SPAM ||
               currentRadioMode == BLE_BEACON_TEST ||
               currentRadioMode == BLE_DEVICE_STABLE ||
               currentRadioMode == BLE_RADAR ||
               currentRadioMode == BLE_SIGNAL_LOGGER ||
               currentRadioMode == BLE_SIGNAL_LOGGER_PICK ||
               currentRadioMode == RF_SCANNER ||
               currentRadioMode == RF_MONITOR ||
               currentRadioMode == RF_SIGNAL_CAPTURE) {
        delay(20);
    } else if (currentRadioMode == BLE_SCAN) {
        return;
    }
}

void system_update() {
    static RadioMode lastMode = RADIO_IDLE;

    companion_service();
    storage_update();
    serviceStaleScanData();
    ui_frameTick();
    serviceBatteryMonitor();

    if (serviceScreenSaver()) {
        return;
    }

    if (lastMode != currentRadioMode) {
        postAppEvent(APP_EVENT_MODE_CHANGED, (uint32_t)currentRadioMode);
        lastMode = currentRadioMode;
    }

    if (currentScreen == SCREEN_TOOL) {
        serviceToolScreen();
        ui_renderDebugOverlay();
        return;
    }

    system_handleInput();
    ui_renderDebugOverlay();
}
