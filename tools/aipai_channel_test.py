#!/usr/bin/env python3
"""Simulate and safely exercise AIPAI local-channel control.

The live command uses the same local HTTP command as the Home Assistant
integration.  It changes a channel *temporarily* and never sends ``save=``;
the device's persisted schedule is not overwritten.  A deliberate --confirm
flag is required for every write.

Examples:
  python3 tools/aipai_channel_test.py --simulate
  python3 tools/aipai_channel_test.py 192.168.178.141 --read
  python3 tools/aipai_channel_test.py 192.168.178.141 --set royal_blue 10 --confirm
"""
from __future__ import annotations

import argparse
import re
import sys
from urllib.error import HTTPError, URLError
from urllib.request import urlopen


CHANNELS = {
    "white": ("w", "White"),
    "blue": ("b", "Blue"),
    "red": ("r", "Red"),
    "green": ("g", "Green"),
    "royal_blue": ("b2", "Royal Blue"),
    "purple": ("p", "Purple"),
    "uv": ("uv", "UV"),
    "warm_white": ("wm", "Warm White"),
}


def extract_config(raw: str) -> str | None:
    match = re.search(r"(?:on|off)\|[^\"']+", raw)
    return match.group(0).strip() if match else None


def read(ip: str, timeout: float) -> str:
    with urlopen(f"http://{ip}/?read=config", timeout=timeout) as response:  # nosec B310: LAN target
        raw = response.read().decode("utf-8", "replace")
    body = extract_config(raw)
    if not body:
        raise RuntimeError("not an AIPAI config response")
    return body


def levels(body: str, count: int) -> list[int]:
    fields = body.split("|")
    return [int(float(fields[5 + index])) for index in range(count)]


def print_state(body: str, count: int) -> None:
    values = levels(body, count)
    names = list(CHANNELS.values())[:count]
    print("Current temporary channel levels (device-reported):")
    for (_, label), value in zip(names, values, strict=True):
        print(f"  {label:12} {value:3}%")


def write(ip: str, code: str, percent: int, timeout: float) -> None:
    # The device uses 10-bit PWM for temporary channel writes.
    value = round(percent * 1023 / 100)
    with urlopen(f"http://{ip}/?{code}={value}", timeout=timeout) as response:  # nosec B310: LAN target
        response.read()


def main() -> int:
    parser = argparse.ArgumentParser(description="AIPAI local channel simulator / live tester")
    parser.add_argument("ip", nargs="?", help="light IP; omitted with --simulate")
    parser.add_argument("--read", action="store_true", help="read only; never changes the light")
    parser.add_argument("--set", nargs=2, metavar=("CHANNEL", "PERCENT"), help="temporary channel value")
    parser.add_argument("--confirm", action="store_true", help="required with --set")
    parser.add_argument("--channels", type=int, choices=(6, 8), default=6)
    parser.add_argument("--simulate", action="store_true", help="test command mapping without network access")
    parser.add_argument("--timeout", type=float, default=4)
    args = parser.parse_args()

    if args.simulate:
        if args.set:
            key, raw_percent = args.set
            if key not in CHANNELS:
                parser.error(f"unknown channel: {key}")
            percent = int(raw_percent)
            print(f"SIMULATED: GET /?{CHANNELS[key][0]}={round(percent * 1023 / 100)}")
        else:
            print("SIMULATED: GET /?read=config (read-only)")
        return 0
    if not args.ip:
        parser.error("IP is required unless --simulate is used")

    try:
        before = read(args.ip, args.timeout)
        print_state(before, args.channels)
        if args.read:
            return 0
        if not args.set:
            parser.error("select --read or --set CHANNEL PERCENT")
        if not args.confirm:
            parser.error("--set requires --confirm because it changes a live LED value")
        key, raw_percent = args.set
        if key not in CHANNELS or list(CHANNELS).index(key) >= args.channels:
            parser.error(f"channel must be one of: {', '.join(list(CHANNELS)[:args.channels])}")
        percent = int(raw_percent)
        if not 0 <= percent <= 100:
            parser.error("PERCENT must be from 0 to 100")
        write(args.ip, CHANNELS[key][0], percent, args.timeout)
        print(f"Temporarily set {CHANNELS[key][1]} to {percent}%. Nothing was persisted.")
        return 0
    except (HTTPError, URLError, TimeoutError, RuntimeError, ValueError) as error:
        print(f"AIPAI test failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
