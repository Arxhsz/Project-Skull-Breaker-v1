#!/usr/bin/env python3
"""
Build a searchable device map from Skull Breaker WiFi/BLE saved MAC CSV files.

Examples:
  python tools/mac_mapper.py import F:\\wifi\\saved_macs F:\\ble\\saved_macs
  python tools/mac_mapper.py import F:\\ --db device_map.json --out-html devices.html
  python tools/mac_mapper.py assign AA:BB:CC:DD:EE:FF --label "Front Door Sensor" --kind BLE --image images/front_door.jpg
"""

from __future__ import annotations

import argparse
import csv
import html
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any


MAC_RE = re.compile(r"^[0-9A-F]{2}(:[0-9A-F]{2}){5}$")


def normalize_mac(value: str | None) -> str:
    if not value:
        return ""
    cleaned = re.sub(r"[^0-9A-Fa-f]", "", value)
    if len(cleaned) != 12:
        return ""
    mac = ":".join(cleaned[i : i + 2] for i in range(0, 12, 2)).upper()
    return mac if MAC_RE.match(mac) else ""


def csv_files(inputs: list[Path]) -> list[Path]:
    files: list[Path] = []
    for item in inputs:
        if item.is_file() and item.suffix.lower() == ".csv":
            files.append(item)
        elif item.is_dir():
            files.extend(sorted(item.rglob("*.csv")))
    return sorted(set(files))


