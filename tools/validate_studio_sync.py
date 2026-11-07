"""Keep the two packages on one Studio UI; validate independently versioned firmware."""
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
panel = ROOT / "custom_components/aipai_light/panel/reef-studio.js"
manifest = json.loads((ROOT / "custom_components/aipai_light/manifest.json").read_text())
bridge = ROOT / "reef-bridge"
update = json.loads((bridge / "installer/bridge-update.json").read_text())
installer = json.loads((bridge / "installer/manifest.json").read_text())
studio_version = re.search(r'REEF_STUDIO_VERSION = "([^"]+)"', panel.read_text()).group(1)
assert studio_version == manifest["version"] == update["studio_version"], "Studio versions diverged"
flags = (bridge / "platformio.ini").read_text()
assert update["version"] == installer["version"], "Firmware and installer versions diverged"
assert update["version"] in flags, "PlatformIO firmware version diverged"
assert "/* SHARED_STUDIO */" in (bridge / "web/index.html").read_text(), "Bridge must embed shared panel"
assert "custom_components/aipai_light/panel/reef-studio.js" in (bridge / "tools/embed_web.py").read_text()
def previous(path):
    result = subprocess.run(["git", "show", f"HEAD^:{path}"], cwd=ROOT, capture_output=True, text=True)
    return result.stdout if result.returncode == 0 else None

old_manifest = previous("reef-bridge/installer/manifest.json")
if old_manifest:
    firmware_sources = ["custom_components/aipai_light/panel/reef-studio.js", "reef-bridge/src/main.cpp", "reef-bridge/web/bridge-adapter.js", "reef-bridge/web/index.html", "reef-bridge/tools/embed_web.py"]
    changed = any(previous(path) != (ROOT / path).read_text() for path in firmware_sources)
    if changed:
        assert json.loads(old_manifest)["version"] != update["version"], "Shared UI/firmware changed without a discoverable Bridge firmware version bump"
print(f"Shared Studio {studio_version}; Bridge firmware {update['version']}: valid")
