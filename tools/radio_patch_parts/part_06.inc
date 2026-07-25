
RSSI_MEMORY_MARKER = "// LIVE_RSSI_INT8_STORAGE"


def shrink_live_rssi_storage(project_dir: Path) -> None:
    runtime = project_dir / "src" / "system" / "system_runtime.cpp"
    text = runtime.read_text(encoding="utf-8")
    if RSSI_MEMORY_MARKER in text:
        print("Compact live RSSI storage already applied.")
        return

    old_decl = "int16_t rfSweepRssiDbm[RF_SWEEP_POINTS] = {0};\n"
    new_decl = "int8_t rfSweepRssiDbm[RF_SWEEP_POINTS] = {0};\n" + RSSI_MEMORY_MARKER + "\n"
    if old_decl not in text:
        raise RuntimeError("compact RSSI storage: expected live RSSI array was not found")
    text = text.replace(old_decl, new_decl, 1)
    text = text.replace("static_cast<int16_t>(rssi)", "static_cast<int8_t>(rssi)")
    runtime.write_text(text, encoding="utf-8")
    print("Applied compact signed RSSI storage.")


shrink_live_rssi_storage(project_root())
