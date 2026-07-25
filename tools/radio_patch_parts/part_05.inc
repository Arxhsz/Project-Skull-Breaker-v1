
LIVE_DIAGNOSTICS_MARKER = "// LIVE_RADIO_DIAGNOSTICS_V1"


def rewrite_live_radio_diagnostics(project_dir: Path) -> None:
    runtime = project_dir / "src" / "system" / "system_runtime.cpp"
    text = runtime.read_text(encoding="utf-8")
    if LIVE_DIAGNOSTICS_MARKER in text:
        print("Live radio diagnostics already applied.")
        return

    text = replace_once(
        text,
        "uint8_t rfSweepNoiseFloor[RF_SWEEP_POINTS] = {0};\n",
        "uint8_t rfSweepNoiseFloor[RF_SWEEP_POINTS] = {0};\n"
        "int16_t rfSweepRssiDbm[RF_SWEEP_POINTS] = {0};\n"
        + LIVE_DIAGNOSTICS_MARKER + "\n",
        "real CC1101 RSSI storage",
    )

    text = replace_function(
        text,
        "cc1101ApplyFrequencyCorrection",
        '''float cc1101ApplyFrequencyCorrection(float mhz) {
    // No hidden offset. The displayed frequency is the frequency programmed
    // into the CC1101. Calibration can be added later as an explicit setting.
    return constrain(mhz, 300.0f, 928.0f);
}''',
    )

    text = replace_function(
        text,
        "resetRFSweepLevels",
        '''void resetRFSweepLevels() {
    for (int i = 0; i < RF_SWEEP_POINTS; i++) {
        rfSweepLevels[i] = 0;
        rfSweepPeakHold[i] = 0;
        rfSweepNoiseFloor[i] = 0;
        rfSweepRssiDbm[i] = -127;
    }
    rfSweepWarmupPasses = 0;
    rfLastSampleMs = 0;
    rfSweepLastStepUs = 0;
    rfSweepCursor = 0;
    rfSweepWaterfallColumn = 0;
    rfSweepStrongestRSSI = -127;
    rfSweepStrongestMHz = rfLockedFrequencyMHz;
    uint16_t waterfallBg = tft.color565(2, 4, 12);
    for (size_t i = 0; i < (sizeof(rfSweepWaterfallBuffer) / sizeof(rfSweepWaterfallBuffer[0])); i++) {
        rfSweepWaterfallBuffer[i] = waterfallBg;
    }
    memset(rfSweepSpectrumBuffer, 0, sizeof(rfSweepSpectrumBuffer));
}''',
    )

    text = replace_function(
        text,
        "runRFScanner",
        '''void runRFScanner() {
    if (!prepareCC1101PassiveTool(rfLockedFrequencyMHz, rfScannerFirstDraw)) {
        drawRFUnavailable("RF Scanner", "CC1101 real RSSI view");
        delay(80);
        return;
    }

    static const int GX = 22;
    static const int GY = 76;
    static const int GW = 204;
    static const int GH = 138;
    static float lastCenterMHz = -1.0f;
    static int displayedFloor = -127;

    const RFBandPreset& band = rfBandPresets[rfBandIndex];
    const float requestedSpan = rfBandIndex >= 2 ? 5.0f : 3.0f;
    float startMHz = max(band.startMHz, rfLockedFrequencyMHz - requestedSpan * 0.5f);
    float endMHz = min(band.endMHz, rfLockedFrequencyMHz + requestedSpan * 0.5f);
    if (endMHz - startMHz < 0.25f) {
        startMHz = band.startMHz;
        endMHz = band.endMHz;
    }

    const bool centerChanged = fabsf(lastCenterMHz - rfLockedFrequencyMHz) >= 0.0005f;
    if (rfScannerFirstDraw || centerChanged) {
        lastCenterMHz = rfLockedFrequencyMHz;
        drawRFScreenHeader("RF Scanner", "REAL RSSI - no synthetic floor");
        tft.drawRect(GX - 1, GY - 1, GW + 2, GH + 2, tft.color565(58, 58, 58));
        tft.fillRect(GX, GY, GW, GH, ILI9341_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(95, 95, 105));
        tft.setCursor(0, GY - 3); tft.print("-35");
        tft.setCursor(0, GY + 31); tft.print("-55");
        tft.setCursor(0, GY + 66); tft.print("-75");
        tft.setCursor(0, GY + 101); tft.print("-95");
        tft.setCursor(GX, GY + GH + 5); tft.print(startMHz, 2);
        tft.setCursor(GX + 82, GY + GH + 5); tft.print(rfLockedFrequencyMHz, 2);
        tft.setCursor(GX + 164, GY + GH + 5); tft.print(endMHz, 2);
        drawRFFooterLine(0, 272, "UP/DN center  SELECT pause");
        drawRFFooterLine(1, 286, "HOLD-R SEL/L MHz  UP/DN band");
        resetRFSweepLevels();
        displayedFloor = -127;
        rfScannerFirstDraw = false;
    }

    if (!rfSweepPaused && (rfLastSampleMs == 0 || millis() - rfLastSampleMs >= 12UL)) {
        rfLastSampleMs = millis();
        const int index = rfSweepCursor;
        const float frequencyMHz = startMHz +
            ((endMHz - startMHz) * index) / static_cast<float>(RF_SWEEP_POINTS - 1);
        const int rssi = cc1101MeasureSweepRSSI(frequencyMHz, index);
        rfSweepRssiDbm[index] = static_cast<int16_t>(rssi);
        rfCurrentRSSI = rssi;

        const uint8_t level = static_cast<uint8_t>(constrain(map(rssi, -115, -35, 0, 100), 0, 100));
        rfSweepLevels[index] = level;
        if (level > rfSweepPeakHold[index]) rfSweepPeakHold[index] = level;

        int x0 = GX + (index * GW) / RF_SWEEP_POINTS;
        int x1 = GX + ((index + 1) * GW) / RF_SWEEP_POINTS - 1;
        if (x1 < x0) x1 = x0;
        const int width = x1 - x0 + 1;
        tft.fillRect(x0, GY, width, GH, ILI9341_BLACK);
        for (int gridDbm : {-55, -75, -95}) {
            int y = GY + map(gridDbm, -35, -115, 0, GH - 1);
            for (int x = x0; x <= x1; x += 3) tft.drawPixel(x, y, tft.color565(32, 32, 40));
        }
        const int barHeight = (level * (GH - 2)) / 100;
        if (barHeight > 0) {
            const uint16_t color = getWaterfallPaletteColor((level * 25) / 100);
            tft.fillRect(x0, GY + GH - barHeight, width, barHeight, color);
        }

        rfSweepCursor++;
        if (rfSweepCursor >= RF_SWEEP_POINTS) {
            rfSweepCursor = 0;
            int realPeak = -127;
            int64_t sum = 0;
            int valid = 0;
            int peakIndex = 0;
            for (int i = 0; i < RF_SWEEP_POINTS; ++i) {
                const int value = rfSweepRssiDbm[i];
                if (value < -127 || value > 0) continue;
                sum += value;
                valid++;
                if (value > realPeak) {
                    realPeak = value;
                    peakIndex = i;
                }
            }
            displayedFloor = valid > 0 ? static_cast<int>(sum / valid) : -127;
            rfSweepStrongestRSSI = realPeak;
            rfSweepStrongestMHz = startMHz +
                ((endMHz - startMHz) * peakIndex) / static_cast<float>(RF_SWEEP_POINTS - 1);
            orionToolsServiceRfCsv("scan", rfSweepStrongestMHz, rfSweepStrongestRSSI);
            orionToolsRssiToneTick(rfSweepStrongestRSSI);
            orionToolsDecayRfPeakHold(rfSweepPeakHold, rfSweepLevels, RF_SWEEP_POINTS, 2);
        }
    }

    drawRFStatLine(44, String("Center ") + String(rfLockedFrequencyMHz, 3),
                   rfSweepPaused ? "PAUSED" : String("Now ") + String(cc1101CurrentMHz, 3));
    drawRFStatLine(58, String("Peak ") + String(rfSweepStrongestMHz, 3),
                   String(rfSweepStrongestRSSI) + " dBm");
    drawRFStatLine(220, String("Mean floor ") + String(displayedFloor) + " dBm",
                   String("Live ") + String(rfCurrentRSSI) + " dBm");
}''',
    )

    text = replace_function(
        text,
        "beginRFSignalCaptureRecording",
        '''void beginRFSignalCaptureRecording() {
    resetRFSignalCaptureRecording();
    if (!radioManagerPrepareCcPassive(rfLockedFrequencyMHz, true)) {
        rfCaptureRecording = false;
        rfCaptureStatus = "CC1101 offline";
        rfCaptureStatusUntil = millis() + 1800;
        return;
    }

    rfCaptureRecording = true;
    rfCaptureRecordingStartMs = millis();
    rfCaptureWindowStart = millis();
    rfCaptureHits = 0;
    rfCapturePeakHits = 0;
    rfCaptureStatus = "Real GDO0 pulses";
    rfCaptureStatusUntil = millis() + 1500;
    pinMode(CC1101_GDO0, INPUT);
    tft.fillRect(14, 124, 212, 116, ILI9341_BLACK);
    LOG("Real asynchronous GDO0 capture started on pin " + String(CC1101_GDO0));
}''',
    )

    text = replace_function(
        text,
        "saveRFSignalCaptureToSD",
        '''bool saveRFSignalCaptureToSD() {
    if (rfCaptureRecordedCount < 4) {
        rfCaptureStatus = "No real pulses";
        rfCaptureStatusUntil = millis() + 1800;
        return false;
    }
    if (!sdMounted || sdError || !validateSDReady(false)) {
        rfCaptureStatus = "Mount SD first";
        rfCaptureStatusUntil = millis() + 1800;
        return false;
    }

    ensureSDFolders();
    beginSPIOperation(true);
    char captureName[48];
    time_t now = time(nullptr);
    if (now > 1700000000) {
        struct tm timeInfo;
        localtime_r(&now, &timeInfo);
        snprintf(captureName, sizeof(captureName),
                 "/captures/rf/%04d%02d%02d_%02d%02d%02d.sub",
                 timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
                 timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    } else {
        snprintf(captureName, sizeof(captureName), "/captures/rf/capture_%010lu.sub", millis());
    }

    String filePath = captureName;
    File file = SD.open(filePath, FILE_WRITE);
    if (!file) {
        delay(5);
        file = SD.open(filePath, FILE_WRITE);
    }
    if (!file) {
        rfCaptureStatus = "Save failed";
        rfCaptureStatusUntil = millis() + 1800;
        endSPIOperation(true, true);
        prepareCC1101PassiveTool(rfLockedFrequencyMHz, true);
        return false;
    }

    file.println("Filetype: Flipper SubGhz RAW File");
    file.println("Version: 1");
    file.println("Frequency: " + String((uint32_t)(rfLockedFrequencyMHz * 1000000.0f)));
    file.println("Preset: FuriHalSubGhzPresetOok650Async");
    file.println("Protocol: RAW");

    String rawLine;
    rawLine.reserve(224);
    int writtenPulses = 0;
    auto flushLine = [&]() {
        if (rawLine.length() == 0) return;
        file.println("RAW_Data: " + rawLine);
        rawLine = "";
    };
    for (int i = 0; i < rfCaptureRecordedCount; ++i) {
        const long pulse = rfCaptureRecordedHistory[i];
        if (pulse == 0) continue;
        String token = String(pulse);
        if (rawLine.length() > 0 && rawLine.length() + token.length() + 1 > 220) flushLine();
        if (rawLine.length() > 0) rawLine += " ";
        rawLine += token;
        writtenPulses++;
        if ((i & 0x1F) == 0x1F) yield();
    }
    flushLine();
    file.close();

    if (writtenPulses < 4) {
        SD.remove(filePath);
        endSPIOperation(true, true);
        prepareCC1101PassiveTool(rfLockedFrequencyMHz, true);
        rfCaptureStatus = "No real pulses";
        rfCaptureStatusUntil = millis() + 1800;
        return false;
    }

    endSPIOperation(true, true);
    prepareCC1101PassiveTool(rfLockedFrequencyMHz, true);
    rfCaptureStatus = "Saved real RAW";
    rfCaptureStatusUntil = millis() + 1800;
    return true;
}''',
    )

    text = replace_once(
        text,
        '''            int rssi = cc1101MeasureSweepRSSI(freq, rfSweepCursor);
            uint8_t rawLevel = rfLevelFromRSSI(rssi);''',
        '''            int rssi = cc1101MeasureSweepRSSI(freq, rfSweepCursor);
            rfSweepRssiDbm[rfSweepCursor] = static_cast<int16_t>(rssi);
            uint8_t rawLevel = rfLevelFromRSSI(rssi);''',
        "wide sweep real RSSI storage",
    )

    text = replace_once(
        text,
        '''        rfSweepStrongestRSSI = -120;
        rfSweepStrongestMHz = rfBandPresets[rfBandIndex].startMHz;
        for (int i = 0; i < RF_SWEEP_POINTS; i++) {
            int pseudoRssi = map(rfSweepLevels[i], 0, 100, -110, -40);
            if (pseudoRssi >= rfSweepStrongestRSSI) {
                rfSweepStrongestRSSI = pseudoRssi;
                rfSweepStrongestMHz = rfSweepFrequencyForIndex(i);
            }

            if ((i & 0x0F) == 0x0F) {
                yield();
            }
        }''',
        '''        rfSweepStrongestRSSI = -127;
        rfSweepStrongestMHz = rfBandPresets[rfBandIndex].startMHz;
        for (int i = 0; i < RF_SWEEP_POINTS; i++) {
            const int realRssi = rfSweepRssiDbm[i];
            if (realRssi > rfSweepStrongestRSSI) {
                rfSweepStrongestRSSI = realRssi;
                rfSweepStrongestMHz = rfSweepFrequencyForIndex(i);
            }
            if ((i & 0x0F) == 0x0F) yield();
        }''',
        "wide sweep real peak calculation",
    )

    runtime.write_text(text, encoding="utf-8")
    print("Applied truthful live radio diagnostics.")


rewrite_live_radio_diagnostics(project_root())
