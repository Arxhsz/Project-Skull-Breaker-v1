#include <Arduino.h>
#include <Adafruit_ILI9341.h>
#include <SD.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "storage.h"
#include "system.h"
#include "system_types.h"
#include "ui.h"
#include "orion_tools.h"

extern bool shouldRedraw;
extern bool displayUpdatesSuspended;
extern bool flashFsReady;
extern bool sdError;
extern bool sdMounted;
extern bool radio1Ok;
extern bool cc1101Ok;
extern bool radio3Ok;
extern bool wifiDetailActive;
extern bool bleDetailActive;
extern bool fileManagerNeedsRedraw;
extern bool fileDeleteConfirmYes;
extern bool fileDeleteBatchMode;
extern bool rfSubLoaded;
extern bool sdFormatConfirmEraseSelected;

extern int wifiDetailIndex;
extern int bleDetailIndex;
extern int bleDeviceCount;
extern int fileManagerCount;
extern int fileManagerSelectedIndex;
extern int fileManagerScrollOffset;
extern int fileManagerMarkedCount;
extern int currentMenuSize;
extern int currentMenuIndex;
extern int selectedIndex;
extern int totalItems;
extern int tileW;
extern int tileH;
extern int spacing;
extern int tileTopY;
extern int tileVerticalSpacing;
extern int fileRenameCursor;
extern int fileDeleteTargetCount;

extern unsigned long fileRenameStatusUntil;

extern int wifiNetworkCount;
extern char wifiSSIDs[][33];
extern char wifiEncStr[][8];
extern char wifiBSSID[][18];
extern bool wifiDetailSnapshotValid;
extern char wifiDetailSnapshotSSID[33];
extern char wifiDetailSnapshotBSSID[18];
extern char wifiDetailSnapshotEnc[8];
extern char wifiDetailSnapshotHint[16];
extern int wifiDetailSnapshotRSSI;
extern int wifiDetailSnapshotChannel;
extern char bleNames[][32];
extern char bleMACs[][18];
extern String fileManagerEntries[];
extern String fileManagerFullPaths[];
extern String fileManagerPath;
extern String fileDetailPath;
extern String fileDetailSavedText;
extern String fileTextViewerContent;
extern String fileRenamePath;
extern String fileRenameDraft;
extern String fileRenameExtension;
extern String fileRenameStatus;
extern String fileDeleteTargetPath;
extern String currentTitle;
extern String rfSubLoadedPath;
extern String wirelessUpdateSSID;
extern String wirelessUpdatePassword;
extern String wirelessUpdateIP;
extern String wirelessUpdateStatus;
extern int currentMenuScrollOffset;

extern int wifiRSSI[];
extern int wifiChannel[];
extern int bleRSSI[];

extern bool fileManagerIsDir[];
extern bool fileManagerMarked[];
extern uint32_t fileManagerSizes[];
extern const char** currentMenu;
extern const char* settingsMenu[];
extern const char* screenSaverMenu[];
extern const char* brightnessMenu[];
extern String g_fileDetailToast;
extern unsigned long g_fileDetailToastUntil;
extern bool fileTextViewerActive;
extern int fileTextViewerScrollLine;
extern int fileTextViewerLineCount;
extern bool fileTextViewerTruncated;
extern const char* items[];
extern const char* iconPaths[];

extern ScreenState currentScreen;
extern RadioMode currentRadioMode;
extern Adafruit_ILI9341 tft;
extern uint8_t batteryPercent;
extern bool chargerConnected;
float estimateDistance(int rssi);
const char** getFileRenameKeyArray();
void drawIconFromFSColored(int x, int y, const char* path, uint16_t newColor, int size);
void drawIconFromFS(int x, int y, const char* path);
void drawScaledIconFromFSColored(int x, int y, const char* path, int iconSize, int scale, uint16_t newColor);
void drawBLEBeaconCard(int x, int y, int w, int h, const char* label, uint16_t accent);
void drawCountBadge(int x, int y, int count);
void drawListScrollArrows(int totalItems, int visibleItems, int scrollOffset);
const char** getCurrentIconArray();
bool isMenuItemDisabled(int index);
String getScreenSaverTimeoutLabel();
const char* getScreenSaverStyleLabel();

