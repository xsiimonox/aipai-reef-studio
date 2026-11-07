"""Embed the standalone UI into firmware; no filesystem flash is required."""
from pathlib import Path

Import("env")
root = Path(env.subst("$PROJECT_DIR"))
page = (root / "web/index.html").read_bytes()
studio = (root.parent / "custom_components/aipai_light/panel/reef-studio.js").read_bytes()
adapter = (root / "web/bridge-adapter.js").read_bytes()
page = page.replace(b"/* SHARED_STUDIO */", studio).replace(b"/* BRIDGE_ADAPTER */", adapter)
target = root / "include/web_ui.h"
target.parent.mkdir(exist_ok=True)
target.write_text("#pragma once\n#include <Arduino.h>\nconst uint8_t WEB_UI[] PROGMEM = {" +
                  ",".join(map(str, page)) + "};\n", encoding="utf-8")
