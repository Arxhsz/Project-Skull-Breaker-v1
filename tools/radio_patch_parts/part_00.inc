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

