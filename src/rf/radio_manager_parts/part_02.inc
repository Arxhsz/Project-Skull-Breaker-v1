    cc1101PreparedPassiveMode = -1;
    cc1101PreparedPassiveMHz = -1.0f;
    g_ccProfile = CcProfile::None;
    g_ccActiveMHz = -1.0f;
    g_status.cc1101LastTxVerified = false;
    if (g_status.owner == RadioBusOwner::CcPassive ||
        g_status.owner == RadioBusOwner::CcActive) {
        markOwnerLocked(RadioBusOwner::None);
    }
}

float correctedFrequency(float frequencyMHz) {
    const float requested = constrain(frequencyMHz, 300.0f, 928.0f);
    return constrain(requested * (1.0f + kFrequencyCorrectionPpm / 1000000.0f), 300.0f, 928.0f);
}

void ccSetFrequencyLocked(float frequencyMHz, bool quick) {
    const float requested = constrain(frequencyMHz, 300.0f, 928.0f);
    const uint32_t word = static_cast<uint32_t>((correctedFrequency(requested) * 65536.0f / 26.0f) + 0.5f);

    ccStrobeLocked(kCcSidle);
    ccWriteRegLocked(0x0D, static_cast<uint8_t>((word >> 16) & 0xFF));
    ccWriteRegLocked(0x0E, static_cast<uint8_t>((word >> 8) & 0xFF));
    ccWriteRegLocked(0x0F, static_cast<uint8_t>(word & 0xFF));

    if (!quick) {
        ccStrobeLocked(kCcScal);
        delayMicroseconds(800);
    }
    ccStrobeLocked(kCcSrx);
    delayMicroseconds(quick ? 120 : 180);
    cc1101CurrentMHz = requested;
    g_status.cc1101FrequencyMHz = requested;
}

void ccConfigureBaseLocked() {
    // Truthful RSSI profile. GDO0 reports carrier sense and the receiver stays
    // in RX; no synthetic values are generated anywhere in this path.
    static const struct {
        uint8_t reg;
        uint8_t value;
    } config[] = {
        {0x00, 0x29}, {0x01, 0x2E}, {0x02, 0x0E}, {0x03, 0x47},
        {0x04, 0xD3}, {0x05, 0x91}, {0x06, 0x3D}, {0x07, 0x04},
        {0x08, 0x05}, {0x0B, 0x06}, {0x0C, 0x00}, {0x10, 0xCA},
        {0x11, 0x83}, {0x12, 0x13}, {0x13, 0x22}, {0x14, 0xF8},
        {0x15, 0x34}, {0x16, 0x07}, {0x17, 0x30}, {0x18, 0x18},
        {0x19, 0x16}, {0x1A, 0x6C}, {0x1B, 0x07}, {0x1C, 0x00},
        {0x1D, 0x91}, {0x21, 0x56}, {0x22, 0x10}, {0x23, 0xE9},
        {0x24, 0x2A}, {0x25, 0x00}, {0x26, 0x1F}, {0x2C, 0x81},
        {0x2D, 0x35}, {0x2E, 0x09},
    };

    ccStrobeLocked(kCcSidle);
    for (const auto& entry : config) {
        ccWriteRegLocked(entry.reg, entry.value);
    }
    const uint8_t maxPower = 0xC0;
    ccWriteBurstLocked(kCcPaTable, &maxPower, 1);
    ccSetFrequencyLocked(rfLockedFrequencyMHz, false);
    g_ccProfile = CcProfile::Rssi;
    g_ccActiveMHz = -1.0f;
    cc1101Configured = true;
}

void ccConfigureRawOokLocked(float frequencyMHz) {
    ccStrobeLocked(kCcSidle);
    ccConfigureBaseLocked();
    ccStrobeLocked(kCcSidle);

    // Asynchronous OOK receive. GDO0 is the real demodulated data stream used
    // by pulse capture/replay tools; this replaces threshold-generated pulses.
    ccWriteRegLocked(0x02, 0x0D);  // IOCFG0: asynchronous serial data output
    ccWriteRegLocked(0x07, 0x00);  // PKTCTRL1: no packet automation
    ccWriteRegLocked(0x08, 0x32);  // PKTCTRL0: asynchronous serial mode
    ccWriteRegLocked(0x12, 0x30);  // MDMCFG2: ASK/OOK, no sync word
    ccWriteRegLocked(0x13, 0x00);
    ccWriteRegLocked(0x14, 0x00);
    ccWriteRegLocked(0x17, 0x00);  // MCSM1: no CCA gating
    ccSetFrequencyLocked(frequencyMHz, false);
    pinMode(RadioPins::kCc1101Gdo0, INPUT);
    g_ccProfile = CcProfile::RawOok;
    g_ccActiveMHz = -1.0f;
    cc1101Configured = true;
}

