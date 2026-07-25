        '#include "rf.h"\n#include "radio_manager.h"\n',
        "radio manager include",
    )

    legacy_radio_block = """#define NRF24_RADIO1_CE  17
#define NRF24_RADIO1_CSN 16
#define NRF24_RADIO2_CE  14
#define NRF24_RADIO2_CSN 20
#define NRF24_RADIO3_CE  21
#define NRF24_RADIO3_CSN 9

// RADIO_DETECTION_PATCH_V2
// GPIO 9 is connected to SPI flash on the ESP32-WROOM/esp32dev target.
// Keep the legacy object for link compatibility, but never probe or drive it on this board.
constexpr bool NRF24_RADIO3_ENABLED = false;

RF24 radio1(NRF24_RADIO1_CE, NRF24_RADIO1_CSN);
RF24 radio2(NRF24_RADIO2_CE, NRF24_RADIO2_CSN, 1000000);  // Legacy unused object
RF24 radio3(NRF24_RADIO3_CE, NRF24_RADIO3_CSN, 1000000);  // Disabled on esp32dev: CSN is a flash pin
"""
    canonical_radio_block = """// RADIO_DETECTION_PATCH_V2
// RADIO_SYSTEM_REWRITE_V3
// V1 contains exactly one NRF24 and one CC1101. The invalid legacy GPIO20
// and flash-connected GPIO9 radio definitions have been removed completely.
RF24 radio1(RadioPins::kNrfCe, RadioPins::kNrfCsn, RadioPins::kNrfSpiHz);
"""
    text = replace_once(
        text,
        legacy_radio_block,
        canonical_radio_block,
        "canonical external radio declarations",
    )

    startup_pattern = re.compile(
        r"    // ===== INIT RADIO 1 =====.*?    refreshDiagnosticsStatus\(\);\n\n    pinMode\(BTN_UP, INPUT_PULLUP\);",
        re.S,
    )
    startup_replacement = """    // ===== EXTERNAL RADIO HARDWARE =====
    BOOT_SECTION("External Radios");
    BOOT_LOG("VSPI SCK=18 MISO=19 MOSI=23");
    BOOT_LOG("NRF24 CE=17 CSN=16");
    BOOT_LOG("CC1101 CSN=22 GDO0=12");
    BOOT_LOG("Legacy NRF pins GPIO20/GPIO9 are disabled");
    radioManagerBeginHardware();
    BOOT_LOG(String("NRF24: ") + (radio1Ok ? "OK" : "FAIL"));
    BOOT_LOG(String("CC1101: ") + (cc1101Ok ? "OK" : "FAIL") +
             " version=0x" + String(cc1101Version, HEX));

    pinMode(BTN_UP, INPUT_PULLUP);"""
    text, count = startup_pattern.subn(startup_replacement, text, count=1)
    if count != 1:
        raise RuntimeError(f"radio startup block: expected one match, found {count}")

    replacements = {
        "claimRadio2ControlPin": """void claimRadio2ControlPin() {
    // Removed: the V1 board has no second NRF24 on a valid GPIO pair.
}""",
        "restoreRadio2ControlPin": """void restoreRadio2ControlPin() {
    // No legacy control pin was claimed.
}""",
        "primeSharedSPIBus": """void primeSharedSPIBus() {
    radioManagerPrimeBus();
}""",
        "probeNrf24ChipWithRetries": """bool probeNrf24ChipWithRetries(RF24& radio, int cePin, int csnPin, const char* label) {
    return radioManagerProbeNrf(radio, cePin, csnPin, label);
}""",
        "cc1101SelectReady": """bool cc1101SelectReady() {
    return radioManagerCcSelect();
}""",
        "cc1101Deselect": """void cc1101Deselect() {
    radioManagerCcDeselect();
}""",
        "cc1101WriteReg": """void cc1101WriteReg(uint8_t reg, uint8_t value) {
    radioManagerCcWriteReg(reg, value);
}""",
        "cc1101WriteBurst": """void cc1101WriteBurst(uint8_t reg, const uint8_t* data, size_t len) {
    radioManagerCcWriteBurst(reg, data, len);
}""",
        "cc1101ReadBurstStatus": """void cc1101ReadBurstStatus(uint8_t reg, uint8_t* buf, uint8_t len) {
    radioManagerCcReadBurst(reg, buf, len);
}""",
        "cc1101ReadReg": """uint8_t cc1101ReadReg(uint8_t reg) {
    return radioManagerCcReadReg(reg);
}""",
        "cc1101ReadStatusReg": """uint8_t cc1101ReadStatusReg(uint8_t reg) {
    return radioManagerCcReadStatus(reg);
}""",
        "cc1101Strobe": """uint8_t cc1101Strobe(uint8_t command) {
    return radioManagerCcStrobe(command);
}""",
        "cc1101ResetChip": """bool cc1101ResetChip() {
    return radioManagerCcReset();
}""",
        "cc1101EnterRx": """void cc1101EnterRx() {
    radioManagerCcEnterRx();
}""",
        "cc1101SetFrequencyMHz": """void cc1101SetFrequencyMHz(float mhz) {
    radioManagerCcSetFrequency(mhz, false);
}""",
        "cc1101SetFrequencyMHzQuick": """void cc1101SetFrequencyMHzQuick(float mhz) {
    radioManagerCcSetFrequency(mhz, true);
}""",
        "cc1101ConfigureBase": """void cc1101ConfigureBase() {
    radioManagerCcConfigureBase();
}""",
        "cc1101ReadRSSI": """int cc1101ReadRSSI() {
    return radioManagerCcReadRssi();
}""",
        "probeCC1101Chip": """bool probeCC1101Chip(bool configureAfter) {
    return radioManagerProbeCc1101(configureAfter, true);
}""",
        "ensureCC1101Ready": """bool ensureCC1101Ready() {
    return radioManagerEnsureCc1101();
}""",
        "prepareCC1101PassiveTool": """bool prepareCC1101PassiveTool(float tuneMHz, bool forceReconfigure) {
    const float target = tuneMHz > 0.0f ? tuneMHz : rfLockedFrequencyMHz;
    return radioManagerPrepareCcPassive(target, forceReconfigure);
}""",
        "prepareCC1101ActiveTool": """bool prepareCC1101ActiveTool(const char* contextLabel, bool forceReconfigure) {
    (void)contextLabel;
    return radioManagerPrepareCcActive(rfLockedFrequencyMHz, forceReconfigure);
}""",
        "cc1101TransmitBurstPacket": """bool cc1101TransmitBurstPacket(const uint8_t* payload, size_t len) {
    return radioManagerCcTransmitPacket(payload, len);
}""",
        "probeRadio2Chip": """bool probeRadio2Chip(bool keepClaimed) {
    (void)keepClaimed;
    radio2Ok = cc1101Ok;
    return cc1101Ok;
}""",
        "configureJamRadio": """void configureJamRadio(RF24& radio, byte startChannel) {
    (void)radio;
    radioManagerPrepareNrfPacketTx(startChannel, true);
}""",
        "initializeJamRadios": """void initializeJamRadios() {
    if (jammerMode == DEACTIVE_MODE) {
        radioManagerStopNrf();
        jammerActive = false;
        return;
    }
    byte channel = 37;
    if (jammerMode == Bluetooth_MODULE) channel = 32;
    if (jammerMode == WIFI_MODULE) channel = 6;
    if (jammerMode == DRONE_MODULE) channel = 45;
    if (jammerMode == CONSTANT_CARRIER) channel = constantCarrierChannel;
    jammerActive = radioManagerPrepareNrfPacketTx(channel, true);
    currentJamChannel = channel;
    jamPacketCount = 0;
    lastJammingTime = millis();
}""",
        "prepare24GHzActiveMode": """bool prepare24GHzActiveMode() {
    if (radio24ActivePrepared) return radio1Ok;
    radio24ActivePrepared = true;
    radio24ActiveTxCount = 0;
    radio24ActiveChannel1 = 0;
    radio24ActiveChannel2 = 0;
    radio24ActiveStatus = "Ready";
    const uint8_t channel = static_cast<uint8_t>(constrain(getSelected24GHzIndex(), 0, 125));
    radio1Ok = radioManagerPrepareNrfPacketTx(channel, true);
    if (!radio1Ok) radio24ActiveStatus = "No radio";
    return radio1Ok;
}""",
        "stop24GHzActiveMode": """void stop24GHzActiveMode() {
    radioManagerStopNrf();
    jammerMode = DEACTIVE_MODE;
    jammerActive = false;
    radioLocked = false;
    radio24ActiveFirstDraw = true;
    radio24ActivePrepared = false;
    radio24ActiveTxCount = 0;
    radio24ActiveChannel1 = 0;
    radio24ActiveChannel2 = 0;
    radio24ActiveStatus = "Idle";
    jamPacketCount = 0;
    currentJamChannel = 0;
    modeChangeRequested = false;
}""",
        "stopCCTool": """void stopCCTool() {
    radioManagerStopCc1101();
    cc1101PreparedPassiveMode = -1;
    cc1101PreparedPassiveMHz = -1.0f;
    rfTransmitActive = false;
    if (wifiTaskHandle != NULL) vTaskResume(wifiTaskHandle);
    if (bleScanTaskHandle != NULL) vTaskResume(bleScanTaskHandle);
    if (spiOperationDepth > 0) endSPIOperation(true, false);
}""",
        "beginSPIOperation": """void beginSPIOperation(bool suspendDisplay) {
    if (suspendDisplay) displayUpdatesSuspended = true;
    if (spiOperationDepth == 0) {
        radioManagerSuspendForSharedSpi();
        digitalWrite(TFT_CS, HIGH);
    }
    spiOperationDepth++;
}""",
        "endSPIOperation": """void endSPIOperation(bool restoreDisplay, bool heavyRestore) {
    if (spiOperationDepth > 0) spiOperationDepth--;
    if (spiOperationDepth > 0) return;
    digitalWrite(TFT_CS, HIGH);
    radioManagerResumeAfterSharedSpi();
    displayUpdatesSuspended = false;
    if (!restoreDisplay) return;
    if (heavyRestore) restoreDisplayAfterHeavyStorageAccess();
    else restoreDisplayAfterStorageAccess();
}""",
        "restoreDisplayAfterStorageAccess": """void restoreDisplayAfterStorageAccess() {
    displayUpdatesSuspended = false;
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    if (startupSequenceActive) return;
    tft.fillRect(0, 0, 240, 320, ILI9341_BLACK);
    markCurrentScreenForRedraw();
}""",
        "restoreDisplayAfterCriticalStorageFailure": """void restoreDisplayAfterCriticalStorageFailure() {
    displayUpdatesSuspended = false;
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    delay(2);
    tft.begin();
    tft.setRotation(2);
