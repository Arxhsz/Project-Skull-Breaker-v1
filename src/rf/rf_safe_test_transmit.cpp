#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <Adafruit_ILI9341.h>

#include "system_types.h"

extern RadioMode currentRadioMode;
extern bool displayUpdatesSuspended;
extern RF24 radio1;
extern RF24 radio3;
extern bool radio1Ok;
extern bool radio3Ok;
extern bool cc1101Ok;
extern bool cc1101Configured;
extern bool attackFirstDraw;
extern bool radio24ActiveFirstDraw;
extern bool rfJammerFirstDraw;
extern float rfLockedFrequencyMHz;
extern Adafruit_ILI9341 tft;

int getSelected24GHzIndex();
void drawStatusBar();
void refreshDiagnosticsStatus();
bool ensureCC1101Ready();
void cc1101SetFrequencyMHz(float mhz);
void cc1101WriteBurst(uint8_t reg, const uint8_t* data, size_t len);
uint8_t cc1101Strobe(uint8_t command);
bool cc1101TransmitBurstPacket(const uint8_t* payload, size_t len);

namespace {
constexpr uint8_t kNrfAddressWidth = 5;
constexpr uint8_t kNrfPayloadSize = 16;
constexpr uint16_t kNrfPacketIntervalMs = 50;
constexpr uint16_t kNrfUiIntervalMs = 500;
constexpr uint16_t kNrfRetryIntervalMs = 2500;
constexpr uint16_t kCcPacketIntervalMs = 100;
constexpr uint16_t kCcUiIntervalMs = 500;
constexpr uint16_t kCcRetryIntervalMs = 2500;
constexpr uint8_t kCcPaTableReg = 0x3E;
constexpr uint8_t kCcStrobeSidle = 0x36;
constexpr uint8_t kCcStrobeSftx = 0x3B;
constexpr uint8_t kCcMaxPowerPa = 0xC0;

const uint8_t kNrfAddress1[kNrfAddressWidth] = {'S', 'B', 'T', '1', 'A'};
const uint8_t kNrfAddress3[kNrfAddressWidth] = {'S', 'B', 'T', '1', 'B'};

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

RadioMode safeMode = RADIO_IDLE;
bool nrf1Configured = false;
bool nrf3Configured = false;
uint8_t nrfConfiguredChannel = 0xFF;
uint8_t nextNrfRadio = 0;
uint32_t nrfSequence = 0;
uint32_t ccSequence = 0;
unsigned long lastNrfPacketMs = 0;
unsigned long lastNrfUiMs = 0;
unsigned long lastNrfRetryMs = 0;
unsigned long lastCcPacketMs = 0;
unsigned long lastCcUiMs = 0;
unsigned long lastCcRetryMs = 0;

void resetSafeState(RadioMode mode) {
    safeMode = mode;
    nrf1Configured = false;
    nrf3Configured = false;
    nrfConfiguredChannel = 0xFF;
    nextNrfRadio = 0;
    nrfSequence = 0;
    ccSequence = 0;
    lastNrfPacketMs = 0;
    lastNrfUiMs = 0;
    lastNrfRetryMs = 0;
    lastCcPacketMs = 0;
    lastCcUiMs = 0;
    lastCcRetryMs = 0;
}

bool configureOneNrf(RF24& radio, bool& available, const uint8_t* address, uint8_t channel) {
    radio.stopConstCarrier();
    radio.stopListening();

    if (!available || !radio.isChipConnected()) {
        radio.powerDown();
        delay(3);
        available = radio.begin(&SPI);
        if (!available) {
            return false;
        }
    }

    radio.powerUp();
    delay(2);
    radio.setAutoAck(false);
    radio.setRetries(0, 0);
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_1MBPS);
    radio.setCRCLength(RF24_CRC_16);
    radio.setAddressWidth(kNrfAddressWidth);
    radio.disableDynamicPayloads();
    radio.setPayloadSize(kNrfPayloadSize);
    radio.setChannel(channel);
    radio.openWritingPipe(address);
    radio.flush_rx();
    radio.flush_tx();
    return true;
}

