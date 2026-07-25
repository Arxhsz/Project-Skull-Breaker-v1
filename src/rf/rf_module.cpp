#include <Arduino.h>
#include <math.h>

#include "rf.h"
#include "radio_manager.h"
#include "system_types.h"

extern RadioMode currentRadioMode;
extern bool displayUpdatesSuspended;
extern int scanChannel;
extern uint8_t channelActivity[];
extern float noiseLevel[];

extern bool ghzScannerFirstDraw;
extern bool noiseFirstDraw;
extern bool attackFirstDraw;
extern bool radio24ActiveFirstDraw;
extern bool rfScannerFirstDraw;
extern bool rfMonitorFirstDraw;
extern bool rfJammerFirstDraw;
extern bool rfCaptureFirstDraw;
extern bool rfSweepFirstDraw;
extern bool rfTransmitFirstDraw;
extern bool rfRollingFirstDraw;
extern bool rfReplayFirstDraw;

int getSelected24GHzIndex();
void drawTopScanner();
void drawScannerInfo();
void draw24GHzWaterfall();
void drawNoiseAnalyzer();
void handleRFProtocols();
void drawRadio24ActiveScreen();
void runProtocolAnalyzer();
void runPacketFlooder();
void runRFScanner();
void runRFMonitor();
void runRFJammer();
void runRFSignalCapture();
void runRFSweep();
void runRFTransmit();
void runRFSquelchActivate();
void runRFReplay();
void runRollingCapture();

namespace {
constexpr int kNrfChannelCount = 126;
constexpr uint16_t kSessionGapMs = 650;

RadioMode g_lastMode = RADIO_IDLE;
uint16_t g_lastInvocationMs = 0;
uint16_t g_lastScannerUiMs = 0;
uint16_t g_lastScannerInfoMs = 0;
uint16_t g_lastWaterfallMs = 0;
uint16_t g_lastNoiseUiMs = 0;
uint16_t g_lastActiveUiMs = 0;
uint16_t g_lastModeServiceMs = 0;

uint16_t compactMillis(uint32_t now) {
    return static_cast<uint16_t>(now & 0xFFFFU);
}

bool elapsed(uint32_t now, uint16_t& previous, uint16_t intervalMs) {
    const uint16_t current = compactMillis(now);
    if (previous != 0 && static_cast<uint16_t>(current - previous) < intervalMs) {
        return false;
    }
    previous = current;
    return true;
}

bool isNrfMode(RadioMode mode) {
    return mode == RADIO_24_SCAN ||
           mode == RADIO_NOISE_ANALYZER ||
           mode == RADIO_24_ACTIVE ||
           mode == RADIO_24_PROTOCOL_ANALYZER ||
           mode == RADIO_24_PACKET_FLOOD;
}

bool isPassiveNrfMode(RadioMode mode) {
    return mode == RADIO_24_SCAN ||
           mode == RADIO_NOISE_ANALYZER ||
           mode == RADIO_24_PROTOCOL_ANALYZER;
}

bool isCcMode(RadioMode mode) {
    return mode == RF_SCANNER ||
           mode == RF_MONITOR ||
           mode == RF_SIGNAL_CAPTURE ||
           mode == RF_FREQUENCY_SWEEP ||
           mode == RF_TRANSMIT ||
           mode == RF_JAMMER ||
           mode == RF_SQUELCH_ACTIVATE ||
           mode == RF_REPLAY ||
           mode == RF_ROLLING_CAPTURE;
}

void resetTimers() {
    g_lastScannerUiMs = 0;
    g_lastScannerInfoMs = 0;
    g_lastWaterfallMs = 0;
    g_lastNoiseUiMs = 0;
    g_lastActiveUiMs = 0;
    g_lastModeServiceMs = 0;
}

void markNrfUiForRedraw() {
    ghzScannerFirstDraw = true;
    noiseFirstDraw = true;
    attackFirstDraw = true;
    radio24ActiveFirstDraw = true;
}

void markCcUiForRedraw() {
    rfScannerFirstDraw = true;
    rfMonitorFirstDraw = true;
    rfJammerFirstDraw = true;
    rfCaptureFirstDraw = true;
    rfSweepFirstDraw = true;
    rfTransmitFirstDraw = true;
    rfRollingFirstDraw = true;
    rfReplayFirstDraw = true;
}

void handleModeTransition(RadioMode previous, RadioMode next) {
    if (previous == next) return;

    if (isNrfMode(previous)) {
        radioManagerStopNrf();
    }
    if (isCcMode(previous)) {
        radioManagerStopCc1101();
    }

    resetTimers();
    if (isNrfMode(next)) markNrfUiForRedraw();
    if (isCcMode(next)) markCcUiForRedraw();

    if (isPassiveNrfMode(next)) {
        radioManagerPrepareNrfPassive(true);
    }
}

void sampleNrfChannel(bool noiseAnalyzer) {
    const uint8_t channel = static_cast<uint8_t>(constrain(scanChannel, 0, kNrfChannelCount - 1));
    uint8_t hits = 0;
    if (!radioManagerSampleNrfRpd(channel, 25, noiseAnalyzer ? 90 : 140, hits)) {
        return;
    }

    if (noiseAnalyzer) {
        float normalized = static_cast<float>(hits) / 25.0f;
        if (normalized < 0.05f) {
            normalized = static_cast<float>(random(1, 6)) / 100.0f;
        }
        normalized = powf(normalized, 0.7f);
        noiseLevel[channel] = noiseLevel[channel] * 0.85f + normalized * 0.15f;
    } else {
        if (channel == getSelected24GHzIndex()) {
            hits = static_cast<uint8_t>(min(static_cast<int>(hits) * 2, 25));
        }
        channelActivity[channel] = hits;
    }

    scanChannel = (channel + 1) % kNrfChannelCount;
}

void service24Scanner(uint32_t now) {
    sampleNrfChannel(false);
    if (!displayUpdatesSuspended && elapsed(now, g_lastScannerUiMs, 85)) drawTopScanner();
    if (!displayUpdatesSuspended && elapsed(now, g_lastScannerInfoMs, 140)) drawScannerInfo();
    if (!displayUpdatesSuspended && elapsed(now, g_lastWaterfallMs, 125)) draw24GHzWaterfall();
}

void serviceNoiseAnalyzer(uint32_t now) {
    sampleNrfChannel(true);
    if (!displayUpdatesSuspended && elapsed(now, g_lastNoiseUiMs, 90)) drawNoiseAnalyzer();
}

void service24Active(uint32_t now) {
    handleRFProtocols();
    if (!displayUpdatesSuspended && elapsed(now, g_lastActiveUiMs, 125)) {
        drawRadio24ActiveScreen();
    }
}

bool serviceDue(uint32_t now, uint16_t intervalMs) {
    return elapsed(now, g_lastModeServiceMs, intervalMs);
}
}  // namespace

