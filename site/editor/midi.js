// SPDX-License-Identifier: GPL-3.0-only
const EditorMidi = (() => {
  "use strict";
  const decoder = new TextDecoder();
  const clamp = (x, min, max) => Math.max(min, Math.min(max, x));

  function parse(bytes) {
    let at = 0, limit = bytes.length;
    function take(size) {
      if (size > limit - at) throw Error("Truncated MIDI file");
      const part = bytes.subarray(at, at + size);
      at += size;
      return part;
    }
    const byte = () => take(1)[0];
    const int = n => take(n).reduce((value, b) => value * 256 + b, 0);
    const tag = () => decoder.decode(take(4));
    function variable() {
      let value = 0;
      for (let k = 0; k < 4; k++) {
        const b = byte();
        value = 128 * value + (b & 127);
        if (b < 128) return value;
      }
      throw Error("Invalid MIDI variable-length value");
    }
    if (tag() !== "MThd") throw Error("Missing MIDI header");
    const header = int(4);
    if (header < 6) throw Error("Invalid MIDI header");
    const format = int(2), count = int(2), ppq = int(2);
    if (format > 2 || !count || !ppq || ppq & 0x8000) throw Error("MIDI requires beat-based timing (PPQ).");
    take(header - 6);
    const tracks = [];
    while (tracks.length < count) {
      const type = tag(), size = int(4);
      if (type !== "MTrk") { take(size); continue; }
      if (size > bytes.length - at) throw Error("Truncated MIDI track");
      limit = at + size;
      let tick = 0, running = 0, name = "";
      const notes = [], lyrics = [], texts = [], tempos = [], pending = new Map();
      while (at < limit) {
        tick += variable();
        let status = byte();
        if (status < 128) {
          if (!running) throw Error("Invalid MIDI running status");
          at--;
          status = running;
        }
        if (status === 255) {
          const kind = byte(), data = take(variable());
          if (kind === 0x2f) break;
          if (kind === 3) name = decoder.decode(data);
          if (kind === 1 || kind === 5) {
            (kind === 5 ? lyrics : texts).push({tick, text: decoder.decode(data)});
          }
          if (kind === 0x51 && data.length === 3) {
            const micros = data.reduce((sum, b) => sum * 256 + b, 0);
            if (micros) tempos.push({tick, micros});
          }
          continue;
        }
        if (status === 0xf0 || status === 0xf7) {
          running = 0;
          take(variable());
          continue;
        }
        if (status >= 0xf0) throw Error("Invalid MIDI track event");
        running = status;
        const kind = status >> 4, note = byte(), value = kind === 12 || kind === 13 ? 0 : byte();
        if (note > 127 || value > 127) throw Error("Invalid MIDI data byte");
        if (kind !== 8 && kind !== 9) continue;
        const key = (status & 15) * 128 + note;
        if (kind === 9 && value) {
          if (!pending.has(key)) pending.set(key, []);
          pending.get(key).push({tick, note});
        } else if (pending.get(key)?.length) {
          notes.push({...pending.get(key).shift(), end: tick});
        }
      }
      tracks.push({name, notes: notes.sort((a, b) => a.tick - b.tick || a.note - b.note), lyrics, texts, tempos});
      at = limit;
      limit = bytes.length;
    }
    return {format, ppq, tracks};
  }

  function convert(midi, index, compile, deflen) {
    const track = midi.tracks[index];
    if (!track?.notes.length) throw Error("Choose a MIDI track containing notes.");
    const selected = track.notes.slice(0, 48);
    const tempoTracks = midi.format === 2 ? [track] : midi.tracks;
    const tempos = tempoTracks.flatMap(t => t.tempos).sort((a, b) => a.tick - b.tick);
    const bpm = clamp(Math.round(60000000 / (tempos[0]?.micros || 500000)), 40, 240);
    const noteEnd = Math.max(...selected.map(note => note.end));
    const events = t => (t.lyrics.length ? t.lyrics : t.texts).filter(e => e.tick <= noteEnd && !/^\s*@/.test(e.text))
      .map(e => ({tick: e.tick, text: e.text.replace(/[\\/]+/g, " ").replace(/[^ -~]/g, " ").trim()}))
      .filter(e => e.text);
    const lyrics = events(track);
    if (!lyrics.length && midi.format !== 2) {
      const donor = midi.tracks.find(t => !t.notes.length && events(t).length);
      if (donor) lyrics.push(...events(donor));
    }
    // A karaoke hyphen joins the next fragment to the same word.
    const text = lyrics.map(e => e.text.endsWith("-") ? e.text.slice(0, -1) : e.text + " ")
      .join("").trim().slice(0, 128);
    function compileLyrics(value) {
      try { return compile(value, 0, deflen); }
      catch (error) { if (error.kind !== "english") throw error; }
    }
    let phrase = compileLyrics(text);
    let fallback = false;
    if (phrase?.n !== selected.length) {
      fallback = true;
      const labels = selected.map(() => []);
      for (const event of lyrics) {
        // Match the closest onset, favoring the next note on ties.
        let best = 0;
        for (let i = 1; i < selected.length; i++) {
          if (Math.abs(selected[i].tick - event.tick) <= Math.abs(selected[best].tick - event.tick)) best = i;
        }
        labels[best].push(event.text);
      }
      phrase = compile("", 0, deflen);
      phrase.text = text;
      phrase.s = labels.map(parts => {
        const label = parts.join(" ").replace(/-$/, "").slice(0, 128);
        const syl = compileLyrics(label)?.s[0] || compile("ah", 0, deflen).s[0];
        return {...syl, spell: (label || "ah").slice(0, 5)};
      });
      phrase.n = phrase.s.length;
    }
    const quantize = ticks => Math.round(ticks * 4 / midi.ppq);
    for (const [i, note] of selected.entries()) {
      const length = clamp(quantize(note.end) - quantize(note.tick), 1, 64);
      phrase.s[i].note = Math.max(1, note.note);
      phrase.s[i].len = (phrase.s[i].len & 128) | length;
    }
    phrase.bpm = bpm;
    phrase.flags = 1;
    return {phrase, fallback, cut: track.notes.length > 48};
  }
  return {parse, convert};
})();
if (typeof module === "object") module.exports = EditorMidi;