void ccConfigurePacketTxLocked(float frequencyMHz) {
    ccStrobeLocked(kCcSidle);
    ccConfigureBaseLocked();
    ccStrobeLocked(kCcSidle);

    // Deterministic variable-length packet TX. CCA is disabled so a range-test
    // packet cannot be silently rejected, and GDO0 brackets the real RF packet.
    ccWriteRegLocked(0x02, 0x06);  // IOCFG0: sync sent -> end of packet
    ccWriteRegLocked(0x04, 0xD3);
    ccWriteRegLocked(0x05, 0x91);
    ccWriteRegLocked(0x06, 0x3D);
    ccWriteRegLocked(0x07, 0x04);
    ccWriteRegLocked(0x08, 0x05);  // variable length + CRC
    ccWriteRegLocked(0x10, 0xCA);
    ccWriteRegLocked(0x11, 0x83);
    ccWriteRegLocked(0x12, 0x13);
    ccWriteRegLocked(0x13, 0x22);
    ccWriteRegLocked(0x14, 0xF8);
    ccWriteRegLocked(0x15, 0x34);
    ccWriteRegLocked(0x17, 0x00);  // MCSM1: always allow STX; return IDLE
    ccWriteRegLocked(0x18, 0x18);
    const uint8_t maxPower = 0xC0;
    ccWriteBurstLocked(kCcPaTable, &maxPower, 1);
    ccSetFrequencyLocked(frequencyMHz, false);
    ccStrobeLocked(kCcSidle);
    ccStrobeLocked(kCcSftx);
    pinMode(RadioPins::kCc1101Gdo0, INPUT);
    g_ccProfile = CcProfile::PacketTx;
    g_ccActiveMHz = constrain(frequencyMHz, 300.0f, 928.0f);
    g_status.cc1101LastTxVerified = false;
    cc1101Configured = true;
}


bool ccResetLocked() {
    ensureBusLocked();
    ccDeselectLocked();
    digitalWrite(RadioPins::kCc1101Cs, HIGH);
    delayMicroseconds(50);
    digitalWrite(RadioPins::kCc1101Cs, LOW);
    delayMicroseconds(10);
    digitalWrite(RadioPins::kCc1101Cs, HIGH);
    delayMicroseconds(50);

    if (!ccSelectLocked()) {
        return false;
    }
    SPI.transfer(kCcSres);
    if (!waitCcReadyLocked(12000UL)) {
        ccDeselectLocked();
        return false;
    }
    ccDeselectLocked();
    delay(2);

    const uint8_t version = ccReadLocked(kCcVersion, true);
    g_status.cc1101Version = version;
    g_ccProfile = CcProfile::None;
    g_ccActiveMHz = -1.0f;
    g_status.cc1101LastTxVerified = false;
    return version != 0x00 && version != 0xFF;
}

bool probeNrfLocked(RF24& radio, int cePin, int csnPin, const char* label) {
    ensureBusLocked();
    pinMode(cePin, OUTPUT);
    pinMode(csnPin, OUTPUT);
    digitalWrite(cePin, LOW);
    digitalWrite(csnPin, HIGH);

    for (uint8_t attempt = 0; attempt < 5; ++attempt) {
        deselectAllLocked();
        digitalWrite(cePin, LOW);
        digitalWrite(csnPin, HIGH);
        delay(12);

        const bool beginOk = radio.begin(&SPI);
        delay(5);
        bool connected = beginOk && radio.isChipConnected();
        if (connected) {
            radio.stopListening();
            radio.setChannel(76);
            delayMicroseconds(80);
            connected = radio.isChipConnected() && radio.getChannel() == 76;
        }

        orionLogPrintf("RADIO", "%s probe %u begin=%u register=%u",
                       label ? label : "NRF24",
                       static_cast<unsigned>(attempt + 1),
                       beginOk ? 1U : 0U,
                       connected ? 1U : 0U);

        if (connected) {
            radio.setAutoAck(false);
            radio.setRetries(0, 0);
            radio.setPALevel(RF24_PA_MIN, true);
            radio.setDataRate(RF24_1MBPS);
            radio.setCRCLength(RF24_CRC_16);
            radio.setAddressWidth(5);
            radio.disableDynamicPayloads();
