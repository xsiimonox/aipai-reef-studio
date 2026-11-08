# AiPai Studio / Reef Bridge maintenance

- Maintain the Home Assistant panel and ESP32 Reef Bridge together. The canonical
  editor and styles are `custom_components/aipai_light/panel/reef-studio.js`;
  `reef-bridge/tools/embed_web.py` embeds that exact source. Do not fork the UI.
- Implement user-facing features for both transports. Bridge-only hardware setup
  belongs in `reef-bridge/web/bridge-adapter.js`. Document hardware limits openly.
- Bump HA manifest, panel version/cache and CHANGELOG for panel changes. Update
  `studio_version` in `reef-bridge/installer/bridge-update.json`; bump Bridge
  firmware in platformio.ini, installer manifest and bridge-update.json so already
  installed Bridges can discover changes. Update the Bridge changelog too.
- Run `python3 tools/validate_release.py`, `python3 tools/validate_studio_sync.py`,
  JS syntax checks, both browser previews and `pio run -d reef-bridge` when the
  shared panel or Bridge changes. Use mocks, not real aquarium writes for tests.
- Publish both packages through their existing GitHub Actions workflows. Shared
  source changes must trigger the Bridge build and deployment too.
