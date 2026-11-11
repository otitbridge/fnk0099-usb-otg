"""Capture a bounded UART observation without requesting a reset."""
import argparse
import time
from pathlib import Path
import serial

parser = argparse.ArgumentParser()
parser.add_argument("--port", required=True)
parser.add_argument("--seconds", type=float, default=35)
parser.add_argument("--output", required=True)
a = parser.parse_args()
if a.seconds <= 0:
    parser.error("--seconds must be positive")
s = serial.Serial()
s.port, s.baudrate, s.timeout = a.port, 115200, 0.2
s.dtr = False
s.rts = False
s.open()
try:
    deadline = time.monotonic() + a.seconds
    with Path(a.output).open("wb") as out:
        while time.monotonic() < deadline:
            data = s.read(4096)
            if data:
                out.write(data)
                out.flush()
                print(data.decode("utf-8", errors="replace"), end="", flush=True)
finally:
    s.close()
