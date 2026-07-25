#include "radio_manager.h"

#include <SPI.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "system_types.h"
#define LOG_SCOPE "RADIO"
#include "logging.h"

extern RF24 radio1;
extern bool radio1Ok;
extern bool radio2Ok;
extern bool radio3Ok;
extern bool cc1101Ok;
extern bool cc1101Configured;
extern uint8_t cc1101Version;
extern float cc1101CurrentMHz;
extern int cc1101PreparedPassiveMode;
extern float cc1101PreparedPassiveMHz;
extern unsigned long cc1101LastProbeAttemptMs;
extern unsigned long cc1101LastReadyMs;
extern float rfLockedFrequencyMHz;
extern RadioMode currentRadioMode;

void refreshDiagnosticsStatus();

namespace {
constexpr uint8_t kCcVersion = 0x31;
constexpr uint8_t kCcRssi = 0x34;
constexpr uint8_t kCcMarcState = 0x35;
constexpr uint8_t kCcTxBytes = 0x3A;
constexpr uint8_t kCcPaTable = 0x3E;
constexpr uint8_t kCcTxFifo = 0x3F;

constexpr uint8_t kCcWriteBurst = 0x40;
constexpr uint8_t kCcReadSingle = 0x80;
constexpr uint8_t kCcReadBurst = 0xC0;

constexpr uint8_t kCcSres = 0x30;
constexpr uint8_t kCcScal = 0x33;
constexpr uint8_t kCcSrx = 0x34;
constexpr uint8_t kCcStx = 0x35;
constexpr uint8_t kCcSidle = 0x36;
constexpr uint8_t kCcSfrx = 0x3A;
constexpr uint8_t kCcSftx = 0x3B;

constexpr uint8_t kCcStateRxFifoOverflow = 0x11;
constexpr uint8_t kCcStateTxFifoUnderflow = 0x16;

constexpr uint32_t kNrfProbeBackoffMs = 900;
constexpr uint32_t kCcProbeBackoffMs = 900;
constexpr uint32_t kHealthIntervalMs = 2500;
constexpr float kFrequencyCorrectionPpm = 34.0f;

constexpr uint8_t kNrfAddress[5] = {'S', 'B', 'T', '1', 'A'};

RadioManagerStatus g_status;
SemaphoreHandle_t g_spiMutex = nullptr;
bool g_ccTransactionOpen = false;
bool g_nrfPassiveConfigured = false;
bool g_nrfTxConfigured = false;
uint8_t g_nrfTxChannel = 0xFF;
uint32_t g_lastNrfProbeMs = 0;
uint32_t g_lastCcProbeMs = 0;
uint32_t g_lastHealthMs = 0;

class RecursiveLock {
public:
    RecursiveLock() {
        if (g_spiMutex != nullptr) {
            locked_ = xSemaphoreTakeRecursive(g_spiMutex, pdMS_TO_TICKS(250)) == pdTRUE;
        }
    }
    ~RecursiveLock() {
        if (locked_ && g_spiMutex != nullptr) {
            xSemaphoreGiveRecursive(g_spiMutex);
        }
    }
    bool locked() const { return locked_; }

private:
    bool locked_ = false;
};

void ensureMutex() {
    if (g_spiMutex == nullptr) {
        g_spiMutex = xSemaphoreCreateRecursiveMutex();
    }
}

void setOutputHigh(int pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
}

void deselectAllLocked() {
    pinMode(RadioPins::kNrfCe, OUTPUT);
    pinMode(RadioPins::kNrfCsn, OUTPUT);
    pinMode(RadioPins::kCc1101Cs, OUTPUT);
    pinMode(RadioPins::kSdCs, OUTPUT);
    pinMode(RadioPins::kTftCs, OUTPUT);

    digitalWrite(RadioPins::kNrfCe, LOW);
    digitalWrite(RadioPins::kNrfCsn, HIGH);
    digitalWrite(RadioPins::kCc1101Cs, HIGH);
    digitalWrite(RadioPins::kSdCs, HIGH);
    digitalWrite(RadioPins::kTftCs, HIGH);
}

void ensureBusLocked() {
    if (!g_status.busReady) {
        deselectAllLocked();
        SPI.begin(RadioPins::kSpiSck, RadioPins::kSpiMiso, RadioPins::kSpiMosi);
        delay(2);
        g_status.busReady = true;
    } else {
        deselectAllLocked();
    }