namespace {
unsigned long lastFrameMs = 0;
float smoothedFrameMs = 0.0f;
bool debugOverlayEnabled = false;

constexpr int kMenuHeaderTitleY = 36;
constexpr int kMenuHeaderSubtitleY = 54;
constexpr int kMenuHeaderDividerY = 66;
constexpr int kMenuFooterDividerY = 294;
constexpr int kMenuFooterTextY = 302;

constexpr int kHomeTileWUi = 108;
constexpr int kHomeTileHUi = 66;
constexpr int kHomeTileGapXUi = 8;
constexpr int kHomeTileGapYUi = 8;
constexpr int kHomeTileStartXUi = 8;
constexpr int kHomeTileStartYUi = 76;

constexpr int kSubMenuRowHeightUi = 34;
constexpr int kSubMenuRowBoxHUi = 27;
constexpr int kSubMenuStartYUi = 78;

constexpr int kFileRenameKeyColsUi = 8;
constexpr int kFileRenameKeyCountUi = 48;
constexpr int kFileRenameKeyWUi = 20;
constexpr int kFileRenameKeyHUi = 14;
constexpr int kFileRenameGapXUi = 6;
constexpr int kFileRenameGapYUi = 4;
constexpr int kFileRenameKeyStartXUi = 18;
constexpr int kFileRenameKeyStartYUi = 158;

String getWrappedTextViewerLine(int wrappedIndex, int maxCharsPerLine) {
    if (wrappedIndex < 0) {
        return "";
    }

    String line = "";
    line.reserve(maxCharsPerLine);
    int currentWrapped = 0;

    for (size_t i = 0; i < fileTextViewerContent.length(); i++) {
        char c = fileTextViewerContent[i];

        if (c == '\n') {
            if (currentWrapped == wrappedIndex) {
                return line;
            }
            currentWrapped++;
            line = "";
            line.reserve(maxCharsPerLine);
            continue;
        }

        line += c;
        if ((int)line.length() >= maxCharsPerLine) {
            if (currentWrapped == wrappedIndex) {
                return line;
            }
            currentWrapped++;
            line = "";
            line.reserve(maxCharsPerLine);
        }
    }

    return (currentWrapped == wrappedIndex) ? line : String("");
}

uint16_t lerpColor565(uint16_t from, uint16_t to, uint8_t t) {
    uint8_t fromR = (from >> 11) & 0x1F;
    uint8_t fromG = (from >> 5) & 0x3F;
    uint8_t fromB = from & 0x1F;
    uint8_t toR = (to >> 11) & 0x1F;
    uint8_t toG = (to >> 5) & 0x3F;
    uint8_t toB = to & 0x1F;

    uint8_t outR = fromR + (int(toR) - int(fromR)) * t / 255;
    uint8_t outG = fromG + (int(toG) - int(fromG)) * t / 255;
    uint8_t outB = fromB + (int(toB) - int(fromB)) * t / 255;

    return (uint16_t(outR) << 11) | (uint16_t(outG) << 5) | outB;
}

uint16_t getBatteryStatusColor(int pct) {
    const uint16_t white = ILI9341_WHITE;
    const uint16_t yellow = ILI9341_YELLOW;
    const uint16_t red = ILI9341_RED;

    // 100-50%: White
    if (pct >= 50) {
        return white;
    }
    // 50-20%: White to Yellow gradient
    if (pct >= 20) {
        uint8_t t = uint8_t(((50 - pct) * 255) / 30);
        return lerpColor565(white, yellow, t);
    }
    // 20-10%: Yellow to Red gradient
    if (pct >= 10) {
        uint8_t t = uint8_t(((20 - pct) * 255) / 10);
        return lerpColor565(yellow, red, t);
    }
    // Below 10%: Red
    return red;
}

void drawMenuSectionHeader(const char* title, const char* subtitle) {
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t divider = tft.color565(52, 52, 52);

    tft.fillRect(0, 20, 240, kMenuHeaderDividerY - 20, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(18, kMenuHeaderTitleY);
    tft.print(title);
    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(18, kMenuHeaderSubtitleY);
    tft.print(subtitle);
    tft.drawFastHLine(10, kMenuHeaderDividerY, 220, divider);
}

void drawMenuFooterHints(const char* leftText, const char* rightText) {
    const uint16_t divider = tft.color565(52, 52, 52);
    const uint16_t textSoft = tft.color565(160, 160, 160);

    tft.drawFastHLine(10, kMenuFooterDividerY, 220, divider);
    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(14, kMenuFooterTextY);
    tft.print(leftText);

    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(rightText, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(226 - (int)w, kMenuFooterTextY);
    tft.print(rightText);
}

void getHomeTileBounds(int index, int& x, int& y) {
    int row = index / 2;
    int col = index % 2;
    x = kHomeTileStartXUi + col * (kHomeTileWUi + kHomeTileGapXUi);
    y = kHomeTileStartYUi + row * (kHomeTileHUi + kHomeTileGapYUi);
}

void drawChargingBoltIcon(int x, int y, uint16_t color) {
    tft.drawLine(x + 4, y, x + 1, y + 4, color);
    tft.drawLine(x + 1, y + 4, x + 4, y + 4, color);
    tft.drawLine(x + 4, y + 4, x + 2, y + 8, color);
    tft.drawLine(x + 2, y + 8, x + 7, y + 3, color);
    tft.drawLine(x + 7, y + 3, x + 4, y + 3, color);
    tft.drawLine(x + 4, y + 3, x + 4, y, color);
}

void drawStatusNrf24Icon(int x, int y, uint16_t color) {
    tft.drawRect(x + 2, y + 3, 8, 8, color);
    tft.drawFastHLine(x + 3, y + 5, 6, color);
    tft.drawFastHLine(x + 3, y + 8, 6, color);
    tft.drawFastVLine(x + 10, y + 5, 3, color);
    tft.drawLine(x + 10, y + 6, x + 13, y + 4, color);
    tft.drawLine(x + 10, y + 7, x + 13, y + 9, color);
    tft.drawPixel(x + 1, y + 5, color);
    tft.drawPixel(x + 1, y + 8, color);
}

void drawStatusCc1101Icon(int x, int y, uint16_t color) {
    tft.drawFastVLine(x + 4, y + 5, 6, color);
    tft.drawLine(x + 3, y + 10, x + 4, y + 13, color);
    tft.drawLine(x + 5, y + 10, x + 4, y + 13, color);
    tft.drawPixel(x + 4, y + 3, color);
    tft.drawPixel(x + 2, y + 4, color);
    tft.drawPixel(x + 6, y + 4, color);
    tft.drawPixel(x + 1, y + 6, color);
    tft.drawPixel(x + 7, y + 6, color);
    tft.drawPixel(x, y + 8, color);
    tft.drawPixel(x + 8, y + 8, color);
    tft.drawFastHLine(x + 10, y + 6, 3, color);
    tft.drawFastHLine(x + 11, y + 8, 2, color);
}

void drawFileRenameKeyCellInternal(int index, bool selected) {
    if (index < 0 || index >= kFileRenameKeyCountUi) {
        return;
    }

    const char** keys = getFileRenameKeyArray();
    const char* label = keys[index];
    const int row = index / kFileRenameKeyColsUi;
    const int col = index % kFileRenameKeyColsUi;
    const int x = kFileRenameKeyStartXUi + col * (kFileRenameKeyWUi + kFileRenameGapXUi);
    const int y = kFileRenameKeyStartYUi + row * (kFileRenameKeyHUi + kFileRenameGapYUi);
    const uint16_t keyFill = tft.color565(28, 28, 28);
    const uint16_t keySelected = tft.color565(42, 42, 42);
    const uint16_t keyBorder = tft.color565(76, 76, 76);
    const uint16_t keyText = ILI9341_WHITE;

    tft.fillRoundRect(x, y, kFileRenameKeyWUi, kFileRenameKeyHUi, 3, selected ? keySelected : keyFill);
    tft.drawRoundRect(x, y, kFileRenameKeyWUi, kFileRenameKeyHUi, 3, selected ? ILI9341_WHITE : keyBorder);

    if (label == nullptr || label[0] == '\0') {
        return;
    }

    tft.setTextSize(1);
    tft.setTextColor(selected ? ILI9341_WHITE : keyText);
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
    int textX = x + ((kFileRenameKeyWUi - (int)w) / 2);
    int textY = y + ((kFileRenameKeyHUi - (int)h) / 2) + 1;
    tft.setCursor(textX, textY);
    tft.print(label);
}
}

void drawScreenSaverFrame(uint8_t styleIndex, bool resetFrame) {
    struct MoireLineSlot {
        int16_t ax;
        int16_t ay;
        int16_t bx;
        int16_t by;
        int16_t cx;
        int16_t cy;
        int16_t dx;
        int16_t dy;
        bool used;
    };

    static int moireAx = 24;
    static int moireAy = 44;
    static int moireBx = 216;
    static int moireBy = 88;
    static int moireCx = 36;
    static int moireCy = 280;
    static int moireDx = 204;
    static int moireDy = 236;
    static int moireVxA = 2;
    static int moireVyA = 1;
    static int moireVxB = -2;
    static int moireVyB = 1;
    static int moireVxC = 1;
    static int moireVyC = -2;
    static int moireVxD = -1;
    static int moireVyD = -2;
    static MoireLineSlot moireTrail[10];
    static int moireTrailWrite = 0;
    static float orbitPhase = 0.0f;
    static int orbitPrevX[6];
    static int orbitPrevY[6];
    static bool orbitHasPrev = false;
    static int rainY[10];
    static int rainSpeed[10];
    static int rainPrevTop[10];
    static int rainPrevBottom[10];
    static bool rainInit = false;
    static unsigned long pixelNextDotMs = 0;
    static uint16_t pixelDrawCount = 0;

    if (resetFrame) {
        tft.fillScreen(ILI9341_BLACK);
        moireAx = 24; moireAy = 44; moireBx = 216; moireBy = 88;
        moireCx = 36; moireCy = 280; moireDx = 204; moireDy = 236;
        moireVxA = 2; moireVyA = 1; moireVxB = -2; moireVyB = 1;
        moireVxC = 1; moireVyC = -2; moireVxD = -1; moireVyD = -2;
        for (int i = 0; i < 10; i++) {
            moireTrail[i].used = false;
        }
        moireTrailWrite = 0;
        orbitPhase = 0.0f;
        for (int i = 0; i < 6; i++) {
            orbitPrevX[i] = -20;
            orbitPrevY[i] = -20;
        }
        orbitHasPrev = false;
        for (int i = 0; i < 10; i++) {
            rainY[i] = (i * 28) % 320;
            rainSpeed[i] = 3 + (i % 4);
            rainPrevTop[i] = -1;
            rainPrevBottom[i] = -1;
        }
        rainInit = true;
        pixelDrawCount = 0;
        pixelNextDotMs = millis() + 1000UL + (unsigned long)random(0, 1001);
    }

    auto bouncePoint = [](int& x, int& y, int& vx, int& vy) {
        x += vx;
        y += vy;
        if (x < 0) { x = 0; vx = -vx; }
        if (x > 239) { x = 239; vx = -vx; }
        if (y < 0) { y = 0; vy = -vy; }
        if (y > 319) { y = 319; vy = -vy; }
    };

    switch (styleIndex % 4) {
        case 0: {
            MoireLineSlot& slot = moireTrail[moireTrailWrite];
            if (slot.used) {
                tft.drawLine(slot.ax, slot.ay, slot.bx, slot.by, ILI9341_BLACK);
                tft.drawLine(slot.cx, slot.cy, slot.dx, slot.dy, ILI9341_BLACK);
            }

            bouncePoint(moireAx, moireAy, moireVxA, moireVyA);
            bouncePoint(moireBx, moireBy, moireVxB, moireVyB);
            bouncePoint(moireCx, moireCy, moireVxC, moireVyC);
            bouncePoint(moireDx, moireDy, moireVxD, moireVyD);

            if ((random(0, 20) == 0)) {
                moireVxA = -moireVxA;
            }
            if ((random(0, 24) == 0)) {
                moireVyD = -moireVyD;
            }

            int lineOffset = (moireTrailWrite % 6) * 3;
            slot.ax = constrain(moireAx + lineOffset, 0, 239);
            slot.ay = moireAy;
            slot.bx = constrain(moireBx - lineOffset, 0, 239);
            slot.by = moireBy;
            slot.cx = moireCx;
            slot.cy = constrain(moireCy - (lineOffset * 2), 0, 319);
            slot.dx = moireDx;
            slot.dy = constrain(moireDy + (lineOffset * 2), 0, 319);
            slot.used = true;

            uint16_t colorA = tft.color565(80 + ((moireTrailWrite * 9) % 80),
                                           140 + ((moireTrailWrite * 7) % 70),
                                           200 + ((moireTrailWrite * 3) % 40));
            uint16_t colorB = tft.color565(40 + ((moireTrailWrite * 5) % 60),
                                           90 + ((moireTrailWrite * 6) % 80),
                                           170 + ((moireTrailWrite * 4) % 70));
            tft.drawLine(slot.ax, slot.ay, slot.bx, slot.by, colorA);
            tft.drawLine(slot.cx, slot.cy, slot.dx, slot.dy, colorB);
            moireTrailWrite = (moireTrailWrite + 1) % 10;
            break;
        }
        case 1: {
            orbitPhase += 0.08f;
            int cx = 120;
            int cy = 160;

            if (resetFrame) {
                tft.fillCircle(cx, cy, 2, tft.color565(120, 180, 255));
            }

            for (int i = 0; i < 6; i++) {
                if (orbitHasPrev && orbitPrevX[i] >= 0 && orbitPrevY[i] >= 0) {
                    tft.fillCircle(orbitPrevX[i], orbitPrevY[i], 2 + (i & 0x01), ILI9341_BLACK);
                }

                float phase = orbitPhase + (i * 1.17f);
                int rx = 24 + (i * 14);
                int ry = 20 + (i * 16);
                int x = cx + (int)(cosf(phase) * rx);
                int y = cy + (int)(sinf(phase * 1.3f) * ry);
                uint16_t color = tft.color565((70 + i * 22) & 0xFF, (120 + i * 18) & 0xFF, (255 - i * 18) & 0xFF);
                orbitPrevX[i] = x;
                orbitPrevY[i] = y;
                tft.fillCircle(x, y, 2 + (i & 0x01), color);
            }
            orbitHasPrev = true;
            break;
        }
        case 2: {
            if (!rainInit) {
                for (int i = 0; i < 10; i++) {
                    rainY[i] = (i * 28) % 320;
                    rainSpeed[i] = 3 + (i % 4);
                    rainPrevTop[i] = -1;
                    rainPrevBottom[i] = -1;
                }
                rainInit = true;
            }
            for (int i = 0; i < 10; i++) {
                int x = 18 + (i * 22);

                if (rainPrevTop[i] >= 0 && rainPrevBottom[i] >= rainPrevTop[i]) {
                    int clearHeight = rainPrevBottom[i] - rainPrevTop[i] + 1;
                    tft.fillRect(max(0, x - 3), rainPrevTop[i], 7, clearHeight, ILI9341_BLACK);
                }

                int y = rainY[i];
                int top = max(0, y - 18);
                int bottom = min(319, y + 2);
                uint16_t head = tft.color565(90 + i * 10, 220, 255);
                uint16_t trail = tft.color565(20, 80 + i * 10, 120 + i * 8);
                if (bottom >= top) {
                    tft.drawFastVLine(x, top, bottom - top + 1, trail);
                    rainPrevTop[i] = top;
                    rainPrevBottom[i] = bottom;
                } else {
                    rainPrevTop[i] = -1;
                    rainPrevBottom[i] = -1;
                }
                if (y >= 0 && y < 320) {
                    tft.fillCircle(x, y, 2, head);
                }
                rainY[i] += rainSpeed[i];
                if (rainY[i] > 338) {
                    rainY[i] = -18 - (i * 7);
                }
            }
            break;
        }
        default: {
            unsigned long nowMs = millis();
            if (resetFrame) {
                pixelDrawCount = 0;
                pixelNextDotMs = nowMs + 1000UL + (unsigned long)random(0, 1001);
            }

            if (nowMs < pixelNextDotMs) {
                return;
            }

            int x = random(0, 239);
            int y = random(0, 319);
            tft.drawPixel(x, y, ILI9341_WHITE);
            tft.drawPixel(min(239, x + 1), y, ILI9341_WHITE);
            tft.drawPixel(x, min(319, y + 1), ILI9341_WHITE);
            tft.drawPixel(min(239, x + 1), min(319, y + 1), ILI9341_WHITE);
            pixelDrawCount += 4;
            pixelNextDotMs = nowMs + 1000UL + (unsigned long)random(0, 1001);

            if (pixelDrawCount >= 1800) {
                tft.fillScreen(ILI9341_BLACK);
                pixelDrawCount = 0;
            }
            break;
        }
    }
}

void drawWirelessUpdateScreen() {
    const uint16_t panel = tft.color565(18, 18, 18);
    const uint16_t panelBorder = tft.color565(76, 76, 76);
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t accent = tft.color565(236, 236, 236);

    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();

    tft.fillRoundRect(10, 30, 220, 236, 12, panel);
    tft.drawRoundRect(10, 30, 220, 236, 12, panelBorder);
    tft.drawFastHLine(18, 67, 204, tft.color565(52, 52, 52));

    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(22, 40);
    tft.print("Wireless Update");

    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(18, 58);
    tft.print("Join AP, open page, upload firmware");

    drawBLEBeaconCard(18, 82, 204, 34, "Status", accent);
    drawBLEBeaconCard(18, 120, 204, 34, "SSID", accent);
    drawBLEBeaconCard(18, 158, 204, 34, "Password", accent);
    drawBLEBeaconCard(18, 196, 204, 34, "Open", accent);

    auto drawValue = [&](int x, int y, const String& value, uint16_t color) {
        tft.fillRect(x, y, 184, 12, panel);
        tft.setTextSize(1);
        tft.setTextColor(color);
        tft.setCursor(x, y + 2);
        tft.print(value);
    };

    bool readyLike = wirelessUpdateStatus.startsWith("AP ready") || wirelessUpdateStatus.startsWith("Updating") || wirelessUpdateStatus.startsWith("Update complete");
    drawValue(26, 95, wirelessUpdateStatus, readyLike ? ILI9341_GREEN : ILI9341_WHITE);
    drawValue(26, 133, wirelessUpdateSSID, ILI9341_WHITE);
    drawValue(26, 171, wirelessUpdatePassword, ILI9341_WHITE);
    drawValue(26, 209, String("http://") + wirelessUpdateIP, accent);

    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(20, 246);
    tft.print("Browser upload page for firmware .bin");
    tft.setCursor(54, 286);
    tft.print("LEFT back");
}

void ui_frameTick() {
    unsigned long nowMs = millis();
    if (lastFrameMs == 0) {
        lastFrameMs = nowMs;
        return;
    }

    unsigned long frameMs = nowMs - lastFrameMs;
    lastFrameMs = nowMs;
    smoothedFrameMs = (smoothedFrameMs <= 0.0f)
        ? (float)frameMs
        : ((smoothedFrameMs * 0.9f) + ((float)frameMs * 0.1f));
}

void ui_renderDebugOverlay() {
    if (!debugOverlayEnabled || displayUpdatesSuspended || currentScreen != SCREEN_MAIN) {
        return;
    }

    uint16_t bg = tft.color565(0, 0, 0);
    uint16_t fg = tft.color565(180, 180, 180);
    tft.fillRect(156, 2, 82, 16, bg);
    tft.setTextSize(1);
    tft.setTextColor(fg);
    tft.setCursor(158, 4);
    tft.print((int)roundf(1000.0f / max(1.0f, smoothedFrameMs)));
    tft.print("fps ");
    tft.print(ESP.getFreeHeap() / 1024);
    tft.print("k");
}

void drawWiFiDetail() {
    bool liveValid = (wifiDetailIndex >= 0 && wifiDetailIndex < wifiNetworkCount);
    if (!liveValid && !wifiDetailSnapshotValid) {
        if (!wifiDetailActive) {
            wifiDetailActive = true;
            tft.fillScreen(ILI9341_BLACK);
            drawStatusBar();
            tft.setTextSize(2);
            tft.setTextColor(tft.color565(100, 200, 255));
            tft.setCursor(18, 40);
            tft.print("WiFi Detail");
            tft.setTextSize(1);
            tft.setTextColor(tft.color565(160, 160, 160));
            tft.setCursor(18, 88);
            tft.print("Network details expired.");
            tft.setCursor(18, 104);
            tft.print("LEFT back, then rescan.");
        }
        return;
    }

    int i = wifiDetailIndex;
    const char* detailSSID = wifiDetailSnapshotValid ? wifiDetailSnapshotSSID : wifiSSIDs[i];
    const char* detailBSSID = wifiDetailSnapshotValid ? wifiDetailSnapshotBSSID : wifiBSSID[i];
    const char* detailEnc = wifiDetailSnapshotValid ? wifiDetailSnapshotEnc : wifiEncStr[i];
    const char* detailHint = wifiDetailSnapshotValid ? wifiDetailSnapshotHint : "";
    int detailRSSI = wifiDetailSnapshotValid ? wifiDetailSnapshotRSSI : wifiRSSI[i];
    int detailChannel = wifiDetailSnapshotValid ? wifiDetailSnapshotChannel : wifiChannel[i];
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t accent = tft.color565(100, 200, 255);  // Blue like WiFi scanner
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t rowFill = tft.color565(18, 18, 18);
    const uint16_t rowBorder = tft.color565(44, 44, 44);

    // First draw - full screen
    if (!wifiDetailActive) {
        wifiDetailActive = true;
        
        tft.fillScreen(ILI9341_BLACK);
        drawStatusBar();

        // Title
        tft.drawFastHLine(8, 68, 224, tft.color565(52, 52, 52));
        tft.setTextSize(2);
        tft.setTextColor(accent);
        tft.setCursor(18, 40);
        char title[19];
        strncpy(title, detailSSID, 18); title[18] = 0;
        if (title[0] == 0) strcpy(title, "(hidden)");
        tft.print(title);

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("Network details");

        // Draw static card backgrounds
        int y = 78;
        
        // Signal card
        tft.fillRoundRect(8, y, 108, 32, 4, rowFill);
        tft.drawRoundRect(8, y, 108, 32, 4, rowBorder);
        tft.setTextColor(textSoft);
        tft.setCursor(14, y + 6);
        tft.print("Signal");

        // Channel card
        tft.fillRoundRect(124, y, 108, 32, 4, rowFill);
        tft.drawRoundRect(124, y, 108, 32, 4, rowBorder);
        tft.setTextColor(textSoft);
        tft.setCursor(130, y + 6);
        tft.print("Channel");

        y += 40;

        // Security card
        tft.fillRoundRect(8, y, 108, 32, 4, rowFill);
        tft.drawRoundRect(8, y, 108, 32, 4, rowBorder);
        tft.setTextColor(textSoft);
        tft.setCursor(14, y + 6);
        tft.print("Security");

        // Distance card
        tft.fillRoundRect(124, y, 108, 32, 4, rowFill);
        tft.drawRoundRect(124, y, 108, 32, 4, rowBorder);
        tft.setTextColor(textSoft);
        tft.setCursor(130, y + 6);
        tft.print("Distance");

        y += 40;

        // MAC card
        tft.fillRoundRect(8, y, 224, 32, 4, rowFill);
        tft.drawRoundRect(8, y, 224, 32, 4, rowBorder);
        tft.setTextColor(textSoft);
        tft.setCursor(14, y + 6);
        tft.print("MAC Address");
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(14, y + 18);
        tft.print(detailBSSID);

        // Instructions
        tft.setTextColor(textSoft);
        tft.setCursor(12, 278);
        tft.print(detailHint[0] ? detailHint : "LEFT back");
    }

    // Live update - only update dynamic values
    int y = 78;
    
    // Update Signal
    tft.fillRect(14, y + 18, 90, 10, rowFill);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    tft.setCursor(14, y + 18);
    tft.print(detailRSSI);
    tft.print(" dBm");

    // Update Channel
    tft.fillRect(130, y + 18, 90, 10, rowFill);
    tft.setCursor(130, y + 18);
    tft.print(detailChannel);

    y += 40;

    // Update Security
    tft.fillRect(14, y + 18, 90, 10, rowFill);
    tft.setCursor(14, y + 18);
    tft.print(detailEnc);

    // Update Distance
    tft.fillRect(130, y + 18, 90, 10, rowFill);
    tft.setCursor(130, y + 18);
    float dist = estimateDistance(detailRSSI);
    tft.print(dist, 1);
    tft.print(" m");
}

void drawBLEDetail() {
    // Bounds check to prevent crash
    if (bleDetailIndex < 0 || bleDetailIndex >= bleDeviceCount) {
        return;
    }

    int i = bleDetailIndex;
    const uint16_t bg = ILI9341_BLACK;
    const uint16_t accent = tft.color565(150, 100, 255);  // Purple like BLE scanner
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t rowFill = tft.color565(18, 18, 18);
    const uint16_t rowBorder = tft.color565(44, 44, 44);

    // First draw - full screen
    if (!bleDetailActive) {
        bleDetailActive = true;
        
        tft.fillScreen(ILI9341_BLACK);
        drawStatusBar();

        // Title
        tft.drawFastHLine(8, 68, 224, tft.color565(52, 52, 52));
        tft.setTextSize(2);
        tft.setTextColor(accent);
        tft.setCursor(18, 40);
        char title[19];
        strncpy(title, bleNames[i], 18); title[18] = 0;
        if (strlen(title) == 0) strcpy(title, "Unknown");
        tft.print(title);

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("Device details");

        // Draw static card backgrounds
        int y = 78;
        
        // Signal card
        tft.fillRoundRect(8, y, 108, 32, 4, rowFill);
        tft.drawRoundRect(8, y, 108, 32, 4, rowBorder);
        tft.setTextColor(textSoft);
        tft.setCursor(14, y + 6);
        tft.print("Signal");

        // Distance card
        tft.fillRoundRect(124, y, 108, 32, 4, rowFill);
        tft.drawRoundRect(124, y, 108, 32, 4, rowBorder);
        tft.setTextColor(textSoft);
        tft.setCursor(130, y + 6);
        tft.print("Distance");

        y += 40;

        // MAC card
        tft.fillRoundRect(8, y, 224, 32, 4, rowFill);
        tft.drawRoundRect(8, y, 224, 32, 4, rowBorder);
        tft.setTextColor(textSoft);
        tft.setCursor(14, y + 6);
        tft.print("MAC Address");
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(14, y + 18);
        tft.print(bleMACs[i]);

        y += 40;

        // Name card
        tft.fillRoundRect(8, y, 224, 32, 4, rowFill);
        tft.drawRoundRect(8, y, 224, 32, 4, rowBorder);
        tft.setTextColor(textSoft);
        tft.setCursor(14, y + 6);
        tft.print("Device Name");
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(14, y + 18);
        if (strlen(bleNames[i]) > 0) {
            tft.print(bleNames[i]);
        } else {
            tft.setTextColor(textSoft);
            tft.print("(no name)");
        }

        // Instructions
        tft.setTextColor(textSoft);
        tft.setCursor(12, 278);
        tft.print("LEFT back");
    }

    // Live update - only update dynamic values
    int y = 78;
    
    // Update Signal
    tft.fillRect(14, y + 18, 90, 10, rowFill);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    tft.setCursor(14, y + 18);
    tft.print(bleRSSI[i]);
    tft.print(" dBm");

    // Update Distance
    tft.fillRect(130, y + 18, 90, 10, rowFill);
    tft.setCursor(130, y + 18);
    float dist = estimateDistance(bleRSSI[i]);
    tft.print(dist, 1);
    tft.print(" m");
}

void drawBLEDetailOld() {
    if (bleDetailActive) return;
    bleDetailActive = true;

    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();

    uint16_t panel = tft.color565(18, 18, 18);
    uint16_t panelBorder = tft.color565(76, 76, 76);
    uint16_t accent = tft.color565(236, 236, 236);
    uint16_t textSoft = tft.color565(160, 160, 160);

    if (bleDetailIndex < 0 || bleDetailIndex >= bleDeviceCount) {
        tft.fillRoundRect(8, 28, 224, 202, 12, panel);
        tft.drawRoundRect(8, 28, 224, 202, 12, panelBorder);
        tft.drawFastHLine(18, 67, 204, tft.color565(52, 52, 52));
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 40);
        tft.print("BLE Detail");
        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("No device selected");
        tft.setCursor(12, 240);
        tft.print("Hint");
        tft.setCursor(52, 240);
        tft.print("LEFT back");
        return;
    }

    int i = bleDetailIndex;
    const char* name = bleNames[i][0] ? bleNames[i] : "Unknown";

    tft.fillRoundRect(8, 28, 224, 202, 12, panel);
    tft.drawRoundRect(8, 28, 224, 202, 12, panelBorder);
    tft.drawFastHLine(18, 67, 204, tft.color565(52, 52, 52));

    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(18, 40);
    char nameShort[19]; strncpy(nameShort, name, 18); nameShort[18] = 0;
    tft.print(nameShort);

    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(18, 58);
    tft.print("BLE device details");

    drawBLEBeaconCard(16, 78, 100, 44, "Signal", accent);
    drawBLEBeaconCard(124, 78, 100, 44, "Distance", accent);
    drawBLEBeaconCard(16, 130, 208, 44, "MAC", accent);
    drawBLEBeaconCard(16, 182, 208, 44, "Label", accent);

    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(24, 96);
    tft.print(bleRSSI[i]);
    tft.print(" dBm");

    tft.setCursor(132, 96);
    tft.print(estimateDistance(bleRSSI[i]), 2);
    tft.print(" m");

    tft.setTextColor(textSoft);
    tft.setCursor(24, 148);
    tft.print(bleMACs[i]);

    tft.setCursor(24, 200);
    char label[25]; strncpy(label, name, 24); label[24] = 0;
    tft.print(label);

    tft.setCursor(12, 240);
    tft.print("Hint");
    tft.setCursor(52, 240);
    tft.print("LEFT back");
}

void drawFileManager() {
    if (!fileManagerNeedsRedraw) return;
    fileManagerNeedsRedraw = false;

    const uint16_t bg = ILI9341_BLACK;
    const uint16_t accent = tft.color565(236, 236, 236);
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const uint16_t rowFill = tft.color565(18, 18, 18);
    const uint16_t rowSelected = tft.color565(34, 34, 34);
    const int visibleItems = 6;
    const int rowX = 8;
    const int rowW = 224;
    const int rowH = 28;
    const int rowStep = 32;
    const int firstRowY = 82;

    drawStatusBar();
    tft.fillRect(0, 20, 240, 300, bg);
    tft.drawFastHLine(10, 68, 220, tft.color565(52, 52, 52));
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(18, 40);
    tft.print("File Manager");
    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(18, 58);
    String pathLabel = fileManagerPath;
    if (pathLabel.length() > 24) pathLabel = "..." + pathLabel.substring(pathLabel.length() - 21);
    tft.print(pathLabel);

    if (!sdMounted || sdError) {
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(30, 126);
        tft.print("SD card not mounted");
        tft.setTextColor(textSoft);
        tft.setCursor(22, 146);
        tft.print("Insert card or mount it");
        tft.setCursor(12, 258);
        tft.print("SEL retry");
        tft.setCursor(12, 274);
        tft.print("LEFT back");
        return;
    }

    drawCountBadge(186, 38, fileManagerCount);

    if (fileManagerSelectedIndex < fileManagerScrollOffset) fileManagerScrollOffset = fileManagerSelectedIndex;
    if (fileManagerSelectedIndex >= fileManagerScrollOffset + visibleItems) {
        fileManagerScrollOffset = fileManagerSelectedIndex - visibleItems + 1;
    }

    drawListScrollArrows(fileManagerCount, visibleItems, fileManagerScrollOffset);

    for (int i = 0; i < visibleItems; i++) {
        int index = i + fileManagerScrollOffset;
        if (index >= fileManagerCount) break;

        int y = firstRowY + (i * rowStep);
        bool selected = (index == fileManagerSelectedIndex);
        tft.fillRoundRect(rowX, y - 4, rowW, rowH, 5, selected ? rowSelected : rowFill);
        tft.drawRoundRect(rowX, y - 4, rowW, rowH, 5, selected ? accent : tft.color565(44, 44, 44));

        tft.setTextSize(1);
        tft.setTextColor(fileManagerIsDir[index] ? accent : ILI9341_WHITE);
        tft.setCursor(18, y + 4);
        String label = fileManagerEntries[index];
        String typePath = fileManagerFullPaths[index];
        typePath.toLowerCase();
        if (label.length() > 11) label = label.substring(0, 11);
        tft.print(fileManagerMarked[index] ? "* " : "  ");
        if (fileManagerIsDir[index]) {
            tft.print("[DIR] ");
        } else if (typePath.endsWith(".sub")) {
            tft.print("[SUB] ");
        } else {
            tft.print("[FIL] ");
        }
        tft.print(label);

        if (!fileManagerIsDir[index]) {
            tft.setTextColor(textSoft);
            String sizeLabel = String(fileManagerSizes[index]) + " B";
            int16_t x1, y1;
            uint16_t w, h;
            tft.getTextBounds(sizeLabel, 0, 0, &x1, &y1, &w, &h);
            tft.setCursor(222 - (int)w, y + 4);
            tft.print(sizeLabel);
        }
    }

    tft.setTextColor(textSoft);
    tft.setCursor(12, 258);
    if (fileManagerMarkedCount > 0) {
        tft.print("SEL delete  RIGHT mark ");
        tft.print(fileManagerMarkedCount);
    } else {
        tft.print("SEL load  HOLD details");
    }
    tft.setCursor(12, 274);
    tft.print("RIGHT mark  LEFT ");
    tft.print(fileManagerPath == "/" ? "back" : "up");
}

void drawFileDetailScreen() {
    const uint16_t panel = tft.color565(18, 18, 18);
    const uint16_t border = tft.color565(60, 60, 60);
    const uint16_t textSoft = tft.color565(160, 160, 160);

    int index = findFileManagerIndexByPath(fileDetailPath);

    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();

    if (fileTextViewerActive) {
        const int visibleLines = 12;
        const int maxCharsPerLine = 28;

        tft.fillRoundRect(10, 28, 220, 214, 10, panel);
        tft.drawRoundRect(10, 28, 220, 214, 10, border);
        tft.drawFastHLine(18, 68, 204, tft.color565(52, 52, 52));

        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 40);
        tft.print("Text Viewer");

        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        String leaf = getFileManagerLeafName(fileDetailPath);
        if (leaf.length() > 24) leaf = leaf.substring(0, 24);
        tft.print(leaf);

        tft.setTextColor(ILI9341_WHITE);
        for (int i = 0; i < visibleLines; i++) {
            int wrappedIndex = fileTextViewerScrollLine + i;
            if (wrappedIndex >= fileTextViewerLineCount) {
                break;
            }

            String line = getWrappedTextViewerLine(wrappedIndex, maxCharsPerLine);
            tft.setCursor(18, 82 + (i * 12));
            tft.print(line);
        }

        tft.setTextColor(textSoft);
        tft.setCursor(18, 230);
        tft.print(String(fileTextViewerScrollLine + 1) + "/" + String(max(1, fileTextViewerLineCount)));
        if (fileTextViewerTruncated) {
            tft.setCursor(80, 230);
            tft.print("preview");
        }

        tft.setCursor(12, 258);
        tft.print("UP/DN scroll  SEL details");
        tft.setCursor(12, 274);
        tft.print("LEFT back");
        return;
    }

