#pragma once

#include <Arduino.h>
#include <RF24.h>

// Canonical V1 wiring. All external radios share VSPI (SCK 18, MISO 19,
// MOSI 23) and have independent chip-select/control pins.
namespace RadioPins {
constexpr int kSpiSck = 18;
constexpr int kSpiMiso = 19;
constexpr int kSpiMosi = 23;

constexpr int kTftCs = 15;
constexpr int kSdCs = 5;

constexpr int kNrfCe = 17;
constexpr int kNrfCsn = 16;
constexpr int kCc1101Cs = 22;
constexpr int kCc1101Gdo0 = 12;

constexpr uint32_t kNrfSpiHz = 2000000UL;
constexpr uint32_t kCc1101SpiHz = 1000000UL;
}  // namespace RadioPins

enum class RadioBusOwner : uint8_t {
    None,
    NrfPassive,
    NrfPacketTx,
    CcPassive,
    CcActive,
    ExternalSpi,
};

struct RadioManagerStatus {
    bool busReady = false;
    bool nrfPresent = false;
    bool cc1101Present = false;
    uint8_t cc1101Version = 0xFF;
    RadioBusOwner owner = RadioBusOwner::None;
    uint32_t nrfRecoveries = 0;
    uint32_t cc1101Recoveries = 0;
    uint8_t nrfChannel = 0xFF;
    float cc1101FrequencyMHz = 0.0f;
    bool cc1101LastTxVerified = false;
    int cc1101LastRssiDbm = -127;
};

void radioManagerPrimeBus();
void radioManagerBeginHardware();
void radioManagerStopAll();
void radioManagerSuspendForSharedSpi();
void radioManagerResumeAfterSharedSpi();
void radioManagerService();
const RadioManagerStatus& radioManagerStatus();

bool radioManagerProbeNrf(RF24& radio, int cePin, int csnPin, const char* label);
bool radioManagerProbePrimaryNrf(bool force = false);
bool radioManagerNrfConnected();
bool radioManagerPrepareNrfPassive(bool force = false);
bool radioManagerSampleNrfRpd(uint8_t channel, uint8_t samples, uint16_t settleUs, uint8_t& hits);
bool radioManagerPrepareNrfPacketTx(uint8_t channel, bool force = false);
bool radioManagerWriteNrfPacket(const void* payload, size_t length, uint8_t channel);
uint8_t radioManagerNrfReadChannel();
void radioManagerStopNrf();

bool radioManagerProbeCc1101(bool configureAfter = true, bool force = false);
bool radioManagerEnsureCc1101();
bool radioManagerPrepareCcPassive(float frequencyMHz, bool force = false);
bool radioManagerPrepareCcActive(float frequencyMHz, bool force = false);
void radioManagerStopCc1101();

bool radioManagerCcSelect();
void radioManagerCcDeselect();
void radioManagerCcWriteReg(uint8_t reg, uint8_t value);
void radioManagerCcWriteBurst(uint8_t reg, const uint8_t* data, size_t length);
void radioManagerCcReadBurst(uint8_t reg, uint8_t* data, size_t length);
uint8_t radioManagerCcReadReg(uint8_t reg);
uint8_t radioManagerCcReadStatus(uint8_t reg);
uint8_t radioManagerCcStrobe(uint8_t command);
bool radioManagerCcReset();
void radioManagerCcConfigureBase();
void radioManagerCcEnterRx();
void radioManagerCcSetFrequency(float frequencyMHz, bool quick = false);
int radioManagerCcReadRssi();
float radioManagerCcFrequencyMHz();
bool radioManagerCcLastTxVerified();
bool radioManagerCcTransmitPacket(const uint8_t* payload, size_t length);
