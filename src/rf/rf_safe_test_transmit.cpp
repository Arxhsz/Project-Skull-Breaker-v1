#include <Arduino.h>
#include <Adafruit_ILI9341.h>

#include "radio_manager.h"
#include "system_types.h"

extern RadioMode currentRadioMode;
extern bool displayUpdatesSuspended;
extern bool attackFirstDraw;
extern bool radio24ActiveFirstDraw;
extern bool rfJammerFirstDraw;
extern bool radio1Ok;
extern bool cc1101Ok;
extern float rfLockedFrequencyMHz;
extern Adafruit_ILI9341 tft;

int getSelected24GHzIndex();
void drawStatusBar();

namespace {
constexpr uint16_t kNrfPacketIntervalMs = 10;   // 100 packets/s
constexpr uint16_t kCcPacketIntervalMs = 25;    // 40 packets/s
constexpr uint16_t kUiIntervalMs = 400;
constexpr uint16_t kRetryIntervalMs = 1000;

struct __attribute__((packed)) NrfTestPayload {
    uint8_t magic[4];
    uint8_t version;
    uint8_t radioId;
    uint8_t channel;
    uint8_t mode;
    uint32_t sequence;
    uint32_t uptimeMs;
};

struct __attribute__((packed)) CcTestPayload {
    uint8_t magic[4];
    uint8_t version;
    uint8_t mode;
    uint16_t frequencyKHzLow;
    uint32_t sequence;
    uint32_t uptimeMs;
};

static_assert(sizeof(NrfTestPayload) == 16, "NRF test payload must remain 16 bytes");
static_assert(sizeof(CcTestPayload) == 16, "CC test payload must remain 16 bytes");

RadioMode g_testMode = RADIO_IDLE;
uint32_t g_nrfSequence = 0;
uint32_t g_ccSequence = 0;
uint32_t g_nrfFailures = 0;
uint32_t g_ccFailures = 0;
unsigned long g_lastNrfPacketMs = 0;
unsigned long g_lastCcPacketMs = 0;
unsigned long g_lastUiMs = 0;
unsigned long g_lastRetryMs = 0;

void resetTestState(RadioMode mode) {
    radioManagerStopAll();
    g_testMode = mode;
    g_nrfSequence = 0;
    g_ccSequence = 0;
    g_nrfFailures = 0;
    g_ccFailures = 0;
    g_lastNrfPacketMs = 0;
    g_lastCcPacketMs = 0;
    g_lastUiMs = 0;
    g_lastRetryMs = 0;
}

void serviceNrfPackets() {
    const unsigned long now = millis();
    const uint8_t channel = static_cast<uint8_t>(constrain(getSelected24GHzIndex(), 0, 125));

    if (!radio1Ok && (g_lastRetryMs == 0 || now - g_lastRetryMs >= kRetryIntervalMs)) {
        g_lastRetryMs = now;
        radioManagerProbePrimaryNrf(true);
    }
    if (!radio1Ok || (g_lastNrfPacketMs != 0 && now - g_lastNrfPacketMs < kNrfPacketIntervalMs)) {
        return;
    }
    g_lastNrfPacketMs = now;

    NrfTestPayload payload = {
        {'S', 'B', 'T', '2'},
        2,
        1,
        channel,
        static_cast<uint8_t>(currentRadioMode),
        g_nrfSequence,
        now,
    };

    if (radioManagerWriteNrfPacket(&payload, sizeof(payload), channel)) {
        ++g_nrfSequence;
    } else {
        ++g_nrfFailures;
    }
}

void serviceCcPackets() {
    const unsigned long now = millis();
    if (!cc1101Ok && (g_lastRetryMs == 0 || now - g_lastRetryMs >= kRetryIntervalMs)) {
        g_lastRetryMs = now;
        radioManagerProbeCc1101(true, true);
    }
    if (!cc1101Ok || (g_lastCcPacketMs != 0 && now - g_lastCcPacketMs < kCcPacketIntervalMs)) {
        return;
    }
    g_lastCcPacketMs = now;

    const uint16_t frequencyKHzLow = static_cast<uint16_t>(
        static_cast<uint32_t>(rfLockedFrequencyMHz * 1000.0f) & 0xFFFFU);
    CcTestPayload payload = {
        {'S', 'B', 'C', 'C'},
        2,
        static_cast<uint8_t>(currentRadioMode),
        frequencyKHzLow,
        g_ccSequence,
        now,
    };

    if (radioManagerCcTransmitPacket(reinterpret_cast<const uint8_t*>(&payload), sizeof(payload))) {
        ++g_ccSequence;
    } else {
        ++g_ccFailures;
    }
}

void drawNrfScreen(bool fullRedraw) {
    if (displayUpdatesSuspended) return;
    const unsigned long now = millis();
    if (!fullRedraw && g_lastUiMs != 0 && now - g_lastUiMs < kUiIntervalMs) return;
    g_lastUiMs = now;

    if (fullRedraw) {
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        drawStatusBar();
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 40);
        tft.print(F("2.4 Range Test"));
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(18, 58);
        tft.print(F("Continuous NRF24 test packets"));
        tft.drawFastHLine(10, 68, 220, tft.color565(52, 52, 52));
        tft.setCursor(16, 252);
        tft.print(F("100 packets/s  PA MAX + LNA"));
        tft.setCursor(16, 270);
        tft.print(F("LEFT back"));
    }

