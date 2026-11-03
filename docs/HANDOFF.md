# Handoff (2026-11-03, after Step 3 / firmware 0.5)

## Where things stand

- **main** at the merge of PR #6. Firmware **0.5** (identity `FM-1_39000500`), built on Dinghy `f77bf03`
  (GPL-3.0-only). Not yet tested on a physical FM-1 and never flashed. No GitHub release made yet.
- Site (GitHub Pages, dev build from main, labelled "not attested"): https://michaelaperry.github.io/MIKU-FM1/
  with fm1-emu at `/fm1-emu/`.
- Done: Step 0 (identity, licence, release/pages workflows, docs), Step 1 (integer formant voice `fvoice`, singer
  sprites, STAGE face), 0.3 (level, OCT transpose), Step 2 (four-track songwriter: `firmware/song/`,
  `firmware/audio/fmband/`, `firmware/app/engine.c`, SONG/PATTERN/MIXER pages, 8 song slots), Step 3 (Japanese
  consonants in `fvoice`, ん hum, lips-shut face).
- Owner decisions: Dinghy platform; name Miku-FM; pitch variant A (transpose 0); the ChatGPT Miku-inspired art is the
  main art (unofficial, non-commercial fan work, `assets/NOTICE.md`); the agent merges PRs itself.

## Open bug (top priority): white-noise crackles on some consonants

The owner hears **crackles like white noise on some sounds that are hard to tell apart**, in the Step 3 renders
(`tools/mora_render` all 70 morae on note 72; `tools/song_render` the demo). Not yet diagnosed. Suspects, in order:

1. The consonant noise band (`fvoice.c`, the noise resonator(s) driven by the LFSR) clipping or wrapping: a high-Q
   2-pole band at full-scale noise, the `clampi(y,-32767,32767)` per resonator turning overload into hard-clipped
   crackle, or int32 overflow in `g*x + a1*y1 + a2*y2` before the shift.
2. Abrupt gating: noise switched on/off (or level/coefficient changes applied per 16-frame block) without a ramp,
   giving clicks at block edges. Same for the plosive closure (digital silence) cutting in and out.
3. Aspiration / glottal-phase-gated noise (`nz = noise * |a|`) producing buzzy spikes when `a` has a sharp edge.
4. The final limiter (`mx_limit`, hard cap 32767) or the app gain ×1.5 in `firmware/app/engine.c` hitting the cap
   on fricative peaks.

Already ruled out (2026-11-03, measured): no output clipping in `morae.wav`; inside the voice the six resonators,
the noise band and the resonator sum clamp on only 3 samples in total over all 69 morae at notes 48/72/84 (on "hi"),
so internal clamping is not the cause. A click-spike count on per-track solo renders of the demo
(`song_render --solo …`, 22.5 s) gave chords 0, vocal 54, bass 83 (chip-square bass: naive square edges, a likely
aliasing "crackle" source), drums 143 (hats/snare from naive squares and one-pole-filtered noise). Ask the owner
which solo file crackles before changing code. Next suspects if it is the voice: noise-band coefficients swapped
in one step at consonant phase changes (`enter()` sets `nbc` while `ny1/ny2` keep old state), the glottal-gated
breath noise, the non-band-limited glottal pulse (aliasing at high notes). If bass/drums: band-limit the squares
(polyBLEP or wavetables per octave) and use 2-pole noise filters.

How to find it: render each mora alone (`tools/mora_render`), and for each one measure (a) the count of
samples at or within 1% of ±32767 at every stage (per resonator before clamp, voice out, mix out), (b) the largest
sample-to-sample jump relative to the local RMS, (c) energy above 10 kHz in 5 ms windows, (d) jumps that line up
with 16-frame block boundaries. Rank the morae and fix the cause, not the symptom (ramps per sample for level and
noise gain, lower noise drive / Q, headroom before the clamp, soft saturation instead of hard clamp). Add a
regression test: for all 70 morae × notes 48/72/96, no clamp hits inside the voice and no block-edge jumps above
a threshold. Then send the owner new `morae.wav`, `verse_vocal_solo.wav` and `otonohako_sung.wav` and ask which
syllables still crackle.

