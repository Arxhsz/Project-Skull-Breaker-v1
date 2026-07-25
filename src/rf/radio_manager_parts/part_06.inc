        publishStatusLocked();
        return radioManagerProbeCc1101(true, false);
    }
    g_status.cc1101Version = version;
    cc1101Version = version;
    cc1101LastReadyMs = millis();
    return true;
}

bool radioManagerPrepareCcPassive(float frequencyMHz, bool force) {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked() || !radioManagerEnsureCc1101()) {
        return false;
    }

    const float target = constrain(frequencyMHz > 0.0f ? frequencyMHz : rfLockedFrequencyMHz,
                                   300.0f, 928.0f);
    const bool rawOokMode = currentRadioMode == RF_SIGNAL_CAPTURE ||
                            currentRadioMode == RF_ROLLING_CAPTURE ||
                            currentRadioMode == RF_REPLAY;
    const CcProfile wantedProfile = rawOokMode ? CcProfile::RawOok : CcProfile::Rssi;

    if (!force && g_status.owner == RadioBusOwner::CcPassive &&
        cc1101PreparedPassiveMode == static_cast<int>(currentRadioMode) &&
        fabsf(cc1101PreparedPassiveMHz - target) < 0.0005f &&
        g_ccProfile == wantedProfile) {
        cc1101LastReadyMs = millis();
        return true;
    }

    stopNrfLocked();
    pinMode(RadioPins::kCc1101Gdo0, INPUT);
    if (rawOokMode) {
        ccConfigureRawOokLocked(target);
    } else {
        ccConfigureBaseLocked();
        if (fabsf(g_status.cc1101FrequencyMHz - target) >= 0.0005f) {
            ccSetFrequencyLocked(target, false);
        }
        g_ccProfile = CcProfile::Rssi;
    }

    cc1101PreparedPassiveMode = static_cast<int>(currentRadioMode);
    cc1101PreparedPassiveMHz = target;
    cc1101LastReadyMs = millis();
    markOwnerLocked(RadioBusOwner::CcPassive);
    return true;
}


bool radioManagerPrepareCcActive(float frequencyMHz, bool force) {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked() || !radioManagerEnsureCc1101()) {
        return false;
    }

    const float target = constrain(frequencyMHz > 0.0f ? frequencyMHz : rfLockedFrequencyMHz,
                                   300.0f, 928.0f);
    const bool frequencyChanged = fabsf(g_ccActiveMHz - target) >= 0.0005f;
    if (!force && g_status.owner == RadioBusOwner::CcActive &&
        g_ccProfile == CcProfile::PacketTx && !frequencyChanged) {
        cc1101LastReadyMs = millis();
        return true;
    }

    stopNrfLocked();
    ccConfigurePacketTxLocked(target);
    cc1101PreparedPassiveMode = -1;
    cc1101PreparedPassiveMHz = -1.0f;
    cc1101LastReadyMs = millis();
    markOwnerLocked(RadioBusOwner::CcActive);
    return g_ccProfile == CcProfile::PacketTx &&
           fabsf(g_ccActiveMHz - target) < 0.0005f;
}


void radioManagerStopCc1101() {
    ensureMutex();
    RecursiveLock lock;
    if (lock.locked()) {
        ccStopLocked();
    }
}

bool radioManagerCcSelect() {
    ensureMutex();
    if (g_spiMutex == nullptr || xSemaphoreTakeRecursive(g_spiMutex, pdMS_TO_TICKS(250)) != pdTRUE) {
        return false;
    }
    if (!ccSelectLocked()) {
        xSemaphoreGiveRecursive(g_spiMutex);
        return false;
    }
    return true;
}

void radioManagerCcDeselect() {
    ccDeselectLocked();
    if (g_spiMutex != nullptr) {
        xSemaphoreGiveRecursive(g_spiMutex);
    }
}

void radioManagerCcWriteReg(uint8_t reg, uint8_t value) {
    ensureMutex();
    RecursiveLock lock;
    if (lock.locked()) ccWriteRegLocked(reg, value);
}

void radioManagerCcWriteBurst(uint8_t reg, const uint8_t* data, size_t length) {
    ensureMutex();
    RecursiveLock lock;
    if (lock.locked()) ccWriteBurstLocked(reg, data, length);
}

void radioManagerCcReadBurst(uint8_t reg, uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) return;
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked() || !ccSelectLocked()) return;
    SPI.transfer(reg | kCcReadBurst);
    for (size_t i = 0; i < length; ++i) data[i] = SPI.transfer(0x00);
    ccDeselectLocked();
}

uint8_t radioManagerCcReadReg(uint8_t reg) {
    ensureMutex();
    RecursiveLock lock;
    return lock.locked() ? ccReadLocked(reg, false) : 0xFF;
}

uint8_t radioManagerCcReadStatus(uint8_t reg) {
    ensureMutex();
    RecursiveLock lock;
    return lock.locked() ? ccReadLocked(reg, true) : 0xFF;
}

