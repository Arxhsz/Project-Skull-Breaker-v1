#include <Arduino.h>
#include <cstring>

#include "app_state.h"
#include "scan_cleanup.h"
#include "system_types.h"

extern RadioMode currentRadioMode;
extern ScreenState currentScreen;

namespace {
bool holdBLEEntries() {
    return *bleStateView.scanRunning ||
           currentRadioMode == BLE_SCAN ||
           currentRadioMode == BLE_SIGNAL_LOGGER_PICK ||
           currentRadioMode == BLE_SIGNAL_LOGGER ||
           currentScreen == SCREEN_BLE_DETAIL;
}

bool holdWiFiEntries() {
    return *wifiStateView.scanRunning ||
           currentRadioMode == WIFI_SCAN ||
           currentScreen == SCREEN_WIFI_DETAIL;
}

void compactBLEEntry(int removeIndex) {
    for (int i = removeIndex; i < (*bleStateView.deviceCount) - 1; i++) {
        memcpy(bleStateView.names[i], bleStateView.names[i + 1], sizeof(bleStateView.names[i]));
        memcpy(bleStateView.macs[i], bleStateView.macs[i + 1], sizeof(bleStateView.macs[i]));
        bleStateView.rssi[i] = bleStateView.rssi[i + 1];
        bleStateView.lastSeen[i] = bleStateView.lastSeen[i + 1];
    }

    int last = (*bleStateView.deviceCount) - 1;
    if (last >= 0) {
        bleStateView.names[last][0] = '\0';
        bleStateView.macs[last][0] = '\0';
        bleStateView.rssi[last] = -100;
        bleStateView.lastSeen[last] = 0;
    }
}

void compactWiFiEntry(int removeIndex) {
    for (int i = removeIndex; i < (*wifiStateView.networkCount) - 1; i++) {
        memcpy(wifiStateView.ssids[i], wifiStateView.ssids[i + 1], sizeof(wifiStateView.ssids[i]));
        memcpy(wifiStateView.bssids[i], wifiStateView.bssids[i + 1], sizeof(wifiStateView.bssids[i]));
        memcpy(wifiStateView.enc[i], wifiStateView.enc[i + 1], sizeof(wifiStateView.enc[i]));
        wifiStateView.rssi[i] = wifiStateView.rssi[i + 1];
        wifiStateView.channels[i] = wifiStateView.channels[i + 1];
        wifiStateView.lastSeen[i] = wifiStateView.lastSeen[i + 1];
    }

    int last = (*wifiStateView.networkCount) - 1;
    if (last >= 0) {
        wifiStateView.ssids[last][0] = '\0';
        wifiStateView.bssids[last][0] = '\0';
        wifiStateView.enc[last][0] = '\0';
        wifiStateView.rssi[last] = -100;
        wifiStateView.channels[last] = 0;
        wifiStateView.lastSeen[last] = 0;
    }
}

void clampSelection(int* selectedIndex, int* scrollOffset, int count) {
    if (*selectedIndex >= count) {
        *selectedIndex = max(0, count - 1);
    }
    if (*scrollOffset > *selectedIndex) {
        *scrollOffset = *selectedIndex;
    }
    if (*scrollOffset < 0) {
        *scrollOffset = 0;
    }
}
}  // namespace

void cleanupBLEDevices(unsigned long nowMs) {
    if (holdBLEEntries()) {
        return;
    }

    bool removed = false;

    for (int i = (*bleStateView.deviceCount) - 1; i >= 0; i--) {
        unsigned long seenAt = bleStateView.lastSeen[i];
        if (seenAt == 0 || (nowMs - seenAt) <= APP_DEVICE_STALE_MS) {
            continue;
        }

        compactBLEEntry(i);
        (*bleStateView.deviceCount)--;
        removed = true;

        if (*bleStateView.detailIndex == i) {
            *bleStateView.detailIndex = -1;
            *bleStateView.detailActive = false;
        } else if (*bleStateView.detailIndex > i) {
            (*bleStateView.detailIndex)--;
        }
    }

    if (removed) {
        clampSelection(bleStateView.selectedIndex, bleStateView.scrollOffset, *bleStateView.deviceCount);
        *bleStateView.needsRedraw = true;
        *bleStateView.scannerFirstDraw = true;
    }
}

void cleanupWiFiNetworks(unsigned long nowMs) {
    if (holdWiFiEntries()) {
        return;
    }

    bool removed = false;

    for (int i = (*wifiStateView.networkCount) - 1; i >= 0; i--) {
        unsigned long seenAt = wifiStateView.lastSeen[i];
        if (seenAt == 0 || (nowMs - seenAt) <= APP_DEVICE_STALE_MS) {
            continue;
        }

        compactWiFiEntry(i);
        (*wifiStateView.networkCount)--;
        removed = true;

        if (*wifiStateView.detailIndex == i) {
            *wifiStateView.detailIndex = -1;
            *wifiStateView.detailActive = false;
        } else if (*wifiStateView.detailIndex > i) {
            (*wifiStateView.detailIndex)--;
        }
    }

    if (removed) {
        clampSelection(wifiStateView.selectedIndex, wifiStateView.scrollOffset, *wifiStateView.networkCount);
        *wifiStateView.needsRedraw = true;
        *wifiStateView.scannerFirstDraw = true;
    }
}

void serviceStaleScanData() {
    static unsigned long lastCleanupMs = 0;
    unsigned long nowMs = millis();
    if (nowMs - lastCleanupMs < 1000UL) {
        return;
    }

    lastCleanupMs = nowMs;
    cleanupBLEDevices(nowMs);
    cleanupWiFiNetworks(nowMs);
}
