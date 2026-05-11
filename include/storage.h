#pragma once

#include <Arduino.h>

bool validateSDReady(bool tryAutoMount = true);
void ensureSDFolders();
bool mountSD(bool redrawMenu = true, bool rememberChoice = true, bool autoTriggered = false, const char* callerContext = "DIRECT");
void unmountSD(bool redrawMenu = true);
void serviceSDCardState();

bool deleteSDEntry(const String& path, bool refreshDisplay = true);
bool deleteSDEntryInternal(const String& path);
bool renameSDEntry(const String& path, const String& newLeafName);
bool formatMountedSDCard();
void setFormatProgressCallback(void (*cb)(uint32_t));
String getSDEntrySavedText(const String& path);

void loadFileManagerEntries();
int findFileManagerIndexByPath(const String& path);
String getFileManagerLeafName(const String& path);
String getFileManagerParentPath(const String& path);
void storage_update();
