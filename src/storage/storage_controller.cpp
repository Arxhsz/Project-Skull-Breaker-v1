#include <Arduino.h>
#include <ESP.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>

#include "storage.h"
#include "system.h"
#include "system_types.h"
#define LOG_SCOPE "SD"
#include "logging.h"

extern bool sdMounted;
extern bool sdDetected;
extern bool sdError;
extern bool sdAutoMountWanted;
extern bool sdAutoMountRetryBlocked;
extern bool sdAutoMountErrorLatched;
extern bool displayUpdatesSuspended;

void primeSharedSPIBus();

namespace {
constexpr int kFileManagerMaxEntries = 32;
constexpr int kSDCS = 5;
constexpr int kCC1101CS = 22;
constexpr int kTFTCS = 15;
constexpr uint8_t kSDFailureBlockThreshold = 3;

uint8_t gSdFailureCount = 0;
bool sdCurrentlyMounted = false;
volatile bool sdMountInProgress = false;

void prepareSDBusForMount(uint16_t settleDelayMs);
bool confirmMountedCardReady(uint8_t& cardTypeOut);
bool attemptMountAtSpeed(uint32_t speedHz, uint16_t settleDelayMs, uint8_t& cardTypeOut);

File openSDPathWithRetry(const String& path) {
    File file = SD.open(path.c_str());
    if (file) {
        return file;
    }

    delay(5);
    yield();
    return SD.open(path.c_str());
}

File openSDPathWithRetry(const String& path, const char* mode) {
    File file = SD.open(path.c_str(), mode);
    if (file) {
        return file;
    }

    delay(5);
    yield();
    return SD.open(path.c_str(), mode);
}

uint8_t readSDCardTypeWithRetry() {
    uint8_t cardType = SD.cardType();
    if (cardType != CARD_NONE) {
        return cardType;
    }

    delay(5);
    yield();
    return SD.cardType();
}

bool warmupSDRoot() {
    File root = openSDPathWithRetry("/");
    if (!root) {
        return false;
    }

    root.close();
    return true;
}

void resetSDFailureState() {
    gSdFailureCount = 0;
    sdAutoMountRetryBlocked = false;
}

void recordSDFailure() {
    if (gSdFailureCount < 255) {
        gSdFailureCount++;
    }
    if (gSdFailureCount >= kSDFailureBlockThreshold) {
        sdAutoMountRetryBlocked = true;
    }
}

void recoverSharedSPIBusAfterSDFailure() {
    SD.end();
    sdCurrentlyMounted = false;
    delay(5);
    SPI.end();
    delay(5);
    primeSharedSPIBus();
    delayMicroseconds(200);
    delay(5);
    SPI.begin(18, 19, 23);
    delay(10);
}

void prepareSDBusForMount(uint16_t settleDelayMs) {
    SD.end();
    sdCurrentlyMounted = false;
    delay(5);
    SPI.end();
    delayMicroseconds(200);
    primeSharedSPIBus();
    delay(5);
    SPI.begin(18, 19, 23, kSDCS);
    delay(settleDelayMs);
}

bool confirmMountedCardReady(uint8_t& cardTypeOut) {
    cardTypeOut = CARD_NONE;

    for (int attempt = 0; attempt < 3; attempt++) {
        File root = openSDPathWithRetry("/");
        bool rootOk = (bool)root;
        if (root) {
            root.close();
        }

        uint8_t cardType = readSDCardTypeWithRetry();
        if (rootOk || cardType != CARD_NONE) {
            cardTypeOut = (cardType != CARD_NONE) ? cardType : CARD_SD;
            return true;
        }

        delay(8);
        yield();
    }

    return false;
}

bool attemptMountAtSpeed(uint32_t speedHz, uint16_t settleDelayMs, uint8_t& cardTypeOut) {
    prepareSDBusForMount(settleDelayMs);
    LOG(String("SD mount try @ ") + String(speedHz));
    LOG("Attempting SD.begin...");
    LOG(String("sdMounted flag: ") + (sdMounted ? "true" : "false"));
    LOG(String("sdCurrentlyMounted: ") + (sdCurrentlyMounted ? "true" : "false"));
    LOG(String("free heap before SD.begin: ") + String(ESP.getFreeHeap()));
    if (sdCurrentlyMounted) {
        LOG("SD already mounted - skipping begin");
        cardTypeOut = readSDCardTypeWithRetry();
        return cardTypeOut != CARD_NONE || warmupSDRoot();
    }
    pinMode(kTFTCS, OUTPUT);
    digitalWrite(kTFTCS, HIGH);
    pinMode(kCC1101CS, OUTPUT);
    digitalWrite(kCC1101CS, HIGH);
    delay(2);
    if (!SD.begin(kSDCS, SPI, speedHz)) {
        SD.end();
        sdCurrentlyMounted = false;
        SPI.end();
        delay(10);
        return false;
    }
    sdCurrentlyMounted = true;
    LOG(String("free heap after SD.begin: ") + String(ESP.getFreeHeap()));

    if (confirmMountedCardReady(cardTypeOut)) {
        return true;
    }

    SD.end();
    sdCurrentlyMounted = false;
    SPI.end();
    delay(10);
    delay(6);
    yield();
    return false;
}
}