    const int channel = constrain(getSelected24GHzIndex(), 0, 125);
    tft.fillRect(14, 88, 212, 138, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_CYAN);
    tft.setCursor(24, 96);
    tft.print(F("CH "));
    tft.print(channel);

    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(24, 132);
    tft.print(F("Primary NRF24: "));
    tft.print(radio1Ok ? F("TX") : F("OFFLINE"));
    tft.setCursor(24, 150);
    tft.print(F("Packets: "));
    tft.print(g_nrfSequence);
    tft.setCursor(24, 168);
    tft.print(F("Failures: "));
    tft.print(g_nrfFailures);
    tft.setCursor(24, 194);
    tft.setTextColor(tft.color565(160, 160, 160));
    tft.print(F("Payload magic: SBT2"));
}

void drawCcScreen(bool fullRedraw) {
    if (displayUpdatesSuspended) return;
    const unsigned long now = millis();
    if (!fullRedraw && g_lastUiMs != 0 && now - g_lastUiMs < kUiIntervalMs) return;
    g_lastUiMs = now;

    if (fullRedraw) {
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        drawStatusBar();
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 40);
        tft.print(F("RF Range Test"));
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(18, 58);
        tft.print(F("Continuous CC1101 test packets"));
        tft.drawFastHLine(10, 68, 220, tft.color565(52, 52, 52));
        tft.setCursor(16, 252);
        tft.print(F("40 packets/s  PA MAX"));
        tft.setCursor(16, 270);
        tft.print(F("UP/DN tune  LEFT back"));
    }

    tft.fillRect(14, 88, 212, 138, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(24, 96);
    tft.print(rfLockedFrequencyMHz, 2);
    tft.print(F(" MHz"));

    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(24, 136);
    tft.print(F("CC1101: "));
    tft.print(cc1101Ok ? F("TX") : F("OFFLINE"));
    tft.setCursor(24, 154);
    tft.print(F("Packets: "));
    tft.print(g_ccSequence);
    tft.setCursor(24, 172);
    tft.print(F("Failures: "));
    tft.print(g_ccFailures);
    tft.setCursor(24, 198);
    tft.setTextColor(tft.color565(160, 160, 160));
    tft.print(F("Payload magic: SBCC"));
}

void serviceNrfTest() {
    if (g_testMode != currentRadioMode) {
        resetTestState(currentRadioMode);
        attackFirstDraw = true;
        radio24ActiveFirstDraw = true;
    }
    serviceNrfPackets();
    const bool fullRedraw = attackFirstDraw || radio24ActiveFirstDraw;
    drawNrfScreen(fullRedraw);
    attackFirstDraw = false;
    radio24ActiveFirstDraw = false;
}

void serviceCcTest() {
    if (g_testMode != currentRadioMode) {
        resetTestState(currentRadioMode);
        rfJammerFirstDraw = true;
    }
    serviceCcPackets();
    drawCcScreen(rfJammerFirstDraw);
    rfJammerFirstDraw = false;
}
}  // namespace

extern "C" void wrappedHandleRFProtocols() asm("__wrap__Z17handleRFProtocolsv");
extern "C" void wrappedHandleRFProtocols() {
    serviceNrfTest();
}

extern "C" void wrappedDrawRadio24ActiveScreen() asm("__wrap__Z23drawRadio24ActiveScreenv");
extern "C" void wrappedDrawRadio24ActiveScreen() {
    serviceNrfTest();
}

extern "C" void wrappedRunPacketFlooder() asm("__wrap__Z16runPacketFlooderv");
extern "C" void wrappedRunPacketFlooder() {
    serviceNrfTest();
}

extern "C" void wrappedRunRFJammer() asm("__wrap__Z11runRFJammerv");
extern "C" void wrappedRunRFJammer() {
    serviceCcTest();
}
