// GO-GO web UI: single embedded page (dark soundkorb style).
// Served from PROGMEM; talks to the JSON API in web_ui.cpp.
#pragma once

const char WEB_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GO-GO</title>
<style>
:root{--bg:#0f1622;--card:#171d27;--line:#2b3950;--tx:#e8eef7;--mut:#8fa1b8;--acc:#39d353;--red:#e5484d;--sel:#f2c53d}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--tx);font:15px/1.5 -apple-system,'Segoe UI',Roboto,sans-serif;padding:16px;max-width:760px;margin:0 auto}
h1{font-size:22px;letter-spacing:2px;margin:4px 0 2px}
h1 b{color:var(--acc)}
.sub{color:var(--mut);font-size:12px;margin-bottom:14px}
.tabs{display:flex;gap:6px;margin-bottom:14px;flex-wrap:wrap}
.tabs button{background:var(--card);border:1px solid var(--line);color:var(--mut);padding:8px 14px;border-radius:8px;cursor:pointer;font-size:14px}
.tabs button.on{color:var(--tx);border-color:var(--acc)}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin-bottom:12px}
.kv{display:grid;grid-template-columns:1fr 1fr;gap:6px 14px}
.kv div{padding:4px 0;border-bottom:1px solid #1e2735;font-size:14px}
.kv span{color:var(--mut);display:block;font-size:11px;text-transform:uppercase;letter-spacing:1px}
label{display:block;color:var(--mut);font-size:12px;text-transform:uppercase;letter-spacing:1px;margin:10px 0 4px}
input,select{width:100%;background:#0c1320;border:1px solid var(--line);color:var(--tx);padding:8px 10px;border-radius:8px;font-size:15px}
.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.btn{background:var(--acc);color:#08240f;border:0;padding:10px 18px;border-radius:8px;font-weight:700;font-size:15px;cursor:pointer;margin-top:14px}
.btn.red{background:var(--red);color:#fff}
.btn.ghost{background:transparent;border:1px solid var(--line);color:var(--tx)}
.bigbtns{display:flex;gap:12px}
.bigbtns .btn{flex:1;padding:16px;font-size:20px;margin-top:0}
canvas{width:100%;background:#05080e;border-radius:8px;border:1px solid var(--line)}
.warn{color:var(--sel);font-size:13px;margin:8px 0}
h3{font-size:12px;letter-spacing:1.5px;text-transform:uppercase;color:var(--acc);margin:0 0 4px}
.tgrid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-top:10px}
.tgrid .btn{margin:0;padding:12px 4px;font-size:14px}
.cue{display:flex;align-items:center;gap:8px;padding:7px 6px;border-bottom:1px solid #1e2735;font-size:14px}
.cue .num{color:var(--sel);min-width:34px;font-family:monospace}
.cue .nm{flex:1}
.cue button{background:#0c1320;border:1px solid var(--line);color:var(--tx);border-radius:6px;padding:4px 10px;cursor:pointer}
.gobig{width:100%;padding:26px;font-size:34px;letter-spacing:4px}
.ok{color:var(--acc)} .bad{color:var(--red)}
#msg{position:fixed;top:10px;right:10px;background:var(--acc);color:#08240f;padding:8px 14px;border-radius:8px;display:none;font-weight:600}
</style></head><body>
<h1>GO<b>-</b>GO</h1>
<div class="sub" id="devline">&nbsp;</div>
<div id="msg"></div>
<div class="tabs">
<button class="on" onclick="tab(0,this)">Status</button>
<button onclick="tab(1,this)">Settings</button>
<button onclick="tab(2,this)">Spectrum</button>
<button onclick="tab(4,this)">QLab</button>
<button onclick="tab(3,this)">Firmware</button>
</div>

<div class="page" id="p0">
<div class="card bigbtns">
<button class="btn" onclick="act('go')">GO</button>
<button class="btn red" onclick="act('panic')">PANIC</button>
</div>
<div class="card"><div class="kv" id="stat"></div></div>
</div>

<div class="page" id="p1" style="display:none">
<div class="card">
<h3>Mode</h3>
<label>Operating mode</label><select id="mode" onchange="vis()">
<option value="0">OSC / WiFi</option><option value="1">BLE keyboard</option>
<option value="2">LoRa TX (remote)</option><option value="3">LoRa RX (gateway)</option></select>
<div id="b_out"><label>Gateway output</label><select id="out" onchange="vis()">
<option value="0">BLE keyboard</option><option value="1">OSC / WiFi</option></select></div>
</div>
<div class="card" id="b_radio">
<h3>LoRa radio</h3>
<div class="row"><div><label>Region</label><select id="region"></select></div>
<div><label>Frequency</label><select id="chan"></select></div></div>
</div>
<div class="card" id="b_keys">
<h3>Keyboard output</h3>
<div class="row"><div><label>GO key</label><select id="gokey"></select></div>
<div><label>PANIC key</label><select id="pankey"></select></div></div>
</div>
<div class="card" id="b_wifi">
<h3>WiFi network</h3>
<label>Network name</label><input id="ssid" placeholder="venue WiFi">
<label>Password</label><input id="wpass" type="password" placeholder="leave empty to keep current">
</div>
<div class="card" id="b_osc">
<h3>OSC target</h3>
<div class="row"><div><label>Target IP</label><input id="oscIp"></div>
<div><label>Port</label><input id="oscPort" type="number"></div></div>
<div class="row"><div><label>GO address</label><input id="goAddr"></div>
<div><label>PANIC address</label><input id="panAddr"></div></div>
</div>
<button class="btn" onclick="saveCfg()">Save</button>
<div class="warn">Changing mode, region or frequency reboots the device.</div>
</div>

<div class="page" id="p2" style="display:none">
<div class="card">
<canvas id="spec" width="640" height="220"></canvas>
<div class="warn">Sweeping pauses the radio link. It reconnects within a second after stop.</div>
<button class="btn" id="sw" onclick="spec()">Start sweep</button>
</div>
</div>

<div class="page" id="p4" style="display:none">
<div class="card">
<button class="btn gobig" onclick="qcmd('/go')">GO</button>
<div class="tgrid">
<button class="btn red" onclick="qcmd('/panic')">Panic</button>
<button class="btn ghost" onclick="qcmd('/pause')">Pause</button>
<button class="btn ghost" onclick="qcmd('/resume')">Resume</button>
<button class="btn ghost" onclick="qcmd('/stop')">Stop</button>
<button class="btn ghost" onclick="qcmd('/playhead/previous')">&#9650; Prev</button>
<button class="btn ghost" onclick="qcmd('/playhead/next')">&#9660; Next</button>
<button class="btn ghost" onclick="qcmd('/load')">Load</button>
<button class="btn ghost" onclick="qcmd('/reset')">Reset</button>
</div>
</div>
<div class="card">
<h3>Cue list</h3>
<div id="qcues" class="warn">Press Refresh to load the workspace.</div>
<button class="btn ghost" onclick="qcues()">Refresh</button>
</div>
<div class="card">
<h3>Active cues</h3>
<div id="qactive">&mdash;</div>
</div>
<div class="card">
<h3>Connection</h3>
<div class="warn">Works in OSC mode: the board relays commands to the QLab target configured in Settings. If the workspace has a passcode, connect first.</div>
<div class="row"><div><input id="qpass" placeholder="passcode"></div>
<div><button class="btn ghost" style="margin-top:0" onclick="qconnect()">Connect</button></div></div>
</div>
</div>

<div class="page" id="p3" style="display:none">
<div class="card">
<label>Online update</label>
<div id="upline" class="warn">Needs internet through the venue WiFi.</div>
<button class="btn ghost" onclick="checkUpd()">Check for updates</button>
<button class="btn" id="instBtn" style="display:none" onclick="installUpd()">Install update</button>
</div>
<div class="card">
<label>Manual firmware file (.bin)</label><input type="file" id="fw">
<button class="btn" onclick="ota()">Upload &amp; flash</button>
<div class="warn" id="otamsg"></div>
<button class="btn ghost" onclick="act('reboot')">Reboot device</button>
</div>
</div>

<script>
let specOn=false,cfg=null;
function tab(i,b){document.querySelectorAll('.page').forEach((p,j)=>p.style.display=i==j?'':'none');
document.querySelectorAll('.tabs button').forEach(x=>x.classList.remove('on'));b.classList.add('on');}
function toast(t){let m=document.getElementById('msg');m.textContent=t;m.style.display='block';setTimeout(()=>m.style.display='none',2500);}
async function act(a){await fetch('/api/'+a,{method:'POST'});toast(a.toUpperCase()+' sent');}
function kv(k,v){return '<div><span>'+k+'</span>'+v+'</div>';}
function show(id,on){document.getElementById(id).style.display=on?'':'none';}
function vis(){let m=+document.getElementById('mode').value,o=+document.getElementById('out').value;
show('b_out',m==3);
show('b_radio',m==2||m==3);
show('b_keys',m==1||(m==3&&o==0));
show('b_wifi',m==0||(m==3&&o==1));
show('b_osc',m==0||(m==3&&o==1));}
async function poll(){try{
let s=await(await fetch('/api/status')).json();
document.getElementById('devline').textContent=s.name+' · '+s.ver;
let lora=s.mode==2||s.mode==3, bleOut=s.mode==1||(s.mode==3&&s.out==0), oscOut=s.mode==0||(s.mode==3&&s.out==1);
let h=kv('Mode',s.modeName);
if(lora)h+=kv('Link',s.link?'<b class="ok">OK</b>':'<b class="bad">no link</b>')
+kv('Region / freq',s.region+' '+s.freq.toFixed(2)+' MHz'+(s.auto?' (auto)':''))
+kv('Peer',s.peer||'—')+kv('RSSI / SNR',s.rssi+' dBm / '+s.snr.toFixed(1))
+kv('TX / RX packets',s.tx+' / '+s.rx);
if(bleOut)h+=kv('BLE',s.ble?'<b class="ok">connected</b>':'<b class="bad">waiting</b>')
+kv('GO / PANIC keys',s.goKey+' / '+s.panicKey);
if(oscOut)h+=kv('WiFi',s.wifi?('<b class="ok">'+s.ip+'</b>'):'<b class="bad">no network</b>')
+kv('OSC target',s.osc);
h+=kv('Battery',s.batt>=0?s.batt+'%':'USB')+kv('Device ID',s.id);
document.getElementById('stat').innerHTML=h;}catch(e){}}
async function loadCfg(){cfg=await(await fetch('/api/config')).json();
let r=document.getElementById('region');r.innerHTML=cfg.regions.map((n,i)=>'<option value="'+i+'">'+n+'</option>').join('');
r.value=cfg.region;fillChan();
let ks=cfg.keys.map(k=>'<option value="'+k.c+'">'+k.n+'</option>').join('');
document.getElementById('gokey').innerHTML=ks;document.getElementById('pankey').innerHTML=ks;
for(let[i,v]of[['mode',cfg.mode],['out',cfg.out],['gokey',cfg.goKey],['pankey',cfg.panicKey],
['oscIp',cfg.oscIp],['oscPort',cfg.oscPort],['goAddr',cfg.go],['panAddr',cfg.panic],['ssid',cfg.ssid]])
document.getElementById(i).value=v;
vis();}
let updUrl='';
async function checkUpd(){document.getElementById('upline').textContent='Checking…';
try{let r=await(await fetch('/api/otacheck')).json();
if(r.err){document.getElementById('upline').textContent=r.err;return;}
if(r.latest&&r.latest!=r.cur){updUrl=r.url;
document.getElementById('upline').innerHTML='<b class="ok">Update available: '+r.latest+'</b> (installed '+r.cur+')';
document.getElementById('instBtn').style.display='';
toast('Update available: '+r.latest);}
else document.getElementById('upline').textContent='Up to date ('+r.cur+')';}
catch(e){document.getElementById('upline').textContent='Check failed';}}
async function installUpd(){if(!updUrl)return;
document.getElementById('upline').textContent='Downloading & flashing… do not power off';
let f=new FormData();f.append('url',updUrl);
try{let r=await(await fetch('/api/otainstall',{method:'POST',body:f})).json();
document.getElementById('upline').textContent=r.ok?'Done — rebooting':'FAILED: '+(r.err||'');}
catch(e){document.getElementById('upline').textContent='Device rebooting…';}}
setTimeout(checkUpd,1500);
function fillChan(){let c=document.getElementById('chan');
c.innerHTML='<option value="auto">Auto (RX picks, TX follows)</option>'+
cfg.channels.map((f,i)=>'<option value="'+i+'">'+f.toFixed(2)+' MHz</option>').join('');
c.value=cfg.freqAuto?'auto':cfg.chan;}
document.getElementById('region').addEventListener('change',fillChan);
async function saveCfg(){let f=new FormData();
for(let i of['mode','out','region','gokey','pankey','oscIp','oscPort','goAddr','panAddr'])
f.append(i,document.getElementById(i).value);
f.append('chan',document.getElementById('chan').value);
let ns=document.getElementById('ssid').value,np=document.getElementById('wpass').value;
if(ns!=cfg.ssid||np){f.append('ssid',ns);f.append('wpass',np);}
let r=await(await fetch('/api/config',{method:'POST',body:f})).json();
toast(r.reboot?'Saved — rebooting…':'Saved');}
async function spec(){specOn=!specOn;
document.getElementById('sw').textContent=specOn?'Stop sweep':'Start sweep';
await fetch('/api/spectrum?on='+(specOn?1:0),{method:'POST'});
if(specOn)drawLoop();}
async function drawLoop(){if(!specOn)return;
try{let s=await(await fetch('/api/spectrum')).json();
let c=document.getElementById('spec'),x=c.getContext('2d');
x.fillStyle='#05080e';x.fillRect(0,0,640,220);
x.fillStyle='#39d353';
for(let i=0;i<64;i++){let h=s.levels[i]*4.5;x.fillRect(i*10,200-h,8,h);}
x.strokeStyle='#f2c53d';x.setLineDash([4,4]);
let mx=(s.freq-s.from)/(s.to-s.from)*640;x.beginPath();x.moveTo(mx,10);x.lineTo(mx,200);x.stroke();x.setLineDash([]);
x.fillStyle='#8fa1b8';x.font='12px monospace';
x.fillText(s.from.toFixed(1),4,215);x.fillText(s.to.toFixed(1),600,215);
x.fillText(s.freq.toFixed(2)+' MHz',Math.min(mx+4,540),22);}catch(e){}
setTimeout(drawLoop,400);}
async function qcmd(a){await fetch('/api/qlab/cmd?addr='+encodeURIComponent(a),{method:'POST'});
toast('QLab '+a);setTimeout(qactive,600);}
async function qconnect(){let p=document.getElementById('qpass').value;
await fetch('/api/qlab/cmd?addr=/connect&s='+encodeURIComponent(p),{method:'POST'});toast('connect sent');}
function walkCues(list,out,depth){for(let c of list){out.push({d:depth,n:c.number||'',m:c.listName||c.name||'',t:c.type||''});
if(c.cues)walkCues(c.cues,out,depth+1);}}
async function qcues(){let el=document.getElementById('qcues');el.textContent='Loading…';
try{let r=await(await fetch('/api/qlab/query?addr=/cueLists')).json();
if(r.err){el.textContent=r.err;return;}
let flat=[];walkCues(r.data||[],flat,0);
el.innerHTML=flat.map(c=>'<div class="cue" style="padding-left:'+(6+c.d*14)+'px">'+
'<span class="num">'+c.n+'</span><span class="nm">'+c.m+'</span>'+
(c.n?'<button onclick="qsel(\''+c.n+'\')">&#8982;</button><button onclick="qgo(\''+c.n+'\')">&#9654;</button>':'')+
'</div>').join('')||'empty';}catch(e){el.textContent='failed';}}
function qsel(n){qcmd('/playhead/'+n);}
function qgo(n){qcmd('/cue/'+n+'/start');}
async function qactive(){try{
let r=await(await fetch('/api/qlab/query?addr=/runningOrPausedCues')).json();
let el=document.getElementById('qactive');
if(r.err||!r.data){el.textContent='\u2014';return;}
el.innerHTML=r.data.map(c=>'<div class="cue"><span class="num">'+(c.number||'')+'</span><span class="nm">'+(c.listName||c.name||'')+'</span><span>'+(c.type||'')+'</span></div>').join('')||'\u2014';}catch(e){}}
setInterval(()=>{if(document.getElementById('p4').style.display!='none')qactive();},2500);
async function ota(){let f=document.getElementById('fw').files[0];if(!f)return toast('choose a .bin');
document.getElementById('otamsg').textContent='Uploading… do not power off';
let d=new FormData();d.append('fw',f);
let r=await fetch('/update',{method:'POST',body:d});
document.getElementById('otamsg').textContent=r.ok?'Done — rebooting':'FAILED';}
loadCfg();poll();setInterval(poll,2000);
</script></body></html>)rawliteral";
