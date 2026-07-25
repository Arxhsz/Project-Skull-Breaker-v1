uint8_t radioManagerCcStrobe(uint8_t command) {
    ensureMutex();
    RecursiveLock lock;
    return lock.locked() ? ccStrobeLocked(command) : 0xFF;
}

bool radioManagerCcReset() {
    ensureMutex();
    RecursiveLock lock;
    return lock.locked() && ccResetLocked();
}

void radioManagerCcConfigureBase() {
    ensureMutex();
    RecursiveLock lock;
    if (lock.locked()) ccConfigureBaseLocked();
}

void radioManagerCcEnterRx() {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) return;
    ccStrobeLocked(kCcSidle);
    delayMicroseconds(60);
    ccStrobeLocked(kCcSfrx);
    ccStrobeLocked(kCcSftx);
    ccStrobeLocked(kCcSrx);
    delayMicroseconds(180);
}

void radioManagerCcSetFrequency(float frequencyMHz, bool quick) {
    ensureMutex();
    RecursiveLock lock;
    if (lock.locked()) ccSetFrequencyLocked(frequencyMHz, quick);
}

int radioManagerCcReadRssi() {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked() || !radioManagerEnsureCc1101()) {
        return -127;
    }

    // RSSI is an 8-bit two's-complement status value. 0xFF is a valid
    // measurement (~-74.5 dBm), so it must not be treated as an SPI failure.
    const uint8_t raw = ccReadLocked(kCcRssi, true);
    const int signedRaw = raw >= 128 ? static_cast<int>(raw) - 256 : raw;
    return constrain((signedRaw / 2) - 74, -127, 0);
}

float radioManagerCcFrequencyMHz() {
    return g_status.cc1101FrequencyMHz;
}

bool radioManagerCcLastTxVerified() {
    return g_status.cc1101LastTxVerified;
}


bool radioManagerCcTransmitPacket(const uint8_t* payload, size_t length) {
    if (payload == nullptr || length == 0 || length > 61 ||
        !radioManagerPrepareCcActive(rfLockedFrequencyMHz, false)) {
        g_status.cc1101LastTxVerified = false;
        return false;
    }

    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        g_status.cc1101LastTxVerified = false;
        return false;
    }

    ccStrobeLocked(kCcSidle);
    ccStrobeLocked(kCcSftx);
    const uint8_t packetLength = static_cast<uint8_t>(length);
    ccWriteBurstLocked(kCcTxFifo, &packetLength, 1);
    ccWriteBurstLocked(kCcTxFifo, payload, length);
    pinMode(RadioPins::kCc1101Gdo0, INPUT);

    const uint8_t stxStatus = ccStrobeLocked(kCcStx);
    bool sawGdoHigh = false;
    bool sawPacketEnd = false;
    bool underflow = false;
    uint8_t lastTxBytes = 0x7F;
    const uint32_t startedUs = micros();

    while (static_cast<uint32_t>(micros() - startedUs) < 120000UL) {
        const bool gdoHigh = digitalRead(RadioPins::kCc1101Gdo0) == HIGH;
        if (gdoHigh) {
            sawGdoHigh = true;
        } else if (sawGdoHigh) {
            sawPacketEnd = true;
        }

        lastTxBytes = ccReadLocked(kCcTxBytes, true);
        underflow = (lastTxBytes & 0x80U) != 0;
        if (underflow) {
            break;
        }
        if ((lastTxBytes & 0x7FU) == 0U && (sawPacketEnd || sawGdoHigh)) {
            break;
        }
        delayMicroseconds(120);
    }

    const uint8_t finalState = ccReadLocked(kCcMarcState, true) & 0x1F;
    const bool fifoDrained = !underflow && ((lastTxBytes & 0x7FU) == 0U);
    const bool strobeAccepted = stxStatus != 0xFF;
    const bool verified = strobeAccepted && fifoDrained && sawGdoHigh;

    g_status.cc1101LastTxVerified = verified;
    ccStrobeLocked(kCcSidle);
    ccStrobeLocked(kCcSftx);

    if (!verified) {
        orionLogPrintf("RADIO", "CC TX verify fail stx=0x%02X gdo=%u end=%u bytes=0x%02X state=0x%02X",
                       stxStatus,
                       sawGdoHigh ? 1U : 0U,
                       sawPacketEnd ? 1U : 0U,
                       lastTxBytes,
                       finalState);
    }
    return verified;
}


void radioManagerService() {
    const uint32_t now = millis();
    if (now - g_lastHealthMs < kHealthIntervalMs) return;
    g_lastHealthMs = now;

    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked() || g_status.owner == RadioBusOwner::ExternalSpi) return;

    if (g_status.owner == RadioBusOwner::NrfPassive) {
        if (!radio1.isChipConnected()) {
            g_status.nrfPresent = false;
            radio1Ok = false;
            g_nrfPassiveConfigured = false;
            publishStatusLocked();
            radioManagerProbePrimaryNrf(false);
        }
    } else if (g_status.owner == RadioBusOwner::CcPassive) {
        const uint8_t version = ccReadLocked(kCcVersion, true);
        const uint8_t state = ccReadLocked(kCcMarcState, true) & 0x1F;
        if (version == 0x00 || version == 0xFF) {
            g_status.cc1101Present = false;
            cc1101Ok = false;
            cc1101Configured = false;
            publishStatusLocked();
            radioManagerProbeCc1101(true, false);
        } else if (state == kCcStateRxFifoOverflow || state == kCcStateTxFifoUnderflow) {
            ccStrobeLocked(kCcSidle);
            if (state == kCcStateRxFifoOverflow) ccStrobeLocked(kCcSfrx);
            if (state == kCcStateTxFifoUnderflow) ccStrobeLocked(kCcSftx);
            ccStrobeLocked(kCcSrx);
        }
    }
}
