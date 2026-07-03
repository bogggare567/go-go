// GO-GO WiFi onboarding page: scan nearby networks, join one, report the
// board's new LAN address. Served on the device AP during WiFi Setup;
// the full control panel (web_html.h) lives on the venue LAN afterwards.
#pragma once

const char WIFI_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GO-GO WiFi</title>
<link rel="stylesheet" href="/font.css">
<style>
:root{--bg:#0b0e12;--card:#12161c;--line:rgba(255,255,255,.09);--tx:#f4f2ef;--mut:#88929c;--acc:#5cb2a2;--ok:#8ec9a8;--sel:#d4b896;--red:#e0564f}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--tx);font:15px/1.5 -apple-system,'Segoe UI',Roboto,sans-serif;padding:16px;max-width:480px;margin:0 auto}
h1{font-family:'Unbounded',system-ui,sans-serif;font-weight:600;font-size:20px;letter-spacing:1px;margin:4px 0 2px}
h1 b{color:var(--acc)}
.sub{color:var(--mut);font-size:12px;margin-bottom:14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin-bottom:12px}
h3{font-family:'Unbounded',system-ui,sans-serif;font-weight:400;font-size:11px;letter-spacing:2px;text-transform:uppercase;color:var(--acc);margin:0 0 8px}
label{display:block;color:var(--mut);font-size:12px;text-transform:uppercase;letter-spacing:1px;margin:10px 0 4px}
input{width:100%;background:#0c1320;border:1px solid var(--line);color:var(--tx);padding:10px;border-radius:8px;font-size:16px}
.btn{font-family:'Unbounded',system-ui,sans-serif;background:var(--acc);color:#0b0e12;border:0;padding:13px 18px;border-radius:10px;font-weight:500;font-size:14px;letter-spacing:.5px;cursor:pointer;margin-top:14px;width:100%}
.btn.ghost{background:transparent;border:1px solid var(--line);color:var(--tx)}
.net{display:flex;align-items:center;gap:10px;padding:10px 6px;border-bottom:1px solid #1e2735;cursor:pointer;font-size:15px}
.net:active{background:#1e2735}
.net .nm{flex:1}
.net .rs{color:var(--mut);font-size:12px}
.net .lk{color:var(--sel)}
.showp{display:flex;align-items:center;gap:8px;margin-top:8px;color:var(--mut);font-size:14px}
.showp input{width:auto}
#st{margin-top:12px;font-size:14px;color:var(--sel)}
.ok{color:var(--ok)} .bad{color:var(--red)}
</style></head><body>
<h1>GO<b>-</b>GO</h1>
<div class="sub">WiFi Setup — connect the device to your network</div>

<div class="card">
<h3>Nearby networks</h3>
<div id="nets" class="sub">Scanning&hellip;</div>
<button class="btn ghost" onclick="scan(1)">Rescan</button>
</div>

<div class="card" id="osccard">
<h3>QLab / OSC target (optional)</h3>
<label>Target IP (your computer)</label><input id="oscIp">
<label>Port</label><input id="oscPort" type="number">
<label>GO address</label><input id="goAddr">
<label>PANIC address</label><input id="panAddr">
<button class="btn ghost" onclick="saveOsc()">Save OSC settings</button>
<div id="oscst" class="sub"></div>
</div>

<div class="card">
<h3>Join</h3>
<label>Network</label><input id="ssid" placeholder="network name">
<label>Password</label><input id="pass" type="password">
<div class="showp"><input type="checkbox" id="showp" onchange="pass.type=this.checked?'text':'password'"><span>show password</span></div>
<button class="btn" onclick="join()">Connect</button>
<div id="st"></div>
</div>

<script>
let timer=null;
function bars(r){return r>-55?'&#9608;&#9608;&#9608;&#9608;':r>-67?'&#9608;&#9608;&#9608;&#9617;':r>-78?'&#9608;&#9608;&#9617;&#9617;':'&#9608;&#9617;&#9617;&#9617;';}
async function scan(force){if(force)await fetch('/api/scan?restart=1');
try{let r=await(await fetch('/api/scan')).json();
if(r.nets){document.getElementById('nets').innerHTML=r.nets.map(n=>
'<div class="net" onclick="pick(\''+n.ssid.replace(/'/g,"\\'")+'\')">'+
'<span class="nm">'+n.ssid+'</span>'+(n.enc?'<span class="lk">&#128274;</span>':'')+
'<span class="rs">'+bars(n.rssi)+'</span></div>').join('')||'nothing found';
}else setTimeout(()=>scan(0),1000);}catch(e){setTimeout(()=>scan(0),1500);}}
function pick(s){document.getElementById('ssid').value=s;document.getElementById('pass').focus();}
async function join(){let f=new FormData();
f.append('ssid',document.getElementById('ssid').value);
f.append('pass',document.getElementById('pass').value);
document.getElementById('st').textContent='Connecting… the device radio is busy, this page may pause for ~15 s.';
await fetch('/api/join',{method:'POST',body:f});
clearInterval(timer);timer=setInterval(poll,1500);}
async function poll(){try{
let r=await(await fetch('/api/joinstatus')).json();
if(r.state=='connecting')document.getElementById('st').textContent='Connecting… attempt '+r.try+'/3 (page may pause while the radio works)';
if(r.state=='ok'){clearInterval(timer);
document.getElementById('st').innerHTML='<b class="ok">Connected!</b> Control panel: <b>http://'+r.ip+
'</b><br>The device reboots by itself in a few seconds — rejoin your network and open that address.';}
else if(r.state=='fail'){clearInterval(timer);
document.getElementById('st').innerHTML='<b class="bad">Failed</b> — '+(r.reason||'check the password and try again.');}
}catch(e){}}
async function loadOsc(){try{let c=await(await fetch('/api/config')).json();
for(let[i,v]of[['oscIp',c.oscIp],['oscPort',c.oscPort],['goAddr',c.go],['panAddr',c.panic]])
document.getElementById(i).value=v;}catch(e){}}
async function saveOsc(){let f=new FormData();
for(let i of['oscIp','oscPort','goAddr','panAddr'])f.append(i,document.getElementById(i).value);
await fetch('/api/config',{method:'POST',body:f});
document.getElementById('oscst').textContent='Saved.';}
loadOsc();scan(0);
</script></body></html>)rawliteral";