def load_json(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {}


def save_json(path: Path, data: dict[str, Any]) -> None:
    path.write_text(json.dumps(data, indent=2, sort_keys=True), encoding="utf-8")


def best_text(values: set[str]) -> str:
    clean = sorted(v for v in values if v and v != "(hidden)")
    return clean[0] if clean else ""


def parse_scans(files: list[Path]) -> dict[str, dict[str, Any]]:
    devices: dict[str, dict[str, Any]] = {}

    for file_path in files:
        source = "ble" if "ble" in str(file_path).lower() else "wifi"
        try:
            with file_path.open("r", encoding="utf-8-sig", newline="") as handle:
                reader = csv.DictReader(handle)
                for row in reader:
                    mac = normalize_mac(row.get("bssid") or row.get("mac"))
                    if not mac:
                        continue

                    entry = devices.setdefault(
                        mac,
                        {
                            "mac": mac,
                            "oui": mac[:8],
                            "sources": set(),
                            "names": set(),
                            "type_hints": set(),
                            "channels": set(),
                            "security": set(),
                            "seen_records": 0,
                            "strongest_rssi": -999,
                            "last_seen_ms": 0,
                            "scan_files": set(),
                        },
                    )

                    entry["sources"].add(source)
                    entry["scan_files"].add(file_path.name)
                    entry["seen_records"] += 1
                    entry["names"].add(row.get("ssid") or row.get("name") or "")
                    entry["type_hints"].add(row.get("type_hint") or "")
                    entry["channels"].add(row.get("channel") or "")
                    entry["security"].add(row.get("security") or "")

                    try:
                        entry["strongest_rssi"] = max(entry["strongest_rssi"], int(row.get("rssi") or -999))
                    except ValueError:
                        pass

                    try:
                        entry["last_seen_ms"] = max(entry["last_seen_ms"], int(row.get("last_seen_ms") or 0))
                    except ValueError:
                        pass
        except OSError as exc:
            print(f"Skipping {file_path}: {exc}")

    for entry in devices.values():
        entry["sources"] = sorted(entry["sources"])
        entry["scan_files"] = sorted(entry["scan_files"])
        entry["display_name"] = best_text(entry["names"])
        entry["type_hint"] = best_text(entry["type_hints"])
        entry["channels"] = sorted(v for v in entry["channels"] if v)
        entry["security"] = sorted(v for v in entry["security"] if v)
        entry.pop("names", None)
        entry.pop("type_hints", None)

    return dict(sorted(devices.items()))


def merge_assignments(devices: dict[str, dict[str, Any]], db: dict[str, Any]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for mac, entry in devices.items():
        assigned = db.get(mac, {})
        merged = dict(entry)
        merged["label"] = assigned.get("label") or entry.get("display_name") or "Unassigned device"
        merged["kind"] = assigned.get("kind") or "/".join(entry.get("sources", []))
        merged["image"] = assigned.get("image") or ""
        merged["notes"] = assigned.get("notes") or ""
        rows.append(merged)
    return rows


def write_summary_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    fields = [
        "mac",
        "label",
        "kind",
        "oui",
        "display_name",
        "type_hint",
        "strongest_rssi",
        "seen_records",
        "last_seen_ms",
        "channels",
        "security",
        "image",
        "notes",
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            clean = dict(row)
            clean["channels"] = ";".join(clean.get("channels", []))
            clean["security"] = ";".join(clean.get("security", []))
            writer.writerow({field: clean.get(field, "") for field in fields})


def write_html(path: Path, rows: list[dict[str, Any]]) -> None:
    data = json.dumps(rows).replace("</", "<\\/")
    page = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Skull Breaker Device Map</title>
<style>
body {{ margin:0; font-family:Segoe UI, sans-serif; background:#101419; color:#eef4f8; }}
header {{ padding:24px; background:linear-gradient(135deg,#153040,#102018); border-bottom:1px solid #29404a; }}
h1 {{ margin:0 0 8px; font-size:26px; }}
.wrap {{ padding:18px; max-width:1180px; margin:auto; }}
input {{ width:100%; box-sizing:border-box; padding:13px 14px; border-radius:12px; border:1px solid #38515d; background:#172029; color:#fff; font-size:16px; }}
.grid {{ display:grid; grid-template-columns:repeat(auto-fill,minmax(270px,1fr)); gap:14px; margin-top:18px; }}
.card {{ background:#151c23; border:1px solid #293844; border-radius:16px; overflow:hidden; box-shadow:0 10px 30px #0006; }}
.photo {{ height:118px; background:#0b1015; display:flex; align-items:center; justify-content:center; color:#70818b; }}
.photo img {{ width:100%; height:100%; object-fit:cover; }}
.body {{ padding:14px; }}
.label {{ font-size:18px; font-weight:700; margin-bottom:4px; }}
.mac {{ font-family:Consolas, monospace; color:#87d8ff; }}
.pill {{ display:inline-block; margin:8px 6px 0 0; padding:4px 8px; border-radius:999px; background:#22313a; color:#bfd4df; font-size:12px; }}
.meta {{ margin-top:10px; color:#b9c7ce; font-size:13px; line-height:1.45; }}
</style>
</head>
<body>
<header><h1>Skull Breaker Device Map</h1><div>{len(rows)} devices loaded from saved scan CSV files.</div></header>
<div class="wrap">
<input id="q" placeholder="Search MAC, label, type, OUI, notes..." autofocus>
<div id="grid" class="grid"></div>
</div>
<script>
const devices = {data};
const grid = document.getElementById('grid');
const q = document.getElementById('q');
function esc(s) {{ return String(s ?? '').replace(/[&<>"']/g, c => ({{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}}[c])); }}
function draw() {{
  const needle = q.value.toLowerCase();
  grid.innerHTML = '';
  devices.filter(d => JSON.stringify(d).toLowerCase().includes(needle)).forEach(d => {{
    const card = document.createElement('div');
    card.className = 'card';
    const img = d.image ? `<img src="${{esc(d.image)}}" alt="">` : 'No image assigned';
    card.innerHTML = `
      <div class="photo">${{img}}</div>
      <div class="body">
        <div class="label">${{esc(d.label)}}</div>
        <div class="mac">${{esc(d.mac)}}</div>
        <span class="pill">${{esc(d.kind || 'unknown')}}</span>
        <span class="pill">OUI ${{esc(d.oui)}}</span>
        <span class="pill">RSSI ${{esc(d.strongest_rssi)}}</span>
        <div class="meta">
          Name: ${{esc(d.display_name || '-')}}<br>
          Type: ${{esc(d.type_hint || '-')}}<br>
          Channels: ${{esc((d.channels || []).join(', ') || '-')}}<br>
          Security: ${{esc((d.security || []).join(', ') || '-')}}<br>
          Seen records: ${{esc(d.seen_records)}}<br>
          Notes: ${{esc(d.notes || '-')}}
        </div>
      </div>`;
    grid.appendChild(card);
  }});
}}
q.addEventListener('input', draw);
draw();
</script>
</body>
</html>
"""
    path.write_text(page, encoding="utf-8")


def command_import(args: argparse.Namespace) -> None:
    files = csv_files([Path(p) for p in args.inputs])
    devices = parse_scans(files)
    db = load_json(Path(args.db))
    rows = merge_assignments(devices, db)
    write_summary_csv(Path(args.out_csv), rows)
    write_html(Path(args.out_html), rows)
    print(f"Read {len(files)} CSV files and wrote {len(rows)} devices.")
    print(f"CSV:  {Path(args.out_csv).resolve()}")
    print(f"HTML: {Path(args.out_html).resolve()}")


def command_assign(args: argparse.Namespace) -> None:
    mac = normalize_mac(args.mac)
    if not mac:
        raise SystemExit("Invalid MAC address.")
    db_path = Path(args.db)
    db = load_json(db_path)
    entry = db.setdefault(mac, {})
    if args.label is not None:
        entry["label"] = args.label
    if args.kind is not None:
        entry["kind"] = args.kind
    if args.image is not None:
        entry["image"] = args.image
    if args.notes is not None:
        entry["notes"] = args.notes
    save_json(db_path, db)
    print(f"Saved assignment for {mac} in {db_path.resolve()}")


def main() -> None:
parser = argparse.ArgumentParser(description="Skull Breaker saved MAC parser and mapper")
    sub = parser.add_subparsers(dest="command", required=True)

    import_parser = sub.add_parser("import", help="Import saved scan CSVs and build reports")
    import_parser.add_argument("inputs", nargs="+", help="CSV files or folders from the SD card")
    import_parser.add_argument("--db", default="device_map.json", help="MAC assignment JSON database")
    import_parser.add_argument("--out-html", default="devices.html", help="Searchable HTML output")
    import_parser.add_argument("--out-csv", default="devices_summary.csv", help="Summary CSV output")
    import_parser.set_defaults(func=command_import)

    assign_parser = sub.add_parser("assign", help="Assign a human label/image/notes to a MAC")
    assign_parser.add_argument("mac")
    assign_parser.add_argument("--db", default="device_map.json")
    assign_parser.add_argument("--label")
    assign_parser.add_argument("--kind")
    assign_parser.add_argument("--image")
    assign_parser.add_argument("--notes")
    assign_parser.set_defaults(func=command_assign)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