extern bool rfSubLoaded;
extern bool rfTransmitReturnToFileManager;

extern int fileManagerCount;
extern int fileManagerSelectedIndex;
extern int fileManagerScrollOffset;
extern int fileManagerMarkedCount;

extern bool fileManagerNeedsRedraw;
extern bool fileManagerMarked[];
extern bool fileManagerIsDir[];
extern uint32_t fileManagerSizes[];

extern unsigned long lastSDStateCheck;

extern String fileManagerEntries[];
extern String fileManagerFullPaths[];
extern String fileManagerPath;
extern String rfSubLoadedPath;
extern String rfSubLoadedName;
extern String fileDetailPath;
extern String fileRenamePath;

extern ScreenState currentScreen;
extern Preferences prefs;

void refreshDiagnosticsStatus();
void clearLoadedSubFile();
void restoreDisplayAfterStorageAccess();
void restoreDisplayAfterHeavyStorageAccess();
void restoreDisplayAfterCriticalStorageFailure();
void releaseIconCacheMemory();
void primeSharedSPIBus();

namespace {
void persistSDAutoMountErrorLatch(bool enabled) {
    sdAutoMountErrorLatched = enabled;
    prefs.putBool("sdAutoErr", enabled);
}
}

static uint32_t wipeFileCount = 0;
static void (*wipeProgressCb)(uint32_t count) = nullptr;

static bool wipeSDDirectory(const String& path) {
    while (true) {
        File dir = openSDPathWithRetry(path);
        if (!dir || !dir.isDirectory()) {
            if (path == "/" || SD.exists(path.c_str())) {
                return false;
            }
            return true;
        }

        File entry = dir.openNextFile();
        if (!entry) {
            dir.close();
            break;
        }

        String entryPath = entry.name();
        bool entryIsDir = entry.isDirectory();
        entry.close();
        dir.close();

        if (!entryPath.startsWith("/")) {
            entryPath = (path == "/") ? ("/" + entryPath) : (path + "/" + entryPath);
        }

        if (entryIsDir) {
            if (!wipeSDDirectory(entryPath)) {
                return false;
            }
            if (SD.exists(entryPath.c_str()) && !SD.rmdir(entryPath.c_str())) {
                return false;
            }
        } else {
            if (SD.exists(entryPath.c_str()) && !SD.remove(entryPath.c_str())) {
                return false;
            }
            wipeFileCount++;
            if (wipeProgressCb) wipeProgressCb(wipeFileCount);
        }

        yield();
    }

    return true;
}

