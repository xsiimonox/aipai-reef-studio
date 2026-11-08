// Run the actual shared firmware UI against a simulated local API, never lamps.
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{
 const root=path.resolve(__dirname,'..'),repo=path.resolve(root,'..');
 const firmware=JSON.parse(fs.readFileSync(path.join(root,'installer/manifest.json'),'utf8')).version;
 const html=fs.readFileSync(path.join(root,'web/index.html'),'utf8')
   .replace('/* SHARED_STUDIO */',()=>fs.readFileSync(path.join(repo,'custom_components/aipai_light/panel/reef-studio.js'),'utf8'))
   .replace('/* BRIDGE_ADAPTER */',()=>fs.readFileSync(path.join(root,'web/bridge-adapter.js'),'utf8'));
 const points=[0,540,660,1080,1200].map((minute,i)=>({minute,levels:[25,70,3,2,85,12,5,15].map(v=>i===2||i===3?v:0)}));
 const workspace={schema:1,lamps:[],programs:{day:{name:'Riff · Tageslicht',points}},scenes:{blue:{name:'Blaulicht',levels:[0,40,0,0,80,10,0,0]}},groups:{}};
 const calls=[],runtime=[];let state;
 const reset=()=>{state={version:firmware,workspace,now_epoch:Date.now()/1000,update:{available:false,latest:firmware},lights:runtime};};reset();
 const browser=await chromium.launch({headless:true});
 try{
  const page=await browser.newPage({locale:'de-DE',viewport:{width:1440,height:1000}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.route('http://bridge.test/**',async route=>{
   const url=new URL(route.request().url());if(url.pathname==='/'){await route.fulfill({contentType:'text/html; charset=utf-8',body:html});return;}
   const msg=route.request().postDataJSON();if(msg){calls.push({path:url.pathname,body:msg});
    if(url.pathname==='/api/fixture'){
     workspace.lamps.push({...msg,points,program:'day'});runtime.push({program:'day',program_execution:'hold',startup_hold:true,mode:'firmware',scene:null,preview:null,online:true,minute:780,temperature:38.5,log:[]});
    }
    if(url.pathname==='/api/studio'){
     if(msg.action==='save')workspace[msg.kind][msg.item_id||'saved-'+calls.length]=msg.item;
     if(msg.action==='group_save')workspace.groups[msg.item_id||'group1']={name:msg.name,serials:msg.serials};
     const targets=msg.serial?.startsWith('group:')?workspace.groups[msg.serial.slice(6)].serials:[msg.serial];
     workspace.lamps.forEach((lamp,i)=>{if(!targets.includes(lamp.serial))return;
      if(msg.action==='activate'){if(msg.kind==='programs'){lamp.program=msg.item_id;lamp.program_execution='device';runtime[i].program=msg.item_id;runtime[i].program_execution='device';runtime[i].startup_hold=false;runtime[i].mode='firmware';}else runtime[i].scene={id:msg.item_id,until:Date.now()/1000+msg.duration*60};}
      if(msg.action==='manual')runtime[i].scene={id:msg.item_id,until:null,manual:true};
      if(msg.action==='preview')runtime[i].preview={until:Date.now()/1000+msg.preview_seconds};
      if(msg.action==='preview_cancel')runtime[i].preview=null;
      if(msg.action==='resume'){runtime[i].preview=null;runtime[i].scene=null;}
     });
    }reset();await route.fulfill({json:{ok:true}});return;
   }state.now_epoch=Date.now()/1000;await route.fulfill({json:state});
  });
  await page.goto('http://bridge.test/');const ui=page.locator('reef-bridge-panel');
  // Empty workspace must still show an accessible onboarding form.
  await ui.locator('#device-name').fill('Riff · A7 SIII');await ui.locator('#device-ip').fill('192.0.2.20');await ui.locator('#device-serial').fill('DEMO6');await ui.locator('#store-device').click();
  await ui.locator('#library').waitFor();await ui.locator('#library').selectOption('day');
  assert.equal(await ui.locator('#activate').textContent(),'Auf Lampe speichern & aktivieren');
  await ui.locator('#activate').click();
  assert(calls.some(c=>c.body.action==='activate'&&c.body.kind==='programs'&&c.body.item_id==='day'));
  assert.equal(workspace.lamps[0].program_execution,'device');
  await page.waitForFunction(()=>document.querySelector('reef-bridge-panel').shadowRoot.querySelector('#active-mode').textContent.startsWith('Auf Lampe gespeichert'));
  assert((await ui.locator('#active-mode').textContent()).startsWith('Auf Lampe gespeichert'));
  assert.equal(await ui.locator('[data-level][type=range]').count(),6);
  await ui.locator('[data-point="2"]').click();
  const originalPoints=await ui.evaluate(el=>structuredClone(el.draft.points));
  await ui.locator('#master-value').fill('50');
  const scaledPoints=await ui.evaluate(el=>structuredClone(el.draft.points));
  assert.deepEqual(scaledPoints.map(point=>point.levels),originalPoints.map(point=>point.levels.map(level=>Math.round(level*.5))),'master intensity must scale all points');
  await ui.locator('[data-point="1"]').click();assert.equal(await ui.locator('#master-value').inputValue(),'50');await ui.locator('[data-point="2"]').click();
  const pointBeforeCopy=await ui.evaluate(el=>structuredClone(el.draft.points[el.point]));await ui.locator('#duplicate-point').click();
  const copyResult=await ui.evaluate(el=>({count:el.draft.points.length,point:structuredClone(el.draft.points[el.point])}));
  assert.equal(copyResult.count,originalPoints.length+1);assert.deepEqual(copyResult.point.levels,pointBeforeCopy.levels);assert.notEqual(copyResult.point.minute,pointBeforeCopy.minute);
  await ui.locator('[data-level="1"][type=number]').fill('55');
  await page.waitForTimeout(350);
  assert(calls.some(c=>c.body.action==='preview'&&c.body.preview_seconds===60&&c.body.levels.length===6));
  await ui.locator('#name').fill('Riff · Neues Programm');await ui.locator('#save-new').click();
  await page.waitForFunction(()=>document.querySelector('reef-bridge-panel').light.programs['saved-'+0]!==undefined || Object.keys(document.querySelector('reef-bridge-panel').light.programs).length===2);
  const screenshotDir=process.env.BRIDGE_SCREENSHOT_DIR||'/tmp/reef-bridge-studio';fs.mkdirSync(screenshotDir,{recursive:true});
  await ui.locator('#library').selectOption('day');await ui.locator('[data-point="2"]').click();
  for(const width of [1440,1024,768,390,320]){await page.setViewportSize({width,height:1000});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'overflow at '+width);assert.deepEqual(await ui.evaluate(el=>[...el.shadowRoot.querySelectorAll('button,input,select,.panel')].filter(n=>n.getBoundingClientRect().right>innerWidth+1).map(n=>n.id||n.className)),[],'control overflow at '+width);if(width===1440||width===390)await page.screenshot({path:path.join(screenshotDir,'bridge-'+width+'.png'),fullPage:true});}
  await page.setViewportSize({width:1440,height:1000});
  await ui.getByRole('button',{name:'Einrichtung',exact:true}).click();await page.screenshot({path:path.join(screenshotDir,'setup.png'),fullPage:true});await ui.locator('#add-lamp').click();
  await ui.locator('#device-name').fill('Riff · MAX Pro');await ui.locator('#device-ip').fill('192.0.2.21');await ui.locator('#device-serial').fill('DEMO8');await ui.locator('#device-count').selectOption('8');await ui.locator('#store-device').click();
  await page.waitForFunction(()=>document.querySelector('reef-bridge-panel').lights.length===2);
  await ui.locator('[data-tab=groups]').click();await ui.locator('#new-group').click();await ui.locator('#group-name').fill('Hauptbecken');
  for(const box of await ui.locator('[type=checkbox]').all())await box.check();await ui.locator('#save-group').click();
  await ui.locator('#lamp').selectOption('group:group1');await ui.locator('[data-tab=programs]').click();await ui.locator('#library').selectOption('day');await ui.locator('#activate').click();
  assert(runtime.every(lamp=>lamp.program_execution==='device'&&!lamp.startup_hold));
  await ui.locator('#lamp').selectOption('group:group1');await ui.locator('[data-tab=scenes]').click();await ui.locator('#library').selectOption('blue');
  await ui.locator('#manual').click();await page.waitForFunction(()=>document.querySelector('reef-bridge-panel').light.scene?.manual===true);
  assert.equal(await ui.locator('[data-level][type=range]').count(),8);
  await ui.locator('[data-action=resume]').click();await ui.locator('#library').selectOption('blue');await ui.locator('#duration').fill('12');await ui.locator('#activate').click();
  assert(calls.some(c=>c.body.action==='activate'&&c.body.duration===12&&c.body.serial==='group:group1'));
  await ui.locator('#new').click();assert(await ui.locator('#save').isVisible());
  await ui.locator('#name').fill('Gruppe · UV und Warm White');await ui.locator('[data-level="6"][type=number]').fill('71');await ui.locator('[data-level="7"][type=number]').fill('83');await ui.locator('#save').click();
  assert(calls.some(c=>c.body.action==='save'&&c.body.serial==='group:group1'&&c.body.item.levels?.[6]===71&&c.body.item.levels?.[7]===83));
  await ui.locator('[data-tab=programs]').click();await ui.locator('#new').click();
  assert.equal(await ui.locator('[data-level][type=range]').count(),8);
  await ui.locator('#name').fill('Gruppe · Achtkanal-Tag');await ui.locator('[data-level="6"][type=number]').fill('65');await ui.locator('#save').click();
  assert(calls.some(c=>c.body.action==='save'&&c.body.serial==='group:group1'&&c.body.item.points?.[0].levels[6]===65));
  // Native countdown continues between network polls.
  const before=await ui.evaluate(el=>{el.testDeadline=Date.now()/1000+60;return el.secondsUntil(el.testDeadline,el.light);});await page.waitForTimeout(1100);const after=await ui.evaluate(el=>el.secondsUntil(el.testDeadline,el.light));assert(after<before,'Countdown must tick between polls');
  assert.deepEqual(errors,[]);console.log('PASS: global intensity, point copy, onboarding, 6/8 channels, shared library, mixed groups, responsive 320–1440px; no JS errors. '+screenshotDir);
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
