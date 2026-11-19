// SPDX-License-Identifier: GPL-3.0-only
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const MKX = require("../site/js/mkx.js");
const Editor = require("../site/editor/core.js");
const MIDI = require("../site/editor/midi.js");

const page = fs.readFileSync(path.resolve(__dirname, "../site/editor.html"), "utf8");
const [reference, output] = process.argv.slice(2);
const doc = {getElementById: id => ({textContent: page.match(new RegExp(`id="${id}"[^>]*>([^<]+)`))[1]})};
const bytes = hex => Uint8Array.from(Buffer.from(hex, "hex"));
const vlq = value => value < 128 ? [value] : [0x80 | (value >> 7), value & 127];
function chunk(type, data) {
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length);
  return [...Buffer.from(type), ...length, ...data];
}
const meta = (tick, kind, text) => [...vlq(tick), 0xff, kind, text.length, ...Buffer.from(text)];
const eot = [0, 0xff, 0x2f, 0];

function midi(lyricKind, fragments) {
  return Uint8Array.from([
    ...chunk("MThd", [0, 1, 0, 2, 0x01, 0xe0]),
    ...chunk("MTrk", [0, 0xff, 0x51, 3, 0x09, 0x27, 0xc0, ...eot]),
    ...chunk("MTrk", [
      ...meta(0, 3, "Voice"), ...meta(0, lyricKind, fragments[0]),
      0, 0x90, 65, 90, ...vlq(120), 65, 0,
      ...meta(0, lyricKind, fragments[1]),
      0, 0x90, 67, 90, ...vlq(240), 0x80, 67, 0,
      ...eot
    ])
  ]);
}

async function main() {
  assert(Buffer.byteLength(page) < 3000000);
  assert(!/<(?:script|link|img)\b[^>]+(?:src|rel="stylesheet")=/i.test(page));
  const engine = await Editor.embedded(doc);
  const phrase = engine.compile("Hello world, I sing for you", 0, 4);
  const expected = bytes(fs.readFileSync(reference, "utf8"));
  assert.deepEqual(MKX.phraseEncode(phrase), expected);
  assert.deepEqual(phrase.s.map(Editor.arpa), ["HH AX", "L OW", "W ER L D", "AY", "S IH NG", "F AO R", "Y UW"]);

  const jp = engine.compile("mi-ku:3 _:2 / a! | i", 1, 2);
  assert.deepEqual(jp.s.map(s => s.spell), ["mi", "ku", "_", "/", "a!", "|", "i"]);
  assert.throws(() => engine.compile("mi xyz", 1, 4), {offset: 3, kind: "romaji"});
  assert.throws(() => engine.compile("a ".repeat(49).trim(), 0, 4), {offset: 96, kind: "english"});

  const overrides = {hello: "HH IY1"};
  const changed = engine.compile(phrase.text, 0, 4, overrides);
  assert.equal(Editor.arpa(changed.s[0]), "HH IY");
  assert.equal(changed.n, phrase.n - 1);
  assert.notEqual(Editor.arpa(engine.compile("xylophone", 0, 4).s[0]), "");
  changed.name = "TEST PHRASE ";
  changed.flags = 1;
  changed.s[0].note = 69;
  changed.s[0].len |= 3;
  const record = MKX.phraseEncode(changed);
  const frame = MKX.frameBuild(3, 31, Uint8Array.of(17, ...record));
  const wire = MKX.frameParse(frame).payload.subarray(1);
  assert.deepEqual(MKX.phraseDecode(wire), changed);
  assert.deepEqual(engine.roundtrip(wire), record);
  fs.writeFileSync(output, Buffer.from(wire).toString("hex"));

  const bank = Array(64).fill(null);
  bank[17] = {record, overrides};
  for (const selected of [null, 17]) {
    const exported = Editor.serialize(bank, selected);
    const imported = Editor.deserialize(exported);
    const entry = imported.bank?.[17] || imported.phrase;
    assert.deepEqual(entry.record, record);
    assert.deepEqual({...entry.overrides}, overrides);
    assert.equal(Editor.serialize(imported.bank || bank, imported.slot ?? null), exported);
  }
  assert.throws(() => Editor.deserialize('{"format":"mikuphr","version":2}'));
  assert.throws(() => Editor.validateOverrides({hello: "W1 HH"}));

  for (const [p, index] of [[changed, -1], [changed, 0], [jp, -1]]) {
    const rendered = engine.render(p, 2, 20, index);
    const lengths = (index < 0 ? p.s : [p.s[index]]).reduce((total, s) => total + ((s.len & 127) || p.deflen), 0);
    assert.equal(rendered[0].length, Math.ceil(lengths * 661500 / p.bpm) + 44100);
    for (const channel of rendered) {
      assert(channel.every(x => Number.isFinite(x) && Math.abs(x) <= 1));
      assert(channel.some(x => Math.abs(x) > 0.01));
    }
  }
  const song = engine.song(changed, 2, 20);
  assert.equal(Buffer.from(song.subarray(0, 4)).toString(), "MKS2");
  assert.equal(new DataView(song.buffer).getUint32(song.length - 4, true), MKX.crc32(song.subarray(0, -4)));

  const source = midi(5, ["Hel-", "lo"]);
  for (const input of [source, midi(1, ["/Hel-", "\\lo"])]) {
    const parsed = MIDI.parse(input);
    assert.equal(parsed.tracks[1].name, "Voice");
    const result = MIDI.convert(parsed, 1, engine.compile, 4);
    assert.equal(result.fallback, false);
    assert.equal(result.phrase.bpm, 100);
    assert.equal(result.phrase.text, "Hello");
    assert.deepEqual(result.phrase.s.map(s => [s.spell, s.note, s.len & 127]), [["Hel", 65, 1], ["lo", 67, 2]]);
  }
  const fallback = MIDI.convert(MIDI.parse(midi(1, ["World", "you and I"])), 1, engine.compile, 4);
  assert.equal(fallback.fallback, true);
  assert.deepEqual(fallback.phrase.s.map(s => s.spell), ["World", "you a"]);
  const many = MIDI.parse(source);
  many.tracks[1].notes = Array(60).fill(many.tracks[1].notes[0]);
  assert.equal(MIDI.convert(many, 1, engine.compile, 4).phrase.n, 48);
  assert.throws(() => MIDI.parse(source.subarray(0, -1)), /Truncated/);
  console.log("Offline editor: host codecs, WASM compiler/voice, MIDI/KAR and .mikuphr passed");
}
main().catch(error => { console.error(error); process.exitCode = 1; });
