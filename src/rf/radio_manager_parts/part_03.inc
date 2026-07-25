            radio.setPayloadSize(16);
            radio.openWritingPipe(kNrfAddress);
            radio.flush_rx();
            radio.flush_tx();
            radio.powerDown();
            digitalWrite(cePin, LOW);
            digitalWrite(csnPin, HIGH);
            return true;
        }

        digitalWrite(cePin, LOW);
        digitalWrite(csnPin, HIGH);
        delay(20);
    }
    return false;
}
}  // namespace

void radioManagerPrimeBus() {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        return;
    }
    ensureBusLocked();
}

void radioManagerBeginHardware() {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        return;
    }

    ensureBusLocked();
    pinMode(RadioPins::kCc1101Gdo0, INPUT);
    delay(20);

    g_status.nrfPresent = probeNrfLocked(radio1, RadioPins::kNrfCe, RadioPins::kNrfCsn, "Primary NRF24");
    radio1Ok = g_status.nrfPresent;
    g_lastNrfProbeMs = millis();

    g_status.cc1101Present = false;
    for (uint8_t attempt = 0; attempt < 5 && !g_status.cc1101Present; ++attempt) {
        g_status.cc1101Present = ccResetLocked();
        orionLogPrintf("RADIO", "CC1101 probe %u version=0x%02X ok=%u",
                       static_cast<unsigned>(attempt + 1),
                       g_status.cc1101Version,
                       g_status.cc1101Present ? 1U : 0U);
        if (!g_status.cc1101Present) {
            delay(20);
        }
    }
    g_lastCcProbeMs = millis();

    if (g_status.cc1101Present) {
        ccConfigureBaseLocked();
        cc1101LastReadyMs = millis();
        ccStopLocked();
    } else {
        cc1101Configured = false;
        cc1101LastReadyMs = 0;
    }

    markOwnerLocked(RadioBusOwner::None);
    publishStatusLocked();
}

void radioManagerStopAll() {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        return;
    }
    ensureBusLocked();
    stopNrfLocked();
    ccStopLocked();
    deselectAllLocked();
    markOwnerLocked(RadioBusOwner::None);
}

void radioManagerSuspendForSharedSpi() {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        return;
    }
    ensureBusLocked();
    stopNrfLocked();
    ccStopLocked();
    deselectAllLocked();
    markOwnerLocked(RadioBusOwner::ExternalSpi);
}

void radioManagerResumeAfterSharedSpi() {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        return;
    }
    ensureBusLocked();
    deselectAllLocked();
    markOwnerLocked(RadioBusOwner::None);
}

const RadioManagerStatus& radioManagerStatus() {
    return g_status;
}

bool radioManagerProbeNrf(RF24& radio, int cePin, int csnPin, const char* label) {
    ensureMutex();
    RecursiveLock lock;
    if (!lock.locked()) {
        return false;
    }
    const bool ok = probeNrfLocked(radio, cePin, csnPin, label);
    if (&radio == &radio1) {
        g_status.nrfPresent = ok;
        radio1Ok = ok;
        g_lastNrfProbeMs = millis();
