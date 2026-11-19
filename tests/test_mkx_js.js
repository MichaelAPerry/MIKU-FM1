// SPDX-License-Identifier: GPL-3.0-only
"use strict";
const assert = require("assert");
const fs = require("fs");
const M = require("../site/js/mkx.js");

const lines = fs.readFileSync(0, "ascii").split("\n");
const hex = line => Uint8Array.from(Buffer.from(line, "hex"));
const depths = Array.from({length: 21}, (_, i) => i).concat(3000);
for (const n of depths) {
  const [length, crc] = lines.shift().split(" ");
  const source = Uint8Array.from({length: n}, (_, i) => (i * 173 + 0xb5) & 255);
  const packed = hex(lines.shift()), frame = hex(lines.shift());
  assert.strictEqual(+length, n);
  assert.strictEqual(M.crc32(source), parseInt(crc, 16));
  assert.deepStrictEqual(M.pack(source), packed);
  assert.deepStrictEqual(M.unpack(packed), source);
  assert.deepStrictEqual(M.frameBuild(6, 127, source), frame);
  for (const wire of [frame, frame.subarray(1, frame.length - 1)]) {
    assert.deepStrictEqual(M.frameParse(wire), {cmd: 6, seq: 127, payload: source});
  }
  frame[8] ^= 1;
  assert.throws(() => M.frameParse(frame), /CRC/);
  frame[8] ^= 1;
  frame[frame.length - 1] = 0;
  assert.throws(() => M.frameParse(frame), /terminator/);
}
assert.throws(() => M.unpack(Uint8Array.of(2, 0)), /high bits/);
assert.throws(() => M.unpack(Uint8Array.of(0, 128)), /low bits/);
assert.throws(() => M.frameBuild(1, 0, new Uint8Array(3001)), /frame fields/);

for (let lang = 0; lang < 2; lang++) {
  const rec = hex(lines.shift()), p = M.phraseDecode(rec);
  assert.strictEqual(p.lang, lang);
  assert.strictEqual(p.name, "MIKU 01     ");
  assert.strictEqual(p.bpm, 136);
  assert.strictEqual(p.deflen, 3);
  assert.strictEqual(p.flags, 1);
  assert.strictEqual(p.s[0].note, 66);
  if (lang === 0) {
    assert.strictEqual(p.text, "Hello, Miku!");
    assert.strictEqual(p.s[0].ph[3], 40);
    assert.strictEqual(p.s[2].ph[3], 11);
  } else {
    assert.strictEqual(p.text, "mi-ku:2 _:2 | a!");
    assert.strictEqual(p.s[2].spell, "_");
    assert.strictEqual(p.s[2].ph[0], p.s[1].ph[0]);
    assert.strictEqual(p.s[3].spell, "|");
    assert.strictEqual(p.s[3].ph[0], 0);
  }
  assert.deepStrictEqual(M.phraseEncode(p), rec);
  rec[928] = 1;
  assert.throws(() => M.phraseDecode(rec), /CRC/);
  new DataView(rec.buffer).setUint32(1020, M.crc32(rec.subarray(0, 1020)), true);
  assert.throws(() => M.phraseDecode(rec), /fields/);
  p.s[0].ph[0] = lang === 0 ? 41 : 73;
  assert.throws(() => M.phraseEncode(p), /syllable fields/);
}
assert.deepStrictEqual(lines, [""]);
console.log("MKX JS matches C for 22 binary payloads and two phrase records");
