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
        labels:BRIDGE_LABELS.slice(0,Math.min(...members.map(lamp=>lamp.labels.length))),
        online:members.every(lamp=>lamp.online),program:common('program'),scene:common('scene'),preview:common('preview'),
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
    node.innerHTML=`<style>#bridge-settings .field{margin-top:16px}#bridge-settings h2{margin-bottom:8px}</style><div id="bridge-settings"><h2>Reef Bridge einrichten</h2><p class="muted">Bis zu vier Lampen im gleichen WLAN. Programme und Szenen werden auf dieser Bridge gemeinsam gespeichert.</p>
      <div class="edit-grid"><div><h2>Lampen</h2><div class="point-list">${lamps.map((lamp,i)=>`<button data-fixture="${i}">${safe(lamp.name)}</button>`).join('')}<button id="add-lamp">+ Lampe</button></div>
      <label class="field">Name<input id="device-name" maxlength="64" required></label>
      <label class="field">Lampen-IP<input id="device-ip" placeholder="192.168.178.141" required></label>
      <label class="field">Seriennummer<input id="device-serial" maxlength="32" required></label>
      <label class="field">Kanäle<select id="device-count"><option value="6">6 · A7 SIII / A8SE v1</option><option value="8">8 · MAX / MAX Pro</option></select></label>
      <label class="field">Zeitzone (POSIX)<input id="device-zone" value="CET-1CEST,M3.5.0,M10.5.0/3" required></label>
      <small>Europa/Berlin ist vorbelegt, inklusive Sommerzeit. Die Seriennummer wird vor jedem Schreibzyklus geprüft.</small>
      <div class="actions"><button id="store-device" class="primary">Lampe speichern</button></div></div>
      <div><h2>WLAN</h2><label class="field">2,4-GHz WLAN / SSID<input id="wifi-ssid" maxlength="32"></label><label class="field">WLAN-Passwort<input id="wifi-password" type="password" maxlength="63" autocomplete="new-password"></label>
      <div class="actions"><button id="store-wifi">WLAN speichern</button></div><small>Nach Verbindung schließt das Setup-WLAN. Die neue Bridge-IP findest du im Router. Im Setup-WLAN ist die Oberfläche unter http://192.168.4.1 erreichbar.</small>
      <h2 style="margin-top:28px">Sicherung</h2><p class="muted">Enthält Lampen, Gruppen, Programme und Szenen. WLAN-Passwörter werden nicht exportiert.</p><div class="actions"><button id="backup">Backup herunterladen</button></div><label class="field">Backup importieren<input id="restore" type="file" accept="application/json"></label>
      <small>Import ersetzt die lokale Bibliothek und Lampeneinrichtung. Laufende Vorschauen enden.</small></div></div></div>`;
    let index=-1;
    const select=i=>{index=i;const lamp=lamps[i];node.querySelector('#device-name').value=lamp?.name||'';node.querySelector('#device-ip').value=lamp?.lamp_ip||'';node.querySelector('#device-serial').value=lamp?.serial||'';node.querySelector('#device-count').value=lamp?.channels||6;node.querySelector('#device-zone').value=lamp?.timezone||'CET-1CEST,M3.5.0,M10.5.0/3';};
    node.querySelectorAll('[data-fixture]').forEach(button=>button.onclick=()=>select(Number(button.dataset.fixture)));
    node.querySelector('#add-lamp').onclick=()=>select(-1);select(lamps.length?0:-1);
    const run=async task=>{try{await task();await this.refresh();this.status('Gespeichert.');}catch(error){this.status(error.message);}};
    node.querySelector('#store-device').onclick=()=>run(async()=>{
      for (const id of ['device-name','device-ip','device-serial','device-zone']) if(!node.querySelector('#'+id).reportValidity())return;
      await this._hass.api('fixture',{index,name:node.querySelector('#device-name').value,lamp_ip:node.querySelector('#device-ip').value,serial:node.querySelector('#device-serial').value,channels:Number(node.querySelector('#device-count').value),timezone:node.querySelector('#device-zone').value});
    });
    node.querySelector('#store-wifi').onclick=()=>run(()=>this._hass.api('wifi',{ssid:node.querySelector('#wifi-ssid').value,password:node.querySelector('#wifi-password').value}));
    node.querySelector('#backup').onclick=()=>{const a=document.createElement('a');const url=URL.createObjectURL(new Blob([JSON.stringify(this._hass.state.workspace,null,2)],{type:'application/json'}));a.href=url;a.download='reef-bridge-backup.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
    node.querySelector('#restore').onchange=event=>run(async()=>{const file=event.target.files[0];if(file && confirm('Lokale Lampen und Bibliothek durch dieses Backup ersetzen?'))await this._hass.api('restore',JSON.parse(await file.text()));});
  }
}
customElements.define('reef-bridge-panel',ReefBridgePanel);
const bridge=document.createElement('reef-bridge-panel');bridge.hass=new BridgeTransport();document.body.append(bridge);
