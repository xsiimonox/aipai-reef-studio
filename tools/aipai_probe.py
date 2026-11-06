#!/usr/bin/env python3
"""Read-only diagnostic for an AIPAI A7/A8 light's local HTTP API.

This does not change light output, schedules, clock, or firmware.  It only
requests ``/?read=config`` and prints the decoded identity and channel state.

Usage:
    python3 tools/aipai_probe.py 192.168.1.42
"""
from __future__ import annotations

import argparse
import re
import sys
from urllib.error import HTTPError, URLError
from urllib.request import urlopen


def config_body(raw: str) -> str | None:
    """Extract the pipe-separated config state from a possibly wrapped reply."""
    match = re.search(r"(?:on|off)\|[^\"']+", raw)
    return match.group(0).strip() if match else None


def integer(value: str) -> int:
    try:
        return int(float(value))
    except ValueError:
        return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Read an AIPAI light without changing it")
    parser.add_argument("ip", help="IPv4 address or hostname of the light")
    parser.add_argument("--timeout", type=float, default=4, help="HTTP timeout in seconds")
    args = parser.parse_args()

    url = f"http://{args.ip}/?read=config"
    try:
        with urlopen(url, timeout=args.timeout) as response:  # nosec B310: user-supplied LAN IP
            raw = response.read().decode("utf-8", "replace")
    except (HTTPError, URLError, TimeoutError) as error:
        print(f"Not reachable via {url}: {error}", file=sys.stderr)
        return 2

    body = config_body(raw)
    if not body:
        print("Response was not a recognized AIPAI config payload.", file=sys.stderr)
        return 3

    fields = [item.strip() for item in body.split("|")]
    channels = 8 if len(fields) > 28 else 6
    offset = 2 * (channels - 6)
    serial_index = 21 + offset
    model_index = 24 + offset
    levels = [integer(fields[5 + index]) for index in range(channels)]

    print("AIPAI local API detected")
    print(f"Address:  {args.ip}")
    print(f"Power:    {fields[0]}")
    print(f"Mode:     {'schedule' if fields[1] == '1' else 'manual'}")
    print(f"Channels: {channels}")
    print(f"Levels:   {', '.join(f'{level}%' for level in levels)}")
    print(f"Serial:   {fields[serial_index] if len(fields) > serial_index else 'unknown'}")
    print(f"Model:    {fields[model_index] if len(fields) > model_index else 'unknown'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
