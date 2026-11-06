#!/usr/bin/env python3
"""Validate versioning and HACS release metadata without third-party modules."""
from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "custom_components" / "aipai_light" / "manifest.json"
CHANGELOG = ROOT / "CHANGELOG.md"
VERSION_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")


def fail(message: str) -> None:
    print(f"release validation failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    try:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"cannot read manifest: {error}")
    version = manifest.get("version")
    if not isinstance(version, str) or not VERSION_RE.fullmatch(version):
        fail("manifest version must use MAJOR.MINOR.PATCH")
    if manifest.get("domain") != "aipai_light":
        fail("manifest domain must be aipai_light")
    changelog = CHANGELOG.read_text(encoding="utf-8")
    if not re.search(rf"^## \[{re.escape(version)}\] - \d{{4}}-\d{{2}}-\d{{2}}$", changelog, re.MULTILINE):
        fail(f"CHANGELOG.md needs a dated heading for {version}")
    required = ("domain", "name", "documentation", "issue_tracker", "version")
    missing = [key for key in required if not manifest.get(key)]
    if missing:
        fail(f"manifest is missing {', '.join(missing)}")
    ref_type = os.environ.get("GITHUB_REF_TYPE", "")
    tag = os.environ.get("GITHUB_REF_NAME", "")
    if ref_type == "tag" or tag.startswith("v"):
        expected_tag = f"v{version}"
        if tag != expected_tag:
            fail(f"tag {tag} does not match expected tag {expected_tag}")
    print(f"release metadata valid for {version}")


if __name__ == "__main__":
    main()