## How to build, test, run (verified recipe)

- Host: `cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release && cmake --build build/host && ctest --test-dir build/host`
  (11 tests; keep 0 warnings). Tables: `python3 tools/gen_fvoice_tables.py`, `python3 tools/gen_fmband_tables.py`
  (must leave no diff); sprites: `python3 -I tools/sprite_pack.py --check`.
- Renders: `build/host/firmware/app/song_render out.wav [--solo vocal|chords|bass|drums] [--rows A B]`;
  `tools/mora_render` (all morae); `fvoice_demo [-t semitones]`.
- Device: JieLi toolchain `curl -fL https://pkgman.jieliapp.com/s/linux-toolchain | tar -xJ` (clang 4.0.1); SDK: only
  `cpu/wl82/tools/{uboot.boot,cfg_tool.bin,cfg/eq_cfg_hw.bin}` from gitee fw-AC79_AIoT_SDK at `d179b44` via raw
  links (retry; the sandbox resets connections; a full clone fails there). Then
  `JIELI_TOOLCHAIN=… AC79_SDK=… bash native/build.sh [--release X.Y]` → `build/native/miku-fm[-X.Y].fwsc`;
  Dinghy's size/register checks must print ok.
- Emulator: `git clone https://github.com/joakimeriksson/fm1-emu` at `9de1ef0`, `cargo build --release -p fm1-emu`;
  run from a dir with `work/` (snapshots fail silently without it); `tools/emu_smoke.sh PKG OUT EMU` is the CI gate.
  Save/reboot needs a local 2-line patch (FM1_FLASH_OUT dumps the chip at the end, FM1_FLASH_IN calls
  `restore_flash` after `take_boot`); never commit or upstream it. It is slow (~30–60 s per emulated second).
- Device rules: no float/double/libm, no 64-bit division (avoid 64-bit multiplies), no libc at all, no heap; audio
  ISR 5.8 ms per 256 frames for everything (keep under half); per-block (16 frames) control work; no screen/flash
  in the ISR. Emulator CPU% is not a device figure.

## Workflow the owner set

- Coordinator plus subagents (haiku for lookups, sonnet for self-contained coding with gates). Agents that edit the
  same files run one after another. Agents commit (by explicit path) but don't push; the coordinator verifies
  (ctest, table/sprite checks, device build, emu_smoke, screenshots) then pushes `claude/sharp-archimedes-iajisp`,
  opens a PR and merges it. Restart the branch from origin/main after each merge.
- Don't commit an agent's files while it is mid-edit to silence a stop hook; wait.
- Logs to the scratchpad; show tail/grep. Keep the terse code style; change few lines. Short replies, findings first.
- Every feature: host tests, device build, fm1-emu run, then docs (CONTROLS, MANUAL, FEATURES, VALIDATION).
- **Hardware: never flash or tell the owner to flash without their explicit go-ahead in that session.** One flash
  bank. Dinghy recovery: hold OCT− at power-on (USB rescue); two failed boots go there by themselves.

## Next, after the crackle fix

1. Owner listening pass on consonants (h, fu, z, j, nasals, fricative level); consonant look-ahead so the vowel lands
   on the beat (the sequencer can start VOCAL notes `fv_cons_ms()` early).
2. Step 4: generator (styles, key/scale, seeds, track locks, regenerate a phrase, accept/reject; all output editable).
3. First hardware test with the owner (install from V15, from another firmware, install another firmware from it;
   measure CPU% on STAGE; save across power cycles). Then a tagged release (v0.x → attested via zvenson/fm1-build),
   the `github-pages` environment tag rule `v*`, and the Firmware Hub mail (fmhub.json is in place).
4. Later: browser companion editor (MKS2 import/export, piano roll, lyrics), MIDI, pan in the song format.