    tft.fillRoundRect(10, 28, 220, 214, 10, panel);
    tft.drawRoundRect(10, 28, 220, 214, 10, border);
    tft.drawFastHLine(18, 68, 204, tft.color565(52, 52, 52));

    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(18, 40);
    tft.print("File Detail");

    if (index < 0) {
        tft.setTextSize(1);
        tft.setTextColor(textSoft);
        tft.setCursor(18, 58);
        tft.print("File no longer available");
        tft.setCursor(12, 258);
        tft.print("LEFT back");
        return;
    }

    String lowerPath = fileManagerFullPaths[index];
    lowerPath.toLowerCase();
    String type = fileManagerIsDir[index]
        ? "Folder"
        : (lowerPath.endsWith(".sub") ? ".sub file" : (lowerPath.endsWith(".txt") ? ".txt file" : "File"));
    String sizeText = fileManagerIsDir[index] ? "--" : String(fileManagerSizes[index]) + " B";
    bool marked = fileManagerMarked[index];
    bool loaded = rfSubLoaded && rfSubLoadedPath == fileManagerFullPaths[index];

    drawBLEBeaconCard(16, 78, 100, 44, "Type", ILI9341_WHITE);
    drawBLEBeaconCard(124, 78, 100, 44, "Size", ILI9341_WHITE);
    drawBLEBeaconCard(16, 130, 100, 44, "Marked", ILI9341_WHITE);
    drawBLEBeaconCard(124, 130, 100, 44, "Loaded", ILI9341_WHITE);
    drawBLEBeaconCard(16, 182, 208, 44, "Saved / Path", ILI9341_WHITE);

    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(24, 96);
    tft.print(type);
    tft.setCursor(132, 96);
    tft.print(sizeText);
    tft.setCursor(24, 148);
    tft.print(marked ? "Yes" : "No");
    tft.setCursor(132, 148);
    tft.print(loaded ? "Active" : "No");