bool deleteSDEntryInternal(const String& path) {
    if (path.length() == 0 || path == "/") {
        LOG("DELETE skip invalid path");
        return false;
    }

    String primaryPath = path;
    String alternatePath = path.startsWith("/") ? path.substring(1) : ("/" + path);
    const char* resolvedPath = nullptr;
    LOG(String("DELETE request: ") + path);
    LOG(String("DELETE exists primary=") + (SD.exists(primaryPath.c_str()) ? "yes" : "no") +
        " alt=" + (alternatePath.length() > 0 && SD.exists(alternatePath.c_str()) ? "yes" : "no"));

    if (SD.exists(primaryPath.c_str())) {
        resolvedPath = primaryPath.c_str();
    } else if (alternatePath.length() > 0 && SD.exists(alternatePath.c_str())) {
        resolvedPath = alternatePath.c_str();
    } else {
        LOG("DELETE path not found");
        return false;
    }

    File entry = openSDPathWithRetry(String(resolvedPath));
    if (!entry) {
        LOG(String("DELETE open failed: ") + resolvedPath);
        return false;
    }

    bool isDir = entry.isDirectory();
    entry.close();
    LOG(String("DELETE type: ") + (isDir ? "dir" : "file"));

    bool ok = false;
    if (isDir) {
        String resolvedString = String(resolvedPath);
        ok = wipeSDDirectory(resolvedString) && SD.rmdir(resolvedString.c_str());
        if (!ok && resolvedString != primaryPath) {
            ok = wipeSDDirectory(primaryPath) && SD.rmdir(primaryPath.c_str());
        }
    } else {
        ok = SD.remove(resolvedPath);
        if (!ok && strcmp(resolvedPath, primaryPath.c_str()) != 0) {
            ok = SD.remove(primaryPath.c_str());
        }
        if (!ok && alternatePath.length() > 0 && strcmp(resolvedPath, alternatePath.c_str()) != 0) {
            ok = SD.remove(alternatePath.c_str());
        }
    }
    LOG(String("DELETE remove result: ") + (ok ? "ok" : "fail"));

    if (ok && rfSubLoaded && rfSubLoadedPath == path) {
        clearLoadedSubFile();
        rfTransmitReturnToFileManager = false;
    }

    if (ok) {
        bool stillExists = SD.exists(primaryPath.c_str()) ||
                           (alternatePath.length() > 0 && SD.exists(alternatePath.c_str()));
        LOG(String("DELETE still exists after remove: ") + (stillExists ? "yes" : "no"));
        if (stillExists) {
            ok = false;
        }
    }

    if (!ok) {
        LOG(String("DELETE final fail: ") + path);
    } else {
        LOG(String("DELETE final ok: ") + path);
    }
    return ok;
}

bool formatMountedSDCard() {
    if (!validateSDReady(false)) {
        LOG("FORMAT failed: SD not ready");
        return false;
    }

    LOG("FORMAT start");
    displayUpdatesSuspended = true;
    primeSharedSPIBus();
    SPI.begin(18, 19, 23);
    digitalWrite(kTFTCS, HIGH);
    digitalWrite(kCC1101CS, HIGH);

    wipeFileCount = 0;
    wipeProgressCb = nullptr;  // caller sets this before calling

    bool ok = wipeSDDirectory("/");
    LOG(String("FORMAT wiped ") + String(wipeFileCount) + " files");
    if (ok) {
        ensureSDFolders();
        clearLoadedSubFile();
    }
    LOG(String("FORMAT result: ") + (ok ? "ok" : "fail"));

    refreshDiagnosticsStatus();
    displayUpdatesSuspended = false;
    restoreDisplayAfterHeavyStorageAccess();
    return ok;
}

void setFormatProgressCallback(void (*cb)(uint32_t)) {
    wipeProgressCb = cb;
}

bool validateSDReady(bool tryAutoMount) {
    if (sdMountInProgress) {
        return false;
    }

    if (!sdMounted || sdError) {
        if (tryAutoMount && sdAutoMountWanted && !sdAutoMountRetryBlocked && !sdAutoMountErrorLatched) {
            return mountSD(false, false, true, "AUTO_VALIDATE");
        }
        return false;
    }

    beginSPIOperation(true);

    File root = openSDPathWithRetry("/");
    bool ok = (bool)root;
    if (root) {
        root.close();
    }
    if (!ok) {
        uint8_t cardType = readSDCardTypeWithRetry();
        if (cardType != CARD_NONE) {
            File retryRoot = openSDPathWithRetry("/");
            ok = (bool)retryRoot;
            if (retryRoot) {
                retryRoot.close();
            }
        }
    }
    endSPIOperation(false);

    if (ok) {
        sdDetected = true;
        sdError = false;
        resetSDFailureState();
        refreshDiagnosticsStatus();
        return true;
    }

    recordSDFailure();
    if (sdMounted && gSdFailureCount < 2) {
        recoverSharedSPIBusAfterSDFailure();
        sdDetected = true;
        sdError = false;
        refreshDiagnosticsStatus();
        return true;
    }
    if (sdAutoMountWanted) {
        sdAutoMountRetryBlocked = true;
        persistSDAutoMountErrorLatch(true);
    }
    recoverSharedSPIBusAfterSDFailure();
    sdMounted = false;
    sdDetected = false;
    sdError = true;
    refreshDiagnosticsStatus();
    restoreDisplayAfterHeavyStorageAccess();
    return false;
}

