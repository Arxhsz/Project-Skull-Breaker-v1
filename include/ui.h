#pragma once

#include <Arduino.h>

void drawStatusBar();
void drawUI();
void drawTile(int x, int y, const char* label, int iconType, bool isSelected);
void redrawTileByIndex(int index, bool selected);

void drawWiFiDetail();
void drawBLEDetail();
void drawFileManager();
void drawFileDetailScreen();
void drawFileRenameScreen();
void redrawFileRenameSelection(int previousIndex, int currentIndex);
void redrawFileRenameDraftArea();
void drawFileDeleteConfirmScreen();
void drawSDFormatConfirmScreen();
void drawScreenSaverFrame(uint8_t styleIndex, bool resetFrame);
void drawWirelessUpdateScreen();

void handleInput();
void handleSubMenuInput();

void drawSubMenuItem(int i, bool selected);
void drawSubMenu();

void ui_frameTick();
void ui_renderDebugOverlay();
