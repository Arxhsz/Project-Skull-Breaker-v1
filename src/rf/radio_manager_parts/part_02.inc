    cc1101PreparedPassiveMode = -1;
    cc1101PreparedPassiveMHz = -1.0f;
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
}

void ccConfigureBaseLocked() {
    // Stable wideband OOK/2-FSK receive baseline. Individual tools may change
    // modulation registers after this profile is established.
    static const struct {
        uint8_t reg;
        uint8_t value;
    } config[] = {
        {0x00, 0x29}, {0x01, 0x2E}, {0x02, 0x0D}, {0x03, 0x47},
        {0x06, 0x3D}, {0x07, 0x04}, {0x08, 0x05}, {0x0B, 0x06},
        {0x0C, 0x00}, {0x10, 0xCA}, {0x11, 0x83}, {0x12, 0x13},
        {0x13, 0x22}, {0x14, 0xF8}, {0x15, 0x34}, {0x16, 0x07},
        {0x17, 0x30}, {0x18, 0x18}, {0x19, 0x16}, {0x1A, 0x6C},
        {0x1B, 0x07}, {0x1C, 0x00}, {0x1D, 0x91}, {0x21, 0x56},
        {0x22, 0x10}, {0x23, 0xE9}, {0x24, 0x2A}, {0x25, 0x00},
        {0x26, 0x1F}, {0x2C, 0x81}, {0x2D, 0x35}, {0x2E, 0x09},
    };

    for (const auto& entry : config) {
        ccWriteRegLocked(entry.reg, entry.value);
    }
    const uint8_t maxPower = 0xC0;
    ccWriteBurstLocked(kCcPaTable, &maxPower, 1);
    ccSetFrequencyLocked(rfLockedFrequencyMHz, false);
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
