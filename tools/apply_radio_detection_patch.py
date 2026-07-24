from __future__ import annotations

import re
from pathlib import Path

PATCH_MARKER = "// RADIO_DETECTION_PATCH_V2"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def patch_runtime(project_dir: Path) -> None:
    runtime = project_dir / "src" / "system" / "system_runtime.cpp"
    text = runtime.read_text(encoding="utf-8")

    if PATCH_MARKER in text:
        print("Radio detection patch already applied.")
        return

    text = replace_once(
        text,
        "bool radio1Ok = true;\n",
        "bool radio1Ok = false;\n",
        "radio1 initial state",
    )

    text = replace_once(
        text,
        """#define NRF24_RADIO3_CE  21
#define NRF24_RADIO3_CSN 9

RF24 radio1(NRF24_RADIO1_CE, NRF24_RADIO1_CSN);
RF24 radio2(NRF24_RADIO2_CE, NRF24_RADIO2_CSN, 1000000);  // CC1101 handling (unused NRF24 object)
RF24 radio3(NRF24_RADIO3_CE, NRF24_RADIO3_CSN, 1000000);  // Second NRF24 for dual jamming
""",
        """#define NRF24_RADIO3_CE  21
#define NRF24_RADIO3_CSN 9

// RADIO_DETECTION_PATCH_V2
// GPIO 9 is connected to SPI flash on the ESP32-WROOM/esp32dev target.
// Keep the legacy object for link compatibility, but never probe or drive it on this board.
constexpr bool NRF24_RADIO3_ENABLED = false;

RF24 radio1(NRF24_RADIO1_CE, NRF24_RADIO1_CSN);
RF24 radio2(NRF24_RADIO2_CE, NRF24_RADIO2_CSN, 1000000);  // Legacy unused object
RF24 radio3(NRF24_RADIO3_CE, NRF24_RADIO3_CSN, 1000000);  // Disabled on esp32dev: CSN is a flash pin
""",
        "secondary NRF declaration",
    )

    text = replace_once(
        text,
        """void primeSharedSPIBus() {
    pinMode(NRF24_RADIO1_CE, OUTPUT);
    pinMode(NRF24_RADIO2_CE, OUTPUT);
    pinMode(NRF24_RADIO1_CSN, OUTPUT);
    pinMode(NRF24_RADIO2_CSN, OUTPUT);
    pinMode(CC1101_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    pinMode(TFT_CS, OUTPUT);

    digitalWrite(NRF24_RADIO1_CE, LOW);
    digitalWrite(NRF24_RADIO2_CE, LOW);
    digitalWrite(NRF24_RADIO1_CSN, HIGH);
    digitalWrite(NRF24_RADIO2_CSN, HIGH);
    digitalWrite(CC1101_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
}
""",
        """void primeSharedSPIBus() {
    pinMode(NRF24_RADIO1_CE, OUTPUT);
    pinMode(NRF24_RADIO1_CSN, OUTPUT);
    pinMode(CC1101_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    pinMode(TFT_CS, OUTPUT);

    digitalWrite(NRF24_RADIO1_CE, LOW);
    digitalWrite(NRF24_RADIO1_CSN, HIGH);
    digitalWrite(CC1101_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);

    // Do not touch the legacy secondary NRF pins on esp32dev. GPIO 9 is the
    // module's flash data pin and driving it can break every SPI probe.
    if (NRF24_RADIO3_ENABLED) {
        pinMode(NRF24_RADIO3_CE, OUTPUT);
        pinMode(NRF24_RADIO3_CSN, OUTPUT);
        digitalWrite(NRF24_RADIO3_CE, LOW);
        digitalWrite(NRF24_RADIO3_CSN, HIGH);
    }
}

bool probeNrf24ChipWithRetries(RF24& radio, int cePin, int csnPin, const char* label) {
    pinMode(cePin, OUTPUT);
    pinMode(csnPin, OUTPUT);
    digitalWrite(cePin, LOW);
    digitalWrite(csnPin, HIGH);

    for (uint8_t attempt = 0; attempt < 4; ++attempt) {
        primeSharedSPIBus();
        digitalWrite(cePin, LOW);
        digitalWrite(csnPin, HIGH);
        delay(12);

        const bool beginOk = radio.begin(&SPI);
        delay(6);
        bool chipOk = beginOk && radio.isChipConnected();
        if (!chipOk && beginOk) {
            delay(3);
            chipOk = radio.isChipConnected();
        }

        orionLogPrintf("RF24", "%s probe attempt=%u begin=%u chip=%u",
                       label ? label : "NRF24",
                       static_cast<unsigned>(attempt + 1),
                       beginOk ? 1U : 0U,
                       chipOk ? 1U : 0U);

        if (chipOk) {
            digitalWrite(csnPin, HIGH);
            return true;
        }

        digitalWrite(cePin, LOW);
        digitalWrite(csnPin, HIGH);
        SPI.end();
        delay(8);
        SPI.begin(18, 19, 23);
        delay(12);
    }

    digitalWrite(cePin, LOW);
    digitalWrite(csnPin, HIGH);
    return false;
}
""",
        "shared SPI priming",
    )

    text = replace_once(
        text,
        """bool cc1101SelectReady() {
    // Ensure all other CS lines are deselected before starting a CC1101 transaction
    digitalWrite(NRF24_RADIO1_CSN, HIGH);
    digitalWrite(NRF24_RADIO2_CSN, HIGH);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
    SPI.beginTransaction(SPISettings(CC1101_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(CC1101_CS, LOW);

    // Wait for MISO (GPIO19) to go LOW — CC1101 signals ready by pulling MISO low
    uint32_t start = micros();
    while (digitalRead(19) == HIGH && micros() - start < 2000) {
    }

    return digitalRead(19) == LOW;
}
""",
        """bool cc1101SelectReady() {
    // Ensure every other real SPI client is deselected before CC1101 access.
    digitalWrite(NRF24_RADIO1_CE, LOW);
    digitalWrite(NRF24_RADIO1_CSN, HIGH);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
    if (NRF24_RADIO3_ENABLED) {
        digitalWrite(NRF24_RADIO3_CE, LOW);
        digitalWrite(NRF24_RADIO3_CSN, HIGH);
    }

    SPI.beginTransaction(SPISettings(CC1101_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(CC1101_CS, LOW);

    // Clones and amplified modules can take several milliseconds after reset.
    uint32_t start = micros();
    while (digitalRead(19) == HIGH && micros() - start < 10000UL) {
        delayMicroseconds(2);
    }

    return digitalRead(19) == LOW;
}
""",
        "CC1101 chip select",
    )

    text = replace_once(
        text,
        "while (digitalRead(19) == HIGH && micros() - start < 5000) {",
        "while (digitalRead(19) == HIGH && micros() - start < 10000UL) {",
        "CC1101 reset timeout",
    )

    text = replace_once(
        text,
        """    bool ok = false;
    for (uint8_t attempt = 0; attempt < 3 && !ok; attempt++) {
        ok = cc1101ResetChip();
        if (!ok) {
            rfDiagLog(String("CC1101 reset retry ") + String((int)attempt + 1));
            delay(4);
        }
    }
""",
        """    bool ok = false;
    for (uint8_t attempt = 0; attempt < 5 && !ok; attempt++) {
        primeSharedSPIBus();
        ok = cc1101ResetChip();
        if (!ok) {
            rfDiagLog(String("CC1101 reset retry ") + String((int)attempt + 1));
            delay(15);
        }
    }
""",
        "CC1101 reset retries",
    )

    text = replace_once(
        text,
        """    primeSharedSPIBus();
    uint8_t version = cc1101ReadStatusReg(CC1101_VERSION);
    if (version == 0x00 || version == 0xFF) {
        delayMicroseconds(80);
        primeSharedSPIBus();
        version = cc1101ReadStatusReg(CC1101_VERSION);
    }
    if (version == 0x00 || version == 0xFF) {
""",
        """    uint8_t version = 0xFF;
    for (uint8_t attempt = 0; attempt < 4; ++attempt) {
        primeSharedSPIBus();
        version = cc1101ReadStatusReg(CC1101_VERSION);
        if (version != 0x00 && version != 0xFF) {
            break;
        }
        delay(3);
    }
    if (version == 0x00 || version == 0xFF) {
""",
        "CC1101 live version verification",
    )

    stale_cleanup = """    if (radio3Ok) {
        radio2.stopListening();
        radio2.stopConstCarrier();
        radio2.powerDown();
    }
"""
    fixed_cleanup = """    if (NRF24_RADIO3_ENABLED && radio3Ok) {
        radio3.stopListening();
        radio3.stopConstCarrier();
        radio3.powerDown();
    }
"""
    cleanup_count = text.count(stale_cleanup)
    if cleanup_count < 3:
        raise RuntimeError(f"radio cleanup: expected at least three matches, found {cleanup_count}")
    text = text.replace(stale_cleanup, fixed_cleanup)

    primary_pattern = re.compile(
        r"""    if \(radio1\.begin\(&SPI\)\) \{.*?    // ===== INIT RADIO 3 \(Second NRF24 for Dual Jamming\) =====""",
        re.S,
    )
    primary_replacement = """    radio1Ok = probeNrf24ChipWithRetries(radio1, NRF24_RADIO1_CE, NRF24_RADIO1_CSN, "Radio 1");
    BOOT_LOG(String("Radio 1 init: ") + (radio1Ok ? "OK" : "FAIL"));

    if (radio1Ok) {
        radio1.setAutoAck(false);
        radio1.setRetries(0, 0);
        radio1.setPALevel(RF24_PA_MAX, true);
        radio1.setDataRate(RF24_1MBPS);
        radio1.setCRCLength(RF24_CRC_16);
        radio1.setAddressWidth(5);
        radio1.setPayloadSize(16);
        radio1.stopListening();

        const uint8_t addr[5] = {'S', 'B', 'T', '1', 'A'};
        radio1.openWritingPipe(addr);
        radio1.setChannel(37);
        radio1.flush_rx();
        radio1.flush_tx();

        BOOT_LOG_KV("PA level", radio1.getPALevel());
        BOOT_LOG_KV("Data rate", radio1.getDataRate());
        BOOT_LOG_KV("CRC length", radio1.getCRCLength());
        BOOT_LOG_KV("Channel", radio1.getChannel());

        uint8_t testPkt[16] = {'S', 'B', 'T', '2', 1, 1, 37, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        const bool txOk = radio1.write(testPkt, sizeof(testPkt));
        BOOT_LOG_KV("Diagnostic write", txOk ? "OK" : "WARN");
        radio1Ok = radio1.isChipConnected();
        if (!radio1Ok) {
            delay(3);
            radio1Ok = radio1.isChipConnected();
        }
        BOOT_LOG(String("Radio 1 connected after TX: ") + (radio1Ok ? "YES" : "NO"));

        radio1.stopListening();
        radio1.powerDown();
    } else {
        BOOT_LOG("Radio 1 configuration skipped because probe failed");
    }

    digitalWrite(NRF24_RADIO1_CE, LOW);
    digitalWrite(NRF24_RADIO1_CSN, HIGH);
    delay(80);

    // ===== INIT RADIO 3 (optional secondary NRF24) ====="""
    text, count = primary_pattern.subn(primary_replacement, text, count=1)
    if count != 1:
        raise RuntimeError(f"primary NRF startup: expected one match, found {count}")

    secondary_pattern = re.compile(
        r"""    BOOT_SECTION\("Radio 3"\);.*?    delay\(500\);  // Increased wait before CC1101 initialization \(Bug 1\.41 fix\)""",
        re.S,
    )
    secondary_replacement = """    BOOT_SECTION("Radio 3");
    if (NRF24_RADIO3_ENABLED) {
        BOOT_LOG(String("Initializing secondary NRF24 on CE=") + NRF24_RADIO3_CE + " CSN=" + NRF24_RADIO3_CSN);
        radio3Ok = probeNrf24ChipWithRetries(radio3, NRF24_RADIO3_CE, NRF24_RADIO3_CSN, "Radio 3");
        BOOT_LOG(String("Radio 3 init: ") + (radio3Ok ? "OK" : "FAIL"));

        if (radio3Ok) {
            radio3.setAutoAck(false);
            radio3.setRetries(0, 0);
            radio3.setPALevel(RF24_PA_MAX, true);
            radio3.setDataRate(RF24_1MBPS);
            radio3.setCRCLength(RF24_CRC_16);
            radio3.setAddressWidth(5);
            radio3.setPayloadSize(16);
            const uint8_t address3[5] = {'S', 'B', 'T', '1', 'B'};
            radio3.openWritingPipe(address3);
            radio3.stopListening();
            radio3.powerDown();
        }
    } else {
        radio3Ok = false;
        BOOT_LOG("Secondary NRF24 disabled: GPIO 9 is reserved for ESP32 flash");
    }

    delay(100);"""
    text, count = secondary_pattern.subn(secondary_replacement, text, count=1)
    if count != 1:
        raise RuntimeError(f"secondary NRF startup: expected one match, found {count}")

    runtime.write_text(text, encoding="utf-8")
    print("Applied NRF24/CC1101 detection patch.")


def project_root() -> Path:
    try:
        Import("env")  # type: ignore[name-defined]  # noqa: F821
        return Path(env["PROJECT_DIR"])  # type: ignore[name-defined]  # noqa: F821
    except NameError:
        return Path(__file__).resolve().parents[1]


patch_runtime(project_root())