    tft.setTextColor(textSoft);
    tft.setCursor(24, 198);
    tft.print(fileDetailSavedText);
    tft.setCursor(24, 210);
    String pathLine = fileManagerFullPaths[index];
    if (pathLine.length() > 28) pathLine = "..." + pathLine.substring(pathLine.length() - 25);
    tft.print(pathLine);

    if (millis() < g_fileDetailToastUntil && g_fileDetailToast.length() > 0) {
        tft.setTextColor(tft.color565(180, 220, 120));
        tft.setCursor(12, 236);
        tft.print(g_fileDetailToast);
    }

    int hintY = 258;
    if (lowerPath.endsWith(".sub")) {
        tft.setTextColor(textSoft);
        tft.setCursor(12, 250);
        tft.print("UP add favorite");
        hintY = 266;
    }
    tft.setTextColor(textSoft);
    tft.setCursor(12, hintY);
    tft.print("SEL rename  RIGHT delete");
    tft.setCursor(12, hintY + 16);
    tft.print("LEFT back");
}

void drawFileRenameScreen() {
    const uint16_t panel = tft.color565(18, 18, 18);
    const uint16_t border = tft.color565(60, 60, 60);
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const char** keys = getFileRenameKeyArray();

    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();

    tft.fillRoundRect(10, 30, 220, 248, 10, panel);
    tft.drawRoundRect(10, 30, 220, 248, 10, border);
    tft.drawFastHLine(18, 68, 204, tft.color565(52, 52, 52));

    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(18, 42);
    tft.print("Rename File");

    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(18, 58);
    String oldName = getFileManagerLeafName(fileRenamePath);
    if (oldName.length() > 28) oldName = oldName.substring(0, 28);
    tft.print(oldName);

    redrawFileRenameDraftArea();

    for (int i = 0; i < kFileRenameKeyCountUi; i++) {
        const char* label = keys[i];
        if (label == nullptr || label[0] == '\0') {
            continue;
        }
        drawFileRenameKeyCellInternal(i, i == fileRenameCursor);
    }

    redrawFileRenameSelection(-1, fileRenameCursor);
}

