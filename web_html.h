// GO-GO web UI: single embedded page (dark soundkorb style).
// Served from PROGMEM; talks to the JSON API in web_ui.cpp.
#pragma once

const char WEB_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GO-GO</title>
<link rel="stylesheet" href="/font.css">
<style>
:root{--bg:#0b0e12;--card:#12161c;--line:rgba(255,255,255,.09);--tx:#f4f2ef;--mut:#88929c;--acc:#5cb2a2;--ok:#8ec9a8;--red:#e0564f;--sel:#d4b896}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--tx);font:15px/1.5 -apple-system,'Segoe UI',Roboto,sans-serif;padding:16px;max-width:760px;margin:0 auto}
h1{font-family:'Unbounded',system-ui,sans-serif;font-weight:600;font-size:20px;letter-spacing:1px;margin:4px 0 2px;display:inline-block}
h1 b{color:var(--acc)}
.topline{display:flex;align-items:center;justify-content:space-between}
#langBtn{font-family:'Unbounded',system-ui,sans-serif;background:var(--card);border:1px solid var(--line);color:var(--tx);padding:6px 10px;border-radius:8px;cursor:pointer;font-size:11px;letter-spacing:.5px}
.sub{color:var(--mut);font-size:12px;margin-bottom:14px}
.tabs{display:grid;grid-template-columns:repeat(3,1fr);gap:16px 8px;margin-bottom:18px;max-width:320px}
.tabs button{display:flex;flex-direction:column;align-items:center;gap:6px;background:transparent;border:0;color:var(--mut);cursor:pointer;padding:0;font-family:inherit}
.tabs .ic{width:52px;height:52px;border-radius:13px;background:var(--card);border:1px solid var(--line);display:flex;align-items:center;justify-content:center}
.tabs .ic svg{width:24px;height:24px;stroke:var(--mut);fill:none;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round}
.tabs .ic b{font-family:'Unbounded',sans-serif;font-size:20px;font-weight:600;color:var(--mut)}
.tabs .lbl{font-size:11px;letter-spacing:.2px}
.tabs button.on{color:var(--tx)}
.tabs button.on .ic{border-color:var(--acc)}
.tabs button.on .ic svg{stroke:var(--acc)}
.tabs button.on .ic b{color:var(--acc)}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin-bottom:12px}
.kv{display:grid;grid-template-columns:1fr 1fr;gap:6px 14px}
.kv div{padding:4px 0;border-bottom:1px solid rgba(255,255,255,.06);font-size:14px}
.kv span{color:var(--mut);display:block;font-size:11px;text-transform:uppercase;letter-spacing:1px}
label{display:block;color:var(--mut);font-size:12px;text-transform:uppercase;letter-spacing:1px;margin:10px 0 4px}
input,select{width:100%;background:#0c1320;border:1px solid var(--line);color:var(--tx);padding:8px 10px;border-radius:8px;font-size:15px}
.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.btn{font-family:'Unbounded',system-ui,sans-serif;background:var(--acc);color:#0b0e12;border:0;padding:11px 18px;border-radius:10px;font-weight:500;font-size:13px;letter-spacing:.5px;cursor:pointer;margin-top:14px}
.btn.red{background:var(--red);color:#fff}
.btn.ghost{background:transparent;border:1px solid var(--line);color:var(--tx)}
.bigbtns{display:flex;gap:12px}
.bigbtns .btn{flex:1;padding:16px;font-size:20px;margin-top:0}
canvas{width:100%;background:#05080e;border-radius:8px;border:1px solid var(--line)}
.warn{color:var(--sel);font-size:13px;margin:8px 0}
h3{font-family:'Unbounded',system-ui,sans-serif;font-weight:400;font-size:11px;letter-spacing:2px;text-transform:uppercase;color:var(--acc);margin:0 0 4px}
.tgrid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-top:10px}
.tgrid .btn{margin:0;padding:12px 4px;font-size:14px}
.cue{display:flex;align-items:center;gap:8px;padding:7px 6px;border-bottom:1px solid rgba(255,255,255,.06);font-size:14px}
.cue .num{color:var(--sel);min-width:34px;font-family:monospace}
.cue .nm{flex:1}
.cue button{background:#0c1320;border:1px solid var(--line);color:var(--tx);border-radius:6px;padding:4px 10px;cursor:pointer}
.gobig{width:100%;padding:26px;font-size:34px;letter-spacing:4px}
.ok{color:var(--ok)} .bad{color:var(--red)}
#msg{position:fixed;top:10px;right:10px;background:var(--acc);color:#0b0e12;padding:8px 14px;border-radius:10px;display:none;font-weight:600}
</style></head><body>
<div class="topline"><h1>GO<b>-</b>GO</h1><button id="langBtn" onclick="toggleLang()">EN</button></div>
<div class="sub" id="devline">&nbsp;</div>
<div id="msg"></div>
<div class="tabs">
<button class="on" onclick="tab(0,this)"><span class="ic"><svg viewBox="0 0 24 24"><path d="M2 13h4l2-7 4 15 3-11 2 3h5"/></svg></span><span class="lbl" data-t="tabStatus">Статус</span></button>
<button onclick="tab(1,this)"><span class="ic"><svg viewBox="0 0 24 24"><line x1="4" y1="7" x2="20" y2="7"/><circle cx="14" cy="7" r="2.2" fill="var(--bg)"/><line x1="4" y1="12" x2="20" y2="12"/><circle cx="9" cy="12" r="2.2" fill="var(--bg)"/><line x1="4" y1="17" x2="20" y2="17"/><circle cx="16" cy="17" r="2.2" fill="var(--bg)"/></svg></span><span class="lbl" data-t="tabSettings">Настройки</span></button>
<button onclick="tab(2,this)"><span class="ic"><svg viewBox="0 0 24 24"><line x1="5" y1="19" x2="5" y2="11"/><line x1="10.3" y1="19" x2="10.3" y2="5"/><line x1="15.6" y1="19" x2="15.6" y2="13"/><line x1="20" y1="19" x2="20" y2="8"/></svg></span><span class="lbl" data-t="tabSpectrum">Спектр</span></button>
<button onclick="tab(4,this)"><span class="ic"><b>Q</b></span><span class="lbl">QLab</span></button>
<button onclick="tab(3,this)"><span class="ic"><svg viewBox="0 0 24 24"><rect x="7" y="7" width="10" height="10" rx="1.5"/><line x1="9" y1="2" x2="9" y2="7"/><line x1="15" y1="2" x2="15" y2="7"/><line x1="9" y1="17" x2="9" y2="22"/><line x1="15" y1="17" x2="15" y2="22"/><line x1="2" y1="9" x2="7" y2="9"/><line x1="2" y1="15" x2="7" y2="15"/><line x1="17" y1="9" x2="22" y2="9"/><line x1="17" y1="15" x2="22" y2="15"/></svg></span><span class="lbl" data-t="tabFirmware">Прошивка</span></button>
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
<h3 data-t="hMode">Режим</h3>
<label data-t="lOpMode">Режим работы</label><select id="mode" onchange="vis()">
<option value="0">OSC / WiFi</option><option value="1" data-t="oBle">BLE-клавиатура</option>
<option value="2" data-t="oLoraTx">LoRa TX (пульт)</option><option value="3" data-t="oLoraRx">LoRa RX (гейтвей)</option></select>
<div id="b_out"><label data-t="lGwOut">Выход гейтвея</label><select id="out" onchange="vis()">
<option value="0" data-t="oBle">BLE-клавиатура</option><option value="1">OSC / WiFi</option></select></div>
</div>
<div class="card" id="b_radio">
<h3 data-t="hRadio">Радио LoRa</h3>
<div class="row"><div><label data-t="lRegion">Регион</label><select id="region"></select></div>
<div><label data-t="lFreq">Частота</label><select id="chan"></select></div></div>
</div>
<div class="card" id="b_keys">
<h3 data-t="hKeys">Клавиши BLE</h3>
<div class="row">
<div><label data-t="lGoKey">Клавиша GO</label><select id="gokey"></select><button type="button" class="btn ghost" style="margin-top:6px;width:100%" onclick="captureKey('gokey',this)" data-t="bPressKey">Нажать клавишу&hellip;</button></div>
<div><label data-t="lPanKey">Клавиша PANIC</label><select id="pankey"></select><button type="button" class="btn ghost" style="margin-top:6px;width:100%" onclick="captureKey('pankey',this)" data-t="bPressKey">Нажать клавишу&hellip;</button></div>
</div>
<div class="warn" data-t="wPressKey">«Нажать клавишу» считывает следующую клавишу с клавиатуры этого компьютера — быстрее, чем выбирать из списка, и не ограничено пресетами.</div>
</div>
<div class="card" id="b_wifi">
<h3 data-t="hWifi">Сеть WiFi</h3>
<label data-t="lNetName">Имя сети</label><input id="ssid" placeholder="сеть площадки" data-tph="phNetName">
<label data-t="lPass">Пароль</label><input id="wpass" type="password" placeholder="оставить пустым, чтобы сохранить текущий" data-tph="phPass">
</div>
<div class="card" id="b_osc">
<h3 data-t="hOsc">Цель OSC</h3>
<div class="row"><div><label data-t="lTargetIp">IP цели</label><input id="oscIp"></div>
<div><label data-t="lPort">Порт</label><input id="oscPort" type="number"></div></div>
<div class="row"><div><label data-t="lGoAddr">Адрес GO</label><input id="goAddr"></div>
<div><label data-t="lPanAddr">Адрес PANIC</label><input id="panAddr"></div></div>
</div>
<button class="btn" onclick="saveCfg()" data-t="bSave">Сохранить</button>
<div class="warn" data-t="wReboot">Смена режима, региона или частоты перезагружает устройство.</div>
</div>

<div class="page" id="p2" style="display:none">
<div class="card">
<canvas id="spec" width="640" height="220"></canvas>
<div class="warn" data-t="wSweep">Свип ставит радиосвязь на паузу. Восстанавливается в течение секунды после остановки.</div>
<button class="btn" id="sw" onclick="spec()" data-t="bStartSweep">Начать свип</button>
</div>
</div>

<div class="page" id="p4" style="display:none">
<div class="card">
<h3 data-t="hConn">Подключение</h3>
<div class="warn"><span data-t="wQlab">Нужен режим OSC с целью, заданной в Настройках — цель:</span> <b id="qtarget">&mdash;</b>. <span data-t="wQlabPass">Если у воркспейса QLab есть код доступа, сначала подключитесь ниже.</span></div>
<div class="row"><div><input id="qpass" placeholder="код доступа (если есть)" data-tph="phPasscode"></div>
<div><button class="btn ghost" style="margin-top:0" onclick="qconnect()" data-t="bConnect">Подключить</button></div></div>
<button class="btn ghost" onclick="qtest()" data-t="bTestConn">Проверить связь</button>
<div id="qtestout" class="warn"></div>
</div>
<div class="card">
<button class="btn gobig" onclick="qcmd('/go')">GO</button>
<div class="tgrid">
<button class="btn red" onclick="qcmd('/panic')">Panic</button>
<button class="btn ghost" onclick="qcmd('/pause')" data-t="bPause">Пауза</button>
<button class="btn ghost" onclick="qcmd('/resume')" data-t="bResume">Продолжить</button>
<button class="btn ghost" onclick="qcmd('/stop')" data-t="bStop">Стоп</button>
<button class="btn ghost" onclick="qcmd('/playhead/previous')">&#9650; <span data-t="bPrev">Пред.</span></button>
<button class="btn ghost" onclick="qcmd('/playhead/next')">&#9660; <span data-t="bNext">След.</span></button>
<button class="btn ghost" onclick="qcmd('/load')" data-t="bLoad">Загрузить</button>
<button class="btn ghost" onclick="qcmd('/reset')" data-t="bReset">Сброс</button>
</div>
</div>
<div class="card">
<h3 data-t="hCueList">Список кью</h3>
<div id="qcues" class="warn" data-t="tPressRefresh">Нажмите «Обновить», чтобы загрузить воркспейс.</div>
<button class="btn ghost" onclick="qcues()" data-t="bRefresh">Обновить</button>
</div>
<div class="card">
<h3 data-t="hActiveCues">Активные кью</h3>
<div id="qactive">&mdash;</div>
</div>
</div>

<div class="page" id="p3" style="display:none">
<div class="card">
<label data-t="lOnlineUpd">Обновление онлайн</label>
<div id="upline" class="warn" data-t="wInternet">Нужен интернет через сеть площадки.</div>
<button class="btn ghost" onclick="checkUpd()" data-t="bCheckUpd">Проверить обновления</button>
<button class="btn" id="instBtn" style="display:none" onclick="installUpd()" data-t="bInstallUpd">Установить обновление</button>
</div>
<div class="card">
<label data-t="lManualFw">Файл прошивки вручную (.bin)</label><input type="file" id="fw">
<button class="btn" onclick="ota()" data-t="bUploadFlash">Загрузить и прошить</button>
<div class="warn" id="otamsg"></div>
<button class="btn ghost" onclick="act('reboot')" data-t="bRebootDev">Перезагрузить устройство</button>
</div>
</div>

<script>
let specOn=false,cfg=null;
// Two-language dictionary: [ru, en] pairs, keyed by the data-t attribute
// name used on static elements (applyLang() walks those) and referenced
// directly as T.key[lang] from JS for dynamically-built strings. Owner
// ask: OLED stays English for now (no Cyrillic font yet), but everything
// people actually type into on a computer - this panel, WiFi Setup's own
// fields - should be readable in either language via one toggle, not a
// silent mix.
const T={
tabStatus:['Статус','Status'],tabSettings:['Настройки','Settings'],tabSpectrum:['Спектр','Spectrum'],tabFirmware:['Прошивка','Firmware'],
hMode:['Режим','Mode'],lOpMode:['Режим работы','Operating mode'],oBle:['BLE-клавиатура','BLE keyboard'],
oLoraTx:['LoRa TX (пульт)','LoRa TX (remote)'],oLoraRx:['LoRa RX (гейтвей)','LoRa RX (gateway)'],lGwOut:['Выход гейтвея','Gateway output'],
hRadio:['Радио LoRa','LoRa radio'],lRegion:['Регион','Region'],lFreq:['Частота','Frequency'],
hKeys:['Клавиши BLE','Keyboard output'],lGoKey:['Клавиша GO','GO key'],lPanKey:['Клавиша PANIC','PANIC key'],
bPressKey:['Нажать клавишу…','Press a key…'],
wPressKey:['«Нажать клавишу» считывает следующую клавишу с клавиатуры этого компьютера — быстрее, чем выбирать из списка, и не ограничено пресетами.',
'"Press a key" reads the next key you press on this computer\'s own keyboard - quicker than picking from the list, and not limited to the presets.'],
hWifi:['Сеть WiFi','WiFi network'],lNetName:['Имя сети','Network name'],phNetName:['сеть площадки','venue WiFi'],
lPass:['Пароль','Password'],phPass:['оставить пустым, чтобы сохранить текущий','leave empty to keep current'],
hOsc:['Цель OSC','OSC target'],lTargetIp:['IP цели','Target IP'],lPort:['Порт','Port'],lGoAddr:['Адрес GO','GO address'],lPanAddr:['Адрес PANIC','PANIC address'],
bSave:['Сохранить','Save'],wReboot:['Смена режима, региона или частоты перезагружает устройство.','Changing mode, region or frequency reboots the device.'],
wSweep:['Свип ставит радиосвязь на паузу. Восстанавливается в течение секунды после остановки.','Sweeping pauses the radio link. It reconnects within a second after stop.'],
bStartSweep:['Начать свип','Start sweep'],bStopSweep:['Остановить свип','Stop sweep'],
hConn:['Подключение','Connection'],
wQlab:['Нужен режим OSC с целью, заданной в Настройках — цель:','Needs OSC mode with the target set in Settings — target:'],
wQlabPass:['Если у воркспейса QLab есть код доступа, сначала подключитесь ниже.','If the QLab workspace has a passcode, connect below first.'],
phPasscode:['код доступа (если есть)','passcode (if any)'],
bConnect:['Подключить','Connect'],bTestConn:['Проверить связь','Test connection'],
bPause:['Пауза','Pause'],bResume:['Продолжить','Resume'],bStop:['Стоп','Stop'],bPrev:['Пред.','Prev'],bNext:['След.','Next'],
bLoad:['Загрузить','Load'],bReset:['Сброс','Reset'],
hCueList:['Список кью','Cue list'],tPressRefresh:['Нажмите «Обновить», чтобы загрузить воркспейс.','Press Refresh to load the workspace.'],
bRefresh:['Обновить','Refresh'],hActiveCues:['Активные кью','Active cues'],
lOnlineUpd:['Обновление онлайн','Online update'],wInternet:['Нужен интернет через сеть площадки.','Needs internet through the venue WiFi.'],
bCheckUpd:['Проверить обновления','Check for updates'],bInstallUpd:['Установить обновление','Install update'],
lManualFw:['Файл прошивки вручную (.bin)','Manual firmware file (.bin)'],bUploadFlash:['Загрузить и прошить','Upload & flash'],
bRebootDev:['Перезагрузить устройство','Reboot device'],
mSent:['отправлено','sent'],
lModeK:['Режим','Mode'],lLink:['Связь','Link'],lNoLink:['нет связи','no link'],
lRegionFreq:['Регион / частота','Region / freq'],lAuto:[' (авто)',' (auto)'],
lPeer:['Партнёр','Peer'],lRssiSnr:['RSSI / SNR','RSSI / SNR'],lPackets:['Пакетов TX / RX','TX / RX packets'],
lConnected:['подключено','connected'],lWaiting:['ожидание','waiting'],lKeysGP:['Клавиши GO / PANIC','GO / PANIC keys'],
lNoNet:['нет сети','no network'],lOscTarget:['Цель OSC','OSC target'],lBattery:['Батарея','Battery'],lDeviceId:['ID устройства','Device ID'],
mChecking:['Проверка…','Checking…'],mUpdAvail:['Доступно обновление: ','Update available: '],mInstalled:[' (установлено ',' (installed '],
mUpToDate:['Установлена последняя версия (','Up to date ('],mCheckFail:['Проверка не удалась','Check failed'],
mFlashing:['Загрузка и прошивка… не выключайте питание','Downloading & flashing… do not power off'],
mDoneReboot:['Готово — перезагрузка','Done — rebooting'],mError:['ОШИБКА: ','FAILED: '],mRebootingDev:['Устройство перезагружается…','Device rebooting…'],
oAuto:['Авто (RX выбирает, TX следует)','Auto (RX picks, TX follows)'],
mSaved:['Сохранено — перезагрузка…','Saved — rebooting…'],mSavedOnly:['Сохранено','Saved'],
mKeyUnsupported:['Клавиша не поддерживается: ','Key not supported: '],mKeySet:[': клавиша задана — не забудьте Сохранить',' key set - remember to Save'],
mPressAnyKey:['Нажмите любую клавишу…','Press any key…'],
mTesting:['Проверка…','Testing…'],mNoReply:['Нет ответа: ','No reply: '],mCheckOsc:[' — проверьте IP/порт цели OSC (Настройки) и QLab → Settings → Network → OSC access.',
' - check OSC target IP/port (Settings) and QLab → Settings → Network → OSC access.'],
mQlabReplied:['QLab ответил','QLab replied'],mVersion:[' — версия ',' - version '],mReqFailed:['Запрос не удался','Request failed'],
mConnectSent:['connect отправлен','connect sent'],
mLoadingCues:['Загрузка…','Loading…'],mEmpty:['пусто','empty'],mFail:['ошибка','failed'],
mChooseBin:['выберите файл .bin','choose a .bin'],mUploading:['Загрузка… не выключайте питание','Uploading… do not power off'],
mDoneRebootShort:['Готово — перезагрузка','Done — rebooting'],mErrorShort:['ОШИБКА','FAILED'],
};
let lang=localStorage.getItem('gogoLang')||'ru';
function li(){return lang=='ru'?0:1;}
function t(k){return T[k]?T[k][li()]:k;}
function applyLang(){document.querySelectorAll('[data-t]').forEach(el=>el.textContent=t(el.dataset.t));
document.querySelectorAll('[data-tph]').forEach(el=>el.placeholder=t(el.dataset.tph));
document.getElementById('langBtn').textContent=lang=='ru'?'EN':'RU';
document.documentElement.lang=lang;
document.getElementById('sw').textContent=specOn?t('bStopSweep'):t('bStartSweep');}
function toggleLang(){lang=lang=='ru'?'en':'ru';localStorage.setItem('gogoLang',lang);applyLang();poll();}
applyLang();
// Browser KeyboardEvent.code -> USB HID usage code (same table the boards
// speak over BLE HID). Covers letters, digits, function keys and the usual
// navigation/editing keys - standard USB HID Usage Page 0x07 values.
const HID_CODE={Space:0x2C,Enter:0x28,Escape:0x29,Backspace:0x2A,Tab:0x2B,
CapsLock:0x39,PrintScreen:0x46,ScrollLock:0x47,Pause:0x48,Insert:0x49,
Home:0x4A,PageUp:0x4B,Delete:0x4C,End:0x4D,PageDown:0x4E,
ArrowRight:0x4F,ArrowLeft:0x50,ArrowDown:0x51,ArrowUp:0x52,
Minus:0x2D,Equal:0x2E,BracketLeft:0x2F,BracketRight:0x30,Backslash:0x31,
Semicolon:0x33,Quote:0x34,Backquote:0x35,Comma:0x36,Period:0x37,Slash:0x38};
for(let i=0;i<26;i++)HID_CODE['Key'+String.fromCharCode(65+i)]=0x04+i;
for(let i=1;i<=9;i++)HID_CODE['Digit'+i]=0x1E+(i-1);
HID_CODE.Digit0=0x27;
for(let i=1;i<=12;i++)HID_CODE['F'+i]=0x3A+(i-1);
function captureKey(selectId,btn){
let sel=document.getElementById(selectId);
let old=btn.textContent;btn.textContent=t('mPressAnyKey');btn.disabled=true;
function onKey(e){
e.preventDefault();e.stopPropagation();
window.removeEventListener('keydown',onKey,true);
btn.textContent=old;btn.disabled=false;
let code=HID_CODE[e.code];
if(code===undefined){toast(t('mKeyUnsupported')+e.code);return;}
if(!sel.querySelector('option[value="'+code+'"]'))
sel.insertAdjacentHTML('beforeend','<option value="'+code+'">'+(e.key.length==1?e.key.toUpperCase():e.code)+'</option>');
sel.value=code;
toast((selectId=='gokey'?'GO':'PANIC')+t('mKeySet'));
}
window.addEventListener('keydown',onKey,true);
}
function tab(i,b){document.querySelectorAll('.page').forEach(p=>p.style.display=(p.id=='p'+i)?'':'none');
document.querySelectorAll('.tabs button').forEach(x=>x.classList.remove('on'));b.classList.add('on');}
function toast(txt){let m=document.getElementById('msg');m.textContent=txt;m.style.display='block';setTimeout(()=>m.style.display='none',2500);}
async function act(a){await fetch('/api/'+a,{method:'POST'});toast(a.toUpperCase()+' '+t('mSent'));}
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
let h=kv(t('lModeK'),s.modeName);
if(lora)h+=kv(t('lLink'),s.link?'<b class="ok">OK</b>':'<b class="bad">'+t('lNoLink')+'</b>')
+kv(t('lRegionFreq'),s.region+' '+s.freq.toFixed(2)+' MHz'+(s.auto?t('lAuto'):''))
+kv(t('lPeer'),s.peer||'—')+kv(t('lRssiSnr'),s.rssi+' dBm / '+s.snr.toFixed(1))
+kv(t('lPackets'),s.tx+' / '+s.rx);
if(bleOut)h+=kv('BLE',s.ble?'<b class="ok">'+t('lConnected')+'</b>':'<b class="bad">'+t('lWaiting')+'</b>')
+kv(t('lKeysGP'),s.goKey+' / '+s.panicKey);
if(oscOut)h+=kv('WiFi',s.wifi?('<b class="ok">'+s.ip+'</b>'):'<b class="bad">'+t('lNoNet')+'</b>')
+kv(t('lOscTarget'),s.osc);
h+=kv(t('lBattery'),s.batt>=0?s.batt+'%':'USB')+kv(t('lDeviceId'),s.id);
document.getElementById('stat').innerHTML=h;
let qt=document.getElementById('qtarget');if(qt)qt.textContent=s.osc;}catch(e){}}
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
async function checkUpd(){document.getElementById('upline').textContent=t('mChecking');
try{let r=await(await fetch('/api/otacheck')).json();
if(r.err){document.getElementById('upline').textContent=r.err;return;}
if(r.latest&&r.latest!=r.cur){updUrl=r.url;
document.getElementById('upline').innerHTML='<b class="ok">'+t('mUpdAvail')+r.latest+'</b>'+t('mInstalled')+r.cur+')';
document.getElementById('instBtn').style.display='';
toast(t('mUpdAvail')+r.latest);}
else document.getElementById('upline').textContent=t('mUpToDate')+r.cur+')';}
catch(e){document.getElementById('upline').textContent=t('mCheckFail');}}
async function installUpd(){if(!updUrl)return;
document.getElementById('upline').textContent=t('mFlashing');
let f=new FormData();f.append('url',updUrl);
try{let r=await(await fetch('/api/otainstall',{method:'POST',body:f})).json();
document.getElementById('upline').textContent=r.ok?t('mDoneReboot'):t('mError')+(r.err||'');}
catch(e){document.getElementById('upline').textContent=t('mRebootingDev');}}
setTimeout(checkUpd,1500);
function fillChan(){let c=document.getElementById('chan');
c.innerHTML='<option value="auto">'+t('oAuto')+'</option>'+
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
toast(r.reboot?t('mSaved'):t('mSavedOnly'));}
async function spec(){specOn=!specOn;
document.getElementById('sw').textContent=specOn?t('bStopSweep'):t('bStartSweep');
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
toast('QLab '+a);}
async function qtest(){let o=document.getElementById('qtestout');o.textContent=t('mTesting');
try{let r=await(await fetch('/api/qlab/query?addr=/version')).json();
if(r.err)o.textContent=t('mNoReply')+r.err+t('mCheckOsc');
else o.innerHTML='<b class="ok">'+t('mQlabReplied')+'</b>'+t('mVersion')+(r.data||'ok');}
catch(e){o.textContent=t('mReqFailed');}}
async function qconnect(){let p=document.getElementById('qpass').value;
await fetch('/api/qlab/cmd?addr=/connect&s='+encodeURIComponent(p),{method:'POST'});toast(t('mConnectSent'));}
function walkCues(list,out,depth){for(let c of list){out.push({d:depth,n:c.number||'',m:c.listName||c.name||'',t:c.type||''});
if(c.cues)walkCues(c.cues,out,depth+1);}}
async function qcues(){let el=document.getElementById('qcues');el.textContent=t('mLoadingCues');
try{let r=await(await fetch('/api/qlab/query?addr=/cueLists')).json();
if(r.err){el.textContent=r.err;return;}
let flat=[];walkCues(r.data||[],flat,0);
el.innerHTML=flat.map(c=>'<div class="cue" style="padding-left:'+(6+c.d*14)+'px">'+
'<span class="num">'+c.n+'</span><span class="nm">'+c.m+'</span>'+
(c.n?'<button onclick="qsel(\''+c.n+'\')">&#8982;</button><button onclick="qgo(\''+c.n+'\')">&#9654;</button>':'')+
'</div>').join('')||t('mEmpty');}catch(e){el.textContent=t('mFail');}}
function qsel(n){qcmd('/playhead/'+n);}
function qgo(n){qcmd('/cue/'+n+'/start');}
async function qactive(){try{
let r=await(await fetch('/api/qlab/query?addr=/runningOrPausedCues')).json();
let el=document.getElementById('qactive');
if(r.err||!r.data){el.textContent='—';return;}
el.innerHTML=r.data.map(c=>'<div class="cue"><span class="num">'+(c.number||'')+'</span><span class="nm">'+(c.listName||c.name||'')+'</span><span>'+(c.type||'')+'</span></div>').join('')||'—';}catch(e){}}
setInterval(()=>{if(document.getElementById('p4').style.display!='none')qactive();},2500);
async function ota(){let f=document.getElementById('fw').files[0];if(!f)return toast(t('mChooseBin'));
document.getElementById('otamsg').textContent=t('mUploading');
let d=new FormData();d.append('fw',f);
let r=await fetch('/update',{method:'POST',body:d});
document.getElementById('otamsg').textContent=r.ok?t('mDoneRebootShort'):t('mErrorShort');}
loadCfg();poll();setInterval(poll,2000);
</script></body></html>)rawliteral";
