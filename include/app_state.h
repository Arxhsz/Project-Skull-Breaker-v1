#pragma once

#include <Arduino.h>

constexpr unsigned long APP_DEVICE_STALE_MS = 15000UL;

struct BLEState {
    int* deviceCount;
    char (*names)[32];
    char (*macs)[18];
    int* rssi;
    unsigned long* lastSeen;
    int maxResults;
    bool* scanRunning;
    bool* scannerFirstDraw;
    bool* needsRedraw;
    int* selectedIndex;
    int* scrollOffset;
    int* detailIndex;
    bool* detailActive;
};

struct WiFiState {
    int* networkCount;
    char (*ssids)[33];
    char (*bssids)[18];
    char (*enc)[8];
    int* rssi;
    int* channels;
    unsigned long* lastSeen;
    int maxResults;
    bool* scanRunning;
    bool* scannerFirstDraw;
    bool* needsRedraw;
    int* selectedIndex;
    int* scrollOffset;
    int* detailIndex;
    bool* detailActive;
};

struct RFState {
    bool* radioLocked;
    bool* radio24ActivePrepared;
    byte* activeChannel1;
    byte* activeChannel2;
    uint32_t* activeTxCount;
    String* activeStatus;
};

struct UIState {
    bool* shouldRedraw;
    bool* displayUpdatesSuspended;
    unsigned long* toolScreenEnteredAt;
    int* selectedIndex;
    int* currentMenuIndex;
    String* currentTitle;
};

struct FileManagerState {
    String* entries;
    bool* isDir;
    uint32_t* sizes;
    String* fullPaths;
    bool* marked;
    int maxEntries;
    int* count;
    int* selectedIndex;
    int* scrollOffset;
    bool* needsRedraw;
    String* currentPath;
    int* markedCount;
};

struct AppState {
    BLEState ble;
    WiFiState wifi;
    RFState rf;
    UIState ui;
    FileManagerState files;
};

extern AppState app;

#define bleStateView (app.ble)
#define wifiStateView (app.wifi)
#define rfStateView (app.rf)
#define uiStateView (app.ui)
#define fileManagerStateView (app.files)
