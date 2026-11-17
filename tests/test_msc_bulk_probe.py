"""Check the C streaming writer against the independent Python SHA-256 reference."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from msc_pattern import verify  # noqa: E402


def main():
    local = ROOT / ".local"
    local.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="msc-bulk-host-", dir=local) as name:
        work = Path(name)
        program = work / "test-bulk"
        subprocess.run(
            [os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
             str(ROOT / "tests/test_msc_bulk_probe.c"), str(ROOT / "src/msc_file_probe.c"),
             "-o", str(program)],
            check=True,
        )
        generated = work / "BULK1M.BIN"
        subprocess.run([str(program), str(generated)], check=True)
        assert verify(generated), "C writer output differs from the independent reference SHA-256"
        print("Full C-generated file size/SHA-256 after collision refusal: PASS")


if __name__ == "__main__":
    main()
