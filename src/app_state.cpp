#include <Arduino.h>

#include "app_state.h"

extern int bleDeviceCount;
extern char bleNames[][32];
extern char bleMACs[][18];
extern int bleRSSI[];
extern unsigned long bleLastSeen[];
extern bool bleScanRunning;
extern bool bleScannerFirstDraw;
extern bool bleNeedsRedraw;
extern int bleSelectedIndex;
extern int bleScrollOffset;
extern int bleDetailIndex;
extern bool bleDetailActive;

extern int wifiNetworkCount;
extern char wifiSSIDs[][33];
extern char wifiBSSID[][18];
extern char wifiEncStr[][8];
extern int wifiRSSI[];
extern int wifiChannel[];
extern unsigned long wifiLastSeen[];
extern bool wifiScanRunning;
extern bool wifiScannerFirstDraw;
extern bool wifiNeedsRedraw;
extern int wifiSelectedIndex;
extern int wifiScrollOffset;
extern int wifiDetailIndex;
extern bool wifiDetailActive;

extern bool radioLocked;
extern bool radio24ActivePrepared;
extern byte radio24ActiveChannel1;
extern byte radio24ActiveChannel2;
extern uint32_t radio24ActiveTxCount;
extern String radio24ActiveStatus;

extern bool shouldRedraw;
extern bool displayUpdatesSuspended;
extern unsigned long toolScreenEnteredAt;
extern int selectedIndex;
extern int currentMenuIndex;
extern String currentTitle;

extern String fileManagerEntries[];
extern bool fileManagerIsDir[];
extern uint32_t fileManagerSizes[];
extern String fileManagerFullPaths[];
extern bool fileManagerMarked[];
extern int fileManagerCount;
extern int fileManagerSelectedIndex;
extern int fileManagerScrollOffset;
extern bool fileManagerNeedsRedraw;
extern String fileManagerPath;
extern int fileManagerMarkedCount;

AppState app = {
    {
        &bleDeviceCount,
        bleNames,
        bleMACs,
        bleRSSI,
        bleLastSeen,
        10,
        &bleScanRunning,
        &bleScannerFirstDraw,
        &bleNeedsRedraw,
        &bleSelectedIndex,
        &bleScrollOffset,
        &bleDetailIndex,
        &bleDetailActive
    },
    {
        &wifiNetworkCount,
        wifiSSIDs,
        wifiBSSID,
        wifiEncStr,
        wifiRSSI,
        wifiChannel,
        wifiLastSeen,
        10,
        &wifiScanRunning,
        &wifiScannerFirstDraw,
        &wifiNeedsRedraw,
        &wifiSelectedIndex,
        &wifiScrollOffset,
        &wifiDetailIndex,
        &wifiDetailActive
    },
    {
        &radioLocked,
        &radio24ActivePrepared,
        &radio24ActiveChannel1,
        &radio24ActiveChannel2,
        &radio24ActiveTxCount,
        &radio24ActiveStatus
    },
    {
        &shouldRedraw,
        &displayUpdatesSuspended,
        &toolScreenEnteredAt,
        &selectedIndex,
        &currentMenuIndex,
        &currentTitle
    },
    {
        fileManagerEntries,
        fileManagerIsDir,
        fileManagerSizes,
        fileManagerFullPaths,
        fileManagerMarked,
        32,
        &fileManagerCount,
        &fileManagerSelectedIndex,
        &fileManagerScrollOffset,
        &fileManagerNeedsRedraw,
        &fileManagerPath,
        &fileManagerMarkedCount
    }
};
