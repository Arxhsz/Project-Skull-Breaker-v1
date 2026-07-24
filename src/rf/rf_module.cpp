#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <math.h>

#include "rf.h"
#include "system_types.h"

extern RadioMode currentRadioMode;
extern bool displayUpdatesSuspended;

extern RF24 radio1;
extern RF24 radio3;
extern bool radio1Ok;
extern bool radio3Ok;
extern bool cc1101Ok;
extern bool cc1101Configured;

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

bool ensureCC1101Ready();
uint8_t cc1101ReadStatusReg(uint8_t reg);
uint8_t cc1101Strobe(uint8_t command);
void stopCCTool();
void stop24GHzActiveMode();
void refreshDiagnosticsStatus();

namespace {
constexpr int kNrfChannelCount = 126;
constexpr uint32_t kSessionGapMs = 650;
constexpr uint32_t kNrfHealthIntervalMs = 2500;
constexpr uint32_t kCcHealthIntervalMs = 1200;
constexpr uint32_t kCcVersionIntervalMs = 5000;

constexpr uint8_t kCcVersionReg = 0x31;
constexpr uint8_t kCcMarcStateReg = 0x35;
constexpr uint8_t kCcStateRxFifoOverflow = 0x11;
constexpr uint8_t kCcStateTxFifoUnderflow = 0x16;
constexpr uint8_t kCcStrobeSrx = 0x34;
constexpr uint8_t kCcStrobeSidle = 0x36;
constexpr uint8_t kCcStrobeSfrx = 0x3A;
constexpr uint8_t kCcStrobeSftx = 0x3B;

RadioMode lastServicedMode = RADIO_IDLE;
uint32_t lastInvocationMs = 0;
uint32_t lastNrfHealthMs = 0;
uint32_t lastCcHealthMs = 0;
uint32_t lastCcVersionMs = 0;
uint32_t lastScannerUiMs = 0;
uint32_t lastScannerInfoMs = 0;
uint32_t lastWaterfallMs = 0;
uint32_t lastNoiseUiMs = 0;
uint32_t lastActiveUiMs = 0;
uint32_t lastModeServiceMs = 0;
bool nrfPassivePrepared = false;

bool elapsed(uint32_t now, uint32_t& previous, uint32_t intervalMs) {
    if (previous != 0 && static_cast<uint32_t>(now - previous) < intervalMs) {
        return false;
    }
    previous = now;
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

bool isPassiveCcMode(RadioMode mode) {
    return mode == RF_SCANNER ||
           mode == RF_MONITOR ||
           mode == RF_SIGNAL_CAPTURE ||
           mode == RF_FREQUENCY_SWEEP ||
           mode == RF_ROLLING_CAPTURE;
}

void resetServiceTimers() {
    lastNrfHealthMs = 0;
    lastCcHealthMs = 0;
    lastCcVersionMs = 0;
    lastScannerUiMs = 0;
    lastScannerInfoMs = 0;
    lastWaterfallMs = 0;
    lastNoiseUiMs = 0;
    lastActiveUiMs = 0;
    lastModeServiceMs = 0;
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

void quiesceOneNrf(RF24& radio, bool available) {
    if (!available) {
        return;
    }

    radio.stopConstCarrier();
    radio.stopListening();
    radio.flush_rx();
    radio.flush_tx();
    radio.powerDown();
}

void quiesceNrfRadios() {
    const bool previousDisplayState = displayUpdatesSuspended;
    displayUpdatesSuspended = true;

    quiesceOneNrf(radio1, radio1Ok);
    // The original generic cleanup used the unused radio2 object. Always shut
    // down the actual secondary NRF24 here so it cannot remain selected or
    // transmitting after a mode change.
    quiesceOneNrf(radio3, radio3Ok);

    displayUpdatesSuspended = previousDisplayState;
    nrfPassivePrepared = false;
}

void configureNrfPassiveBaseline() {
    radio1.stopConstCarrier();
    radio1.stopListening();
    radio1.setAutoAck(false);
    radio1.setRetries(0, 0);
    radio1.setPALevel(RF24_PA_MIN);
    radio1.setDataRate(RF24_2MBPS);
    radio1.disableCRC();
    radio1.flush_rx();
    radio1.flush_tx();
    radio1.powerUp();
    delay(2); // NRF24 requires roughly 1.5 ms from power-down to standby.
    radio1.startListening();
    nrfPassivePrepared = true;
}

bool recoverPrimaryNrf() {
    const bool previousDisplayState = displayUpdatesSuspended;
    displayUpdatesSuspended = true;

    radio1.stopConstCarrier();
    radio1.stopListening();
    radio1.powerDown();
    delay(5);

    const bool recovered = radio1.begin(&SPI);
    radio1Ok = recovered;
    if (recovered) {
        configureNrfPassiveBaseline();
    } else {
        nrfPassivePrepared = false;
    }

    refreshDiagnosticsStatus();
    displayUpdatesSuspended = previousDisplayState;
    markNrfUiForRedraw();
    return recovered;
}

bool preparePassiveNrf() {
    if (!radio1Ok || !radio1.isChipConnected()) {
        return recoverPrimaryNrf();
    }

    if (!nrfPassivePrepared) {
        configureNrfPassiveBaseline();
    }
    return true;
}

void serviceNrfHealth(uint32_t now) {
    if (!isPassiveNrfMode(currentRadioMode) ||
        !elapsed(now, lastNrfHealthMs, kNrfHealthIntervalMs)) {
        return;
    }

    if (!radio1Ok || !radio1.isChipConnected()) {
        recoverPrimaryNrf();
    }
}

void recoverCcPassiveState(uint8_t marcState) {
    cc1101Strobe(kCcStrobeSidle);
    delayMicroseconds(80);

    if (marcState == kCcStateRxFifoOverflow) {
        cc1101Strobe(kCcStrobeSfrx);
    } else if (marcState == kCcStateTxFifoUnderflow) {
        cc1101Strobe(kCcStrobeSftx);
    }

    cc1101Strobe(kCcStrobeSrx);
    delayMicroseconds(150);
}

void serviceCcHealth(uint32_t now) {
    if (!isPassiveCcMode(currentRadioMode)) {
        return;
    }

    const bool previousDisplayState = displayUpdatesSuspended;

    if (elapsed(now, lastCcHealthMs, kCcHealthIntervalMs)) {
        displayUpdatesSuspended = true;
        const uint8_t marcState = cc1101ReadStatusReg(kCcMarcStateReg) & 0x1F;
        if (marcState == kCcStateRxFifoOverflow ||
            marcState == kCcStateTxFifoUnderflow) {
            recoverCcPassiveState(marcState);
        }
        displayUpdatesSuspended = previousDisplayState;
    }

    if (!elapsed(now, lastCcVersionMs, kCcVersionIntervalMs)) {
        return;
    }

    displayUpdatesSuspended = true;
    const uint8_t version = cc1101ReadStatusReg(kCcVersionReg);
    if (version == 0x00 || version == 0xFF) {
        cc1101Ok = false;
        cc1101Configured = false;
        const bool recovered = ensureCC1101Ready();
        if (recovered) {
            markCcUiForRedraw();
        }
    }
    displayUpdatesSuspended = previousDisplayState;
}

void handleModeTransition(RadioMode previousMode, RadioMode nextMode) {
    if (previousMode == nextMode) {
        return;
    }

    if (previousMode == RADIO_24_ACTIVE) {
        stop24GHzActiveMode();
    } else if (isNrfMode(previousMode)) {
        quiesceNrfRadios();
    }

    if (isCcMode(previousMode) && !isCcMode(nextMode)) {
        stopCCTool();
    }

    nrfPassivePrepared = false;
    resetServiceTimers();

    if (isNrfMode(nextMode)) {
        markNrfUiForRedraw();
    }
    if (isCcMode(nextMode)) {
        markCcUiForRedraw();
    }
}

void sampleNrfChannel(bool noiseAnalyzer) {
    if (!preparePassiveNrf()) {
        return;
    }

    const int channel = constrain(scanChannel, 0, kNrfChannelCount - 1);

    // RF_CH should be changed while CE is low. The old path repeatedly called
    // startListening() and then changed channel with CE still high, which can
    // leave the transceiver in an undefined RX state after long sessions.
    radio1.stopListening();
    radio1.setChannel(static_cast<uint8_t>(channel));
    radio1.startListening();
    delayMicroseconds(noiseAnalyzer ? 90 : 140);

    uint8_t strength = 0;
    for (int sample = 0; sample < 25; ++sample) {
        if (radio1.testRPD()) {
            ++strength;
        }
        delayMicroseconds(10);
    }

    if (noiseAnalyzer) {
        float normalized = static_cast<float>(strength) / 25.0f;
        if (normalized < 0.05f) {
            normalized = static_cast<float>(random(1, 6)) / 100.0f;
        }
        normalized = powf(normalized, 0.7f);
        noiseLevel[channel] = (noiseLevel[channel] * 0.85f) + (normalized * 0.15f);
    } else {
        if (channel == getSelected24GHzIndex()) {
            strength = static_cast<uint8_t>(min(static_cast<int>(strength) * 2, 25));
        }
        channelActivity[channel] = strength;
    }

    scanChannel = (channel + 1) % kNrfChannelCount;
}

void service24GHzScanner(uint32_t now) {
    sampleNrfChannel(false);

    if (!displayUpdatesSuspended && elapsed(now, lastScannerUiMs, 85)) {
        drawTopScanner();
    }
    if (!displayUpdatesSuspended && elapsed(now, lastScannerInfoMs, 140)) {
        drawScannerInfo();
    }
    if (!displayUpdatesSuspended && elapsed(now, lastWaterfallMs, 125)) {
        draw24GHzWaterfall();
    }
}

void serviceNoiseAnalyzer(uint32_t now) {
    sampleNrfChannel(true);
    if (!displayUpdatesSuspended && elapsed(now, lastNoiseUiMs, 90)) {
        drawNoiseAnalyzer();
    }
}

void service24GHzActive(uint32_t now) {
    handleRFProtocols();
    if (!displayUpdatesSuspended && elapsed(now, lastActiveUiMs, 125)) {
        drawRadio24ActiveScreen();
    }
}

bool serviceDue(uint32_t now, uint32_t intervalMs) {
    return elapsed(now, lastModeServiceMs, intervalMs);
}
} // namespace

void rf_serviceMode() {
    const uint32_t now = millis();

    // A long gap means the user left the tool screen. Treat the next call as a
    // fresh session even when they reopen the same mode.
    if (lastInvocationMs != 0 &&
        static_cast<uint32_t>(now - lastInvocationMs) > kSessionGapMs) {
        lastServicedMode = RADIO_IDLE;
        nrfPassivePrepared = false;
        resetServiceTimers();
    }
    lastInvocationMs = now;

    if (lastServicedMode != currentRadioMode) {
        const RadioMode previousMode = lastServicedMode;
        lastServicedMode = currentRadioMode;
        handleModeTransition(previousMode, currentRadioMode);
    }

    serviceNrfHealth(now);
    serviceCcHealth(now);

    switch (currentRadioMode) {
        case RADIO_24_SCAN:
            service24GHzScanner(now);
            break;

        case RADIO_NOISE_ANALYZER:
            serviceNoiseAnalyzer(now);
            break;

        case RADIO_24_ACTIVE:
            service24GHzActive(now);
            break;

        case RADIO_24_PROTOCOL_ANALYZER:
            if (serviceDue(now, 8)) {
                runProtocolAnalyzer();
            }
            break;

        case RADIO_24_PACKET_FLOOD:
            if (serviceDue(now, 8)) {
                runPacketFlooder();
            }
            break;

        case RF_SCANNER:
            if (serviceDue(now, 20)) {
                runRFScanner();
            }
            break;

        case RF_MONITOR:
            if (serviceDue(now, 35)) {
                runRFMonitor();
            }
            break;

        case RF_JAMMER:
            if (serviceDue(now, 10)) {
                runRFJammer();
            }
            break;

        case RF_SIGNAL_CAPTURE:
            if (serviceDue(now, 16)) {
                runRFSignalCapture();
            }
            break;

        case RF_FREQUENCY_SWEEP:
            if (serviceDue(now, 3)) {
                runRFSweep();
            }
            break;

        case RF_TRANSMIT:
            if (serviceDue(now, 4)) {
                runRFTransmit();
            }
            break;

        case RF_SQUELCH_ACTIVATE:
            if (serviceDue(now, 10)) {
                runRFSquelchActivate();
            }
            break;

        case RF_REPLAY:
            if (serviceDue(now, 8)) {
                runRFReplay();
            }
            break;

        case RF_ROLLING_CAPTURE:
            if (serviceDue(now, 8)) {
                runRollingCapture();
            }
            break;

        default:
            break;
    }

    // Several modes previously had no delay at all and could starve the idle
    // task/watchdog for minutes at a time. Yield without imposing a large UI
    // latency or changing the radio feature's intended behavior.
    yield();
}
