"""Generate or independently verify the public 1 MiB MSC v1 test pattern."""
import argparse
import hashlib
from pathlib import Path

PATTERN_BYTES = 1024 * 1024
PATTERN_SHA256 = "f395c263d052f99409f2ac4837dfcc5f5c89270e9d347a8306043155dd08ecf7"


def chunks():
    for offset in range(0, PATTERN_BYTES, 4096):
        yield bytes(
            (i * 73 + (i >> 8) * 151 + (i >> 16) * 199 + 0x5A) & 0xFF
            for i in range(offset, min(offset + 4096, PATTERN_BYTES))
        )


def verify(path):
    digest = hashlib.sha256()
    length = 0
    with Path(path).open("rb") as source:
        while True:
            data = source.read(4096)
            if not data:
                break
            digest.update(data)
            length += len(data)
    return length == PATTERN_BYTES and digest.hexdigest() == PATTERN_SHA256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--output", type=Path, help="exclusively create a local reference file")
    mode.add_argument("--verify", type=Path, help="read and verify an existing file without modifying it")
    args = parser.parse_args()
    if args.verify is not None:
        if not verify(args.verify):
            parser.exit(1, "1 MiB size/SHA-256 verification: FAIL\n")
        print("1 MiB size/SHA-256 verification: PASS")
    else:
        digest = hashlib.sha256()
        if args.output is None:
            for data in chunks():
                digest.update(data)
        else:
            with args.output.open("xb") as out:
                for data in chunks():
                    out.write(data)
                    digest.update(data)
        if digest.hexdigest() != PATTERN_SHA256:
            parser.exit(1, "Pattern generator does not match the v1 SHA-256\n")
    print(f"bytes={PATTERN_BYTES} sha256={PATTERN_SHA256}")


if __name__ == "__main__":
    main()
