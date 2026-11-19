// SPDX-License-Identifier: GPL-3.0-only
// Manual browser check of site/editor.html over the Chrome DevTools protocol (no packages): node tools/editor_browser_check.mjs CHROME_BINARY /abs/path/site/editor.html out.png
// Types a phrase, checks the syllables (EN and JP), renders a preview and measures its level, collects console errors.
import { spawn } from 'node:child_process';
import { writeFileSync } from 'node:fs';
const [,, chrome, page, out] = process.argv;
const proc = spawn(chrome, ['--headless', '--no-sandbox', '--remote-debugging-port=9333', '--autoplay-policy=no-user-gesture-required', '--window-size=1280,1600', 'about:blank'], { stdio: ['ignore', 'ignore', 'pipe'] });
const sleep = ms => new Promise(r => setTimeout(r, ms));
let list; for (let i = 0; i < 50; i++) { try { list = await (await fetch('http://127.0.0.1:9333/json')).json(); if (list.length) break; } catch {} await sleep(200); }
const ws = new WebSocket(list.find(t => t.type === 'page').webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const pending = new Map(); const logs = [];
ws.onmessage = ev => { const m = JSON.parse(ev.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
  if (m.method === 'Runtime.consoleAPICalled') logs.push(m.params.type + ': ' + m.params.args.map(a => a.value ?? a.description).join(' '));
  if (m.method === 'Runtime.exceptionThrown') logs.push('EXCEPTION: ' + (m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text)); };
const send = (method, params = {}) => new Promise(r => { const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async expr => (await send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true })).result?.result?.value;
await send('Runtime.enable'); await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 1700, deviceScaleFactor: 1, mobile: false });
await send('Page.navigate', { url: 'file://' + page });
for (let i = 0; i < 60; i++) { await sleep(250); const t = await ev("document.getElementById('ready')?.textContent||''"); if (t && !/loading/i.test(t)) break; }
const r = {};
r.ready = await ev("document.getElementById('ready').textContent");
await ev("document.querySelector('#slots button, #slots [role=option], #slots li, #slots > *')?.click()");
await sleep(300);
await ev(`(()=>{const w=document.getElementById('words');w.value='Hello world, Miku sings for you';w.dispatchEvent(new Event('input',{bubbles:true}));})()`);
await sleep(800);
r.syllables = await ev("[...document.querySelectorAll('#syllables li')].map(l=>l.textContent.trim().replace(/\\s+/g,' ')).join(' | ')");
r.limits = await ev("document.getElementById('limits').textContent");
r.compileError = await ev("(e=>e.hidden?'':e.textContent)(document.getElementById('compileError'))");
await ev(`(()=>{window.__pk=[];const o=AudioBuffer.prototype.copyToChannel;AudioBuffer.prototype.copyToChannel=function(d,c,s){let p=0,e=0;for(const x of d){const a=Math.abs(x);if(a>p)p=a;e+=x*x;}window.__pk.push({len:d.length,peak:p,rms:Math.sqrt(e/d.length)});return o.call(this,d,c,s)};const g=AudioBuffer.prototype.getChannelData;})()`); await ev("document.getElementById('play').click()"); await sleep(4000); r.preview = await ev("JSON.stringify(window.__pk)");
r.editStatus = await ev("document.getElementById('editStatus').textContent");
r.audioCtx = await ev("typeof AudioContext");
await ev(`(()=>{document.getElementById('language').value='1';document.getElementById('language').dispatchEvent(new Event('change',{bubbles:true}));const w=document.getElementById('words');w.value='sa-ku-ra sa-ku-ra';w.dispatchEvent(new Event('input',{bubbles:true}));})()`);
await sleep(600);
r.jp = await ev("[...document.querySelectorAll('#syllables li')].map(l=>l.textContent.trim().replace(/\\s+/g,' ')).join(' | ')");
r.jpError = await ev("(e=>e.hidden?'':e.textContent)(document.getElementById('compileError'))");
await ev(`(()=>{document.getElementById('language').value='0';document.getElementById('language').dispatchEvent(new Event('change',{bubbles:true}));const w=document.getElementById('words');w.value='Hello world, Miku sings for you';w.dispatchEvent(new Event('input',{bubbles:true}));})()`);
await sleep(600);
const shot = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: true });
writeFileSync(out, Buffer.from(shot.result.data, 'base64'));
r.logs = logs;
console.log(JSON.stringify(r, null, 1));
proc.kill(); process.exit(0);
