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




RADIO_REWRITE_MARKER = "// RADIO_SYSTEM_REWRITE_V3"


def replace_function(text: str, name: str, replacement: str) -> str:
    pattern = re.compile(
        rf"(?m)^(?:bool|void|uint8_t|int|float)\s+{re.escape(name)}\s*\([^;{{]*\)\s*\{{"
    )
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        raise RuntimeError(f"{name}: expected one definition, found {len(matches)}")
    start = matches[0].start()
    brace = text.find("{", matches[0].start(), matches[0].end())
    depth = 0
    end = None
    i = brace
    in_string = False
    in_char = False
    escaped = False
    line_comment = False
    block_comment = False
    while i < len(text):
        c = text[i]
        n = text[i + 1] if i + 1 < len(text) else ""
        if line_comment:
            if c == "\n":
                line_comment = False
        elif block_comment:
            if c == "*" and n == "/":
                block_comment = False
                i += 1
        elif in_string:
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == '"':
                in_string = False
        elif in_char:
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == "'":
                in_char = False
        else:
            if c == "/" and n == "/":
                line_comment = True
                i += 1
            elif c == "/" and n == "*":
                block_comment = True
                i += 1
            elif c == '"':
                in_string = True
            elif c == "'":
                in_char = True
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    end = i + 1
                    break
        i += 1
    if end is None:
        raise RuntimeError(f"{name}: unterminated function body")
    return text[:start] + replacement.rstrip() + "\n" + text[end:]


def rewrite_radio_system(project_dir: Path) -> None:
    runtime = project_dir / "src" / "system" / "system_runtime.cpp"
    text = runtime.read_text(encoding="utf-8")
    if RADIO_REWRITE_MARKER in text:
        print("Radio system rewrite already applied.")
        return

    text = replace_once(
        text,
        '#include "rf.h"\n',