void ensureSDFolders() {
    if (!validateSDReady(false)) return;

    const char* folders[] = {
        "/logs",
        "/captures",
        "/captures/rf",
        "/captures/wifi",
        "/captures/ble",
        "/exports"
    };

    for (const char* folder : folders) {
        if (!SD.exists(folder)) {
            SD.mkdir(folder);
        }
        yield();
    }
}

bool deleteSDEntry(const String& path, bool refreshDisplay) {
    if (!validateSDReady(false) || path.length() == 0 || path == "/") {
        return false;
    }

    beginSPIOperation(true);
    bool ok = deleteSDEntryInternal(path);
    endSPIOperation(refreshDisplay, !ok);
    if (refreshDisplay) validateSDReady(false);
    return ok;
}

String getFileManagerParentPath(const String& path) {
    if (path == "/" || path.length() == 0) return "/";
    int slash = path.lastIndexOf('/');
    if (slash <= 0) return "/";
    return path.substring(0, slash);
}

String getSDEntrySavedText(const String& path) {
    String savedText = "Unknown";
    if (!validateSDReady(false) || path.length() == 0) {
        return savedText;
    }

    beginSPIOperation(true);
    File detailFile = openSDPathWithRetry(path, FILE_READ);
    if (detailFile) {
        time_t lastWrite = detailFile.getLastWrite();
        if (lastWrite > 0) {
            struct tm timeInfo;
            struct tm* infoPtr = localtime_r(&lastWrite, &timeInfo);
            if (infoPtr != nullptr) {
                char timeBuffer[24];
                strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", &timeInfo);
                savedText = timeBuffer;
            }
        }
        detailFile.close();
    }

    endSPIOperation(false);
    primeSharedSPIBus();
    SPI.begin(18, 19, 23);
    digitalWrite(kSDCS, HIGH);
    digitalWrite(kCC1101CS, HIGH);
    digitalWrite(kTFTCS, HIGH);
    return savedText;
}

bool renameSDEntry(const String& path, const String& newLeafName) {
    if (!validateSDReady(false) || path.length() == 0 || path == "/") {
        return false;
    }

    String cleanLeaf = newLeafName;
    cleanLeaf.trim();
    cleanLeaf.replace("/", "_");
    cleanLeaf.replace("\\", "_");
    cleanLeaf.replace(":", "_");
    cleanLeaf.replace("*", "_");
    cleanLeaf.replace("?", "_");
    cleanLeaf.replace("\"", "_");
    cleanLeaf.replace("<", "_");
    cleanLeaf.replace(">", "_");
    cleanLeaf.replace("|", "_");

    if (cleanLeaf.length() == 0) {
        return false;
    }

    String parentPath = getFileManagerParentPath(path);
    if (parentPath.length() == 0) {
        parentPath = "/";
    }

    String newPath = (parentPath == "/") ? ("/" + cleanLeaf) : (parentPath + "/" + cleanLeaf);
    if (newPath == path) {
        return true;
    }
    if (SD.exists(newPath)) {
        return false;
    }

    beginSPIOperation(true);
    bool ok = SD.rename(path, newPath);
    endSPIOperation(true, false);

    if (ok) {
        if (rfSubLoaded && rfSubLoadedPath == path) {
            rfSubLoadedPath = newPath;
            rfSubLoadedName = getFileManagerLeafName(newPath);
        }
        if (fileDetailPath == path) {
            fileDetailPath = newPath;
        }
        if (fileRenamePath == path) {
            fileRenamePath = newPath;
        }
    }

    validateSDReady(false);
    return ok;
}

