        publishStatusLocked();
    }
    return ok;
}

bool radioManagerProbePrimaryNrf(bool force) {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        return false;
    }
    const uint32_t now = millis();
    if (!force && now - g_lastNrfProbeMs < kNrfProbeBackoffMs) {
        return g_status.nrfPresent;
    }
    g_lastNrfProbeMs = now;
    const bool wasPresent = g_status.nrfPresent;
    g_status.nrfPresent = probeNrfLocked(radio1, RadioPins::kNrfCe, RadioPins::kNrfCsn, "Primary NRF24");
    if (!wasPresent && g_status.nrfPresent) {
        ++g_status.nrfRecoveries;
    }
    publishStatusLocked();
    return g_status.nrfPresent;
}

bool radioManagerNrfConnected() {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        return false;
    }
    if (!g_status.nrfPresent) {
        return false;
    }
    deselectAllLocked();
    const bool connected = radio1.isChipConnected();
    if (!connected) {
        g_status.nrfPresent = false;
        radio1Ok = false;
        g_nrfPassiveConfigured = false;
        g_nrfTxConfigured = false;
        publishStatusLocked();
    }
    return connected;
}

bool radioManagerPrepareNrfPassive(bool force) {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        return false;
    }
    ensureBusLocked();

    if (!g_status.nrfPresent || !radio1.isChipConnected()) {
        if (!radioManagerProbePrimaryNrf(force)) {
            return false;
        }
    }

    if (g_nrfPassiveConfigured && !force && g_status.owner == RadioBusOwner::NrfPassive) {
        return true;
    }

    ccStopLocked();
    radio1.stopConstCarrier();
    radio1.stopListening();
    radio1.powerUp();
    delay(3);
    radio1.setAutoAck(false);
    radio1.setRetries(0, 0);
    radio1.setPALevel(RF24_PA_MIN, true);
    radio1.setDataRate(RF24_1MBPS);
    radio1.disableCRC();
    radio1.flush_rx();
    radio1.flush_tx();
    radio1.startListening();
    g_nrfPassiveConfigured = true;
    g_nrfTxConfigured = false;
    markOwnerLocked(RadioBusOwner::NrfPassive);
    return true;
}

bool radioManagerSampleNrfRpd(uint8_t channel, uint8_t samples, uint16_t settleUs, uint8_t& hits) {
    hits = 0;
    if (samples == 0 || !radioManagerPrepareNrfPassive(false)) {
        return false;
    }

    RecursiveLock lock;
    if (!lock.locked()) {
        return false;
    }
    const uint8_t selectedChannel = static_cast<uint8_t>(constrain(static_cast<int>(channel), 0, 125));
    radio1.stopListening();
    radio1.setChannel(selectedChannel);
    for (uint8_t i = 0; i < samples; ++i) {
        // RPD is latched while in RX. Leave RX between samples so every hit is
        // an independent measurement instead of repeating the first detection.
        radio1.startListening();
        delayMicroseconds(settleUs < 140 ? 140 : settleUs);
        if (radio1.testRPD()) {
            ++hits;
        }
        radio1.stopListening();
        delayMicroseconds(12);
    }
    radio1.startListening();
    return true;
}

bool radioManagerPrepareNrfPacketTx(uint8_t channel, bool force) {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        return false;
    }
    ensureBusLocked();

    if (!g_status.nrfPresent || !radio1.isChipConnected()) {
