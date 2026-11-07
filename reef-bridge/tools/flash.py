#!/usr/bin/env python3
"""Cross-platform USB installer for the merged ESP32 factory image."""
import argparse
import subprocess
import sys
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", required=True, help="COM3 (Windows), /dev/cu.usbserial-… (macOS)")
parser.add_argument("--firmware", type=Path, default=Path("firmware.bin"))
args = parser.parse_args()
if not args.firmware.is_file():
    parser.error("Download firmware.bin from the Bridge build first")
subprocess.run([sys.executable, "-m", "esptool", "--chip", "esp32", "--port", args.port,
                "--baud", "460800", "write_flash", "0x0", str(args.firmware)], check=True)
