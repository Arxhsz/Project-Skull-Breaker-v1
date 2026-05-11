#include <Arduino.h>

#include "rf.h"
#include "system_types.h"

extern RadioMode currentRadioMode;

void run24GHzScannerStep();
void drawTopScanner();
void drawScannerInfo();
void draw24GHzWaterfall();
void runNoiseAnalyzerStep();
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

void rf_serviceMode() {
    if (currentRadioMode == RADIO_24_SCAN) {
        run24GHzScannerStep();
        drawTopScanner();
        drawScannerInfo();
        draw24GHzWaterfall();
        return;
    }

    if (currentRadioMode == RADIO_NOISE_ANALYZER) {
        runNoiseAnalyzerStep();
        drawNoiseAnalyzer();
        return;
    }

    if (currentRadioMode == RADIO_24_ACTIVE) {
        handleRFProtocols();
        drawRadio24ActiveScreen();
        return;
    }

    if (currentRadioMode == RADIO_24_PROTOCOL_ANALYZER) {
        runProtocolAnalyzer();
        return;
    }

    if (currentRadioMode == RADIO_24_PACKET_FLOOD) {
        runPacketFlooder();
        return;
    }

    if (currentRadioMode == RF_SCANNER) {
        runRFScanner();
        return;
    }

    if (currentRadioMode == RF_MONITOR) {
        runRFMonitor();
        return;
    }

    if (currentRadioMode == RF_JAMMER) {
        runRFJammer();
        return;
    }

    if (currentRadioMode == RF_SIGNAL_CAPTURE) {
        runRFSignalCapture();
        return;
    }

    if (currentRadioMode == RF_FREQUENCY_SWEEP) {
        runRFSweep();
        return;
    }

    if (currentRadioMode == RF_TRANSMIT) {
        runRFTransmit();
        return;
    }

    if (currentRadioMode == RF_SQUELCH_ACTIVATE) {
        runRFSquelchActivate();
        return;
    }

    if (currentRadioMode == RF_REPLAY) {
        runRFReplay();
        return;
    }

    if (currentRadioMode == RF_ROLLING_CAPTURE) {
        runRollingCapture();
        return;
    }
}
