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
    const uint8_t raw = radioManagerCcReadStatus(kCcRssi);
    if (raw == 0xFF) return -120;
    const int signedRaw = raw >= 128 ? static_cast<int>(raw) - 256 : raw;
    return (signedRaw / 2) - 74;
}

bool radioManagerCcTransmitPacket(const uint8_t* payload, size_t length) {
    if (payload == nullptr || length == 0 || length > 61 ||
        !radioManagerPrepareCcActive(rfLockedFrequencyMHz, false)) {
        return false;
    }

    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) return false;

    ccStrobeLocked(kCcSidle);
    ccStrobeLocked(kCcSftx);
    const uint8_t packetLength = static_cast<uint8_t>(length);
    ccWriteBurstLocked(kCcTxFifo, &packetLength, 1);
    ccWriteBurstLocked(kCcTxFifo, payload, length);
    ccStrobeLocked(kCcStx);

    const uint32_t started = millis();
    while (millis() - started < 80UL) {
        const uint8_t txBytes = ccReadLocked(kCcTxBytes, true);
        if ((txBytes & 0x7F) == 0) break;
        delay(1);
    }
    const uint8_t state = ccReadLocked(kCcMarcState, true) & 0x1F;
    const bool ok = state != kCcStateTxFifoUnderflow;
    ccStrobeLocked(kCcSidle);
    ccStrobeLocked(kCcSftx);
    return ok;
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