bool mountSD(bool redrawMenu, bool rememberChoice, bool autoTriggered, const char* callerContext) {
    LOG("Mounting SD...");
    LOG(String("mount caller: ") + (callerContext ? callerContext : "UNKNOWN"));
    LOG(String("free heap pre-mount: ") + String(ESP.getFreeHeap()));

    if (sdMountInProgress) {
        LOG("SD mount already in progress");
        return false;
    }
    sdMountInProgress = true;

    if (sdCurrentlyMounted) {
        LOG("SD already mounted - skipping begin");
        sdMounted = true;
        sdDetected = true;
        sdError = false;
        resetSDFailureState();
        refreshDiagnosticsStatus();
        if (!redrawMenu) {
            restoreDisplayAfterStorageAccess();
        }
        sdMountInProgress = false;
        return true;
    }

    if (rememberChoice) {
        sdAutoMountWanted = true;
        prefs.putBool("sdAutoMount", true);
    }
    if (!autoTriggered) {
        sdAutoMountRetryBlocked = false;
        if (sdAutoMountErrorLatched) {
            persistSDAutoMountErrorLatch(false);
        }
    }

    releaseIconCacheMemory();
    LOG(String("free heap after icon cache release: ") + String(ESP.getFreeHeap()));
    beginSPIOperation(true);
    uint8_t mountedCardType = CARD_NONE;
    bool mounted = attemptMountAtSpeed(400000, 8, mountedCardType);
    if (!mounted) {
        mounted = attemptMountAtSpeed(250000, 10, mountedCardType);
    }
    if (!mounted) {
        mounted = attemptMountAtSpeed(100000, 12, mountedCardType);
    }

    if (mounted) {
        if (mountedCardType != CARD_NONE) {
            LOG("SD MOUNTED");

            sdMounted = true;
            sdDetected = true;
            sdError = false;
            sdCurrentlyMounted = true;
            resetSDFailureState();
            if (sdAutoMountErrorLatched) {
                persistSDAutoMountErrorLatch(false);
            }
            warmupSDRoot();
            ensureSDFolders();
            refreshDiagnosticsStatus();

        } else {
            LOG("No SD card");

            recordSDFailure();
            if (autoTriggered || sdAutoMountWanted) {
                sdAutoMountRetryBlocked = true;
                persistSDAutoMountErrorLatch(true);
            }
            recoverSharedSPIBusAfterSDFailure();
            sdMounted = false;
            sdDetected = false;
            sdError = true;
            sdCurrentlyMounted = false;
            refreshDiagnosticsStatus();
        }
    } else {
        LOG("SD INIT FAILED");

        recordSDFailure();
        if (autoTriggered || sdAutoMountWanted) {
            sdAutoMountRetryBlocked = true;
            persistSDAutoMountErrorLatch(true);
        }
        recoverSharedSPIBusAfterSDFailure();
        sdMounted = false;
        sdDetected = false;
        sdError = true;
        sdCurrentlyMounted = false;
        refreshDiagnosticsStatus();
    }

    bool useHeavyRestore = true;
    bool criticalFailureRestore = !sdMounted;
    if (criticalFailureRestore) {
        endSPIOperation(false, false);
        restoreDisplayAfterCriticalStorageFailure();
    } else {
        endSPIOperation(redrawMenu, useHeavyRestore);
        if (!redrawMenu) {
            if (useHeavyRestore) {
                restoreDisplayAfterHeavyStorageAccess();
            } else {
                restoreDisplayAfterStorageAccess();
            }
        }
    }

    sdMountInProgress = false;
    return sdMounted;
}

void unmountSD(bool redrawMenu) {
    LOG("Unmounting SD");

    beginSPIOperation(true);
    SD.end();
    sdCurrentlyMounted = false;
    delay(5);
    SPI.end();
    delay(5);
    pinMode(kTFTCS, OUTPUT);
    digitalWrite(kTFTCS, HIGH);
    pinMode(kCC1101CS, OUTPUT);
    digitalWrite(kCC1101CS, HIGH);
    pinMode(kSDCS, OUTPUT);
    digitalWrite(kSDCS, HIGH);
    delay(5);
    SPI.begin(18, 19, 23);

    sdMounted = false;
    sdDetected = false;
    sdError = false;
    resetSDFailureState();
    persistSDAutoMountErrorLatch(false);
    refreshDiagnosticsStatus();
    sdAutoMountWanted = false;
    prefs.putBool("sdAutoMount", false);
    endSPIOperation(redrawMenu, true);
    if (!redrawMenu) {
        restoreDisplayAfterHeavyStorageAccess();
    }
}

