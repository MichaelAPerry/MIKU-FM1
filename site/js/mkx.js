// SPDX-License-Identifier: GPL-3.0-only
(function (root, factory) {
  if (typeof module === "object" && module.exports) module.exports = factory();
  else root.MKX = factory();
}(typeof self === "object" ? self : this, function () {
  "use strict";

  const PAYLOAD_MAX = 3000, RAW_CAP = PAYLOAD_MAX + 4;
  const REC = 1024, SYL = 48, TEXT = 128, CRC_AT = 1020;
  const FRAME_CAP = 8 + RAW_CAP + Math.ceil(RAW_CAP / 7);
  const integer = (value, max, min = 0) => Number.isInteger(value) && value >= min && value <= max;
  const bytes = (value, max) => value instanceof Uint8Array && value.length <= max;
  const bad = reason => { throw new RangeError(reason); };

  function crc32(raw) {
    let crc = 0xffffffff;
    for (const b of raw) {
      crc ^= b;
      for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
    }
    return (~crc) >>> 0;
  }

  const view = raw => new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  function pack(raw) {
    if (!bytes(raw, RAW_CAP)) bad("raw size");
    const packed = new Uint8Array(raw.length + Math.ceil(raw.length / 7));
    for (let i = 0, at = 0; i < raw.length;) {
      const high = at++;
      for (let j = 0; j < 7 && i < raw.length; j++, i++) {
        packed[high] |= (raw[i] >>> 7) << j;
        packed[at++] = raw[i] & 127;
      }
    }
    return packed;
  }

  function unpack(packed) {
    if (!bytes(packed, FRAME_CAP - 8) || packed.length % 8 === 1) bad("packed size");
    const raw = new Uint8Array(packed.length - Math.ceil(packed.length / 8));
    for (let i = 0, at = 0; i < packed.length;) {
      const high = packed[i++], count = Math.min(7, packed.length - i);
      if (high >>> count) bad("high bits");
      for (let j = 0; j < count; j++, i++) {
        if (packed[i] > 127) bad("low bits");
        raw[at++] = packed[i] | (((high >>> j) & 1) << 7);
      }
    }
    return raw;
  }

  function frameBuild(cmd, seq, payload = new Uint8Array(0)) {
    if (!integer(cmd, 127) || !integer(seq, 127) || !bytes(payload, PAYLOAD_MAX)) bad("frame fields");
    const raw = new Uint8Array(payload.length + 4);
    raw.set(payload);
    view(raw).setUint32(payload.length, crc32(payload), true);
    const packed = pack(raw);
    const frame = new Uint8Array(packed.length + 8);
    frame.set([0xf0, 0x7d, 0x4d, 0x4b, 1, cmd, seq]);
    frame.set(packed, 7);
    frame[frame.length - 1] = 0xf7;
    return frame;
  }

  function frameParse(frame) {
    if (!bytes(frame, FRAME_CAP)) bad("frame size");
    if (frame[0] === 0xf0) {
      if (frame[frame.length - 1] !== 0xf7) bad("terminator");
      frame = frame.subarray(1, frame.length - 1);
    }
    if (frame.length < 11 || frame[0] !== 0x7d || frame[1] !== 0x4d || frame[2] !== 0x4b ||
        frame[3] !== 1 || frame[4] > 127 || frame[5] > 127) bad("frame header");
    const raw = unpack(frame.subarray(6));
    if (raw.length < 4) bad("missing CRC");
    const payload = raw.subarray(0, raw.length - 4);
    if (crc32(payload) !== view(raw).getUint32(payload.length, true)) bad("frame CRC");
    return {cmd: frame[4], seq: frame[5], payload};
  }

  const printable = (str, max) => typeof str === "string" && str.length <= max && !/[^ -~]/.test(str);
  const nameOK = str => typeof str === "string" && str.length === 12 && !/[^A-Z0-9 ]/.test(str);
  const zero = raw => raw.every(b => b === 0);
  const headerOK = p => integer(p.lang, 1) && integer(p.bpm, 240, 40) && integer(p.n, SYL) &&
    integer(p.deflen, 16, 1) && integer(p.flags, 1);
  const sylOK = (s, lang) => s && (Array.isArray(s.ph) || bytes(s.ph, 8)) && s.ph.length === 8 &&
    Array.from(s.ph).every((b, i) => integer(b, lang === 0 ? 40 : i === 0 ? 72 : 0)) &&
    printable(s.spell, 5) && integer(s.note, 127) && integer(s.len, 255);
  const strBytes = str => Uint8Array.from(str, c => c.charCodeAt(0));

  function readString(raw, pad) {
    const end = pad ? raw.indexOf(0) : -1;
    if (end < 0) return String.fromCharCode(...raw);
    if (!zero(raw.subarray(end))) bad("string padding");
    return String.fromCharCode(...raw.subarray(0, end));
  }

  // Uses the C field names. Strings omit NUL padding, while name has all 12 characters.
  function phraseEncode(p) {
    if (!p || !headerOK(p) || !nameOK(p.name) || !printable(p.text, TEXT) ||
        !Array.isArray(p.s) || p.s.length < p.n) bad("phrase fields");
    const rec = new Uint8Array(REC);
    rec.set([77, 80, 72, 49, 1, p.lang, p.bpm, p.n, p.deflen, p.flags]);
    rec.set(strBytes(p.name), 12);
    rec.set(strBytes(p.text), 32);
    for (let i = 0; i < p.n; i++) {
      const s = p.s[i], at = 160 + 16 * i;
      if (!sylOK(s, p.lang)) bad("syllable fields");
      rec.set(s.ph, at);
      rec.set(strBytes(s.spell), at + 8);
      rec.set([s.note, s.len], at + 14);
    }
    view(rec).setUint32(CRC_AT, crc32(rec.subarray(0, CRC_AT)), true);
    return rec;
  }

  function phraseDecode(rec) {
    if (!bytes(rec, REC) || rec.length !== REC || readString(rec.subarray(0, 4), false) !== "MPH1" ||
        rec[4] !== 1) bad("phrase header");
    if (crc32(rec.subarray(0, CRC_AT)) !== view(rec).getUint32(CRC_AT, true)) bad("phrase CRC");
    const p = {lang: rec[5], bpm: rec[6], n: rec[7], deflen: rec[8], flags: rec[9],
      name: readString(rec.subarray(12, 24), false), text: readString(rec.subarray(32, 160), true), s: []};
    if (!headerOK(p) || !nameOK(p.name) || !printable(p.text, TEXT) || !zero(rec.subarray(10, 12)) ||
        !zero(rec.subarray(24, 32)) || !zero(rec.subarray(160 + 16 * p.n, CRC_AT))) bad("phrase fields");
    for (let i = 0; i < p.n; i++) {
      const at = 160 + 16 * i;
      const s = {ph: Array.from(rec.subarray(at, at + 8)), spell: readString(rec.subarray(at + 8, at + 14), true),
        note: rec[at + 14], len: rec[at + 15]};
      if (!sylOK(s, p.lang)) bad("syllable fields");
      p.s.push(s);
    }
    return p;
  }

  return {crc32, pack, unpack, frameBuild, frameParse, phraseEncode, phraseDecode, PAYLOAD_MAX, FRAME_CAP};
}));
