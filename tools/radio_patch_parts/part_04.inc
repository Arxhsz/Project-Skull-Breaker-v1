        '''    LOG("FORMAT start");
    beginSPIOperation(true);

    wipeFileCount = 0;
''',
        "SD format begin lifecycle",
    )

    text = replace_once(
        text,
        '''    refreshDiagnosticsStatus();
    displayUpdatesSuspended = false;
    restoreDisplayAfterHeavyStorageAccess();
    return ok;
''',
        '''    refreshDiagnosticsStatus();
    endSPIOperation(true, true);
    return ok;
''',
        "SD format end lifecycle",
    )

    text = replace_once(
        text,
        '''    endSPIOperation(false);
    primeSharedSPIBus();
    SPI.begin(18, 19, 23);
    digitalWrite(kSDCS, HIGH);
    digitalWrite(kCC1101CS, HIGH);
    digitalWrite(kTFTCS, HIGH);
    return savedText;
''',
        '''    endSPIOperation(false);
    return savedText;
''',
        "SD metadata bus restore",
    )

    text = replace_once(
        text,
        '''    beginSPIOperation(true);
    SD.end();
    sdCurrentlyMounted = false;
    delay(5);
    SPI.end();
    delay(5);
    pinMode(kTFTCS, OUTPUT);
    digitalWrite(kTFTCS, HIGH);
    pinMode(kCC1101CS, OUTPUT);
    digitalWrite(kCC1101CS, HIGH);
    pinMode(kSDCS, OUTPUT);
    digitalWrite(kSDCS, HIGH);
    delay(5);
    SPI.begin(18, 19, 23);

    sdMounted = false;
''',
        '''    beginSPIOperation(true);
    SD.end();
    sdCurrentlyMounted = false;
    delay(5);
    pinMode(kTFTCS, OUTPUT);
    digitalWrite(kTFTCS, HIGH);
    pinMode(kCC1101CS, OUTPUT);
    digitalWrite(kCC1101CS, HIGH);
    pinMode(kSDCS, OUTPUT);
    digitalWrite(kSDCS, HIGH);
    radioManagerPrimeBus();

    sdMounted = false;
''',
        "SD unmount bus lifecycle",
    )

    if "SPI.end();" in text or "SPI.begin(18, 19, 23" in text:
        raise RuntimeError("storage SPI rewrite left a global SPI restart behind")

    path.write_text(text, encoding="utf-8")
    print("Applied shared-SPI storage rewrite.")


def rewrite_radio_status_ui(project_dir: Path) -> None:
    path = project_dir / "src" / "ui" / "ui_controller.cpp"
    text = path.read_text(encoding="utf-8")
    if UI_REWRITE_MARKER in text:
        print("Radio status UI rewrite already applied.")
        return

    text = replace_once(
        text,
        '''    drawStatusNrf24Icon(24, 1, radio1Ok ? ILI9341_GREEN : ILI9341_RED);
drawStatusCc1101Icon(40, 1, cc1101Ok ? ILI9341_GREEN : ILI9341_RED);
    drawStatusNrf24Icon(56, 1, radio3Ok ? ILI9341_GREEN : ILI9341_RED);
''',
        '''    // V1_CANONICAL_RADIO_STATUS_V1
    drawStatusNrf24Icon(24, 1, radio1Ok ? ILI9341_GREEN : ILI9341_RED);
    drawStatusCc1101Icon(40, 1, cc1101Ok ? ILI9341_GREEN : ILI9341_RED);
''',
        "V1 status-bar radio icons",
    )
    path.write_text(text, encoding="utf-8")
    print("Applied canonical V1 radio status UI.")


root = project_root()
patch_runtime(root)
rewrite_radio_system(root)
rewrite_storage_spi(root)
rewrite_radio_status_ui(root)
