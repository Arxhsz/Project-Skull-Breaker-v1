#include <Arduino.h>
#include <ESP.h>

#include "ble.h"
#include "system_types.h"
#define LOG_SCOPE "BLE"
#include "logging.h"

extern RadioMode currentRadioMode;

extern bool bleScanRunning;
extern bool bleNeedsRedraw;
extern int bleSelectedIndex;
extern int bleScrollOffset;
extern int bleLoggerTargetIndex;
extern bool radioLocked;
extern TaskHandle_t bleScanTaskHandle;
extern unsigned long toolScreenEnteredAt;
extern unsigned long bleScanStartedAt;
extern unsigned long bleScanReadyAt;
extern uint8_t bleScanWarmupRetries;

void handleBLEScannerInput();
void runBLEScanner();
void handleBLELoggerPickerInput();
void runBLELoggerPicker();
void runBLEBeaconSpammer();
void runBLEBeaconTest();
void runBLEStableDevice();
void runBLERadar();
void runBLELogger();
void bleScanTask(void* parameter);
void drawStatusBar();
void drawSubMenu();
void navigateBack();

// BLE Attack Functions
void runSourApple();
void runSwiftPairSpam();
void runSamsungSpam();
void runBLEBeaconSpam();
void runBTSpamAll();
void runBLEJammer();
void runBLESpoofer();
void runBLERubberDucky();

void ble_startScanTaskIfNeeded() {
    if (!bleScanRunning || bleScanTaskHandle != NULL) {
        if (bleScanRunning && bleScanStartedAt != 0 && millis() - bleScanStartedAt > 9000UL) {
            LOG("BLE worker timeout reset");
            bleScanRunning = false;
            bleScanStartedAt = 0;
            bleScanReadyAt = 0;
            bleScanWarmupRetries = 0;
            bleScanTaskHandle = NULL;
            bleNeedsRedraw = true;
            radioLocked = false;
        }
        return;
    }

    if (bleScanStartedAt == 0) {
        bleScanStartedAt = millis();
    }

    if (bleScanReadyAt != 0 && millis() < bleScanReadyAt) {
        return;
    }
    bleScanReadyAt = 0;

    if (ESP.getFreeHeap() < 48000) {
        LOG("BLE low heap - running inline");
        bleScanTaskHandle = NULL;
        bleScanTask(nullptr);
        return;
    }

    BaseType_t taskOk = xTaskCreatePinnedToCore(
        bleScanTask,
        "BLEScanTask",
        6144,
        NULL,
        1,
        &bleScanTaskHandle,
        0
    );

    if (taskOk != pdPASS) {
        LOG("BLE ERROR: task create failed - running inline");
        bleScanTaskHandle = NULL;
        bleScanTask(nullptr);
    }
}

void ble_serviceMode() {
    if (currentRadioMode == BLE_SCAN) {
        handleBLEScannerInput();
        runBLEScanner();
        ble_startScanTaskIfNeeded();
        return;
    }

    if (currentRadioMode == BLE_SIGNAL_LOGGER_PICK) {
        handleBLELoggerPickerInput();
        runBLELoggerPicker();
        ble_startScanTaskIfNeeded();
        return;
    }

    if (currentRadioMode == BLE_BEACON_SPAM) {
        runBLEBeaconSpammer();
        return;
    }

    if (currentRadioMode == BLE_BEACON_TEST) {
        runBLEBeaconTest();
        return;
    }

    if (currentRadioMode == BLE_DEVICE_STABLE) {
        runBLEStableDevice();
        return;
    }

    if (currentRadioMode == BLE_RADAR) {
        runBLERadar();
        return;
    }

    if (currentRadioMode == BLE_SIGNAL_LOGGER) {
        runBLELogger();
        return;
    }
    
    // BLE Attack Modes
    if (currentRadioMode == BLE_ATTACK_SOUR_APPLE) {
        runSourApple();
        return;
    }
    
    if (currentRadioMode == BLE_ATTACK_SWIFTPAIR) {
        runSwiftPairSpam();
        return;
    }
    
    if (currentRadioMode == BLE_ATTACK_SAMSUNG) {
        runSamsungSpam();
        return;
    }
    
    if (currentRadioMode == BLE_ATTACK_BEACON_SPAM) {
        runBLEBeaconSpam();
        return;
    }
    
    if (currentRadioMode == BLE_ATTACK_SPAM_ALL) {
        runBTSpamAll();
        return;
    }
    
    if (currentRadioMode == BLE_JAMMER) {
        runBLEJammer();
        return;
    }
    
    if (currentRadioMode == BLE_SPOOFER) {
        runBLESpoofer();
        return;
    }
    
    if (currentRadioMode == BLE_RUBBER_DUCKY) {
        runBLERubberDucky();
        return;
    }
}
