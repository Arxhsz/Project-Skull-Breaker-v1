}

void publishStatusLocked() {
    radio1Ok = g_status.nrfPresent;
    cc1101Ok = g_status.cc1101Present;
    cc1101Version = g_status.cc1101Version;
    radio2Ok = cc1101Ok;
    radio3Ok = false;
    refreshDiagnosticsStatus();
}

void markOwnerLocked(RadioBusOwner owner) {
    g_status.owner = owner;
}

void stopNrfLocked() {
    if (g_status.nrfPresent) {
        radio1.stopConstCarrier();
        radio1.stopListening();
        radio1.flush_rx();
        radio1.flush_tx();
        radio1.powerDown();
    }
    digitalWrite(RadioPins::kNrfCe, LOW);
    digitalWrite(RadioPins::kNrfCsn, HIGH);
    g_nrfPassiveConfigured = false;
    g_nrfTxConfigured = false;
    g_nrfTxChannel = 0xFF;
    g_status.nrfChannel = 0xFF;
    if (g_status.owner == RadioBusOwner::NrfPassive ||
        g_status.owner == RadioBusOwner::NrfPacketTx) {
        markOwnerLocked(RadioBusOwner::None);
    }
}

bool waitCcReadyLocked(uint32_t timeoutUs) {
    const uint32_t started = micros();
    while (digitalRead(RadioPins::kSpiMiso) == HIGH) {
        if (static_cast<uint32_t>(micros() - started) >= timeoutUs) {
            return false;
        }
        delayMicroseconds(2);
    }
    return true;
}

bool ccSelectLocked() {
    ensureBusLocked();
    digitalWrite(RadioPins::kNrfCe, LOW);
    digitalWrite(RadioPins::kNrfCsn, HIGH);
    digitalWrite(RadioPins::kSdCs, HIGH);
    digitalWrite(RadioPins::kTftCs, HIGH);

    SPI.beginTransaction(SPISettings(RadioPins::kCc1101SpiHz, MSBFIRST, SPI_MODE0));
    digitalWrite(RadioPins::kCc1101Cs, LOW);
    if (!waitCcReadyLocked(12000UL)) {
        digitalWrite(RadioPins::kCc1101Cs, HIGH);
        SPI.endTransaction();
        g_ccTransactionOpen = false;
        return false;
    }

    g_ccTransactionOpen = true;
    return true;
}

void ccDeselectLocked() {
    digitalWrite(RadioPins::kCc1101Cs, HIGH);
    if (g_ccTransactionOpen) {
        SPI.endTransaction();
        g_ccTransactionOpen = false;
    }
}

uint8_t ccStrobeLocked(uint8_t command) {
    if (!ccSelectLocked()) {
        return 0xFF;
    }
    const uint8_t status = SPI.transfer(command);
    ccDeselectLocked();
    return status;
}

void ccWriteRegLocked(uint8_t reg, uint8_t value) {
    if (!ccSelectLocked()) {
        return;
    }
    SPI.transfer(reg);
    SPI.transfer(value);
    ccDeselectLocked();
}

void ccWriteBurstLocked(uint8_t reg, const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0 || !ccSelectLocked()) {
        return;
    }
    SPI.transfer(reg | kCcWriteBurst);
    for (size_t i = 0; i < length; ++i) {
        SPI.transfer(data[i]);
    }
    ccDeselectLocked();
}

uint8_t ccReadLocked(uint8_t reg, bool statusRegister) {
    if (!ccSelectLocked()) {
        return 0xFF;
    }
    SPI.transfer(reg | (statusRegister ? kCcReadBurst : kCcReadSingle));
    const uint8_t value = SPI.transfer(0x00);
    ccDeselectLocked();
    return value;
}

void ccStopLocked() {
    if (g_status.cc1101Present) {
        ccStrobeLocked(kCcSidle);
        ccStrobeLocked(kCcSfrx);
        ccStrobeLocked(kCcSftx);
    }
    pinMode(RadioPins::kCc1101Gdo0, INPUT);
    digitalWrite(RadioPins::kCc1101Cs, HIGH);
