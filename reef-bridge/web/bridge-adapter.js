// The editor and styles come directly from the HA panel. Only transport and
// device setup live here; no second copy of the editor is maintained.
const BRIDGE_LABELS = ['White','Blue','Red','Green','Royal Blue','Purple','UV','Warm White'];
const safe = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
class BridgeTransport {
  productName = 'Reef Bridge';
  async api(path, body) {
    const response = await fetch('/api/' + path, body === undefined ? {} : {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    const data = await response.json();
    if (!response.ok) throw Error(data.error || 'Die Bridge antwortet mit einem Fehler.');
    return data;
  }
  async callWS(msg) {
    if (msg.action !== 'list') await this.api('studio', msg);
    const data = await this.api('status');
    this.state = data;
    const workspace = data.workspace;
    const lights = workspace.lamps.map((lamp, index) => {
      const runtime = data.lights[index];
      return {...runtime, bridge_mode:runtime.mode,serial:lamp.serial,name:lamp.name,labels:BRIDGE_LABELS.slice(0,lamp.channels),
        programs:workspace.programs,scenes:workspace.scenes,local:true,online:runtime.online,
        firmware_points:lamp.points,timezone:lamp.timezone==='CET-1CEST,M3.5.0,M10.5.0/3'?'Europe/Berlin':lamp.timezone,now_epoch:data.now_epoch,
        activity:(runtime.log || []).map(event=>({at:event.at,level:event.level || 'info',event:event.message}))};
    });
    const groups = Object.entries(workspace.groups).map(([id, group]) => {
      const members = lights.filter(lamp=>group.serials.includes(lamp.serial));
      const first = members[0];
      const common = field => members.every(lamp=>field === 'scene' ? lamp.scene?.id === first.scene?.id && lamp.scene?.manual === first.scene?.manual : field === 'preview' ? !!lamp.preview === !!first.preview : JSON.stringify(lamp[field])===JSON.stringify(first[field])) ? first[field] : null;
      return {...first,serial:'group:'+id,group:true,group_id:id,name:group.name,members:group.serials,
        labels:BRIDGE_LABELS.slice(0,Math.max(...members.map(lamp=>lamp.labels.length))),
        online:members.every(lamp=>lamp.online),program:common('program'),program_execution:common('program_execution'),
        startup_hold:members.some(lamp=>lamp.startup_hold),scene:common('scene'),preview:common('preview'),
        indicator_light:common('indicator_light'),physical_resume:common('physical_resume'),
        sent:null,reported:[],error:members.map(lamp=>lamp.error).filter(Boolean).join(' · ')};
    });
    return {lights,groups,update:{current:data.version,latest:data.update.latest,available:data.update.available,url:data.update.installer_url}};
  }
}
class ReefBridgePanel extends customElements.get('aipai-reef-studio') {
  render() {
    super.render();
    this.$.querySelector('ha-menu-button')?.remove();
    const subtitle = this.$.querySelector('.head p');
    if (subtitle) subtitle.innerHTML += ` <span class="pill">Bridge v${safe(this._hass?.state?.version || '…')}</span>`;
    const tabs = this.$.querySelector('.tabs');
    if (tabs) {
      tabs.style.gridTemplateColumns = 'repeat(auto-fit,minmax(100px,1fr))';
      const button = document.createElement('button');
      button.textContent = 'Einrichtung';button.setAttribute('aria-pressed',this.kind === 'setup');
      button.onclick = ()=>{this.kind='setup';this.reset();this.render();};tabs.append(button);
    } else {
      const section = this.$.querySelector('.empty');
      if (section) {section.id='editor';this.setupEditor();}
    }
    const update = this.$.querySelector('.update-note');
    if (update) update.innerHTML = `Bridge-Update verfügbar: v${safe(this.updateInfo.latest)}. <a style="color:inherit" href="https://xsiimonox.github.io/aipai-reef-studio/" target="_blank" rel="noopener">USB-Installer öffnen</a>`;
  }
  editor() { if (this.kind === 'setup') return this.setupEditor();return super.editor(); }
  async refresh() {
    const previousSetup=JSON.stringify(this._hass?.state?.workspace?.lamps);
    await super.refresh();
    if(this.kind === 'setup' && previousSetup!==JSON.stringify(this._hass?.state?.workspace?.lamps))this.setupEditor();
    if (!this._hass?.state && !this.light) this.status('Verbindung zur Reef Bridge wird hergestellt …');
  }
  setupEditor() {
    const node = this.$.querySelector('#editor');
    if (!node) return;
    const state=this._hass?.state;
    const lamps=state?.workspace?.lamps || [];
    const provision=state?.provision || {};
    const access=this.provisionAccess || provision;
    const networks=this.provisionNetworks || [];
    node.innerHTML=`<style>#bridge-settings .field{margin-top:16px}#bridge-settings h2{margin-bottom:8px}</style><div id="bridge-settings"><h2>Reef Bridge einrichten</h2><p class="muted">Bis zu vier Lampen im gleichen WLAN. Programme und Szenen werden auf dieser Bridge gemeinsam gespeichert.</p>
      <div class="edit-grid"><div><h2>Lampen</h2><div class="point-list">${lamps.map((lamp,i)=>`<button data-fixture="${i}">${safe(lamp.name)}</button>`).join('')}<button id="add-lamp">+ Lampe</button></div>
      <label class="field">Name<input id="device-name" maxlength="64" required></label>
      <label class="field">Lampen-IP<input id="device-ip" placeholder="192.168.178.141" required></label>
      <label class="field">Seriennummer<input id="device-serial" maxlength="32" required></label>
      <label class="field">Kanäle<select id="device-count"><option value="6">6 · A7 SIII / A7N mini / A8SE v1</option><option value="8">8 · MAX / MAX Pro</option></select></label>
      <label class="field">Zeitzone (POSIX)<input id="device-zone" value="CET-1CEST,M3.5.0,M10.5.0/3" required></label>
      <small>Europa/Berlin ist vorbelegt, inklusive Sommerzeit. Die Seriennummer wird vor jedem Schreibzyklus geprüft.</small>
      <div class="actions"><button id="store-device" class="primary">Lampe speichern</button></div>
      <h2 style="margin-top:30px">Fabrikneue Lampe aus Hotspot übernehmen</h2><p class="muted">Die Bridge verbindet sich vorübergehend mit dem AIPAI-Hotspot, überträgt ihr eigenes gespeichertes Heim-WLAN und übernimmt IP sowie Seriennummer. Das Lampen-Hotspot-Passwort bleibt nur im Arbeitsspeicher.</p>
      ${!provision.home_configured ? '<p class="notice">Speichere zuerst unten das Heim-WLAN der Reef Bridge.</p>' : ''}
      <div class="actions"><button id="enable-provision" ${!provision.home_configured ? 'disabled' : ''}>Einrichtungs-WLAN starten</button></div>
      ${provision.active ? `<div class="panel" style="margin-top:16px"><strong>Einrichtungsmodus aktiv</strong><p class="muted">Verbinde dein Telefon oder deinen Computer mit <b>${safe(access.ssid || provision.ssid)}</b>${access.password ? ` · Passwort <b>${safe(access.password)}</b>` : ''} und öffne <a href="${safe(access.url || provision.url)}" style="color:inherit">${safe(access.url || provision.url)}</a>. Bleibe für die folgenden Schritte in diesem WLAN.</p>
        <div class="actions"><button id="scan-lamp-hotspots">Lampen-Hotspots suchen</button></div>
        <label class="field">AIPAI-Lampen-Hotspot<select id="lamp-hotspot" required><option value="">Bitte suchen und auswählen</option>${networks.map(network=>`<option value="${safe(network.ssid)}">${safe(network.ssid)} · ${network.rssi} dBm${network.open ? ' · offen' : ''}</option>`).join('')}</select></label>
        <label class="field">Hotspot-Passwort · falls erforderlich<input id="lamp-hotspot-password" type="password" maxlength="63" autocomplete="new-password"></label>
        <label class="field">Lampenname<input id="provision-name" maxlength="64" required placeholder="z. B. Riff links"></label>
        <label class="field">Kanäle<select id="provision-channels"><option value="6">6 · A7 SIII / A7N mini / A8SE v1</option><option value="8">8 · MAX / MAX Pro</option></select></label>
        <label class="field">Zeitzone (POSIX)<input id="provision-zone" value="CET-1CEST,M3.5.0,M10.5.0/3" required></label>
        <div class="actions"><button id="provision-lamp" class="primary">WLAN übertragen und Lampe übernehmen</button></div>
        ${this.provisionResult ? `<p class="notice">Lampe übernommen: ${safe(this.provisionResult.model)} · ${safe(this.provisionResult.serial)} · ${safe(this.provisionResult.lamp_ip)}</p><div class="actions"><button id="disable-provision">Einrichtungsmodus beenden</button></div>` : ''}
        <small>Vor der Übertragung prüft die Bridge lokal eine gültige AIPAI-Konfiguration und die bestätigte Hotspot-Adresse 192.168.4.1. Das Heim-WLAN-Passwort wird weder angezeigt noch protokolliert.</small></div>` : ''}
      </div>
      <div><h2>WLAN</h2><label class="field">2,4-GHz WLAN / SSID<input id="wifi-ssid" maxlength="32"></label><label class="field">WLAN-Passwort<input id="wifi-password" type="password" maxlength="63" autocomplete="new-password"></label>
      <div class="actions"><button id="store-wifi">WLAN speichern</button></div><small>Nach Verbindung schließt das Setup-WLAN. Danach erreichst du die Bridge unter http://reefbridge.local oder über ihre Router-IP. Das konfliktfreie Setup-WLAN verwendet http://192.168.8.1.</small>
      <h2 style="margin-top:28px">Sicherung</h2><p class="muted">Enthält Lampen, Gruppen, Programme und Szenen. WLAN-Passwörter werden nicht exportiert.</p><div class="actions"><button id="backup">Backup herunterladen</button></div><label class="field">Backup importieren<input id="restore" type="file" accept="application/json"></label>
      <small>Import ersetzt die lokale Bibliothek und Lampeneinrichtung. Laufende Vorschauen enden.</small></div></div></div>`;
    let index=-1;
    const select=i=>{index=i;const lamp=lamps[i];node.querySelector('#device-name').value=lamp?.name||'';node.querySelector('#device-ip').value=lamp?.lamp_ip||'';node.querySelector('#device-serial').value=lamp?.serial||'';node.querySelector('#device-count').value=lamp?.channels||6;node.querySelector('#device-zone').value=lamp?.timezone||'CET-1CEST,M3.5.0,M10.5.0/3';};
    node.querySelectorAll('[data-fixture]').forEach(button=>button.onclick=()=>select(Number(button.dataset.fixture)));
    node.querySelector('#add-lamp').onclick=()=>select(-1);select(lamps.length?0:-1);
    const run=async task=>{if(this.busy)return;this.setBusy(true);try{await task();this.receive(await this._hass.callWS({action:'list'}));this.status('Gespeichert.');}catch(error){this.status(error.message);}finally{this.setBusy(false);this.update();}};
    node.querySelector('#store-device').onclick=()=>run(async()=>{
      for (const id of ['device-name','device-ip','device-serial','device-zone']) if(!node.querySelector('#'+id).reportValidity())return;
      await this._hass.api('fixture',{index,name:node.querySelector('#device-name').value,lamp_ip:node.querySelector('#device-ip').value,serial:node.querySelector('#device-serial').value,channels:Number(node.querySelector('#device-count').value),timezone:node.querySelector('#device-zone').value});
    });
    node.querySelector('#store-wifi').onclick=()=>run(()=>this._hass.api('wifi',{ssid:node.querySelector('#wifi-ssid').value,password:node.querySelector('#wifi-password').value}));
    node.querySelector('#enable-provision').onclick=async()=>{if(this.busy)return;this.setBusy(true);try{this.provisionAccess=await this._hass.api('provision/enable',{});this.receive(await this._hass.callWS({action:'list'}));this.status(`Jetzt mit ${this.provisionAccess.ssid} verbinden und ${this.provisionAccess.url} öffnen.`);this.setupEditor();}catch(error){this.status(error.message);}finally{this.setBusy(false);this.update();}};
    const scan=node.querySelector('#scan-lamp-hotspots');if(scan)scan.onclick=async()=>{if(this.busy)return;this.setBusy(true);try{const result=await this._hass.api('provision/scan');this.provisionNetworks=result.networks||[];this.setupEditor();this.status(this.provisionNetworks.length?'Hotspots gefunden. Bitte deine Lampe auswählen.':'Keine WLAN-Hotspots gefunden.');}catch(error){this.status(error.message);}finally{this.setBusy(false);this.update();}};
    const provisionButton=node.querySelector('#provision-lamp');if(provisionButton)provisionButton.onclick=async()=>{
      for(const id of ['lamp-hotspot','provision-name','provision-zone'])if(!node.querySelector('#'+id).reportValidity())return;
      if(this.busy)return;this.setBusy(true);try{this.provisionResult=await this._hass.api('provision/start',{lamp_ssid:node.querySelector('#lamp-hotspot').value,lamp_password:node.querySelector('#lamp-hotspot-password').value,name:node.querySelector('#provision-name').value,channels:Number(node.querySelector('#provision-channels').value),timezone:node.querySelector('#provision-zone').value});this.receive(await this._hass.callWS({action:'list'}));this.setupEditor();this.status('Lampe wurde ins Heim-WLAN übernommen und in Reef Bridge angelegt.');}catch(error){this.status(error.message);}finally{this.setBusy(false);this.update();}
    };
    const disable=node.querySelector('#disable-provision');if(disable)disable.onclick=async()=>{try{await this._hass.api('provision/disable',{});}catch(_error){}this.status('Einrichtungs-WLAN wird beendet. Verbinde dich wieder mit deinem Heim-WLAN und öffne reefbridge.local.');};
    node.querySelector('#backup').onclick=()=>{const a=document.createElement('a');const url=URL.createObjectURL(new Blob([JSON.stringify(this._hass.state.workspace,null,2)],{type:'application/json'}));a.href=url;a.download='reef-bridge-backup.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
    node.querySelector('#restore').onchange=event=>run(async()=>{const file=event.target.files[0];if(file && confirm('Lokale Lampen und Bibliothek durch dieses Backup ersetzen?'))await this._hass.api('restore',JSON.parse(await file.text()));});
  }
}
customElements.define('reef-bridge-panel',ReefBridgePanel);
const bridge=document.createElement('reef-bridge-panel');bridge.hass=new BridgeTransport();document.body.append(bridge);
