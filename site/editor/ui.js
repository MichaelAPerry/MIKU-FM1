// SPDX-License-Identifier: GPL-3.0-only
(() => {
  "use strict";
  const $ = id => document.getElementById(id);
  const clamp = (x, min, max) => Math.max(min, Math.min(max, x));
  const number = (id, fallback) => $(id).value === "" ? fallback : Number($(id).value);
  const tr = (en, ja) => lang === "ja" ? ja : en;
  const statuses = new Map();
  const staticLabels = [...document.querySelectorAll("[data-ja]")].map(el => [el, el.textContent]);
  let lang = navigator.language?.toLowerCase().startsWith("ja") ? "ja" : "en";
  let api, bank = Array(64).fill(null), slot = 0, phrase, valid = false, midiFile;
  let overrides = Object.create(null), audio, playing, previewId = 0;
  const storageKey = "miku-fm-phrase-bank-v1";
  const empty = () => ({name: " ".repeat(12), text: "", lang: 0, bpm: 120, deflen: 4, flags: 0, n: 0, s: []});
  const same = (a, b) => a === b || !!a && !!b && a.every((value, i) => value === b[i]);

  function say(id, en = "", ja = en, error = false) {
    statuses.set(id, [en, ja, error]);
    const el = $(id);
    el.textContent = tr(en, ja);
    el.hidden = !en;
    el.classList.toggle("error", error);
  }
  function showError(id, error) { say(id, error.message, error.message, true); }

  function autosave() {
    try { localStorage.setItem(storageKey, Editor.serialize(bank)); }
    catch { say("editStatus", "Autosave unavailable. Save a .mikuphr file to keep your work.", "自動保存できません。.mikuphr ファイルに保存してください。", true); }
  }
  function slotLabel(i) {
    return tr(`Slot ${i + 1}`, `スロット ${i + 1}`);
  }
  function fillSlots() {
    const scroll = $("slots").scrollTop;
    const buttons = bank.map((entry, i) => {
      const p = entry && MKX.phraseDecode(entry.record);
      const button = document.createElement("button");
      const text = document.createElement("small");
      button.textContent = `${String(i + 1).padStart(2, "0")} · ${p?.name.trim() || (p ? tr("Untitled", "無題") : tr("Empty", "空き"))}`;
      button.setAttribute("aria-pressed", i === slot);
      if (p) {
        text.textContent = `${p.lang ? "JP" : "EN"} · ${p.text.trim().split(/ +/).slice(0, 5).join(" ")}`;
        button.append(text);
      }
      button.onclick = () => { selectSlot(i); $("slots").children[i].focus(); };
      return button;
    });
    $("slots").replaceChildren(...buttons);
    $("slots").scrollTop = scroll;
    $("slotTitle").textContent = slotLabel(slot);
  }

  function selectSlot(i) {
    slot = i;
    const entry = bank[slot];
    phrase = entry ? MKX.phraseDecode(entry.record) : empty();
    overrides = Editor.validateOverrides(entry?.overrides || {});
    valid = true;
    $("name").value = phrase.name.trimEnd();
    $("words").value = phrase.text;
    $("language").value = phrase.lang;
    $("bpm").value = phrase.bpm;
    $("length").value = phrase.deflen;
    $("melody").checked = !!phrase.flags;
    say("compileError");
    say("editStatus");
    fillSlots();
    showPhrase();
  }

  function saveEdit() {
    bank[slot] = {record: MKX.phraseEncode(phrase), overrides};
    autosave();
    fillSlots();
    showPhrase();
  }
  function showLimits() {
    const count = valid ? phrase.n : "—";
    $("limits").textContent = tr(`${count}/48 syllables · ${$("words").value.length}/128 characters`,
      `${count}/48 音節 · ${$("words").value.length}/128 文字`);
  }

  function showPhrase() {
    showLimits();
    const list = $("syllables");
    list.replaceChildren();
    $("pronunciation").hidden = $("language").value !== "0";
    const previousWord = $("word").value;
    const words = $("words").value.toLowerCase().match(/[a-z0-9']+(?:-[a-z0-9']+)*/g) || [];
    $("word").replaceChildren(...[...new Set(words.map(w => w.replace(/^'+|'+$/g, "")))].filter(Boolean)
      .map(w => new Option(w, w)));
    if ([...$("word").options].some(o => o.value === previousWord)) $("word").value = previousWord;
    $("arpabet").value = overrides[$("word").value] || "";
    $("override").disabled = $("resetOverride").disabled = !$("word").value;
    if (valid) {
      for (const s of phrase.s) {
        const li = document.createElement("li");
        li.textContent = s.spell || "·";
        if (!phrase.lang) {
          const code = document.createElement("code");
          code.textContent = Editor.arpa(s);
          li.append(" ", code);
        }
        list.append(li);
      }
    }
    $("melody").disabled = $("play").disabled = !valid || !phrase.n;
    $("saveOne").disabled = !valid || !bank[slot];
    drawRoll();
    usbButtons();
  }

  function edit(recompile, replacements = overrides) {
    if (!api) return;
    try {
      const lang = Number($("language").value);
      const deflen = clamp(Math.round(number("length", phrase.deflen)), 1, 16);
      const bpm = clamp(Math.round(number("bpm", phrase.bpm)), 40, 240);
      const name = $("name").value.toUpperCase().replace(/[^A-Z0-9 ]/g, "").slice(0, 12);
      $("name").value = name;
      const old = phrase;
      const p = recompile || !valid ? api.compile($("words").value, lang, deflen, replacements) : phrase;
      p.name = name.padEnd(12);
      p.bpm = bpm;
      p.flags = Number($("melody").checked);
      p.deflen = deflen;
      if (p !== old && p.lang === old.lang) {
        const jpDefaults = p.lang && p.text === old.text && p.deflen !== old.deflen
          ? api.compile(old.text, 1, old.deflen) : null;
        const match = (a, b) => a.spell === b.spell && same(a.ph, b.ph);
        let prefix = 0, suffix = 0;
        const common = Math.min(p.n, old.n);
        while (prefix < common && match(p.s[prefix], old.s[prefix])) prefix++;
        while (prefix + suffix < common && match(p.s[p.n - 1 - suffix], old.s[old.n - 1 - suffix])) suffix++;
        for (let i = 0; i < p.n; i++) {
          const j = i >= p.n - suffix ? i + old.n - p.n : i;
          if (i >= prefix && i < p.n - suffix && p.n !== old.n) continue;
          p.s[i].note = old.s[j].note;
          const len = old.s[j].len & 127;
          if (!p.lang || p.text === old.text && (!jpDefaults || (jpDefaults.s[j]?.len & 127) !== len)) {
            p.s[i].len = (p.s[i].len & 128) | len;
          }
        }
      }
      phrase = p;
      overrides = replacements;
      valid = true;
      say("compileError");
      saveEdit();
    } catch (error) {
      if (error.offset === undefined) { showError("editStatus", error); return; }
      valid = false;
      const en = {ascii: "Use printable ASCII", romaji: "Invalid romaji, duration, or more than 48 syllables",
        english: "Could not compile English within 48 syllables"}[error.kind];
      const ja = {ascii: "ASCII 文字を入力してください", romaji: "ローマ字・音価が無効、または48音節を超えています",
        english: "英語を48音節以内で読み取れません"}[error.kind];
      say("compileError", `${en} (byte ${error.offset}).`, `${ja}（バイト位置 ${error.offset}）。`, true);
      showPhrase();
    }
  }

  function drawRoll() {
    const roll = $("roll");
    if (!valid || !phrase.n) { roll.replaceChildren(); return; }
    const scroll = [roll.scrollLeft, roll.scrollTop], first = !roll.firstChild;
    const pitch = s => s.note || 60;
    const length = s => (s.len & 127) || phrase.deflen;
    const high = Math.max(79, ...phrase.s.map(pitch)) + 1;
    const low = Math.min(53, ...phrase.s.map(pitch)) - 1;
    const step = 28, row = 12, gutter = 42, height = (high - low + 1) * row;
    const width = gutter + step * phrase.s.reduce((sum, s) => sum + length(s), 0);
    const element = (tag, css, parent) => {
      const node = document.createElement(tag);
      node.className = css;
      parent.append(node);
      return node;
    };
    const grid = document.createElement("div");
    grid.className = "grid";
    grid.style.cssText = `width:max(100%, ${width}px);height:${height + 20}px;background-position:${gutter}px 0`;
    element("div", "keyrange", grid).style.cssText = `top:${(high - 79) * row}px;height:${27 * row}px`;
    const ruler = element("div", "ruler", grid);
    ruler.style.height = height + "px";
    const pitchName = p => "C C♯ D D♯ E F F♯ G G♯ A A♯ B".split(" ")[p % 12] + (Math.floor(p / 12) - 1);
    for (let p = low; p <= high; p++) {
      if (p % 12 && p !== 53 && p !== 79) continue;
      const label = element("span", "", ruler);
      label.textContent = pitchName(p);
      label.style.top = (high - p) * row + "px";
    }
    let x = gutter;
    phrase.s.forEach((s, i) => {
      const note = element("button", phrase.lang && !s.ph[0] ? "rest" : "", grid);
      note.type = "button";
      const handle = element("span", "", note);
      const label = element("span", "lyric", grid);
      const at = x;
      x += length(s) * step;
      let duration = length(s), key = pitch(s);
      const layout = () => {
        note.style.cssText = `left:${at}px;top:${(high - key) * row}px;width:${duration * step - 2}px`;
        note.title = `${s.spell || "·"} · ${pitchName(key)} · ${duration}/16`;
        note.setAttribute("aria-label", note.title);
        label.style.cssText = `left:${at + 2}px;top:${height}px;width:${duration * step - 3}px`;
      };
      label.textContent = s.spell;
      layout();
      const update = () => {
        s.note = key;
        s.len = (s.len & 128) | duration;
        phrase.flags = 1;
        $("melody").checked = true;
        saveEdit();
        $("roll").querySelectorAll("button")[i].focus({preventScroll: true});
      };
      let drag;
      note.onpointerdown = e => {
        if (e.button !== 0) return;
        note.setPointerCapture(e.pointerId);
        drag = {x: e.clientX, y: e.clientY, key, duration, resize: e.target === handle, moved: false};
      };
      note.onpointermove = e => {
        if (!drag) return;
        const dy = Math.round((drag.y - e.clientY) / row);
        const dx = Math.round((e.clientX - drag.x) / step);
        if (drag.resize) duration = dx ? clamp(drag.duration + dx, 1, 64) : drag.duration;
        else key = clamp(drag.key + dy, 1, 127);
        drag.moved ||= key !== drag.key || duration !== drag.duration;
        layout();
      };
      note.onpointercancel = () => { drag = null; duration = length(s); key = pitch(s); layout(); };
      note.onpointerup = () => {
        if (!drag) return;
        const moved = drag.moved;
        drag = null;
        if (moved) update();
      };
      note.onclick = () => preview(i);
      note.onkeydown = e => {
        if (!["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"].includes(e.key)) return;
        e.preventDefault();
        if (e.key === "ArrowUp" || e.key === "ArrowDown") key = clamp(key + (e.key === "ArrowUp" ? 1 : -1), 1, 127);
        else duration = clamp(duration + (e.key === "ArrowRight" ? 1 : -1), 1, 64);
        update();
      };
    });
    roll.replaceChildren(grid);
    roll.scrollLeft = scroll[0];
    roll.scrollTop = first ? Math.max(0, (high - 79) * row) : scroll[1];
  }

  function stop() {
    previewId++;
    if (playing) { playing.stop(); playing = null; }
  }
  async function preview(index = -1) {
    if (!api || !valid || !phrase.n) return;
    stop();
    const id = previewId;
    try {
      audio ||= new AudioContext();
      const resumed = audio.resume();
      const channels = api.render(phrase, number("voice", 2), number("reverb", 20), index);
      const buffer = audio.createBuffer(2, channels[0].length, 44100);
      channels.forEach((channel, i) => buffer.copyToChannel(channel, i));
      await resumed;
      if (id !== previewId) return;
      const source = audio.createBufferSource();
      source.buffer = buffer;
      source.connect(audio.destination);
      source.onended = () => { if (playing === source) playing = null; };
      playing = source;
      source.start();
      say("editStatus");
    } catch (error) { showError("editStatus", error); }
  }

  function download(slot) {
    const url = URL.createObjectURL(new Blob([Editor.serialize(bank, slot)], {type: "application/json"}));
    const a = document.createElement("a");
    a.href = url;
    a.download = slot === null ? "miku-phrase-bank.mikuphr" : `miku-phrase-${slot + 1}.mikuphr`;
    a.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }
  $("saveBank").onclick = () => download(null);
  $("saveOne").onclick = () => download(slot);
  $("loadFile").onclick = () => $("phraseFile").click();
  $("phraseFile").onchange = async e => {
    const file = e.target.files[0];
    if (!file) return;
    try {
      if (file.size > 2000000) throw Error("The .mikuphr file is too large.");
      const loaded = Editor.deserialize(await file.text());
      if (loaded.bank) bank = loaded.bank;
      else bank[loaded.slot] = loaded.phrase;
      selectSlot(loaded.slot ?? slot);
      autosave();
    } catch (error) { showError("editStatus", error); }
    finally { e.target.value = ""; }
  };
  $("words").oninput = $("language").onchange = () => edit(true);
  for (const id of ["name", "bpm", "length", "melody"]) $(id).oninput = () => edit(id === "length" && phrase.lang === 1);
  for (const id of ["bpm", "length"]) $(id).onblur = () => {
    $(id).value = id === "bpm" ? phrase.bpm : phrase.deflen;
  };
  $("word").onchange = () => { $("arpabet").value = overrides[$("word").value] || ""; };
  function override(reset) {
    const word = $("word").value;
    if (!word) return;
    try {
      const next = {...overrides, [word]: $("arpabet").value};
      if (reset) delete next[word];
      edit(true, Editor.validateOverrides(next));
    } catch (error) { showError("editStatus", error); }
  }
  $("override").onclick = () => override(false);
  $("resetOverride").onclick = () => override(true);
  $("play").onclick = () => preview();
  $("stop").onclick = stop;

  $("midiFile").onchange = async e => {
    const file = e.target.files[0];
    midiFile = null;
    $("track").replaceChildren();
    $("track").disabled = $("importTrack").disabled = true;
    say("importStatus");
    if (!file) return;
    try {
      if (file.size > 16000000) throw Error("The MIDI file is too large.");
      midiFile = EditorMidi.parse(new Uint8Array(await file.arrayBuffer()));
      const tracks = midiFile.tracks.flatMap((t, i) => t.notes.length ?
        [new Option(`${i + 1} · ${t.name || "MIDI"} (${t.notes.length})`, i)] : []);
      if (!tracks.length) throw Error("No tracks contain complete notes.");
      $("track").replaceChildren(...tracks);
      $("track").disabled = false;
      $("importTrack").disabled = !api;
    } catch (error) { showError("importStatus", error); }
  };
  $("importTrack").onclick = () => {
    try {
      const {phrase: imported, fallback, cut} = EditorMidi.convert(midiFile, Number($("track").value), api.compile, phrase.deflen);
      imported.name = (phrase.name.trim() ? phrase.name : "MIDI IMPORT ");
      bank[slot] = {record: MKX.phraseEncode(imported), overrides: {}};
      selectSlot(slot);
      autosave();
      say("importStatus",
        `${imported.n} notes imported.${cut ? " Cut at 48." : ""}${fallback ? " Used one syllable per note." : ""}`,
        `${imported.n} 音を読み込みました。${cut ? "48 音で切り詰めました。" : ""}${fallback ? "1 音につき 1 音節を使用しました。" : ""}`);
    } catch (error) { showError("importStatus", error); }
  };

  const cmd = {hello: 1, get: 2, put: 3, list: 4, begin: 5, chunk: 6, end: 7,
    ident: 0x41, phrase: 0x42, listing: 0x44, ack: 0x7f};
  let midi, input, output, pending, seq = 0, identified = false, busy = false, deviceBank;
  function usbButtons() {
    const available = !busy && identified && input?.state === "connected" && output?.state === "connected";
    $("connect").disabled = busy || !navigator.requestMIDIAccess;
    $("midiIn").disabled = $("midiOut").disabled = busy || !midi;
    $("hello").disabled = busy || !input || !output;
    $("readBank").disabled = $("writeChanged").disabled = !available;
    $("writeOne").disabled = !available || !valid || !bank[slot];
    $("sendSong").disabled = !available || !api || !valid || !phrase?.n;
  }
  function pickPorts() {
    for (const [id, ports] of [["midiIn", midi.inputs], ["midiOut", midi.outputs]]) {
      const previous = $(id).value;
      const list = [...ports.values()].filter(p => p.state === "connected");
      $(id).replaceChildren(...list.map(p => new Option(p.name || p.id, p.id)));
      $(id).value = (list.find(p => p.id === previous) || list.find(p => /FM.?1/i.test(p.name)) || list[0])?.id || "";
    }
    choosePorts();
  }
  function choosePorts() {
    const nextIn = midi.inputs.get($("midiIn").value), nextOut = midi.outputs.get($("midiOut").value);
    if (input !== nextIn || output !== nextOut) {
      if (pending) pending.finish(Error("MIDI ports changed during the transfer."));
      if (input) input.onmidimessage = null;
      input = nextIn;
      output = nextOut;
      identified = false;
      deviceBank = null;
      if (input) input.onmidimessage = receive;
      say("usbStatus", input && output ? "Select the FM-1 ports and identify the device." : "Connect the FM-1 by USB.",
        input && output ? "FM-1 のポートを選んでデバイスを識別してください。" : "FM-1 を USB で接続してください。");
    }
    usbButtons();
  }

  function receive(event) {
    if (!pending) return;
    let frame;
    try { frame = MKX.frameParse(event.data); } catch { return; }
    if (frame.seq !== pending.seq) return;
    const data = frame.payload;
    if (frame.cmd === cmd.ack) {
      if (data.length !== 2 || data[0] !== pending.command) return;
      if (data[1]) {
        const statuses = ["", "Bad frame or CRC", "Bad record", "Busy. Press Stop on the FM-1.", "Refused"];
        const japanese = ["", "フレームまたは CRC エラー", "レコードが無効です", "FM-1 の再生を停止してください。", "拒否されました"];
        pending.finish(Error(tr(statuses[data[1]] || "Unknown ACK status", japanese[data[1]] || "不明な ACK")));
      } else if (pending.reply === cmd.ack) pending.finish(null, data);
    } else if (frame.cmd === pending.reply) pending.finish(null, data);
  }

  function request(command, reply, payload) {
    return new Promise((resolve, reject) => {
      const id = seq++ & 127;
      const finish = (error, data) => {
        clearTimeout(timer);
        pending = null;
        if (error) reject(error);
        else resolve(data);
      };
      const timer = setTimeout(() => finish(Error(tr("FM-1 reply timed out. Check the USB ports and firmware version.",
        "FM-1 の応答がありません。USB ポートとファームウェアのバージョンを確認してください。"))), 3000);
      pending = {seq: id, command, reply, finish};
      try { output.send(MKX.frameBuild(command, id, payload)); }
      catch (error) { finish(error); }
    });
  }

  async function runUSB(work) {
    if (busy) return;
    busy = true;
    usbButtons();
    try { await work(); }
    catch (error) { showError("usbStatus", error); }
    finally { busy = false; usbButtons(); }
  }

  async function identify() {
    identified = false;
    deviceBank = null;
    await input.open();
    await output.open();
    const id = await request(cmd.hello, cmd.ident);
    if (id.length < 4 || id[0] !== 1 || id[1] !== 64 || id.at(-1) !== 0) throw Error("Unexpected FM-1 identity.");
    identified = true;
    const version = new TextDecoder().decode(id.subarray(3, -1));
    say("usbStatus", `FM-1 · firmware ${version} · protocol ${id[0]} · ${id[1]} phrase slots`,
      `FM-1 · ファームウェア ${version} · プロトコル ${id[0]} · フレーズ ${id[1]} 枠`);
  }
  $("connect").onclick = () => runUSB(async () => {
    try { midi = await navigator.requestMIDIAccess({sysex: true}); }
    catch { throw Error(tr("MIDI SysEx access was denied. Allow SysEx in the browser and try again.", "MIDI SysEx のアクセスが拒否されました。ブラウザーで許可してください。")); }
    midi.onstatechange = pickPorts;
    pickPorts();
    if (input && output) await identify();
    else say("usbStatus", "No MIDI ports found. Connect the FM-1 by USB.", "MIDI ポートが見つかりません。FM-1 を USB で接続してください。");
  });
  $("midiIn").onchange = $("midiOut").onchange = choosePorts;
  $("hello").onclick = () => runUSB(identify);

  async function readDevice() {
    const listing = await request(cmd.list, cmd.listing);
    if (listing.length !== 64 * 14) throw Error("Invalid FM-1 bank list.");
    const records = Array(64).fill(null);
    for (let i = 0; i < 64; i++) {
      if (!listing[i * 14]) continue;
      say("usbStatus", `Reading slot ${i + 1}/64…`, `スロット ${i + 1}/64 を受信中…`);
      const reply = await request(cmd.get, cmd.phrase, Uint8Array.of(i));
      if (reply.length !== 1025 || reply[0] !== i) throw Error("Unexpected phrase reply from the FM-1.");
      const record = reply.slice(1);
      if (record.some(Boolean)) {
        MKX.phraseDecode(record);
        records[i] = record;
      }
    }
    return records;
  }
  $("readBank").onclick = () => runUSB(async () => {
    deviceBank = await readDevice();
    bank = deviceBank.map((record, i) => record ? {record, overrides: same(record, bank[i]?.record) ? bank[i].overrides : {}} : null);
    selectSlot(slot);
    autosave();
    say("usbStatus", "Bank read from the FM-1.", "FM-1 のバンクを受信しました。");
  });
  async function write(i, entry) {
    const payload = new Uint8Array(1025);
    payload[0] = i;
    payload.set(entry.record, 1);
    try { await request(cmd.put, cmd.ack, payload); }
    catch (error) { throw Error(`${slotLabel(i)} · ${error.message}`); }
    if (deviceBank) deviceBank[i] = entry.record;
    say("usbStatus", `${slotLabel(i)} acknowledged.`, `${slotLabel(i)} を書き込みました（ACK）。`);
  }
  $("writeOne").onclick = () => runUSB(() => write(slot, bank[slot]));
  $("writeChanged").onclick = () => runUSB(async () => {
    const snapshot = bank.slice();
    deviceBank ||= await readDevice();
    let count = 0;
    for (const [i, entry] of snapshot.entries()) {
      if (!entry || same(entry.record, deviceBank[i])) continue;
      await write(i, entry);
      count++;
    }
    say("usbStatus", `${count} changed phrases acknowledged.`, `${count} 件の変更を書き込みました（ACK）。`);
  });
  $("sendSong").onclick = () => runUSB(async () => {
    const song = api.song(phrase, number("voice", 2), number("reverb", 20));
    const le16 = n => Uint8Array.of(n & 255, n >> 8);
    await request(cmd.begin, cmd.ack, le16(song.length));
    for (let at = 0; at < song.length; at += 2048) {
      const part = song.subarray(at, at + 2048);
      const chunk = new Uint8Array(part.length + 2);
      chunk.set(le16(at));
      chunk.set(part, 2);
      await request(cmd.chunk, cmd.ack, chunk);
    }
    await request(cmd.end, cmd.ack);
    say("usbStatus", "Song acknowledged. Press REC on the FM-1 within 10 seconds, then SAVE to store it.",
      "ソングを受信しました（ACK）。FM-1 の REC を 10 秒以内に押し、SAVE で保存してください。");
  });

  function applyLang() {
    document.documentElement.lang = lang;
    document.title = tr("Miku-FM · Phrase editor", "Miku-FM · フレーズエディター");
    $("lang").textContent = tr("日本語", "English");
    for (const [el, en] of staticLabels) el.textContent = tr(en, el.dataset.ja);
    for (const [id, status] of statuses) say(id, ...status);
    fillSlots();
    showLimits();
  }
  $("lang").onclick = () => { lang = tr("ja", "en"); applyLang(); };

  try {
    const stored = localStorage.getItem(storageKey);
    if (stored) bank = Editor.deserialize(stored).bank || bank;
  } catch { /* File URLs and private mode can restrict storage. */ }
  selectSlot(0);
  applyLang();
  if (!navigator.requestMIDIAccess) say("usbStatus",
    "Web MIDI SysEx is unavailable. Use Chrome or Edge for USB.", "Web MIDI SysEx を利用できません。USB 転送には Chrome または Edge を使用してください。");
  say("ready", "Loading the voice and dictionary…", "音声エンジンと辞書を読み込み中…");
  Editor.embedded(document).then(engine => {
    api = engine;
    $("editing").disabled = false;
    $("importTrack").disabled = $("track").disabled;
    say("ready");
    usbButtons();
  }).catch(error => showError("ready", error));
})();