String getFileManagerLeafName(const String& path) {
    if (path == "/") return "/";
    int slash = path.lastIndexOf('/');
    if (slash < 0) return path;
    if (slash == path.length() - 1) {
        String trimmed = path.substring(0, slash);
        int prev = trimmed.lastIndexOf('/');
        return prev >= 0 ? trimmed.substring(prev + 1) : trimmed;
    }
    return path.substring(slash + 1);
}

void loadFileManagerEntries() {
    fileManagerCount = 0;
    fileManagerSelectedIndex = 0;
    fileManagerScrollOffset = 0;
    fileManagerNeedsRedraw = true;
    fileManagerMarkedCount = 0;
    for (int i = 0; i < kFileManagerMaxEntries; i++) {
        fileManagerMarked[i] = false;
    }

    if (!validateSDReady(false)) {
        return;
    }

    beginSPIOperation(true);
    File dir = openSDPathWithRetry(fileManagerPath);
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        fileManagerPath = "/";
        dir = openSDPathWithRetry("/");
    }

    if (!dir) {
        endSPIOperation(true, false);
        return;
    }

    File entry = dir.openNextFile();
    while (entry && fileManagerCount < kFileManagerMaxEntries) {
        String fullName = entry.name();
        if (!fullName.startsWith("/")) {
            fullName = (fileManagerPath == "/") ? ("/" + fullName) : (fileManagerPath + "/" + fullName);
        }
        fileManagerEntries[fileManagerCount] = getFileManagerLeafName(fullName);
        fileManagerIsDir[fileManagerCount] = entry.isDirectory();
        fileManagerSizes[fileManagerCount] = entry.isDirectory() ? 0 : (uint32_t)entry.size();
        fileManagerFullPaths[fileManagerCount] = fullName;
        fileManagerCount++;
        entry.close();
        entry = dir.openNextFile();
        yield();
    }

    dir.close();

    for (int i = 0; i < fileManagerCount - 1; i++) {
        for (int j = i + 1; j < fileManagerCount; j++) {
            bool swap = false;
            if (fileManagerIsDir[i] != fileManagerIsDir[j]) {
                swap = fileManagerIsDir[i] < fileManagerIsDir[j];
            } else if (fileManagerIsDir[i]) {
                swap = fileManagerEntries[i].compareTo(fileManagerEntries[j]) > 0;
            } else {
                swap = fileManagerEntries[i].compareTo(fileManagerEntries[j]) < 0;
            }

            if (swap) {
                String tmpEntry = fileManagerEntries[i];
                fileManagerEntries[i] = fileManagerEntries[j];
                fileManagerEntries[j] = tmpEntry;

                bool tmpDir = fileManagerIsDir[i];
                fileManagerIsDir[i] = fileManagerIsDir[j];
                fileManagerIsDir[j] = tmpDir;

                uint32_t tmpSize = fileManagerSizes[i];
                fileManagerSizes[i] = fileManagerSizes[j];
                fileManagerSizes[j] = tmpSize;

                String tmpPath = fileManagerFullPaths[i];
                fileManagerFullPaths[i] = fileManagerFullPaths[j];
                fileManagerFullPaths[j] = tmpPath;
            }
        }
        yield();
    }
    endSPIOperation(true, false);
}

int findFileManagerIndexByPath(const String& path) {
    for (int i = 0; i < fileManagerCount; i++) {
        if (fileManagerFullPaths[i] == path) return i;
    }
    return -1;
}

void serviceSDCardState() {
    if (sdMountInProgress) {
        return;
    }

    if (millis() - lastSDStateCheck < 900) {
        return;
    }
    lastSDStateCheck = millis();

    bool prevMounted = sdMounted;
    bool prevDetected = sdDetected;
    bool prevError = sdError;

    if (!sdMounted && sdAutoMountWanted && !sdAutoMountRetryBlocked && !sdAutoMountErrorLatched) {
        validateSDReady(true);
    }

    if (prevMounted != sdMounted || prevDetected != sdDetected || prevError != sdError) {
        if (currentScreen == SCREEN_FILE_MANAGER) {
            if (sdMounted) {
                loadFileManagerEntries();
            } else {
                fileManagerCount = 0;
                fileManagerSelectedIndex = 0;
                fileManagerScrollOffset = 0;
                fileManagerNeedsRedraw = true;
            }
        }
        markCurrentScreenForRedraw();
    }
}

void storage_update() {
    serviceSDCardState();
}
