// SPDX-License-Identifier: GPL-3.0-only
const Editor = (() => {
  "use strict";
  const codec = typeof module === "object" ? require("../js/mkx.js") : MKX;
  const decoder = new TextDecoder();
  const encoder = new TextEncoder();
  const phones = "AA AE AH AO AW AY EH ER EY IH IY OW OY UH UW B CH D DH F G HH JH K L M N NG P R S SH T TH V W Y Z ZH AX".split(" ");
  const arpa = s => s.ph.filter(Boolean).map(p => phones[p - 1]).join(" ");
  const fromBase64 = value => Uint8Array.from(atob(value), c => c.charCodeAt(0));
  function toBase64(bytes) {
    let value = "";
    for (let i = 0; i < bytes.length; i += 8192) value += String.fromCharCode(...bytes.subarray(i, i + 8192));
    return btoa(value);
  }

  function dictionary(text) {
    const words = new Map();
    for (const line of text.split("\n")) {
      const match = /^([^ ]+) +([^#\r\n]+)/.exec(line);
      if (match) words.set(match[1], match[2].trim());
    }
    return words;
  }

  function writeString(memory, address, text) {
    const bytes = encoder.encode(text);
    memory.set(bytes, address);
    memory[address + bytes.length] = 0;
  }

  function validateOverrides(input) {
    if (!input || typeof input !== "object" || Array.isArray(input)) throw Error("Invalid pronunciation overrides");
    const result = Object.create(null);
    for (const [word, value] of Object.entries(input)) {
      if (!/^[a-z0-9'][a-z0-9'-]{0,46}$/.test(word) || typeof value !== "string" || value.length > 379) {
        throw Error("Invalid pronunciation override");
      }
      const tokens = value.trim().toUpperCase().split(/\s+/);
      if (tokens.length > 95 || !tokens.every(token => {
        const phone = phones.indexOf(token.replace(/[012]$/, ""));
        return phone >= 0 && phone < 39 && (phone < 15 || !/[012]$/.test(token));
      })) throw Error("Use valid ARPAbet phones, with optional vowel stress 0, 1 or 2.");
      result[word] = tokens.join(" ");
    }
    return result;
  }

  async function load(wasm, words) {
    let api, memory, overrides;
    const imports = {env: {
      ph_external_lookup(address) {
        const word = decoder.decode(memory.subarray(address, memory.indexOf(0, address)));
        const value = overrides[word] || words.get(word);
        if (!value) return 0;
        const pointer = api.ed_pronunciation();
        writeString(memory, pointer, value);
        return pointer;
      }
    }};
    ({instance: {exports: api}} = await WebAssembly.instantiate(wasm, imports));
    memory = new Uint8Array(api.memory.buffer);
    const readRecord = () => memory.slice(api.ed_record(), api.ed_record() + 1024);
    const putRecord = record => memory.set(record, api.ed_record());
    function compile(text, lang, deflen, replacements = {}) {
      const bad = /[^ -~]/.exec(text);
      if (bad || text.length > 128) {
        const offset = Math.min(bad ? encoder.encode(text.slice(0, bad.index)).length : 128, 128);
        throw Object.assign(Error("Use up to 128 printable ASCII characters."), {offset, kind: "ascii"});
      }
      overrides = validateOverrides(replacements);
      writeString(memory, api.ed_input(), text);
      if (api.ed_compile(lang, deflen)) {
        throw Object.assign(Error("Compilation failed"), {offset: api.ed_error_at(), kind: lang ? "romaji" : "english"});
      }
      return codec.phraseDecode(readRecord());
    }
    function place(phrase, character, reverb, syllable = -1) {
      putRecord(codec.phraseEncode(phrase));
      const rows = api.ed_place(character, reverb, syllable);
      if (rows < 1) throw Error("Add syllables and keep the phrase within 256 sixteenths, with at most 64 per note.");
      return rows;
    }
    return {
      compile,
      roundtrip(record) {
        codec.phraseDecode(record);
        putRecord(record);
        if (api.ed_roundtrip()) throw Error("Firmware rejected the phrase record.");
        return readRecord();
      },
      song(phrase, character, reverb) {
        place(phrase, character, reverb);
        const size = api.ed_encode_song();
        if (!size) throw Error("Could not encode the song.");
        return memory.slice(api.ed_song(), api.ed_song() + size);
      },
      render(phrase, character, reverb, syllable = -1) {
        place(phrase, character, reverb, syllable);
        const units = (syllable < 0 ? phrase.s : [phrase.s[syllable]])
          .reduce((sum, s) => sum + ((s.len & 127) || phrase.deflen), 0);
        const frames = Math.ceil(units * 661500 / phrase.bpm) + 44100;
        const channels = [new Float32Array(frames), new Float32Array(frames)];
        api.ed_start();
        for (let start = 0; start < frames; start += 256) {
          const count = Math.min(256, frames - start);
          const audio = new Int32Array(memory.buffer, api.ed_render(count), 2 * count);
          for (let i = 0; i < count; i++) {
            for (let c = 0; c < 2; c++) channels[c][start + i] = Math.max(-1, Math.min(1, audio[i * 2 + c] / 32768));
          }
        }
        return channels;
      }
    };
  }

  // The binary record is authoritative. Loading never recompiles text or pronunciations.
  function fileEntry(entry) {
    return entry === null ? null : {record: toBase64(entry.record), overrides: entry.overrides};
  }

  function serialize(bank, slot = null) {
    const body = slot === null ? {slots: bank.map(fileEntry)} : {slot, phrase: fileEntry(bank[slot])};
    return JSON.stringify({format: "mikuphr", version: 1, ...body}, null, 2);
  }

  function deserialize(text) {
    const file = JSON.parse(text);
    if (file?.format !== "mikuphr" || file.version !== 1) throw Error("Unsupported .mikuphr format");
    function entry(value) {
      if (value === null) return null;
      if (typeof value?.record !== "string" || value.record.length !== 1368) throw Error("Invalid MPH1 record");
      const record = fromBase64(value.record);
      codec.phraseDecode(record);
      return {record, overrides: validateOverrides(value.overrides)};
    }
    if (Array.isArray(file.slots) && file.slots.length === 64) return {bank: file.slots.map(entry)};
    if (Number.isInteger(file.slot) && file.slot >= 0 && file.slot < 64) return {slot: file.slot, phrase: entry(file.phrase)};
    throw Error("Invalid .mikuphr bank or slot");
  }

  async function embedded(doc) {
    const packed = fromBase64(doc.getElementById("dictionary").textContent.trim());
    const stream = new Blob([packed]).stream().pipeThrough(new DecompressionStream("gzip"));
    const words = dictionary(await new Response(stream).text());
    return load(fromBase64(doc.getElementById("voiceWasm").textContent.trim()), words);
  }

  return {load, embedded, dictionary, arpa, validateOverrides, serialize, deserialize, fromBase64, toBase64};
})();
if (typeof module === "object") module.exports = Editor;