void redrawFileRenameSelection(int previousIndex, int currentIndex) {
    if (previousIndex == currentIndex && currentIndex >= 0) {
        drawFileRenameKeyCellInternal(currentIndex, true);
        return;
    }
    drawFileRenameKeyCellInternal(previousIndex, false);
    drawFileRenameKeyCellInternal(currentIndex, true);
}

void redrawFileRenameDraftArea() {
    const uint16_t textSoft = tft.color565(160, 160, 160);

    tft.fillRect(16, 80, 208, 66, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(18, 84);
    tft.print("Name");
    tft.drawRoundRect(16, 98, 208, 34, 6, tft.color565(80, 80, 80));
    tft.fillRect(20, 104, 200, 22, ILI9341_BLACK);

    String shown = fileRenameDraft;
    if (shown.length() == 0) shown = "_";
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(24, 112);
    tft.print(shown.substring(0, min((int)shown.length(), 16)));

    tft.setTextColor(textSoft);
    tft.setCursor(18, 138);
    tft.print("Ext");
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(52, 138);
    tft.print(fileRenameExtension.length() > 0 ? fileRenameExtension : "(none)");

    tft.fillRect(16, 264, 208, 22, ILI9341_BLACK);
    if (fileRenameStatusUntil > millis() && fileRenameStatus.length() > 0) {
        tft.setTextColor(textSoft);
        tft.setCursor(18, 266);
        tft.print(fileRenameStatus);
    }

    tft.setTextColor(textSoft);
    tft.setCursor(12, 282);
    tft.print("D-pad move  SEL choose");
}

void drawFileDeleteConfirmScreen() {
    const uint16_t panel = tft.color565(18, 18, 18);
    const uint16_t border = tft.color565(60, 60, 60);
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const bool deleteSelected = fileDeleteConfirmYes;

    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();

    tft.fillRoundRect(14, 44, 212, 186, 10, panel);
    tft.drawRoundRect(14, 44, 212, 186, 10, border);

    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    int16_t tx1, ty1;
    uint16_t tw, th;
    tft.getTextBounds("Delete Files", 0, 0, &tx1, &ty1, &tw, &th);
    tft.setCursor((240 - (int)tw) / 2, 62);
    tft.print("Delete Files");

    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(26, 102);
    if (fileDeleteBatchMode) {
        tft.print("Delete ");
        tft.print(fileDeleteTargetCount);
        tft.print(" selected files?");
    } else {
        tft.print("Delete this item?");
    }

    String leaf = getFileManagerLeafName(fileDeleteTargetPath);
    if (fileDeleteBatchMode) {
        leaf = "Marked entries";
    }
    if (leaf.length() > 22) leaf = leaf.substring(0, 22);
    tft.setCursor(26, 126);
    tft.print(leaf);

    tft.fillRoundRect(30, 246, 82, 34, 8, deleteSelected ? panel : tft.color565(36, 36, 36));
    tft.drawRoundRect(30, 246, 82, 34, 8, deleteSelected ? border : ILI9341_WHITE);
    tft.fillRoundRect(128, 246, 82, 34, 8, deleteSelected ? tft.color565(36, 36, 36) : panel);
    tft.drawRoundRect(128, 246, 82, 34, 8, deleteSelected ? ILI9341_WHITE : border);

    tft.setTextSize(2);
    tft.setTextColor(deleteSelected ? tft.color565(170, 170, 170) : ILI9341_WHITE);
    tft.getTextBounds("Cancel", 0, 0, &tx1, &ty1, &tw, &th);
    tft.setCursor(30 + ((82 - (int)tw) / 2), 256);
    tft.print("Cancel");
    tft.setTextColor(deleteSelected ? ILI9341_WHITE : tft.color565(170, 170, 170));
    tft.getTextBounds("Delete", 0, 0, &tx1, &ty1, &tw, &th);
    tft.setCursor(128 + ((82 - (int)tw) / 2), 256);
    tft.print("Delete");

    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(12, 292);
    tft.print("LEFT back  UP/DN choose  SEL confirm");
}

void drawSDFormatConfirmScreen() {
    const uint16_t panel = tft.color565(18, 18, 18);
    const uint16_t border = tft.color565(60, 60, 60);
    const uint16_t accent = ILI9341_WHITE;
    const uint16_t textSoft = tft.color565(160, 160, 160);
    const bool eraseSelected = sdFormatConfirmEraseSelected;

    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();
    tft.fillRoundRect(14, 44, 212, 186, 10, panel);
    tft.drawRoundRect(14, 44, 212, 186, 10, border);

    tft.setTextSize(2);
    tft.setTextColor(accent);
    int16_t tx1, ty1;
    uint16_t tw, th;
    tft.getTextBounds("Format SD", 0, 0, &tx1, &ty1, &tw, &th);
    tft.setCursor((240 - (int)tw) / 2, 62);
    tft.print("Format SD");

    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(30, 98);
    tft.print("This deletes all files on");
    tft.setCursor(30, 112);
    tft.print("the mounted SD card.");
    tft.setCursor(30, 136);
    tft.print("Toolkit folders will be");
    tft.setCursor(30, 150);
    tft.print("rebuilt automatically.");
    tft.setCursor(30, 174);
    tft.print("Card must already mount");
    tft.setCursor(30, 188);
    tft.print("as FAT/FAT32.");

    tft.fillRoundRect(30, 246, 82, 34, 8, eraseSelected ? panel : tft.color565(36, 36, 36));
    tft.drawRoundRect(30, 246, 82, 34, 8, eraseSelected ? border : accent);
    tft.fillRoundRect(128, 246, 82, 34, 8, eraseSelected ? tft.color565(36, 36, 36) : panel);
    tft.drawRoundRect(128, 246, 82, 34, 8, eraseSelected ? accent : border);

    tft.setTextSize(2);
    tft.setTextColor(eraseSelected ? tft.color565(170, 170, 170) : accent);
    tft.getTextBounds("Cancel", 0, 0, &tx1, &ty1, &tw, &th);
    tft.setCursor(30 + ((82 - (int)tw) / 2), 256);
    tft.print("Cancel");
    tft.setTextColor(eraseSelected ? accent : tft.color565(170, 170, 170));
    tft.getTextBounds("Erase", 0, 0, &tx1, &ty1, &tw, &th);
    tft.setCursor(128 + ((82 - (int)tw) / 2), 256);
    tft.print("Erase");

    tft.setTextSize(1);
    tft.setTextColor(textSoft);
    tft.setCursor(12, 292);
    tft.print("LEFT back  UP/DN choose  SEL confirm");
}

void drawStatusBar() {
    uint16_t barColor = tft.color565(50, 50, 50);
    uint16_t inactiveColor = tft.color565(130, 130, 130);

    tft.fillRect(0, 0, 240, 18, barColor);
    tft.drawFastHLine(0, 18, 240, tft.color565(80, 80, 80));

    uint16_t sdColor;
    if (sdError) {
        sdColor = ILI9341_RED;
    } else if (sdMounted) {
        sdColor = ILI9341_WHITE;
    } else {
        sdColor = inactiveColor;
    }
    if (flashFsReady) {
        drawIconFromFSColored(6, 1, "/icons/sd.bin", sdColor, 16);
    } else {
        tft.fillRect(6, 2, 12, 12, sdColor);
    }
    if (sdMounted && !sdError && orionToolsSdLowSpace()) {
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(22, 5);
        tft.print("!");
    }

    drawStatusNrf24Icon(24, 1, radio1Ok ? ILI9341_GREEN : ILI9341_RED);
drawStatusCc1101Icon(40, 1, cc1101Ok ? ILI9341_GREEN : ILI9341_RED);
    drawStatusNrf24Icon(56, 1, radio3Ok ? ILI9341_GREEN : ILI9341_RED);

    int bx = 200;
    int by = 4;
    int pct = batteryPercent;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    uint16_t batteryColor = chargerConnected ? ILI9341_GREEN : getBatteryStatusColor(pct);
    String batteryLabel = String(pct) + "%";
    tft.setTextSize(1);
    tft.setTextColor(batteryColor);
    int16_t labelX1, labelY1;
    uint16_t labelW, labelH;
    tft.getTextBounds(batteryLabel, 0, 0, &labelX1, &labelY1, &labelW, &labelH);
    int batteryLabelX = bx - (int)labelW - 6;
    int chargeIconX = batteryLabelX - 10;
    tft.setCursor(batteryLabelX, 5);
    tft.print(batteryLabel);

    if (chargerConnected) {
        drawChargingBoltIcon(chargeIconX, 4, ILI9341_GREEN);
    }

    if (currentTitle != "") {
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_WHITE);

        int16_t x1, y1;
        uint16_t w, h;
        String titleText = currentTitle;
        const int titleLeft = 74;
        const int titleRight = (chargerConnected ? chargeIconX - 4 : batteryLabelX - 6);
        const int titleMaxW = max(0, titleRight - titleLeft);

        tft.getTextBounds(titleText, 0, 0, &x1, &y1, &w, &h);
        if ((int)w > titleMaxW) {
            titleText = currentTitle;
            while (titleText.length() > 1) {
                String candidate = titleText + "..";
                tft.getTextBounds(candidate, 0, 0, &x1, &y1, &w, &h);
                if ((int)w <= titleMaxW) {
                    titleText = candidate;
                    break;
                }
                titleText.remove(titleText.length() - 1);
            }
        }

        tft.getTextBounds(titleText, 0, 0, &x1, &y1, &w, &h);
        int titleX = titleLeft + max(0, (titleMaxW - (int)w) / 2);
        tft.setCursor(titleX, 5);
        tft.print(titleText);
    }

    tft.drawRect(bx, by, 24, 10, batteryColor);
    tft.fillRect(bx + 24, by + 3, 3, 4, batteryColor);
    int fillW = ((20 * pct) + 99) / 100;
    if (fillW > 0) {
        tft.fillRect(bx + 2, by + 2, fillW, 6, batteryColor);
    }
}

