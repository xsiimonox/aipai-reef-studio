// Render the actual firmware UI with simulated API responses, never a real lamp.
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const fs=require('node:fs');
const path=require('node:path');
const assert=require('node:assert/strict');
(async()=>{
 const root=path.resolve(__dirname,'..');
 const config={lamp_ip:'192.0.2.20',serial:'DEMO',channels:6,timezone:'CET-1CEST,M3.5.0,M10.5.0/3',points:[{minute:0,levels:[0,0,0,0,0,0]},{minute:540,levels:[0,0,0,0,0,0]},{minute:660,levels:[25,70,3,2,85,12]},{minute:1080,levels:[25,70,3,2,85,12]},{minute:1200,levels:[0,0,0,0,0,0]},{minute:1439,levels:[0,0,0,0,0,0]}],scenes:[{name:'Blaulicht',levels:[0,35,0,0,75,10]}]};
 let mode='schedule'; const calls=[];
 const browser=await chromium.launch({headless:true});
 try{
 const page=await browser.newPage({viewport:{width:1360,height:1000}});
 const errors=[];page.on('pageerror',err=>errors.push(String(err)));
 await page.route('http://bridge.test/**',async route=>{
  const url=new URL(route.request().url());
  if(url.pathname==='/'){await route.fulfill({contentType:'text/html',body:fs.readFileSync(path.join(root,'web/index.html'),'utf8')});return;}
  if(route.request().method()==='POST'){
   const body=route.request().postDataJSON();calls.push({path:url.pathname,body});
   if(url.pathname==='/api/control')mode=body.action==='resume'?'schedule':body.action;
   if(url.pathname==='/api/config')Object.assign(config,body);
   await route.fulfill({json:{ok:true}});return;
  }
  await route.fulfill({json:{version:'0.1.1',mode,lamp_online:true,clock_valid:true,temperature:38.4,remaining_seconds:0,update:{available:false,latest:'0.1.1'},config,sent:[25,70,3,2,85,12],log:['DEMO · Beispieldaten, keine reale Lampe','Output schedule; observed power=on, mode=0']}});
 });
 await page.goto('http://bridge.test/');await page.locator('#token').fill('demo-token');await page.locator('#connect').click();
 await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('38.4'));
 await page.waitForFunction(()=>document.querySelector('#update').textContent.includes('Firmware aktuell: v0.1.1'));
 assert.equal(await page.locator('#channels input').count(),6);
 await page.locator('#manual').click();await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('Dauerhaft'));
 await page.locator('#resume').click();await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('Tagesprogramm'));
 await page.locator('#scene-name').fill('Fotoszene');await page.locator('#save-scene').click();
 await page.waitForFunction(()=>document.querySelector('#scenes').options.length===2);
 assert(calls.some(c=>c.body.action==='manual'));assert(calls.some(c=>c.body.action==='resume'));
 assert.equal(config.scenes[1].name,'Fotoszene');assert.deepEqual(errors,[]);
 await page.screenshot({path:path.join(root,'docs/bridge-desktop.png'),fullPage:true});
 await page.setViewportSize({width:390,height:844});
 assert(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth));
 await page.screenshot({path:path.join(root,'docs/bridge-mobile.png'),fullPage:true});
 console.log('PASS: six channels, manual/resume actions, scene persistence request, mobile layout, no JS errors.');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