void rf_serviceMode() {
    const uint32_t now = millis();
    const uint16_t compactNow = compactMillis(now);

    if (g_lastInvocationMs != 0 &&
        static_cast<uint16_t>(compactNow - g_lastInvocationMs) > kSessionGapMs) {
        g_lastMode = RADIO_IDLE;
        resetTimers();
    }
    g_lastInvocationMs = compactNow;

    if (g_lastMode != currentRadioMode) {
        const RadioMode previous = g_lastMode;
        g_lastMode = currentRadioMode;
        handleModeTransition(previous, currentRadioMode);
    }

    radioManagerService();

    switch (currentRadioMode) {
        case RADIO_24_SCAN:
            service24Scanner(now);
            break;
        case RADIO_NOISE_ANALYZER:
            serviceNoiseAnalyzer(now);
            break;
        case RADIO_24_ACTIVE:
            service24Active(now);
            break;
        case RADIO_24_PROTOCOL_ANALYZER:
            if (serviceDue(now, 8)) runProtocolAnalyzer();
            break;
        case RADIO_24_PACKET_FLOOD:
            if (serviceDue(now, 8)) runPacketFlooder();
            break;
        case RF_SCANNER:
            if (serviceDue(now, 20)) runRFScanner();
            break;
        case RF_MONITOR:
            if (serviceDue(now, 35)) runRFMonitor();
            break;
        case RF_JAMMER:
            if (serviceDue(now, 10)) runRFJammer();
            break;
        case RF_SIGNAL_CAPTURE:
            if (serviceDue(now, 16)) runRFSignalCapture();
            break;
        case RF_FREQUENCY_SWEEP:
            if (serviceDue(now, 3)) runRFSweep();
            break;
        case RF_TRANSMIT:
            if (serviceDue(now, 4)) runRFTransmit();
            break;
        case RF_SQUELCH_ACTIVATE:
            if (serviceDue(now, 10)) runRFSquelchActivate();
            break;
        case RF_REPLAY:
            if (serviceDue(now, 8)) runRFReplay();
            break;
        case RF_ROLLING_CAPTURE:
            if (serviceDue(now, 8)) runRollingCapture();
            break;
        default:
            break;
    }

    yield();
}
