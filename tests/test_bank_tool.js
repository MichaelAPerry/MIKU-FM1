// SPDX-License-Identifier: GPL-3.0-only
// site/js/dx7syx.js against vectors from the firmware's C code (tests/dx7_vectors.c). Usage: node test_bank_tool.js vectors.json factory.syx
"use strict";
const fs = require("fs"), path = require("path"), assert = require("assert");
const D = require(path.join(__dirname, "..", "site", "js", "dx7syx.js"));
const vec = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
const factory = new Uint8Array(fs.readFileSync(process.argv[3]));
const eq = (a, b, what) => assert.deepStrictEqual(Array.from(a), Array.from(b), what);
let valid = 0, invalid = 0;

eq(D.initVoice(), vec.init, "INIT VOICE");
vec.voices.forEach((v, n) => {
  eq(D.packedToVced(Uint8Array.from(v.packed)), v.vced, `packed->VCED, voice ${n}`);
  eq(D.vcedToPacked(Uint8Array.from(v.randomVced)), v.randomPacked, `VCED->packed, voice ${n}`);
  const t = Uint8Array.from(v.packed); D.mask(t);
  assert.strictEqual(D.patchValid(t), !!v.valid, `dx_patch_valid, voice ${n}`);
  v.valid ? valid++ : invalid++;
});
assert(valid >= 11 && invalid >= 5, `the vectors cover both valid (${valid}) and invalid (${invalid}) voices`);

// the bulk message the C code builds is the shipped factory file, and our own builder makes it too
const bulk = Uint8Array.from(vec.bulkMessage);
eq(bulk, factory, "factory .syx == dxs_build_bulk of the padded factory bank");
const r = D.parseFile(factory);
assert(r.ok && r.kind === "bulk" && r.voices.length === 32, "factory file parses to 32 voices");
const names = r.voices.map(D.voiceName);
assert.deepStrictEqual(names.slice(0, 10), ["HYPER GLAS", "HYPER 808", "FUTURE SAW", "FUTURE SUB", "CLUB STAB", "CLUB BOUNC", "DIGI PLUCK", "DIGI GROWL", "IDOL KEYS", "IDOL BASS"]);
assert(names.slice(10).every((s) => s === "INIT VOICE"), "voices 11..32 are INIT VOICE");
eq(D.buildBulk(D.packBank(r.voices), 0), bulk, "buildBulk");

// the single-voice message from the C side parses to the voice it was made from, on channel 6; ours builds the same bytes
const sm = Uint8Array.from(vec.singleMessage);
const rs = D.parseFile(sm);
assert(rs.ok && rs.kind === "single" && rs.channel === 5 && rs.voices.length === 1, "single voice parses");
eq(rs.voices[0], vec.singleVoice, "single voice content");
eq(D.buildSingle(Uint8Array.from(vec.singleVoice), 5), sm, "buildSingle");

// every factory voice survives packed -> VCED -> packed
for (const v of r.voices) eq(D.vcedToPacked(D.packedToVced(v)), v, "round trip");

// errors are reported, never thrown
const bad = new Uint8Array(bulk); bad[100] ^= 1;
let e = D.parseFile(bad); assert(!e.ok && /checksum/.test(e.errors.join()), "bad checksum");
e = D.parseFile(bulk.subarray(0, 3000)); assert(!e.ok && e.errors.length, "cut short");
e = D.parseFile(new Uint8Array([1, 2, 3])); assert(!e.ok && e.errors.length, "no sysex");
e = D.parseFile(new Uint8Array(0)); assert(!e.ok && e.errors.length, "empty");
e = D.parseFile(new Uint8Array([0xF0, 0x41, 1, 2, 0xF7])); assert(!e.ok, "not Yamaha");
const wrongLen = Uint8Array.from([0xF0, 0x43, 0, 9, 0x20, 0, 1, 2, 3, 0xF7]); e = D.parseFile(wrongLen); assert(!e.ok && /length/.test(e.errors.join()), "wrong length");
const badVoice = new Uint8Array(bulk); badVoice[6 + 11 * 128 + 5] = 120; badVoice[badVoice.length - 2] = D.checksum(badVoice, 6, 6 + 4096);
e = D.parseFile(badVoice); assert(!e.ok && /voice 12/.test(e.errors.join()), "invalid voice 12");

// a raw packed bank: the 4096 bytes, or the 1280-byte factory.packed (padded)
const raw = new Uint8Array(4096); r.voices.forEach((v, k) => raw.set(v, k * 128));
e = D.parseFile(raw); assert(e.ok && e.source === "bin" && e.voices.length === 32, "raw bank");
e = D.parseFile(raw.subarray(0, 1280)); assert(e.ok && e.voices.length === 32 && D.voiceName(e.voices[10]) === "INIT VOICE", "short raw bank is padded");
const junk = new Uint8Array(4096).fill(200); e = D.parseFile(junk); assert(!e.ok, "junk raw bank");
console.log(`bank tool JS: ${vec.voices.length} voices, ${valid} valid / ${invalid} invalid, all vectors match`);