void prepareNrfTransmitters(uint8_t channel, unsigned long now) {
    const bool channelChanged = channel != nrfConfiguredChannel;
    const bool retryDue = lastNrfRetryMs == 0 || now - lastNrfRetryMs >= kNrfRetryIntervalMs;
    if (!channelChanged && !retryDue && (nrf1Configured || nrf3Configured)) {
        return;
    }

    lastNrfRetryMs = now;
    const bool previousDisplayState = displayUpdatesSuspended;
    displayUpdatesSuspended = true;

    nrf1Configured = configureOneNrf(radio1, radio1Ok, kNrfAddress1, channel);
    if (radio3Ok || channelChanged || !nrf3Configured) {
        nrf3Configured = configureOneNrf(radio3, radio3Ok, kNrfAddress3, channel);
    }

    nrfConfiguredChannel = channel;
    refreshDiagnosticsStatus();
    displayUpdatesSuspended = previousDisplayState;
}

bool sendNrfPacket(RF24& radio, uint8_t radioId, uint8_t channel) {
    NrfTestPayload payload = {
        {'S', 'B', 'T', '2'},
        1,
        radioId,
        channel,
        static_cast<uint8_t>(currentRadioMode),
        nrfSequence++,
        millis()
    };

    radio.stopListening();
    radio.setChannel(channel);
    const bool sent = radio.write(&payload, sizeof(payload));
    if (!sent) {
        radio.flush_tx();
    }
    return sent;
}

void serviceNrfPackets() {
    const unsigned long now = millis();
    const uint8_t channel = static_cast<uint8_t>(constrain(getSelected24GHzIndex(), 0, 125));
    prepareNrfTransmitters(channel, now);

    if (lastNrfPacketMs != 0 && now - lastNrfPacketMs < kNrfPacketIntervalMs) {
        return;
    }
    lastNrfPacketMs = now;

    if (nrf1Configured && nrf3Configured) {
        if (nextNrfRadio == 0) {
            if (!sendNrfPacket(radio1, 1, channel) && !radio1.isChipConnected()) {
                radio1Ok = false;
                nrf1Configured = false;
            }
        } else {
            if (!sendNrfPacket(radio3, 3, channel) && !radio3.isChipConnected()) {
                radio3Ok = false;
                nrf3Configured = false;
            }
        }
        nextNrfRadio ^= 1;
    } else if (nrf1Configured) {
        if (!sendNrfPacket(radio1, 1, channel) && !radio1.isChipConnected()) {
            radio1Ok = false;
            nrf1Configured = false;
        }
    } else if (nrf3Configured) {
        if (!sendNrfPacket(radio3, 3, channel) && !radio3.isChipConnected()) {
            radio3Ok = false;
            nrf3Configured = false;
        }
    }
}

void drawNrfTestScreen(bool fullRedraw) {
    if (displayUpdatesSuspended) {
        return;
    }

    const unsigned long now = millis();
    if (!fullRedraw && lastNrfUiMs != 0 && now - lastNrfUiMs < kNrfUiIntervalMs) {
        return;
    }
    lastNrfUiMs = now;

    if (fullRedraw) {
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        drawStatusBar();
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 40);
        tft.print(F("2.4 Test TX"));
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(18, 58);
        tft.print(F("Continuous valid NRF24 packets"));
        tft.drawFastHLine(10, 68, 220, tft.color565(52, 52, 52));
        tft.setCursor(16, 252);
        tft.print(F("20 packets/s total, PA MAX"));
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
    tft.print(F("Packets: "));
    tft.print(nrfSequence);
    tft.setCursor(24, 150);
    tft.print(F("Primary NRF24: "));
    tft.print(nrf1Configured ? F("TX") : F("OFFLINE"));
    tft.setCursor(24, 168);
    tft.print(F("Secondary NRF24: "));
    tft.print(nrf3Configured ? F("TX") : F("OFFLINE"));
    tft.setCursor(24, 194);
    tft.setTextColor(tft.color565(160, 160, 160));
    tft.print(F("Payload magic: SBT2"));
}