void drawUI() {
    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();
    drawMenuSectionHeader("Skull Breaker", "Choose a module");

    for (int i = 0; i < totalItems; i++) {
        int x = 0;
        int y = 0;
        getHomeTileBounds(i, x, y);
        drawTile(x, y, items[i], i, i == selectedIndex);
    }

    drawMenuFooterHints("UP/DN move", "SEL open");
}

void drawTile(int x, int y, const char* label, int iconType, bool isSelected) {
    const uint16_t tileColor = tft.color565(18, 18, 18);
    const uint16_t selectedColor = tft.color565(34, 34, 34);
    const uint16_t borderColor = tft.color565(52, 52, 52);
    const uint16_t accent = tft.color565(236, 236, 236);
    const uint16_t divider = tft.color565(64, 64, 64);
    const int radius = 8;
    const int renderedIconSize = 18;

    tft.fillRoundRect(x, y, kHomeTileWUi, kHomeTileHUi, radius, isSelected ? selectedColor : tileColor);
    tft.drawRoundRect(x, y, kHomeTileWUi, kHomeTileHUi, radius, isSelected ? accent : borderColor);

    if (isSelected) {
        tft.drawRoundRect(x - 2, y - 2, kHomeTileWUi + 4, kHomeTileHUi + 4, radius, ILI9341_WHITE);
    }

    tft.drawFastHLine(x + 14, y + 42, kHomeTileWUi - 28, divider);

    int iconX = x + (kHomeTileWUi - renderedIconSize) / 2;
    int iconY = y + 10;

    drawScaledIconFromFSColored(iconX, iconY, iconPaths[iconType], 36, 2, isSelected ? accent : ILI9341_WHITE);

    tft.setTextSize(1);
    tft.setTextColor(isSelected ? accent : ILI9341_WHITE);

    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);

    int textX = x + (kHomeTileWUi - (int)w) / 2;
    int textY = y + 50;

    tft.setCursor(textX, textY);
    tft.print(label);
}

