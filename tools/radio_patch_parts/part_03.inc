    if (startupSequenceActive) return;
    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();
    markCurrentScreenForRedraw();
}""",
        "restoreDisplayAfterHeavyStorageAccess": """void restoreDisplayAfterHeavyStorageAccess() {
    displayUpdatesSuspended = false;
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, HIGH);
    digitalWrite(TFT_CS, HIGH);
    delay(2);
    tft.begin();
    tft.setRotation(2);
    if (startupSequenceActive) return;
    tft.fillScreen(ILI9341_BLACK);
    drawStatusBar();
    markCurrentScreenForRedraw();
}""",
    }

    for name, replacement in replacements.items():
        text = replace_function(text, name, replacement)

    legacy_nrf_shutdown = """    radio1.stopListening();
    radio1.stopConstCarrier();
    radio1.powerDown();
    if (NRF24_RADIO3_ENABLED && radio3Ok) {
        radio3.stopListening();
        radio3.stopConstCarrier();
        radio3.powerDown();
    }
"""
    legacy_count = text.count(legacy_nrf_shutdown)
    if legacy_count != 2:
        raise RuntimeError(f"legacy NRF shutdown blocks: expected two matches, found {legacy_count}")
    text = text.replace(legacy_nrf_shutdown, "    radioManagerStopNrf();\n")

    cleanup_pattern = re.compile(
        r"    radio1\.stopListening\(\);\n    radio1\.stopConstCarrier\(\);\n    radio1\.powerDown\(\);\n\n    if \(NRF24_RADIO3_ENABLED && radio3Ok\) \{.*?    \}\n\n    delay\(20\);",
        re.S,
    )
    text, count = cleanup_pattern.subn(
        "    radioManagerStopAll();\n\n    delay(20);", text, count=1
    )
    if count != 1:
        raise RuntimeError(f"mode cleanup block: expected one match, found {count}")

    # The SPI peripheral is initialized once by RadioManager. Repeated begin/end
    # cycles invalidate RF24 state and can make both external radios disappear.
    text = text.replace(
        "    SPI.end();\n    delay(10);\n    SPI.begin(18, 19, 23);\n",
        "    radioManagerPrimeBus();\n    delay(10);\n",
    )
    text = text.replace("    SPI.begin(18, 19, 23);\n", "    radioManagerPrimeBus();\n")

    # SD capture is an explicit shared-SPI transaction. Stop both radios before
    # opening the file and restore the CC1101 receive profile afterward.
    text = replace_once(
        text,
        """    ensureSDFolders();
    displayUpdatesSuspended = true;
    primeSharedSPIBus();
    radioManagerPrimeBus();
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(CC1101_CS, HIGH);
""",
        """    ensureSDFolders();
    beginSPIOperation(true);
""",
        "RF capture SD begin",
    )
    text = replace_once(
        text,
        """        digitalWrite(TFT_CS, HIGH);
        displayUpdatesSuspended = false;
        cc1101EnterRx();
        restoreDisplayAfterHeavyStorageAccess();
        return false;
""",
        """        endSPIOperation(true, true);
        prepareCC1101PassiveTool(rfLockedFrequencyMHz, true);
        return false;
""",
        "RF capture SD failure cleanup",
    )
    text = replace_once(
        text,
        """    digitalWrite(TFT_CS, HIGH);
    displayUpdatesSuspended = false;
    cc1101EnterRx();

    rfCaptureStatus = "Saved .sub";
    rfCaptureStatusUntil = millis() + 1800;
    restoreDisplayAfterHeavyStorageAccess();
""",
        """    endSPIOperation(true, true);
    prepareCC1101PassiveTool(rfLockedFrequencyMHz, true);

    rfCaptureStatus = "Saved .sub";
    rfCaptureStatusUntil = millis() + 1800;
""",
        "RF capture SD success cleanup",
    )

    # V1 has no secondary NRF24. Show it as not installed rather than a fault.
    text = text.replace(
        '    drawStatusLine(116, "NRF2", radio3Ok ? "OK" : "MISS", radio3Ok ? ILI9341_GREEN : textSoft);',
        '    drawStatusLine(116, "NRF2", "N/A", textSoft);',
        1,
    )

    runtime.write_text(text, encoding="utf-8")
    print("Applied centralized V1 radio system rewrite.")

STORAGE_REWRITE_MARKER = "// RADIO_STORAGE_SPI_REWRITE_V1"
UI_REWRITE_MARKER = "// V1_CANONICAL_RADIO_STATUS_V1"


def rewrite_storage_spi(project_dir: Path) -> None:
    path = project_dir / "src" / "storage" / "storage_controller.cpp"
    text = path.read_text(encoding="utf-8")
    if STORAGE_REWRITE_MARKER in text:
        print("Storage SPI rewrite already applied.")
        return

    text = replace_once(
        text,
        '#include "storage.h"\n',
        '#include "storage.h"\n#include "radio_manager.h"\n' + STORAGE_REWRITE_MARKER + '\n',
        "storage radio manager include",
    )

    text = replace_function(
        text,
        "recoverSharedSPIBusAfterSDFailure",
        '''void recoverSharedSPIBusAfterSDFailure() {
    // SD.end() releases only the SD client. Never stop the global VSPI bus:
    // the TFT, NRF24, and CC1101 all share it.
    SD.end();
    sdCurrentlyMounted = false;
    delay(5);
    radioManagerPrimeBus();
    digitalWrite(kSDCS, HIGH);
    digitalWrite(kCC1101CS, HIGH);
    digitalWrite(kTFTCS, HIGH);
    delay(10);
}''',
    )

    text = replace_function(
        text,
        "prepareSDBusForMount",
        '''void prepareSDBusForMount(uint16_t settleDelayMs) {
    SD.end();
    sdCurrentlyMounted = false;
    delay(5);
    radioManagerPrimeBus();
    pinMode(kSDCS, OUTPUT);
    pinMode(kCC1101CS, OUTPUT);
    pinMode(kTFTCS, OUTPUT);
    digitalWrite(kSDCS, HIGH);
    digitalWrite(kCC1101CS, HIGH);
    digitalWrite(kTFTCS, HIGH);
    delay(settleDelayMs);
}''',
    )

    text = replace_once(
        text,
        '''    if (!SD.begin(kSDCS, SPI, speedHz)) {
        SD.end();
        sdCurrentlyMounted = false;
        SPI.end();
        delay(10);
        return false;
    }
''',
        '''    if (!SD.begin(kSDCS, SPI, speedHz)) {
        SD.end();
        sdCurrentlyMounted = false;
        digitalWrite(kSDCS, HIGH);
        radioManagerPrimeBus();
        delay(10);
        return false;
    }
''',
        "SD mount failure bus recovery",
    )

    text = replace_once(
        text,
        '''    SD.end();
    sdCurrentlyMounted = false;
    SPI.end();
    delay(10);
    delay(6);
''',
        '''    SD.end();
    sdCurrentlyMounted = false;
    digitalWrite(kSDCS, HIGH);
    radioManagerPrimeBus();
    delay(10);
    delay(6);
''',
        "SD readiness failure bus recovery",
    )

    text = replace_once(
        text,
        '''    LOG("FORMAT start");
    displayUpdatesSuspended = true;
    primeSharedSPIBus();
    SPI.begin(18, 19, 23);
    digitalWrite(kTFTCS, HIGH);
    digitalWrite(kCC1101CS, HIGH);

    wipeFileCount = 0;
''',
