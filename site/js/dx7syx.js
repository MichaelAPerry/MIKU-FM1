/* SPDX-License-Identifier: GPL-3.0-only
 * DX7 SysEx in the browser: the 32-voice bulk dump (VMEM, format 9) and the single voice (VCED, format 0) to and from the
 * 128-byte packed voice. A line-by-line mirror of firmware/song/dx7_sysex.c and dx_patch_valid (firmware/audio/dx7/dx7.c);
 * tests/test_bank_tool.js checks it against vectors produced by that C code. No dependencies. */
(function (root) {
  "use strict";
  var VOICE = 128, VCED = 155, BANK_N = 32, BANK_BYTES = BANK_N * VOICE;
  var BULK_FRAME = 5 + BANK_BYTES + 1, SINGLE_FRAME = 5 + VCED + 1;       // frame = the bytes between F0 and F7

  function checksum(u8, from, to) {                                       // 7-bit two's complement of the byte sum
    var s = 0, i;
    for (i = from; i < to; i++) s += u8[i];
    return (0 - s) & 127;
  }
  function vcedToPacked(v) {
    var p = new Uint8Array(VOICE), o, i, s, d;
    for (o = 0; o < 6; o++) {
      s = 21 * o; d = 17 * o;
      for (i = 0; i < 11; i++) p[d + i] = v[s + i];
      p[d + 11] = (v[s + 11] & 3) | (v[s + 12] & 3) << 2;
      p[d + 12] = (v[s + 13] & 7) | (v[s + 20] & 15) << 3;
      p[d + 13] = (v[s + 14] & 3) | (v[s + 15] & 7) << 2;
      p[d + 14] = v[s + 16];
      p[d + 15] = (v[s + 17] & 1) | (v[s + 18] & 31) << 1;
      p[d + 16] = v[s + 19];
    }
    for (i = 0; i < 8; i++) p[102 + i] = v[126 + i];
    p[110] = v[134];
    p[111] = (v[135] & 7) | (v[136] & 1) << 3;
    for (i = 0; i < 4; i++) p[112 + i] = v[137 + i];
    p[116] = (v[141] & 1) | (v[142] & 7) << 1 | (v[143] & 7) << 4;
    p[117] = v[144];
    for (i = 0; i < 10; i++) p[118 + i] = v[145 + i];
    return p;
  }
  function packedToVced(p) {
    var v = new Uint8Array(VCED), o, i, s, d;
    for (o = 0; o < 6; o++) {
      s = 17 * o; d = 21 * o;
      for (i = 0; i < 11; i++) v[d + i] = p[s + i];
      v[d + 11] = p[s + 11] & 3; v[d + 12] = p[s + 11] >> 2 & 3;
      v[d + 13] = p[s + 12] & 7; v[d + 20] = p[s + 12] >> 3 & 15;
      v[d + 14] = p[s + 13] & 3; v[d + 15] = p[s + 13] >> 2 & 7;
      v[d + 16] = p[s + 14];
      v[d + 17] = p[s + 15] & 1; v[d + 18] = p[s + 15] >> 1 & 31;
      v[d + 19] = p[s + 16];
    }
    for (i = 0; i < 8; i++) v[126 + i] = p[102 + i];
    v[134] = p[110];
    v[135] = p[111] & 7; v[136] = p[111] >> 3 & 1;
    for (i = 0; i < 4; i++) v[137 + i] = p[112 + i];
    v[141] = p[116] & 1; v[142] = p[116] >> 1 & 7; v[143] = p[116] >> 4 & 7;
    v[144] = p[117];
    for (i = 0; i < 10; i++) v[145 + i] = p[118 + i];
    return v;
  }
  function initVoice() {                                                  // the standard DX7 "INIT VOICE" (dxs_init_voice)
    var p = new Uint8Array(VOICE), o, i, q, name = "INIT VOICE";
    for (o = 0; o < 6; o++) {
      q = 17 * o;
      p[q] = p[q + 1] = p[q + 2] = p[q + 3] = 99; p[q + 4] = p[q + 5] = p[q + 6] = 99; p[q + 7] = 0;
      p[q + 8] = 39; p[q + 12] = 7 << 3; p[q + 14] = o === 5 ? 99 : 0; p[q + 15] = 1 << 1;
    }
    for (i = 0; i < 4; i++) { p[102 + i] = 99; p[106 + i] = 50; }
    p[111] = 8; p[112] = 35; p[116] = 1 | 3 << 4; p[117] = 24;
    for (i = 0; i < 10; i++) p[118 + i] = name.charCodeAt(i);
    return p;
  }
  function mask(p) {                                                      // dxs_mask: drop bits the DX7 does not use
    var o, q;
    for (o = 0; o < 6; o++) { q = 17 * o; p[q + 11] &= 15; p[q + 13] &= 31; p[q + 15] &= 63; }
    p[111] &= 15;
  }
  function patchValid(p) {                                                // dx_patch_valid
    var o, i, q;
    for (i = 0; i < 128; i++) if (p[i] > 127) return false;
    for (o = 0; o < 6; o++) {
      q = 17 * o;
      for (i = 0; i < 11; i++) if (p[q + i] > 99) return false;
      if (p[q + 11] > 15 || p[q + 12] > 119 || p[q + 13] > 31 || p[q + 14] > 99 || p[q + 15] > 63 || p[q + 16] > 99) return false;
    }
    for (i = 102; i < 110; i++) if (p[i] > 99) return false;
    if (p[110] > 31 || p[111] > 15 || p[112] > 99 || p[113] > 99 || p[114] > 99 || p[115] > 99 || ((p[116] >> 1) & 7) > 5 || p[117] > 48) return false;
    return true;
  }
  function voiceName(p) {                                                 // 10 characters, as the device shows them
    var s = "", i, c;
    for (i = 0; i < 10; i++) { c = p[118 + i]; s += String.fromCharCode(c >= 97 && c <= 122 ? c - 32 : c < 32 || c > 126 ? 32 : c); }
    return s.replace(/\s+$/, "");
  }
  function algorithm(p) { return p[110] + 1; }                            // 1..32

  function buildBulk(bank, channel) {                                     // a whole F0 .. F7 message from 4096 packed bytes
    var f = new Uint8Array(BULK_FRAME + 2), i;
    f[0] = 0xF0; f[1] = 0x43; f[2] = (channel || 0) & 15; f[3] = 9; f[4] = 0x20; f[5] = 0;
    for (i = 0; i < BANK_BYTES; i++) f[6 + i] = bank[i] & 127;
    f[BULK_FRAME] = checksum(f, 6, 6 + BANK_BYTES); f[BULK_FRAME + 1] = 0xF7;
    return f;
  }
  function buildSingle(packed, channel) {                                 // a whole F0 .. F7 VCED message from one packed voice
    var f = new Uint8Array(SINGLE_FRAME + 2), v = packedToVced(packed), i;
    f[0] = 0xF0; f[1] = 0x43; f[2] = (channel || 0) & 15; f[3] = 0; f[4] = 1; f[5] = 0x1B;
    for (i = 0; i < VCED; i++) f[6 + i] = v[i] & 127;
    f[SINGLE_FRAME] = checksum(f, 6, 6 + VCED); f[SINGLE_FRAME + 1] = 0xF7;
    return f;
  }

  /* dxs_parse on one frame (the bytes between F0 and F7). Returns {ok, kind, channel, voices, error, bad}.
   * error codes are the firmware's: short, notdx7, format, length, checksum, voice. */
  function parseFrame(f) {
    var n = f.length, r = { ok: false, kind: null, channel: 0, voices: null, error: null, bad: 0 }, i, k, t, v, s;
    if (n < 5) { r.error = "short"; return r; }
    if (f[0] !== 0x43) { r.error = "notdx7"; return r; }
    if ((f[1] & 0x70) !== 0) { r.error = "format"; return r; }
    r.channel = f[1] & 15;
    if (f[2] === 9 && f[3] === 0x20 && f[4] === 0) {
      r.kind = "bulk";
      if (n !== BULK_FRAME) { r.error = "length"; return r; }
      for (i = 5; i < BULK_FRAME; i++) if (f[i] > 127) { r.error = "checksum"; return r; }
      if (checksum(f, 5, 5 + BANK_BYTES) !== f[BULK_FRAME - 1]) { r.error = "checksum"; return r; }
      r.voices = [];
      for (k = 0; k < BANK_N; k++) {
        t = new Uint8Array(f.subarray(5 + k * VOICE, 5 + (k + 1) * VOICE));
        mask(t);
        if (!patchValid(t)) { r.error = "voice"; r.bad = k + 1; r.voices = null; return r; }
        r.voices.push(t);
      }
      r.ok = true; return r;
    }
    if (f[2] === 0 && f[3] === 1 && f[4] === 0x1B) {
      r.kind = "single";
      if (n !== SINGLE_FRAME) { r.error = "length"; return r; }
      for (i = 5; i < SINGLE_FRAME; i++) if (f[i] > 127) { r.error = "checksum"; return r; }
      if (checksum(f, 5, 5 + VCED) !== f[SINGLE_FRAME - 1]) { r.error = "checksum"; return r; }
      v = f.subarray(5, 5 + VCED); r.bad = 1;
      for (k = 0; k < 6; k++) {
        s = 21 * k;
        if (v[s + 11] > 3 || v[s + 12] > 3 || v[s + 13] > 7 || v[s + 14] > 3 || v[s + 15] > 7 || v[s + 17] > 1 || v[s + 18] > 31 || v[s + 20] > 14) { r.error = "voice"; return r; }
      }
      if (v[135] > 7 || v[136] > 1 || v[141] > 1 || v[142] > 5 || v[143] > 7) { r.error = "voice"; return r; }
      t = vcedToPacked(v);
      if (!patchValid(t)) { r.error = "voice"; return r; }
      r.bad = 0; r.voices = [t]; r.ok = true; return r;
    }
    r.error = "format"; return r;
  }
  var MESSAGES = {
    short: "too short to be a DX7 message",
    notdx7: "not a Yamaha (DX7) message",
    format: "a Yamaha message, but not a DX7 voice dump (a request or parameter change?)",
    length: "the wrong length for a DX7 dump (a truncated or damaged file?)",
    checksum: "the checksum does not match (a damaged or edited file?)"
  };
  function describeError(r, kindName) {
    if (r.error === "voice") return r.kind === "bulk"
      ? "voice " + r.bad + " has values the DX7 cannot have; the FM-1 would refuse the whole bank"
      : "the voice has values the DX7 cannot have; the FM-1 would refuse it";
    return (kindName ? kindName + ": " : "") + (MESSAGES[r.error] || r.error);
  }

  /* A whole file: every F0 .. F7 message in it is parsed. Several messages are fine (one bank, then singles); the LAST valid bulk
   * dump becomes the bank, otherwise the single voices are listed. A raw 4096-byte file (no F0) is a packed bank (.bin).
   * Returns {ok, source: "syx"|"bin"|null, voices: [packed ...], kind, channel, notes: [string], errors: [string]}. */
  function parseFile(u8) {
    var out = { ok: false, source: null, voices: [], kind: null, channel: 0, notes: [], errors: [] }, i, j, msg, r, bulk = null, singles = [], seen = 0;
    if (u8.length && u8.length % VOICE === 0 && u8.length <= BANK_BYTES && u8[0] !== 0xF0) {   // a raw packed bank (a shorter one is padded)
      var t = [], k, bad = 0, cnt = u8.length / VOICE;
      for (k = 0; k < cnt; k++) {
        var p = new Uint8Array(u8.subarray(k * VOICE, (k + 1) * VOICE));
        for (i = 0; i < VOICE; i++) if (p[i] > 127) { out.errors.push("voice " + (k + 1) + ": byte " + i + " is not a 7-bit value, so this is not a packed DX7 bank"); return out; }
        mask(p);
        if (!patchValid(p)) { bad = k + 1; break; }
        t.push(p);
      }
      if (bad) { out.errors.push("voice " + bad + " has values the DX7 cannot have; the FM-1 would refuse the whole bank"); return out; }
      for (; k < BANK_N; k++) t.push(initVoice());
      out.ok = true; out.source = "bin"; out.kind = "bulk"; out.voices = t;
      out.notes.push("raw packed bank (" + cnt + " voice" + (cnt === 1 ? "" : "s") + (cnt < BANK_N ? ", padded to 32 with INIT VOICE" : "") + "), wrapped as a DX7 32-voice dump");
      return out;
    }
    i = 0;
    while (i < u8.length) {
      if (u8[i] !== 0xF0) { i++; continue; }
      j = i + 1;
      while (j < u8.length && u8[j] !== 0xF7) j++;
      if (j >= u8.length) { out.errors.push("a SysEx message at byte " + i + " is not closed (no F7): the file is cut short"); break; }
      msg = u8.subarray(i + 1, j); seen++;
      r = parseFrame(msg);
      if (r.kind === null && r.error === "format" && msg.length >= 2 && msg[0] === 0x43) out.notes.push("message " + seen + ": ignored (" + describeError(r) + ")");
      else if (r.kind === null) out.notes.push("message " + seen + ": ignored (" + describeError(r) + ")");
      else if (!r.ok) out.errors.push("message " + seen + " (" + (r.kind === "bulk" ? "32-voice dump" : "single voice") + "): " + describeError(r));
      else if (r.kind === "bulk") { bulk = r; }
      else singles.push(r);
      i = j + 1;
    }
    if (!seen && !out.errors.length) { out.errors.push(u8.length ? "no SysEx message (F0 ... F7) in this file" : "the file is empty"); return out; }
    if (bulk) {
      out.ok = true; out.source = "syx"; out.kind = "bulk"; out.channel = bulk.channel; out.voices = bulk.voices;
      if (singles.length) out.notes.push(singles.length + " single-voice message(s) in the file were ignored: the 32-voice dump was loaded");
    } else if (singles.length) {
      out.ok = true; out.source = "syx"; out.kind = "single"; out.channel = singles[0].channel;
      out.voices = singles.map(function (s) { return s.voices[0]; });
    } else if (!out.errors.length) out.errors.push("no DX7 voice dump in this file");
    if (out.ok && out.errors.length) out.notes.push("some messages in the file were bad and skipped: " + out.errors.join("; "));
    return out;
  }
  function packBank(voices) {                                             // up to 32 voices -> 4096 bytes, padded with INIT VOICE
    var b = new Uint8Array(BANK_BYTES), i, k, v;
    for (k = 0; k < BANK_N; k++) { v = k < voices.length ? voices[k] : initVoice(); for (i = 0; i < VOICE; i++) b[k * VOICE + i] = v[i]; }
    return b;
  }

  var api = {
    VOICE: VOICE, VCED: VCED, BANK_N: BANK_N, BANK_BYTES: BANK_BYTES, BULK_FRAME: BULK_FRAME, SINGLE_FRAME: SINGLE_FRAME,
    checksum: checksum, vcedToPacked: vcedToPacked, packedToVced: packedToVced, initVoice: initVoice, patchValid: patchValid, mask: mask,
    voiceName: voiceName, algorithm: algorithm, buildBulk: buildBulk, buildSingle: buildSingle, parseFrame: parseFrame, parseFile: parseFile,
    packBank: packBank
  };
  if (typeof module !== "undefined" && module.exports) module.exports = api; else root.DX7SYX = api;
})(typeof self !== "undefined" ? self : this);
