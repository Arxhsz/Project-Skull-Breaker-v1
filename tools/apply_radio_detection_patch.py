from pathlib import Path

# PlatformIO executes this loader through SCons, where Python's __file__ is not
# guaranteed to exist. Resolve the fragment directory from PROJECT_DIR first,
# while preserving direct command-line execution for local verification.
try:
    Import("env")  # type: ignore[name-defined]  # noqa: F821
    parts_dir = Path(env["PROJECT_DIR"]) / "tools" / "radio_patch_parts"  # type: ignore[name-defined]  # noqa: F821
except NameError:
    parts_dir = Path(__file__).resolve().with_name("radio_patch_parts")

parts = sorted(parts_dir.glob("part_*.inc"))
if not parts:
    raise RuntimeError(f"No radio rewrite fragments found in {parts_dir}")

source = "".join(path.read_text(encoding="utf-8") for path in parts)
exec(compile(source, str(parts_dir / "combined_radio_rewrite.py"), "exec"), globals(), globals())
