const COLORS = ["#dbefff", "#62bcff", "#ff818a", "#63ddb5", "#838dff", "#ce94ff", "#a983ef", "#ffd18b"];
const REEF_STUDIO_VERSION = "0.22.0";
const escape = value => String(value ?? "").replace(/[&<>"']/g, char =>
  ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[char]);
const clock = minute => [Math.floor(minute / 60), minute % 60].map(n => String(n).padStart(2, "0")).join(":");
const toMinute = time => {
  const [hour, minute] = time.split(":").map(Number);
  return time ? hour * 60 + minute : NaN;
};
const sorted = points => [...points].sort((a, b) => a.minute - b.minute);

function interpolate(points, minute) {
  const right = points.findIndex(point => point.minute > minute);
  const end = points[right < 0 ? 0 : right];
  const start = points[(right < 0 ? points.length : right) - 1] ?? points.at(-1);
  const span = (end.minute - start.minute + 1440) % 1440;
  const elapsed = (minute - start.minute + 1440) % 1440;
  let fraction = span ? elapsed / span : 0;
  return start.levels.map((level, i) => Math.round(level + (end.levels[i] - level) * fraction));
}

const STYLE = `
  :host{display:block;color:#edf5ff;font:400 15px/1.45 system-ui,sans-serif}
  *{box-sizing:border-box} h1,h2,p{margin:0} h1{font-size:clamp(2rem,6vw,3.6rem);letter-spacing:-.07em;line-height:1}
  h2{font-size:1.12rem;letter-spacing:-.02em} small,.muted{color:#a5b7cb} small{font-size:.82rem}
  main{min-height:100vh;padding:24px clamp(12px,4vw,50px) 42px;background:radial-gradient(ellipse at 85% 0%,#193558 0,transparent 46%),#081322}
  .shell{max-width:1200px;margin:auto} .head,.row,.actions,.tabs,.legend{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
  .head{justify-content:space-between;margin:4px 0 22px}.head p{margin-top:9px}
  .eyebrow{color:#76e8e0;text-transform:uppercase;letter-spacing:.18em;font-size:.72rem;font-weight:800;margin-bottom:9px}
  .grid{display:grid;grid-template-columns:minmax(0,2fr) minmax(250px,1fr);gap:14px;margin-top:14px}
  .panel{min-width:0;background:#101f33;border:1px solid #29405b;border-radius:20px;padding:clamp(14px,2.4vw,23px)}
  .panel h2{margin-bottom:8px}.row.spread{justify-content:space-between}.actions{margin-top:14px}
  .tabs{margin-top:18px}.tabs button[aria-pressed=true],button.primary{background:#83eee2;color:#09262e;border-color:#83eee2;font-weight:750}
  button,input,select{font:inherit;color:inherit;background:#152a41;border:1px solid #3a5574;border-radius:10px;padding:8px 11px}
  button{cursor:pointer;min-height:40px}button:disabled{opacity:.5;cursor:default}
  :focus-visible{outline:2px solid #a5fff2;outline-offset:2px}
  select{max-width:100%} .pill{border-radius:30px;padding:4px 10px;background:#203955;color:#b9e3ff;font-size:.82rem}
  .notice{color:#ffd2a0;margin:8px 0;min-height:1.35em}.notice:empty{margin:0;min-height:0}
  .update-note{margin:0 0 15px;padding:11px 14px;border:1px solid #76e8e0;border-radius:12px;background:#123d47;color:#dcfffb}
  .mode-note{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:0 0 15px;padding:12px 15px;border:1px solid #3e6682;border-radius:14px;background:#102c43}.mode-note span{color:#a5b7cb;font-size:.8rem;text-transform:uppercase;letter-spacing:.08em}.mode-note strong{color:#a5fff2;text-align:right}
  .log{max-height:480px;overflow:auto;margin:14px 0 0;padding:0;list-style:none;border:1px solid #29405b;border-radius:12px}.log li{padding:10px 12px;border-bottom:1px solid #29405b}.log li:last-child{border:0}.log time{display:block;color:#8eabc4;font-size:.77rem}.log .warning{color:#ffd580}.log .error{color:#ffabb2}
  .legend{font-size:.78rem;margin-top:5px}.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--c);margin-right:5px}
  svg{width:100%;display:block;overflow:visible}.timeline{margin-top:6px}
  .scrub{display:flex;align-items:center;gap:10px;margin-top:4px}.scrub input{width:100%}
  .bars{display:grid;gap:8px;margin-top:12px}.bar{display:grid;grid-template-columns:minmax(75px,1fr) 2fr 43px;align-items:center;gap:9px;font-size:.83rem}
  .track{height:7px;background:#22354c;border-radius:8px;overflow:hidden}.fill{height:100%;background:var(--c);width:var(--pct);border-radius:8px}
  .bar strong{text-align:right;font-variant-numeric:tabular-nums}
  .editor{margin-top:14px}.editor h2{margin-bottom:10px}.edit-grid{display:grid;grid-template-columns:1fr 1fr;gap:18px;margin-top:15px}
  .field{display:grid;gap:4px;color:#b9cadb;font-size:.82rem}.field input{min-width:0}
  .form-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(125px,1fr));gap:9px;margin:10px 0}
  .point-list{display:flex;flex-wrap:wrap;gap:6px;margin:10px 0}.point-list button[aria-pressed=true]{border-color:#83eee2;color:#83eee2}
  .sliders{display:grid;gap:8px}.slider{display:grid;grid-template-columns:90px minmax(45px,1fr) 65px;align-items:center;gap:8px;font-size:.83rem}
  .slider input[type=range],.scrub input{accent-color:var(--c,#83eee2);padding:0;min-width:0}.slider input[type=number]{width:65px}
  .danger{color:#ffbdc1}.subtle{background:transparent}.empty{padding:16px 0;color:#a5b7cb}
  /* Quiet surfaces, consistent controls and a compact segmented navigation. */
  :host{color-scheme:dark;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;font-size:15px;line-height:1.5;color:#f1f4f8}
  main{padding:32px clamp(16px,3.5vw,48px) 64px;background:radial-gradient(ellipse at 85% 0%,#182b3e 0,transparent 48%),#0c111b}
  .shell{max-width:1280px}.head{gap:24px;margin:0 0 28px;align-items:center}.head>div{min-width:0}
  h1{font-size:clamp(2.2rem,4vw,3rem);font-weight:700;letter-spacing:-.055em;line-height:1.12}
  h2{font-size:1.05rem;font-weight:650;letter-spacing:-.015em}.eyebrow{font-size:.67rem;letter-spacing:.15em;color:#99b2c9;margin:0}
  .head p{margin-top:8px}.head .row{margin-bottom:12px}.muted,small{color:#aebdce}small{font-size:.8rem;line-height:1.5}
  .grid{grid-template-columns:minmax(0,1.65fr) minmax(320px,1fr);gap:20px;margin-top:20px;align-items:stretch}
  .panel{background:#151e2b;border:1px solid #2b3748;border-radius:22px;padding:24px;box-shadow:0 8px 28px #00000012}
  .panel h2{margin-bottom:0}.row.spread{gap:16px;align-items:center}.actions{gap:12px;align-items:flex-end;margin-top:20px}
  button,input,select{min-width:0;max-width:100%;min-height:44px;padding:10px 14px;border-radius:12px;background:#202c3d;border-color:#3d4e64;font-size:.9rem;line-height:1.35}
  button{font-weight:550;transition:background .16s,border-color .16s;white-space:normal;overflow-wrap:anywhere}
  button:hover:not(:disabled){background:#2b3b50;border-color:#7d9cba}button.primary{background:#a0e7df;border-color:#a0e7df;color:#0c302e}
  button.primary:hover:not(:disabled){background:#b4f2eb;border-color:#b4f2eb}.danger{color:#ffc1c8;background:#352631;border-color:#684451}.subtle{background:transparent}
  input:not([type=range]):not([type=checkbox]),select{width:100%}input[type=number]{font-variant-numeric:tabular-nums}
  input[type=checkbox]{min-height:20px;width:20px;height:20px;margin:0;accent-color:#a0e7df;flex:none}
  input[type=range]{min-height:44px;cursor:pointer}input:focus-visible,select:focus-visible,button:focus-visible,summary:focus-visible{outline:2px solid #a0e7df;outline-offset:3px}
  .field{gap:7px;font-size:.8rem;color:#b8c8d9;min-width:0;flex:1 1 160px}.field input,.field select{display:block}
  .device-field{flex:0 1 280px;max-width:360px}.pill{padding:5px 10px;background:#263447;color:#cad9e9;font-size:.75rem;white-space:nowrap}
  .mode-note{padding:16px 20px;background:#172b33;border-color:#33505a;border-radius:16px;gap:20px}.mode-note span{font-size:.68rem;letter-spacing:.1em;flex:none}.mode-note strong{font-size:.92rem;color:#afece5;overflow-wrap:anywhere}
  .tabs{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:4px;padding:5px;margin-top:28px;background:#171f2c;border:1px solid #2b3748;border-radius:16px}
  .tabs button{background:transparent;border-color:transparent;color:#b5c3d4;font-size:.86rem;padding:10px 8px}
  .tabs button[aria-pressed=true]{background:#334355;color:#f3f7fb;border-color:#4b6077;box-shadow:0 2px 7px #0002}
  .editor{margin-top:16px}.editor>.muted{margin-top:8px;max-width:78ch}.edit-grid{grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:24px;margin-top:24px;padding-top:24px;border-top:1px solid #2b3748}
  .edit-grid>div{min-width:0}.edit-grid h2{margin-bottom:16px}.form-grid{grid-template-columns:repeat(2,minmax(0,1fr));gap:16px;margin:16px 0}
  details{padding:16px;background:#111a27;border:1px solid #304055;border-radius:14px}summary{cursor:pointer;font-weight:600;color:#dce8f4}details small{display:block;margin-top:10px}
  .point-list{gap:8px;margin:20px 0}.point-list button{font-variant-numeric:tabular-nums}.point-list button[aria-pressed=true]{color:#b7f0e9;background:#234344;border-color:#6ba49e}
  .sliders{gap:0;margin-top:16px}.slider{grid-template-columns:112px minmax(40px,1fr) 72px;gap:12px;min-height:62px;border-bottom:1px solid #2b3748;font-size:.82rem}.slider:last-child{border-bottom:0}.slider>span{overflow-wrap:anywhere}
  .slider input[type=number]{width:72px;padding:10px 7px;text-align:center}.slider input[type=range]{width:100%}.slider:has(#master-level){padding:10px 12px;background:#1e303c;border:1px solid #395463;border-radius:14px;margin-bottom:8px;grid-template-columns:112px minmax(40px,1fr) 72px}
  .edit-grid small{display:block;margin-top:8px}.save-actions{padding-top:20px;border-top:1px solid #2b3748}.save-actions small{flex-basis:100%;max-width:80ch}
  .library-actions select{flex:2 1 240px;width:auto}.library-actions .field{flex:0 1 150px}.library-actions button{flex:0 1 auto}
  .scrub{display:grid;grid-template-columns:minmax(0,1fr) 52px auto;gap:12px;margin-top:14px}.scrub output{font-variant-numeric:tabular-nums;text-align:center;color:#e1edf9;font-size:.87rem}
  .preview-actions{display:grid;grid-template-columns:minmax(100px,150px) minmax(0,1fr);align-items:end;gap:12px}.preview-actions .field{font-size:.78rem}.preview-actions button{width:100%}
  .legend{gap:8px 16px;margin-top:20px;font-size:.73rem}.legend>span{white-space:nowrap}.timeline{margin-top:20px}
  .bars{gap:12px;margin-top:18px}.bar{grid-template-columns:minmax(80px,1fr) minmax(50px,1.5fr) 46px;gap:12px;font-size:.8rem}.track{height:6px;background:#2b3a4c}.fill{height:6px}
  .comparison{margin-top:20px;padding:12px}.comparison summary{font-size:.8rem}.save-actions small{width:100%}
  #live>small{display:block;margin-top:12px}#control{display:grid;gap:10px}#control button{text-align:left;font-size:.82rem}.group-member{padding:14px;border:1px solid #33465a;border-radius:12px;min-height:52px;background:#1c2939}.group-member:has(:checked){border-color:#75aaa5;background:#203a3d}.group-members{gap:10px;margin:0}
  .notice:not(:empty){padding:12px 16px;background:#332b21;border:1px solid #665034;border-radius:12px;margin-bottom:16px}.empty{padding:32px}
  @media(max-width:1000px){.grid{grid-template-columns:minmax(0,1fr)}.grid>section{height:auto}.bar{grid-template-columns:105px minmax(50px,1fr) 46px}}
  @media(max-width:750px){main{padding:20px 16px 40px}.head{align-items:flex-start;gap:20px}.head .device-field{flex-basis:100%;max-width:none}.panel{padding:20px;border-radius:18px}.edit-grid{grid-template-columns:minmax(0,1fr);gap:28px}.mode-note{align-items:flex-start;flex-direction:column;gap:6px}.mode-note strong{text-align:left}.tabs{margin-top:20px}.tabs button{font-size:.76rem;padding:10px 3px}.actions>button{flex:1 1 160px}.actions>small{flex-basis:100%}.library-actions select{flex-basis:100%;width:100%}.library-actions .field{flex:1 1 120px}.slider{grid-template-columns:96px minmax(36px,1fr) 64px;gap:10px}.slider input[type=number]{width:64px}.slider:has(#master-level){grid-template-columns:96px minmax(30px,1fr) 64px;padding:8px}.preview-actions{grid-template-columns:120px minmax(0,1fr)}}
  @media(prefers-reduced-motion:reduce){button{transition:none}}
  @media(max-width:420px){.tabs{grid-template-columns:repeat(2,minmax(0,1fr))}.tabs button{font-size:.84rem}.preview-actions{grid-template-columns:100px minmax(0,1fr)}}
`;

class ReefStudioPanel extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: "open" });
    this.lights = [];
    this.kind = "programs";
    this.duration = 15;
    this.preview = null;
  }

  set hass(value) {
    this._hass = value;
    const menu = this.$.querySelector("ha-menu-button");
    if (menu) menu.hass = value;
    if (this.isConnected && !this.timer) this.start();
  }

  connectedCallback() {
    this.render();
    if (this._hass) this.start();
  }

  disconnectedCallback() {
    clearInterval(this.timer);
    clearInterval(this.clockTimer);
    clearTimeout(this.previewTimer);
    this.timer = null;
  }

  start() {
    this.timer = setInterval(() => this.refresh(), 8000);
    this.clockTimer = setInterval(() => this.update(), 1000);
    this.refresh();
  }

  get light() { return this.lights.find(light => light.serial === this.serial); }
  get $() { return this.shadowRoot; }

  activeMode(light) {
    if (light.startup_hold) return "Sicherer Start · Lampenzustand unverändert";
    if (light.preview) return "Live-Vorschau";
    if (light.bridge_mode === "lease") return "Home Assistant · zeitlich bestätigte Steuerung";
    if (light.bridge_mode === "manual" && !light.scene) return "Manueller Modus · dauerhaft";
    if (light.scene) {
      const scene = light.scenes[light.scene.id];
      return light.scene.manual ? `Manueller Modus · ${scene?.name ?? "Szene"}` :
        `Szene aktiv · ${scene?.name ?? "Unbekannte Szene"}`;
    }
    if (light.program) {
      const name = light.programs[light.program]?.name ?? "Unbekannt";
      return light.program_execution === "device" ? `Auf Lampe gespeichert · ${name}` : `Tagesprogramm aktiv · ${name}`;
    }
    if (light.restoring) return "Vorherigen Lampenzustand wiederherstellen";
    return "Lampeneigener Modus";
  }

  queueLivePreview(levels) {
    const light = this.light;
    if (!light || light.group || !light.local || !light.online) return;
    // A shared eight-channel MAX profile may be edited from a six-channel
    // fixture. Preview only the channels physically available on that lamp.
    this.previewLevels = levels.slice(0, light.labels.length);
    this.previewSeconds = this.kind === "programs" ? 60 : 300;
    clearTimeout(this.previewTimer);
    this.previewTimer = setTimeout(() => this.sendLivePreview(), 180);
  }

  async sendLivePreview() {
    if (this.previewRequest) {
      this.previewQueued = true;
      return;
    }
    const levels = this.previewLevels;
    if (!levels) return;
    this.previewRequest = true;
    try {
      const data = await this._hass.callWS({
        type: "aipai_light/studio", action: "preview", serial: this.serial, levels,
        preview_seconds: this.previewSeconds,
      });
      if (this.isConnected) this.receive(data);
    } catch (error) {
      this.status(error.message || "Live-Vorschau konnte nicht gesendet werden.");
    } finally {
      this.previewRequest = false;
      if (this.previewQueued || this.previewLevels !== levels) {
        this.previewQueued = false;
        this.sendLivePreview();
      }
    }
  }

  async refresh() {
    if (this.busy || this.reading) return;
    this.reading = true;
    try {
      const lights = await this._hass.callWS({ type: "aipai_light/studio", action: "list" });
      if (!this.busy && this.isConnected) this.receive(lights);
    } catch (error) {
      this.status(error.message || `${this._hass.productName || "Home Assistant"} antwortet gerade nicht.`);
    } finally {
      this.reading = false;
    }
  }

  receive(lights) {
    const payload = Array.isArray(lights) ? { lights, groups: [] } : lights;
    const signature = list => JSON.stringify(list.map(
      light => [
        light.serial, light.name, light.labels, light.programs, light.scenes, light.local,
        light.program, light.program_execution, light.startup_hold, light.scene, light.preview, light.activity,
      ]
    ));
    const changed = signature([...payload.lights, ...payload.groups]) !== signature(this.lights) ||
      JSON.stringify(payload.groups) !== JSON.stringify(this.groups) ||
      JSON.stringify(payload.update) !== JSON.stringify(this.updateInfo);
    this.lights = [...payload.lights, ...payload.groups];
    this.clockOffset = ((payload.lights[0]?.now_epoch ?? Date.now() / 1000) * 1000) - Date.now();
    this.groups = payload.groups;
    this.updateInfo = payload.update;
    if (!this.light) {
      // The WebSocket command returns an object (`{lights, groups}`), rather
      // than the legacy bare array.  Select the first actual lamp from that
      // payload; indexing `lights` here left `serial` undefined and made the
      // empty-state appear even though a lamp had been configured.
      this.serial = payload.lights[0]?.serial ?? payload.groups[0]?.serial ?? null;
      this.reset();
    }
    if (changed) this.render();
    else this.update();
  }

  reset() {
    // Do not use `this.id`: HTMLElement already owns that DOM property and
    // coerces null to the literal string "null".
    this.itemId = null;
    this.draft = null;
    this.point = 0;
    this.preview = null;
    this.masterItem = null;
    this.masterBase = null;
    this.masterValue = 100;
  }

  status(message) {
    const node = this.$.querySelector("#notice");
    if (node) node.textContent = message;
  }

  async send(action, extra = {}) {
    if (this.busy) return;
    this.busy = true;
    this.update();
    this.status("Wird verarbeitet …");
    try {
      const lights = await this._hass.callWS({
        type: "aipai_light/studio", serial: this.serial, kind: this.kind, action, ...extra,
      });
      if (action === "save" || action === "delete") this.reset();
      this.receive(lights);
      this.status(action === "save" ? "In der Bibliothek gespeichert." :
        action === "activate" && this.kind === "programs" ? "Tagesprogramm dauerhaft auf der Lampe gespeichert." : "");
    } catch (error) {
      this.status(error.message || "Der Befehl konnte nicht ausgeführt werden.");
    } finally {
      this.busy = false;
      this.update();
    }
  }

  async group(action, extra = {}) {
    if (this.busy) return;
    this.busy = true;
    this.status("Wird verarbeitet …");
    try {
      const data = await this._hass.callWS({ type: "aipai_light/studio", action, ...extra });
      this.receive(data);
      this.status("");
    } catch (error) { this.status(error.message || "Die Gruppe konnte nicht gespeichert werden."); }
    finally { this.busy = false; this.update(); }
  }

  render() {
    const light = this.light;
    const mode = light ? this.activeMode(light) : "";
    this.$.innerHTML = `<style>${STYLE}</style><main><div class="shell">
      <header class="head"><div><div class="row"><ha-menu-button></ha-menu-button><div class="eyebrow">AIPAI / Aquarienbeleuchtung</div></div>
      <h1>Reef Studio</h1><p class="muted">Tageslicht planen. Spektrum abstimmen. <span class="pill">v${REEF_STUDIO_VERSION}</span></p></div>
        ${light ? `<label class="field device-field">Lampe oder Gruppe<select id="lamp">${this.lights.map(item =>
        `<option value="${escape(item.serial)}" ${item === light ? "selected" : ""}>${item.group ? "Gruppe · " : ""}${escape(item.name)}</option>`).join("")}</select></label>` : ""}
      </header><div id="notice" class="notice" role="status"></div>
      ${this.updateInfo?.available ? `<div class="update-note" role="status">Update verfügbar: <strong>v${escape(this.updateInfo.latest)}</strong> (installiert: v${escape(this.updateInfo.current)}). Bitte über HACS aktualisieren.</div>` : ""}
      ${light ? `<section class="mode-note" aria-live="polite"><span>Aktiver Lichtmodus</span><strong id="active-mode">${escape(mode)}</strong></section>` : ""}
      ${light ? `<div class="grid"><section class="panel">
        <div class="row spread"><h2 id="curve-title">Tagesverlauf</h2><span id="now" class="pill"></span></div>
        <div id="chart" class="timeline"></div>
        <div class="scrub"><input id="preview" type="range" min="0" max="1439" aria-label="Uhrzeit der Vorschau">
          <output id="preview-time"></output><button id="now-button" class="subtle">Jetzt</button></div>
        <div class="actions preview-actions"><label class="field">Vorschau · Minuten<input id="curve-preview-duration" type="number" min="1" max="60" value="${this.curvePreviewMinutes ?? 5}"></label>
          <button id="curve-preview" data-curve-preview>Uhrzeit am Licht ansehen</button></div>
        <p class="muted" style="font-size:.78rem;margin-top:10px">Uhrzeit im Verlauf wählen und für 1–60 Minuten am Licht ansehen. Anschließend endet die Vorschau automatisch.</p>
        <div class="legend">${light.labels.map((label, index) =>
          `<span><i class="dot" style="--c:${COLORS[index]}"></i>${escape(label)}</span>`).join("")}</div>
      </section><section class="panel"><div class="row spread"><h2>Kanalpegel</h2><span id="online" class="pill"></span></div>
        <div id="live"></div><div id="control" class="actions"></div>
      </section></div>
      <div class="tabs"><button data-tab="programs" aria-pressed="${this.kind === "programs"}">Tagesprogramme</button>
      <button data-tab="scenes" aria-pressed="${this.kind === "scenes"}">Szenen</button>
      <button data-tab="groups" aria-pressed="${this.kind === "groups"}">Gruppen</button>
      <button data-tab="diagnostics" aria-pressed="${this.kind === "diagnostics"}">Diagnose</button></div>
      <section class="panel editor" id="editor"></section>` :
      '<section class="panel empty">Sobald eine AIPAI-Lampe eingerichtet ist, erscheint sie hier.</section>'}
      </div></main>`;
    this.$.querySelector("ha-menu-button").hass = this._hass;
    if (!light) return;
    this.$.querySelector("#lamp").onchange = event => {
      this.serial = event.target.value;
      this.reset();
      this.render();
    };
    this.$.querySelectorAll("[data-tab]").forEach(button => {
      button.onclick = () => { this.kind = button.dataset.tab; this.reset(); this.render(); };
    });
    this.$.querySelector("#preview").oninput = event => {
      this.preview = Number(event.target.value);
      this.update();
    };
    this.$.querySelector("#now-button").onclick = () => { this.preview = null; this.update(); };
    const curvePreview = this.$.querySelector("#curve-preview");
    curvePreview.onclick = () => {
      const duration = this.$.querySelector("#curve-preview-duration");
      if (!duration.reportValidity()) return;
      this.curvePreviewMinutes = Number(duration.value);
      const activeProgram = this.light.programs[this.light.program];
      const sourceProgram = this.kind === "programs" && this.draft?.points ? this.draft : activeProgram;
      const source = sourceProgram?.points ?? this.light.firmware_points;
      if (source.length < 2) {
        this.status("Für diese Uhrzeit ist noch kein Tagesverlauf vorhanden.");
        return;
      }
      this.send("preview", {
        levels: interpolate(sorted(source), this.preview ?? this.light.minute),
        preview_seconds: this.curvePreviewMinutes * 60,
      });
    };
    this.editor();
    this.update();
  }

  bars(values) {
    return `<div class="bars">${this.light.labels.map((label, i) => {
      const level = values?.[i];
      return `<div class="bar" style="--c:${COLORS[i]};--pct:${level ?? 0}%"><span>${escape(label)}</span>
        <div class="track"><div class="fill"></div></div><strong>${level == null ? "–" : level + "%"}</strong></div>`;
    }).join("")}</div>`;
  }

  secondsUntil(timestamp, light) {
    if (!timestamp) return null;
    const offset = this.clockOffset ?? 0;
    return Math.max(0, Math.ceil(timestamp - (Date.now() + offset) / 1000));
  }

  durationText(seconds) {
    if (seconds == null) return "";
    return `${String(Math.floor(seconds / 60)).padStart(2, "0")}:${String(seconds % 60).padStart(2, "0")}`;
  }

  update() {
    const light = this.light;
    if (!light || !this.$.querySelector("#chart")) return;
    this.$.querySelector("#active-mode").textContent = this.activeMode(light);
    const program = light.programs[light.program];
    const scene = light.scenes[light.scene?.id];
    const sceneRemaining = this.secondsUntil(light.scene?.until, light);
    const previewRemaining = this.secondsUntil(light.preview?.until, light);
    const draft = this.kind === "programs" && this.draft;
    const points = sorted(draft?.points ?? program?.points ?? light.firmware_points);
    const preview = this.preview ?? light.minute;
    this.$.querySelector("#now").textContent = `${clock(light.minute)} · ${light.timezone}`;
    this.$.querySelector("#curve-title").textContent = draft ? "Programm-Vorschau" : program ? program.name : "Kurve in der Lampe";
    this.$.querySelector("#preview").value = preview;
    this.$.querySelector("#preview-time").textContent = clock(preview);
    this.draw(points, light.minute, preview);
    const temperature = light.temperature;
    const temperatureText = typeof temperature === "number" && Number.isFinite(temperature) ?
      ` · ${temperature.toFixed(1)} °C` : "";
    this.$.querySelector("#online").textContent = light.online ? `Erreichbar${temperatureText}` : "Offline";
    const actual = light.online ? (light.sent ?? light.reported) : null;
    const label = light.sent ? "Zuletzt an die Lampe gesendet" :
      light.program_execution === "device" ? "Aktuelle Pegel aus dem Lampenprogramm" :
      light.mode === "0" ? "Manuelle Pegel der Lampe" : "Von der Lampe gemeldet";
    const comparisonOpen = this.$.querySelector(".comparison")?.open;
    this.$.querySelector("#live").innerHTML = `
      <small>${escape(light.startup_hold ? "Sicherer Startmodus · es wurden nach dem Neustart keine Lichtwerte gesendet" : light.preview ? "Live-Vorschau · noch " + this.durationText(previewRemaining) + " · danach Tagesprofil" : scene ? (light.scene?.manual ? "Manueller Modus · " + scene.name + " · dauerhaft" : "Szene · " + scene.name + " · noch " + this.durationText(sceneRemaining)) : program ? (light.program_execution === "device" ? "Läuft ausfallsicher in der Lampe · " : "Aktiv · ") + program.name :
        light.restoring ? "Warte auf Wiederherstellung" : "Lampeneigener Modus")}</small>
      <div class="notice">${escape(light.error)}</div><small>${label}</small>${this.bars(actual)}
      ${points.length > 1 ? `<details class="comparison" ${comparisonOpen ? "open" : ""}><summary>Geplante Kanalwerte · ${clock(preview)}</summary>
        ${this.bars(interpolate(points, preview))}</details>` : ""}`;
    const control = this.$.querySelector("#control");
    const buttons = [
      light.preview && ["preview_cancel", "Timer abbrechen – Tagesprofil fortsetzen"],
      scene && ["resume", "Szene abbrechen – Tagesprogramm fortsetzen"],
      !light.preview && (light.scene || light.restoring || (light.program && light.program_execution !== "device")) && ["release", "Lampeneigenen Modus wiederherstellen"],
    ].filter(Boolean);
    const signature = JSON.stringify(buttons);
    if (control.dataset.buttons !== signature) {
      control.dataset.buttons = signature;
      control.innerHTML = buttons.map(([action, name]) => `<button data-action="${action}">${name}</button>`).join("");
      control.querySelectorAll("button").forEach(button => { button.onclick = () => this.send(button.dataset.action); });
    }
    control.querySelectorAll("button").forEach(button => { button.disabled = this.busy || !light.local; });
    this.$.querySelectorAll("[data-live]").forEach(button => {
      button.disabled = this.busy || !light.local || !light.online;
    });
    const curvePreview = this.$.querySelector("[data-curve-preview]");
    if (curvePreview) curvePreview.disabled = this.busy || !light.local || !light.online;
  }

  draw(points, now, preview) {
    const x = minute => 42 + minute * 668 / 1440;
    const y = level => 170 - level * 1.48;
    // Sample the complete linear curve densely so the preview remains smooth.
    const samples = points.length > 1 ? Array.from({ length: 721 }, (_, index) => {
      const minute = index * 2;
      return { minute, levels: interpolate(points, minute % 1440) };
    }) : [];
    this.$.querySelector("#chart").innerHTML = `<svg viewBox="0 0 730 200" role="img" aria-label="Tagesverlauf der LED-Kanäle von 0 bis 24 Uhr">
      <defs><linearGradient id="night"><stop stop-color="#122843"/><stop offset=".55" stop-color="#213c5a"/><stop offset="1" stop-color="#122843"/></linearGradient></defs>
      <rect x="42" y="22" width="668" height="148" rx="9" fill="url(#night)"/>
      ${[0, 25, 50, 75, 100].map(level => `<path d="M42 ${y(level)}H710" stroke="#3a526b" opacity=".65"/>
        <text x="35" y="${y(level) + 4}" text-anchor="end" fill="#b1c6da" font-size="12">${level}%</text>`).join("")}
      ${[0, 4, 8, 12, 16, 20, 24].map(hour => `<text x="${x(hour * 60)}" y="191" text-anchor="middle" fill="#b1c6da" font-size="13">${hour}:00</text>`).join("")}
      ${this.light.labels.map((_, index) => `<polyline points="${samples.map(p => `${x(p.minute)},${y(p.levels[index] ?? 0)}`).join(" ")}"
        fill="none" stroke="${COLORS[index]}" stroke-width="2" vector-effect="non-scaling-stroke"/>`).join("")}
      <path d="M${x(now)} 22V170" stroke="#83eee2" stroke-width="2" stroke-dasharray="4 3"/>
      <circle cx="${x(now)}" cy="22" r="4" fill="#83eee2"/>
      ${now !== preview ? `<path d="M${x(preview)} 22V170" stroke="#edf5ff" opacity=".8"/>` : ""}
      </svg>`;
  }

  editor() {
    const light = this.light;
    if (this.kind === "diagnostics") return this.diagnosticsEditor();
    if (this.kind === "groups") return this.groupEditor();
    const programs = this.kind === "programs";
    const library = light[this.kind];
    const node = this.$.querySelector("#editor");
    node.innerHTML = `<div class="row spread"><h2>${programs ? "Programmbibliothek" : "Szenenbibliothek"}</h2>
      <button id="new" class="primary">${programs ? "+ Programm" : "+ Szene"}</button></div>
      <p class="muted">${programs ? "Gemeinsame Bibliothek für alle Lampen. Beim Aktivieren wird das Profil dauerhaft als 24-Stunden-Kurve in der Lampe gespeichert und läuft danach ohne Home Assistant weiter." :
        "Gemeinsame Looks für alle Lampen, etwa Blaulicht oder Foto. Zeitlich starten oder im manuellen Modus dauerhaft halten."}${light.group && light.labels.length > 6 ? " UV und Warm White wirken nur auf Lampen, die diese Kanäle unterstützen." : ""}</p>
      <div class="actions library-actions">${Object.keys(library).length ? `
        <select id="library" aria-label="Gespeicherter Eintrag"><option value="">Eintrag auswählen</option>
        ${Object.entries(library).map(([id, item]) => `<option value="${escape(id)}" ${id === this.itemId ? "selected" : ""}>${escape(item.name)}</option>`).join("")}</select>` : '<small>Noch keine Einträge.</small>'}
        ${this.itemId ? `<button id="activate" data-live class="primary">${programs ? "Auf Lampe speichern & aktivieren" : "Szene starten"}</button>
        ${programs ? "" : `<label class="field">Dauer · 1–60 Minuten<input id="duration" type="number" min="1" max="60" value="${this.duration}"></label>
        <button id="manual" data-live>Dauerhaft manuell</button>`}
        <button id="copy">Duplizieren</button><button id="delete" class="danger">Löschen</button>` : ""}
      </div>${!programs && Object.keys(library).length ? `<div class="actions"><small>Schnell wechseln:</small>
        ${Object.entries(library).map(([id, item]) => `<button data-start-scene="${escape(id)}" class="${light.scene?.id === id ? "primary" : ""}">${escape(item.name)}</button>`).join("")}
      </div>` : ""}<div id="draft"></div>`;
    const newButton = node.querySelector("#new");
    if (newButton) newButton.onclick = () => {
      this.reset();
      const zero = light.labels.map(() => 0);
      this.draft = programs ? { name: "", points: [0, 540, 600, 1140, 1200].map(minute => ({ minute, levels: [...zero] })) } :
        { name: "", levels: [...zero] };
      this.editor();
      this.update();
      this.$.querySelector("#name").focus();
    };
    const select = node.querySelector("#library");
    if (select) select.onchange = () => {
      this.reset();
      this.itemId = select.value || null;
      if (this.itemId) {
        this.draft = structuredClone(library[this.itemId]);
        // Keep channels that are unavailable on this fixture intact. A
        // six-channel lamp can therefore save a MAX profile without silently
        // discarding its UV and warm-white values. Missing channels are only
        // zero-filled when editing a smaller profile on a larger fixture.
        const fit = levels => [...levels, ...Array(Math.max(0, light.labels.length - levels.length)).fill(0)];
        if (programs) this.draft.points.forEach(point => { point.levels = fit(point.levels); });
        else this.draft.levels = fit(this.draft.levels);
      }
      this.editor();
      this.update();
    };
    node.querySelectorAll("[data-start-scene]").forEach(button => {
      button.onclick = () => this.send("activate", { item_id: button.dataset.startScene, duration: 15 });
    });
    if (this.itemId) {
      node.querySelector("#activate").onclick = () => {
        const duration = node.querySelector("#duration");
        if (duration && !duration.reportValidity()) return;
        this.send("activate", { item_id: this.itemId, ...(duration ? { duration: Number(duration.value) } : {}) });
      };
      const manual = node.querySelector("#manual");
      if (manual) manual.onclick = () => this.send("manual", { item_id: this.itemId });
      const duration = node.querySelector("#duration");
      if (duration) duration.oninput = () => { this.duration = Number(duration.value); };
      const copy = node.querySelector("#copy");
      if (copy) copy.onclick = () => {
        this.draft.name = (this.draft.name + " (Kopie)").slice(0, 64);
        this.itemId = null;
        this.editor();
      };
      const remove = node.querySelector("#delete");
      if (remove) remove.onclick = () => {
        if (window.confirm(`„${library[this.itemId].name}“ aus der Bibliothek löschen?`)) this.send("delete", { item_id: this.itemId });
      };
    }
    if (this.draft) this.renderDraft(programs);
  }

  diagnosticsEditor() {
    const node = this.$.querySelector("#editor");
    const events = [...(this.light.activity || [])].reverse();
    const text = events.map(event => `[${event.at}] ${event.level.toUpperCase()} · ${event.event}${event.detail ? " — " + event.detail : ""}`).join("\n");
    node.innerHTML = `<div class="row spread"><div><h2>Diagnoseprotokoll</h2><p class="muted">Die letzten ${events.length} Ereignisse dieser Home-Assistant-Sitzung. Besonders wichtig sind Warnungen zu power=off, Verbindungsabbrüchen und Ausgabefehlern.</p></div>
      <button id="copy-log" ${events.length ? "" : "disabled"}>Protokoll kopieren</button></div>
      ${events.length ? `<ul class="log">${events.map(event => `<li class="${escape(event.level)}"><time>${escape(event.at)}</time><strong>${escape(event.event)}</strong>${event.detail ? `<br><small>${escape(event.detail)}</small>` : ""}</li>`).join("")}</ul>` : '<p class="muted">Noch keine Ereignisse protokolliert.</p>'}`;
    const copy = node.querySelector("#copy-log");
    if (copy) copy.onclick = async () => {
      try {
        await navigator.clipboard.writeText(text);
        this.status("Diagnoseprotokoll wurde in die Zwischenablage kopiert.");
      } catch (_error) {
        this.status("Kopieren nicht möglich – bitte das Protokoll manuell markieren.");
      }
    };
  }

  renderDraft(programs) {
    const item = this.draft;
    const node = this.$.querySelector("#draft");
    const point = programs ? item.points[this.point] : null;
    const levels = point?.levels ?? item.levels;
    const resetMasterBase = () => {
      this.masterItem = item;
      this.masterBase = programs ? item.points.map(entry => [...entry.levels]) : [...levels];
      this.masterValue = 100;
    };
    // A day program has one global intensity reference. Switching between
    // points must not turn the control back into a point-only adjustment.
    if (this.masterItem !== item || !this.masterBase) resetMasterBase();
    node.innerHTML = `<div class="edit-grid"><div>
      <label class="field">${programs ? "Programmname" : "Szenenname"}<input id="name" maxlength="64" required placeholder="${programs ? "z. B. Riff · Tageslicht" : "z. B. Blaulicht"}" value="${escape(item.name)}"></label>
      <small>Änderungen an diesem Eintrag gelten für die gemeinsame Bibliothek aller Lampen.</small>
      ${programs ? `<details class="actions"><summary>Rampen-Assistent</summary>
        <div class="form-grid">
          <label class="field">Sonnenaufgang<input id="rise" type="time" value="09:00"></label>
          <label class="field">Aufhellen · Minuten<input id="rise-length" type="number" min="1" max="720" value="60"></label>
          <label class="field">Sonnenuntergang beginnt<input id="set" type="time" value="19:00"></label>
          <label class="field">Abdimmen · Minuten<input id="set-length" type="number" min="1" max="720" value="60"></label>
        </div><button id="ramp">Tagesverlauf mit diesem Spektrum erstellen</button>
        <small>Ersetzt die Stützpunkte im Entwurf.</small></details>
        <div class="point-list">${item.points.map((p, index) =>
          `<button data-point="${index}" aria-pressed="${this.point === index}">${clock(p.minute)}</button>`).join("")}</div>
        <div class="row"><label class="field">Zeit des Punkts<input id="point-time" type="time" value="${clock(point.minute)}"></label>
          <button id="add" ${item.points.length >= 48 ? "disabled" : ""}>+ Punkt</button>
          <button id="duplicate-point" ${item.points.length >= 48 ? "disabled" : ""}>Punkt duplizieren</button>
          <button id="remove" ${item.points.length <= 2 ? "disabled" : ""}>Punkt entfernen</button></div>` : ""}
      </div><div><h2>${programs ? `Spektrum · ${clock(point.minute)}` : "Spektrum der Szene"}</h2>
        <small>Alle Kanalwerte in Prozent. ${this.light.group ? "Zum Prüfen ein Programm über die Uhrzeit-Vorschau ansehen oder eine gespeicherte Szene starten." : `Regler starten eine Live-Vorschau an der Lampe für ${programs ? "1 Minute" : "5 Minuten"}.`}</small>
        ${programs ? `<label class="slider" style="--c:#83eee2"><span>Gesamt-<br>intensität</span><input id="master-level" type="range" min="0" max="100" value="${this.masterValue}" aria-label="Gesamtintensität des gesamten Tagesprogramms">
          <input id="master-value" type="number" min="0" max="100" step="1" value="${this.masterValue}" aria-label="Gesamtintensität des gesamten Tagesprogramms in Prozent"></label><small>Skaliert alle Kanäle an allen Zeitpunkten des gesamten Tagesprogramms proportional.</small>` : ""}
        <div class="sliders">${this.light.labels.map((label, i) => `<label class="slider" style="--c:${COLORS[i]}">
          <span>${escape(label)}</span><input type="range" min="0" max="100" value="${levels[i]}" data-level="${i}" aria-label="${escape(label)}">
          <input type="number" min="0" max="100" step="1" value="${levels[i]}" data-level="${i}" aria-label="${escape(label)} Prozent"></label>`).join("")}</div>
      </div></div><div class="actions save-actions"><button id="save" class="primary">${this.itemId ? "Änderungen speichern" : "In Bibliothek speichern"}</button>
        ${this.itemId ? `<button id="save-new">${programs ? "Als neues Programm" : "Als neue Szene"} speichern</button>` : ""}
        <small>${programs ? "Speichern aktualisiert die gemeinsame Bibliothek. Ein bereits auf dieser Lampe aktives Profil wird dabei erneut übertragen. „Auf Lampe speichern & aktivieren“ überträgt 24 Stundenpunkte; die Lampen-Firmware interpoliert dazwischen und läuft danach unabhängig weiter." : `Speichern aktualisiert die gemeinsame Bibliothek. Mit „Szene starten“ wählst du den Eintrag für ${this.light.group ? "die Gruppe" : "die Lampe"} aus.`}</small></div>`;
    node.querySelector("#name").oninput = event => { item.name = event.target.value; };
    node.querySelectorAll("[data-level]").forEach(input => {
      input.oninput = () => {
        if (input.value === "" || !input.validity.valid) return;
        levels[input.dataset.level] = Number(input.value);
        node.querySelectorAll(`[data-level="${input.dataset.level}"]`).forEach(peer => {
          if (peer !== input) peer.value = input.value;
        });
        // A manual channel change becomes the new neutral 100 % reference for
        // the complete profile, not only for the selected point.
        this.masterBase = programs ? item.points.map(entry => [...entry.levels]) : [...levels];
        this.masterValue = 100;
        const masterLevel = node.querySelector("#master-level");
        const masterValue = node.querySelector("#master-value");
        if (masterLevel) masterLevel.value = 100;
        if (masterValue) masterValue.value = 100;
        this.queueLivePreview(levels);
        this.update();
      };
      input.onchange = () => { input.value = levels[input.dataset.level]; };
    });
    const masterLevel = node.querySelector("#master-level");
    const masterValue = node.querySelector("#master-value");
    const applyMaster = value => {
      if (!Number.isInteger(value) || value < 0 || value > 100) return;
      item.points.forEach((entry, index) => {
        entry.levels.splice(0, entry.levels.length,
          ...this.masterBase[index].map(level => Math.round(level * value / 100)));
      });
      this.masterValue = value;
      node.querySelectorAll("[data-level]").forEach(input => { input.value = levels[input.dataset.level]; });
      if (masterLevel) masterLevel.value = value;
      if (masterValue) masterValue.value = value;
      this.queueLivePreview(levels);
      this.update();
    };
    if (masterLevel) masterLevel.oninput = () => applyMaster(Number(masterLevel.value));
    if (masterValue) masterValue.oninput = () => applyMaster(Number(masterValue.value));
    node.querySelector("#save").onclick = () => {
      if (!node.querySelector("#name").reportValidity()) return;
      this.send("save", { ...(this.itemId ? { item_id: this.itemId } : {}), item });
    };
    const saveNew = node.querySelector("#save-new");
    if (saveNew) saveNew.onclick = () => {
      if (!node.querySelector("#name").reportValidity()) return;
      // Omitting item_id makes the backend allocate a new library entry.
      this.send("save", { item });
    };
    if (!programs) return;
    const showPoint = () => { this.preview = item.points[this.point].minute; this.renderDraft(true); this.update(); };
    node.querySelectorAll("[data-point]").forEach(button => {
      button.onclick = () => { this.point = Number(button.dataset.point); showPoint(); };
    });
    node.querySelector("#point-time").onchange = event => {
      const minute = toMinute(event.target.value);
      if (!Number.isInteger(minute) || item.points.some(p => p !== point && p.minute === minute)) {
        event.target.value = clock(point.minute);
        this.status("Bitte eine freie Uhrzeit auswählen.");
        return;
      }
      point.minute = minute;
      item.points = sorted(item.points);
      this.point = item.points.indexOf(point);
      showPoint();
    };
    node.querySelector("#add").onclick = () => {
      let minute = this.preview ?? this.light.minute;
      while (item.points.some(p => p.minute === minute)) minute = (minute + 1) % 1440;
      const added = { minute, levels: interpolate(item.points, minute), curve: "linear" };
      item.points = sorted([...item.points, added]);
      this.point = item.points.indexOf(added);
      resetMasterBase();
      showPoint();
    };
    node.querySelector("#duplicate-point").onclick = () => {
      let minute = (point.minute + 1) % 1440;
      while (item.points.some(entry => entry.minute === minute)) minute = (minute + 1) % 1440;
      const duplicate = { ...point, minute, levels: [...point.levels], curve: "linear" };
      item.points = sorted([...item.points, duplicate]);
      this.point = item.points.indexOf(duplicate);
      resetMasterBase();
      showPoint();
      this.status("Zeitpunkt dupliziert. Jetzt die gewünschte Uhrzeit auswählen.");
    };
    node.querySelector("#remove").onclick = () => {
      item.points.splice(this.point, 1);
      this.point = Math.min(this.point, item.points.length - 1);
      resetMasterBase();
      showPoint();
    };
    node.querySelector("#ramp").onclick = () => {
      const rise = toMinute(node.querySelector("#rise").value);
      const set = toMinute(node.querySelector("#set").value);
      const up = Number(node.querySelector("#rise-length").value);
      const down = Number(node.querySelector("#set-length").value);
      if (![rise, set, up, down].every(Number.isInteger) || up < 1 || up > 720 || down < 1 || down > 720 ||
          rise + up >= set || set + down >= 1440) {
        this.status("Die Rampen müssen am selben Tag liegen und dürfen sich nicht überschneiden.");
        return;
      }
      const zero = levels.map(() => 0);
      item.points = [
        { minute: rise, levels: [...zero], curve: "linear" },
        { minute: rise + up, levels: [...levels], curve: "linear" },
        { minute: set, levels: [...levels], curve: "linear" },
        { minute: set + down, levels: [...zero], curve: "linear" },
      ];
      this.point = 1;
      resetMasterBase();
      this.status("");
      showPoint();
    };
  }

  groupEditor() {
    const node = this.$.querySelector("#editor");
    const physical = this.lights.filter(item => !item.group);
    node.innerHTML = `<div class="row spread"><h2>Lampengruppen</h2><button id="new-group" class="primary">+ Gruppe</button></div>
      <p class="muted">Programme und Szenen stammen aus der gemeinsamen Bibliothek. Gruppen starten den ausgewählten Eintrag auf allen Mitgliedern.</p>
      <div class="point-list">${this.groups.map(group => `<button data-group="${group.group_id}">${escape(group.name)} · ${group.members.length} Lampen</button>`).join("") || '<small>Noch keine Gruppen.</small>'}</div>
      <div id="group-form"></div>`;
    node.querySelector("#new-group").onclick = () => this.renderGroupForm();
    node.querySelectorAll("[data-group]").forEach(button => {
      button.onclick = () => this.renderGroupForm(this.groups.find(group => group.group_id === button.dataset.group));
    });
  }

  renderGroupForm(group = null) {
    const node = this.$.querySelector("#group-form");
    node.innerHTML = `<div class="edit-grid"><label class="field">Gruppenname<input id="group-name" maxlength="64" value="${escape(group?.name || "")}"></label>
      <div class="sliders group-members">${this.lights.filter(item => !item.group).map(light => `<label class="row group-member"><input type="checkbox" value="${escape(light.serial)}" ${group?.members.includes(light.serial) ? "checked" : ""}> ${escape(light.name)}</label>`).join("")}</div></div>
      <div class="actions"><button id="save-group" class="primary">Gruppe speichern</button>${group ? '<button id="delete-group" class="danger">Gruppe löschen</button>' : ""}</div>`;
    node.querySelector("#save-group").onclick = () => this.group("group_save", {
      ...(group ? { item_id: group.group_id } : {}),
      name: node.querySelector("#group-name").value,
      serials: [...node.querySelectorAll("input[type=checkbox]:checked")].map(input => input.value),
    });
    const remove = node.querySelector("#delete-group");
    if (remove) remove.onclick = () => this.group("group_delete", { item_id: group.group_id });
  }
}

customElements.define("aipai-reef-studio", ReefStudioPanel);
