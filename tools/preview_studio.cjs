// Exercise the real panel with simulated HA state; never connect to a lamp.
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
(async () => {
  const browser = await chromium.launch({ headless: true });
  try {
    const page = await browser.newPage({ locale: 'de-DE' });
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.route('http://studio.test/**', route => route.fulfill({
      contentType: 'text/html; charset=utf-8', body: '<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><body style="margin:0"><script>' +
        fs.readFileSync(path.join(root, 'custom_components/aipai_light/panel/reef-studio.js'), 'utf8') + '</script></body>',
    }));
    await page.goto('http://studio.test/');
    await page.evaluate(() => {
      const points = [0, 480, 600, 1080, 1200].map((minute, i) => ({ minute, levels: i === 2 || i === 3 ? [25, 65, 3, 2, 80, 10, 5, 15] : Array(8).fill(0) }));
      const light = { serial: 'demo', name: 'Riffaquarium · A8SE II MAX Pro', labels: ['White', 'Blue', 'Red', 'Green', 'Royal Blue', 'Purple', 'UV', 'Warm White'], programs: { day: { name: 'Riff · Tageslicht', points } }, scenes: { blue: { name: 'Blaulicht', levels: [0, 40, 0, 0, 80, 15, 0, 0] } }, program: 'day', scene: null, preview: null, firmware_points: points, local: true, online: true, minute: 780, timezone: 'Europe/Berlin', temperature: 38.5, sent: [25, 65, 3, 2, 80, 10, 5, 15], activity: [] };
      const panel = document.createElement('aipai-reef-studio');
      const six = {...light,serial:'six',name:'A7 SIII',labels:light.labels.slice(0,6)};
      panel.hass = { callWS: async () => ({ lights: [light,six], groups: [{ ...light, serial: 'group:demo', group: true, group_id: 'demo', name: 'Hauptbecken', members: ['six','demo'] }] }) };
      document.body.append(panel);
    });
    const panel = page.locator('aipai-reef-studio');
    await panel.locator('#library').selectOption('day');
    await panel.locator('[data-point="2"]').click();
    const screenshotDir = process.env.STUDIO_SCREENSHOT_DIR || '/tmp/reef-studio-design';
    fs.mkdirSync(screenshotDir, { recursive: true });
    for (const width of [1440, 1024, 768, 390, 320]) {
      await page.setViewportSize({ width, height: 1000 });
      assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), `Page overflow at ${width}`);
      const overflows = await panel.evaluate(el => [...el.shadowRoot.querySelectorAll('button,input,select,.panel,.slider')].filter(n => n.getBoundingClientRect().right > innerWidth + 1 || n.getBoundingClientRect().left < -1).map(n => n.id || n.className));
      assert.deepEqual(overflows, [], `Controls overflow at ${width}`);
      if (width === 1440 || width === 390) await page.screenshot({ path: path.join(screenshotDir, `program-${width}.png`), fullPage: true });
      await panel.locator('details').filter({ has: page.locator('#ramp') }).evaluate(el => { el.open = true; });
      assert(await panel.locator('#ramp').isVisible());
      assert(await panel.locator('#rise').evaluate(el => el.getBoundingClientRect().right <= innerWidth), `Ramp overflow at ${width}`);
      await panel.locator('details').filter({ has: page.locator('#ramp') }).evaluate(el => { el.open = false; });
    }
    await page.setViewportSize({ width: 1440, height: 1000 });
    await panel.locator('[data-tab=scenes]').click();
    await panel.locator('#library').selectOption('blue');
    await page.screenshot({ path: path.join(screenshotDir, 'scenes.png'), fullPage: true });
    await panel.locator('#copy').click();
    assert(await panel.locator('#save').isVisible());
    await panel.locator('#lamp').selectOption('group:demo');
    await panel.locator('#new').click();
    assert.equal(await panel.locator('[data-level][type=range]').count(), 8);
    await panel.locator('[data-tab=groups]').click();
    await panel.locator('#new-group').click();
    assert.equal(await panel.locator('.group-member').count(), 2);
    assert(await panel.locator('.group-member').first().isVisible());
    await page.screenshot({ path: path.join(screenshotDir, 'groups.png'), fullPage: true });
    await page.setViewportSize({ width: 320, height: 900 });
    assert(await panel.locator('#save-group').evaluate(el => el.getBoundingClientRect().right <= innerWidth));
    assert.deepEqual(errors, []);
    console.log('PASS: 320–1440px without overflow, program and scene editors, group creation, no JS errors. Screenshots: ' + screenshotDir);
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exit(1); });
