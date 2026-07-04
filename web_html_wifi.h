// GO-GO WiFi onboarding page: scan -> tap a network -> password -> connect.
// One flow, one primary button. Served on the device AP during WiFi Setup;
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
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:6px 14px;margin-bottom:12px}
.net{display:flex;align-items:center;gap:12px;padding:13px 2px;border-bottom:1px solid rgba(255,255,255,.06);cursor:pointer;font-size:16px}
.net:last-child{border-bottom:0}
.net:active{opacity:.7}
.net .nm{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.net svg{flex:none}
.net .lk{color:var(--mut);font-size:13px;flex:none}
.joinbox{padding:4px 2px 14px;border-bottom:1px solid rgba(255,255,255,.06)}
.joinbox input{width:100%;background:#0c1320;border:1px solid var(--line);color:var(--tx);padding:12px;border-radius:10px;font-size:16px;margin-top:8px}
.btn{font-family:'Unbounded',system-ui,sans-serif;background:var(--acc);color:#0b0e12;border:0;padding:13px 18px;border-radius:10px;font-weight:500;font-size:14px;letter-spacing:.5px;cursor:pointer;margin-top:12px;width:100%}
.showp{display:flex;align-items:center;gap:8px;margin-top:10px;color:var(--mut);font-size:14px}
.showp input{width:auto;margin:0}
.links{display:flex;justify-content:space-between;padding:12px 2px;color:var(--acc);font-size:14px}
.links span{cursor:pointer}
label{display:block;color:var(--mut);font-size:11px;text-transform:uppercase;letter-spacing:1px;margin:10px 0 4px}
.card input{width:100%;background:#0c1320;border:1px solid var(--line);color:var(--tx);padding:10px;border-radius:8px;font-size:15px}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:10px}
#st{margin-top:12px;font-size:14px;color:var(--sel)}
.ok{color:var(--ok)} .bad{color:var(--red)}
.spin{display:inline-block;width:14px;height:14px;border:2px solid var(--mut);border-top-color:var(--acc);border-radius:50%;animation:sp 1s linear infinite;vertical-align:-2px;margin-right:8px}
@keyframes sp{to{transform:rotate(360deg)}}
a{color:var(--acc)}
</style></head><body>
<h1>GO<b>-</b>GO</h1>
<div class="sub">Подключение к WiFi &middot; connect the device to your network</div>

<div class="card">
<div id="nets"><div class="net" style="cursor:default"><span class="spin"></span><span class="nm sub" style="margin:0">Ищу сети&hellip;</span></div></div>
<div class="links"><span onclick="scan(1)">&#8635; Обновить список</span><span onclick="manual()">Ввести имя вручную</span></div>
</div>

<div class="card">
<h3 style="font-family:'Unbounded',sans-serif;font-weight:400;font-size:11px;letter-spacing:2px;text-transform:uppercase;color:var(--acc);margin:10px 0 4px">QLab / OSC (необязательно)</h3>
<div class="row2"><div><label>IP компьютера</label><input id="oscIp" placeholder="192.168.1.10"></div>
<div><label>Порт</label><input id="oscPort" type="number" placeholder="53000"></div></div>
<div class="row2"><div><label>Адрес GO</label><input id="goAddr" placeholder="/go"></div>
<div><label>Адрес PANIC</label><input id="panAddr" placeholder="/panic"></div></div>
<div class="sub" style="margin-top:8px">Можно заполнить сразу или позже в панели — сохранится вместе с подключением к сети.</div>
</div>

<script>
let timer=null,curSsid='';
function wicon(r){let l=r>-55?3:r>-67?2:r>-78?1:0;
let a=(i)=>'stroke-opacity="'+(l>=i?1:.25)+'"';
return '<svg width="22" height="18" viewBox="0 0 24 20" fill="none" stroke="#5cb2a2" stroke-width="2" stroke-linecap="round">'+
'<path d="M2 7a15 15 0 0 1 20 0" '+a(3)+'/>'+
'<path d="M5.5 10.5a10 10 0 0 1 13 0" '+a(2)+'/>'+
'<path d="M9 14a5 5 0 0 1 6 0" '+a(1)+'/>'+
'<circle cx="12" cy="17.4" r="1.6" fill="#5cb2a2" stroke="none"/></svg>';}
function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;');}
async function scan(force){if(force){document.getElementById('nets').innerHTML=
'<div class="net" style="cursor:default"><span class="spin"></span><span class="nm sub" style="margin:0">Ищу сети&hellip;</span></div>';
await fetch('/api/scan?restart=1');}
try{let r=await(await fetch('/api/scan')).json();
if(r.nets){r.nets.sort((a,b)=>b.rssi-a.rssi);
document.getElementById('nets').innerHTML=r.nets.map((n,i)=>
'<div class="net" onclick="pick('+i+')" data-ssid="'+esc(n.ssid)+'">'+wicon(n.rssi)+
'<span class="nm">'+esc(n.ssid)+'</span>'+(n.enc?'<span class="lk">&#128274;</span>':'')+
'</div>').join('')||'<div class="sub" style="padding:10px 0">Сети не найдены — обновите список.</div>';
}else setTimeout(()=>scan(0),1000);}catch(e){setTimeout(()=>scan(0),1500);}}
function joinFormHtml(ssid){return '<div class="joinbox" id="jb">'+
'<b>'+esc(ssid)+'</b>'+
'<input id="pass" type="password" placeholder="пароль сети" autofocus>'+
'<div class="showp"><input type="checkbox" onchange="pass.type=this.checked?\'text\':\'password\'"><span>показать пароль</span></div>'+
'<button class="btn" onclick="join()">Подключить</button>'+
'<div id="st"></div></div>';}
function pick(i){let rows=[...document.querySelectorAll('.net')];let el=rows[i];
curSsid=el.dataset.ssid;closeForm();
el.insertAdjacentHTML('afterend',joinFormHtml(curSsid));
document.getElementById('pass').focus();}
function manual(){closeForm();curSsid='';
document.getElementById('nets').insertAdjacentHTML('beforeend',
'<div class="joinbox" id="jb"><input id="mssid" placeholder="имя сети (SSID)">'+
'<input id="pass" type="password" placeholder="пароль сети">'+
'<div class="showp"><input type="checkbox" onchange="pass.type=this.checked?\'text\':\'password\'"><span>показать пароль</span></div>'+
'<button class="btn" onclick="join()">Подключить</button>'+
'<div id="st"></div></div>');}
function closeForm(){let o=document.getElementById('jb');if(o)o.remove();}
async function join(){let ssid=curSsid||((document.getElementById('mssid')||{}).value||'');
if(!ssid)return;
let f=new FormData();f.append('ssid',ssid);
f.append('pass',document.getElementById('pass').value);
for(let i of['oscIp','oscPort','goAddr','panAddr']){let el=document.getElementById(i);if(el&&el.value)f.append(i,el.value);}
document.getElementById('st').innerHTML='<span class="spin"></span>Подключаюсь&hellip; радио занято, страница может замереть на ~15 с.';
await fetch('/api/join',{method:'POST',body:f});
clearInterval(timer);timer=setInterval(poll,1500);}
async function poll(){try{
let r=await(await fetch('/api/joinstatus')).json();
let st=document.getElementById('st');if(!st)return;
if(r.state=='connecting')st.innerHTML='<span class="spin"></span>Подключаюсь&hellip; попытка '+r.try+'/3';
if(r.state=='ok'){clearInterval(timer);
st.innerHTML='<b class="ok">Подключено!</b> Панель управления:<br>'+
'<a href="http://gogo.local">http://gogo.local</a> или <b>http://'+r.ip+'</b>'+
'<br>Плата перезагрузится сама — вернитесь в свою сеть и откройте адрес.';}
else if(r.state=='fail'){clearInterval(timer);
st.innerHTML='<b class="bad">Не удалось</b> — '+(r.reason||'проверьте пароль и попробуйте ещё раз.');}
}catch(e){}}
async function loadOsc(){try{let c=await(await fetch('/api/config')).json();
for(let[i,v]of[['oscIp',c.oscIp],['oscPort',c.oscPort],['goAddr',c.go],['panAddr',c.panic]])
{let el=document.getElementById(i);if(el&&!el.value)el.value=v;}}catch(e){}}
loadOsc();scan(0);
</script></body></html>)rawliteral";