void serviceSafeNrfMode() {
    if (safeMode != currentRadioMode) {
        resetSafeState(currentRadioMode);
        attackFirstDraw = true;
        radio24ActiveFirstDraw = true;
    }

    serviceNrfPackets();
    const bool fullRedraw = attackFirstDraw || radio24ActiveFirstDraw;
    drawNrfTestScreen(fullRedraw);
    attackFirstDraw = false;
    radio24ActiveFirstDraw = false;
}

void drawCcTestScreen(bool fullRedraw) {
    if (displayUpdatesSuspended) {
        return;
    }

    const unsigned long now = millis();
    if (!fullRedraw && lastCcUiMs != 0 && now - lastCcUiMs < kCcUiIntervalMs) {
        return;
    }
    lastCcUiMs = now;

    if (fullRedraw) {
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        drawStatusBar();
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(18, 40);
        tft.print(F("RF Test TX"));
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160, 160, 160));
        tft.setCursor(18, 58);
        tft.print(F("Continuous valid CC1101 packets"));
        tft.drawFastHLine(10, 68, 220, tft.color565(52, 52, 52));
        tft.setCursor(16, 252);
        tft.print(F("10 packets/s, PA MAX"));
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
    tft.print(F("Packets: "));
    tft.print(ccSequence);
    tft.setCursor(24, 154);
    tft.print(F("CC1101: "));
    tft.print(cc1101Ok ? F("TX") : F("OFFLINE"));
    tft.setCursor(24, 180);
    tft.setTextColor(tft.color565(160, 160, 160));
    tft.print(F("Payload magic: SBCC"));
}

void serviceSafeCcMode() {
    if (safeMode != currentRadioMode) {
        resetSafeState(currentRadioMode);
        rfJammerFirstDraw = true;
    }

    const unsigned long now = millis();
    if (!cc1101Ok && (lastCcRetryMs == 0 || now - lastCcRetryMs >= kCcRetryIntervalMs)) {
        lastCcRetryMs = now;
        cc1101Configured = false;
        cc1101Ok = ensureCC1101Ready();
        refreshDiagnosticsStatus();
    }

    if (cc1101Ok && (lastCcPacketMs == 0 || now - lastCcPacketMs >= kCcPacketIntervalMs)) {
        lastCcPacketMs = now;
        const uint16_t freqKHzLow = static_cast<uint16_t>(static_cast<uint32_t>(rfLockedFrequencyMHz * 1000.0f) & 0xFFFFU);
        CcTestPayload payload = {
            {'S', 'B', 'C', 'C'},
            1,
            static_cast<uint8_t>(currentRadioMode),
            freqKHzLow,
            ccSequence,
            now
        };

        const bool previousDisplayState = displayUpdatesSuspended;
        displayUpdatesSuspended = true;
        cc1101Strobe(kCcStrobeSidle);
        cc1101SetFrequencyMHz(rfLockedFrequencyMHz);
        cc1101WriteBurst(kCcPaTableReg, &kCcMaxPowerPa, 1);
        const bool sent = cc1101TransmitBurstPacket(reinterpret_cast<const uint8_t*>(&payload), sizeof(payload));
        cc1101Strobe(kCcStrobeSidle);
        cc1101Strobe(kCcStrobeSftx);
        displayUpdatesSuspended = previousDisplayState;

        if (sent) {
            ++ccSequence;
        } else {
            cc1101Ok = false;
            cc1101Configured = false;
        }
    }

    drawCcTestScreen(rfJammerFirstDraw);
    rfJammerFirstDraw = false;
}
} // namespace

extern "C" void wrappedHandleRFProtocols() asm("__wrap__Z17handleRFProtocolsv");
extern "C" void wrappedHandleRFProtocols() {
    serviceSafeNrfMode();
}

extern "C" void wrappedDrawRadio24ActiveScreen() asm("__wrap__Z23drawRadio24ActiveScreenv");
extern "C" void wrappedDrawRadio24ActiveScreen() {
    serviceSafeNrfMode();
}

extern "C" void wrappedRunPacketFlooder() asm("__wrap__Z16runPacketFlooderv");
extern "C" void wrappedRunPacketFlooder() {
    serviceSafeNrfMode();
}

extern "C" void wrappedRunRFJammer() asm("__wrap__Z11runRFJammerv");
extern "C" void wrappedRunRFJammer() {
    serviceSafeCcMode();
}
