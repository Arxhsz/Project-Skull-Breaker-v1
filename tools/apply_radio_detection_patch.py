from pathlib import Path

# PlatformIO executes this loader before compilation. The implementation is
# split into textual Python fragments so the deterministic runtime rewrite is
# easy to audit without embedding a second giant generated source file.
parts_dir = Path(__file__).with_name("radio_patch_parts")
source = "".join(path.read_text(encoding="utf-8") for path in sorted(parts_dir.glob("part_*.inc")))
exec(compile(source, str(Path(__file__)), "exec"), globals(), globals())