void redrawTileByIndex(int index, bool selected) {
    int x = 0;
    int y = 0;
    getHomeTileBounds(index, x, y);

    tft.fillRect(x - 3, y - 3, kHomeTileWUi + 6, kHomeTileHUi + 6, ILI9341_BLACK);
    drawTile(x, y, items[index], index, selected);
}

void drawSubMenuItem(int i, bool selected) {
    const int visibleItems = min(currentMenuSize, 6);

    const int rowX = 8;
    const int rowW = 224;
    const int iconX = 17;
    const int dividerX = 44;
    const int textX = 54;
    const uint16_t rowFill = tft.color565(18, 18, 18);
    const uint16_t rowSelected = tft.color565(34, 34, 34);
    const uint16_t rowBorder = tft.color565(52, 52, 52);
    const uint16_t accent = tft.color565(236, 236, 236);
    const uint16_t textSoft = tft.color565(160, 160, 160);

    int visibleIndex = i - currentMenuScrollOffset;
    if (visibleIndex < 0 || visibleIndex >= visibleItems) {
        return;
    }

    bool disabled = isMenuItemDisabled(i);
    int y = kSubMenuStartYUi + visibleIndex * kSubMenuRowHeightUi;

    tft.fillRect(6, y - 7, 228, kSubMenuRowHeightUi + 2, ILI9341_BLACK);
    tft.fillRoundRect(rowX, y - 5, rowW, kSubMenuRowBoxHUi, 5,
                      (selected && !disabled) ? rowSelected : rowFill);
    tft.drawRoundRect(rowX, y - 5, rowW, kSubMenuRowBoxHUi, 5,
                      (selected && !disabled) ? accent : rowBorder);

    const char** iconArray = getCurrentIconArray();
    uint16_t iconColor = disabled ? tft.color565(110, 110, 110) : ((selected && !disabled) ? accent : ILI9341_WHITE);
    uint16_t textColor = disabled ? tft.color565(110, 110, 110) : ((selected && !disabled) ? accent : ILI9341_WHITE);

    if (iconArray != nullptr) {
        drawScaledIconFromFSColored(iconX, y - 1, iconArray[i], 36, 2, iconColor);
    }

    tft.drawFastVLine(dividerX, y, 16, tft.color565(70, 70, 70));

    // Row box: top = y-5, height = 27, so vertical center stays close to y+9.
    // textSize(2) glyph height ~16px, textSize(1) glyph height ~8px
    const int rowCenterY = y + 9;

    String rightValue;
    int minValueX = 150;

    if (currentMenu == screenSaverMenu && (i == 0 || i == 1)) {
        rightValue = (i == 0) ? getScreenSaverTimeoutLabel() : String(getScreenSaverStyleLabel());
        minValueX = 170;
    } else if (currentMenu == brightnessMenu && i == 0) {
        rightValue = getTftBrightnessLabel();
        minValueX = 170;
    }

    String displayLabel = String(currentMenu[i]);
    int maxLabelChars = rightValue.length() > 0 ? 14 : 19;
    if (displayLabel.length() > maxLabelChars) {
        displayLabel = displayLabel.substring(0, maxLabelChars - 1);
    }

    tft.setTextSize(1);
    tft.setTextColor(textColor);
    tft.setCursor(textX, rowCenterY - 4);
    tft.print(displayLabel);

    if (rightValue.length() > 0) {
        tft.setTextSize(1);
        tft.setTextColor(disabled ? tft.color565(110, 110, 110) : textSoft);
        int16_t x1, y1;
        uint16_t w, h;
        tft.getTextBounds(rightValue, 0, 0, &x1, &y1, &w, &h);
        int valueX = 224 - (int)w;
        if (valueX < minValueX) valueX = minValueX;
        tft.setCursor(valueX, rowCenterY - 4);
        tft.print(rightValue);
    }
}

void drawSubMenu() {
    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();

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

    const char* subtitle = (currentMenu == settingsMenu || currentMenu == screenSaverMenu || currentMenu == brightnessMenu)
        ? "Adjust system options"
        : "Open a tool or change a setting";
    drawMenuSectionHeader(currentTitle.length() > 0 ? currentTitle.c_str() : "Menu", subtitle);
    drawListScrollArrows(currentMenuSize, visibleItems, currentMenuScrollOffset);

    for (int i = 0; i < currentMenuSize; i++) {
        drawSubMenuItem(i, i == currentMenuIndex);
    }

    drawMenuFooterHints("SEL open", "LEFT back");
}
