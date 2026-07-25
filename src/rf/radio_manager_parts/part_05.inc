        if (!radioManagerProbePrimaryNrf(force)) {
            return false;
        }
    }

    channel = constrain(channel, 0, 125);
    if (g_nrfTxConfigured && !force && g_nrfTxChannel == channel &&
        g_status.owner == RadioBusOwner::NrfPacketTx) {
        return true;
    }

    ccStopLocked();
    radio1.stopConstCarrier();
    radio1.stopListening();
    radio1.powerUp();
    delay(3);
    radio1.setAutoAck(false);
    radio1.setRetries(0, 0);
    radio1.setPALevel(RF24_PA_MAX, true);
    radio1.setDataRate(RF24_1MBPS);
    radio1.setCRCLength(RF24_CRC_16);
    radio1.setAddressWidth(5);
    radio1.disableDynamicPayloads();
    radio1.setPayloadSize(16);
    radio1.openWritingPipe(kNrfAddress);
    radio1.setChannel(channel);
    radio1.flush_rx();
    radio1.flush_tx();
    g_nrfPassiveConfigured = false;
    g_nrfTxConfigured = true;
    g_nrfTxChannel = channel;
    markOwnerLocked(RadioBusOwner::NrfPacketTx);
    return true;
}

bool radioManagerWriteNrfPacket(const void* payload, size_t length, uint8_t channel) {
    if (payload == nullptr || length == 0 || length > 32 ||
        !radioManagerPrepareNrfPacketTx(channel, false)) {
        return false;
    }

    RecursiveLock lock;
    if (!lock.locked()) {
        return false;
    }
    radio1.setChannel(constrain(channel, 0, 125));
    const bool ok = radio1.write(payload, static_cast<uint8_t>(length));
    if (!ok && !radio1.isChipConnected()) {
        g_status.nrfPresent = false;
        radio1Ok = false;
        g_nrfTxConfigured = false;
        publishStatusLocked();
    }
    return ok;
}

void radioManagerStopNrf() {
    ensureMutex();
    RecursiveLock lock;
    if (lock.locked()) {
        stopNrfLocked();
    }
}

bool radioManagerProbeCc1101(bool configureAfter, bool force) {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        return false;
    }
    const uint32_t now = millis();
    if (!force && now - g_lastCcProbeMs < kCcProbeBackoffMs) {
        return g_status.cc1101Present;
    }
    g_lastCcProbeMs = now;
    cc1101LastProbeAttemptMs = now;

    const bool wasPresent = g_status.cc1101Present;
    g_status.cc1101Present = false;
    for (uint8_t attempt = 0; attempt < 5 && !g_status.cc1101Present; ++attempt) {
        g_status.cc1101Present = ccResetLocked();
        if (!g_status.cc1101Present) {
            delay(20);
        }
    }

    if (!wasPresent && g_status.cc1101Present) {
        ++g_status.cc1101Recoveries;
    }

    if (g_status.cc1101Present && configureAfter) {
        ccConfigureBaseLocked();
        cc1101LastReadyMs = millis();
    } else if (!g_status.cc1101Present) {
        cc1101Configured = false;
        cc1101LastReadyMs = 0;
    }
    cc1101PreparedPassiveMode = -1;
    cc1101PreparedPassiveMHz = -1.0f;
    publishStatusLocked();
    return g_status.cc1101Present;
}

bool radioManagerEnsureCc1101() {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        return false;
    }
    if (!g_status.cc1101Present || !cc1101Configured) {
        return radioManagerProbeCc1101(true, false);
    }
    if (millis() - cc1101LastReadyMs < 750UL) {
        return true;
    }

    const uint8_t version = ccReadLocked(kCcVersion, true);
    if (version == 0x00 || version == 0xFF) {
        g_status.cc1101Present = false;
        cc1101Configured = false;
