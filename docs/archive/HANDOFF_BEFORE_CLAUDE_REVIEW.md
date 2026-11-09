# Historical handoff archive — not current instructions

Archived on 2026-11-09. Read [the current handoff](../../CLAUDE_HANDOFF.md). Dates, feature status, measurements and merge instructions below are historical and may conflict with the current owner instructions. In particular, there is no authorization to merge automatically.

---

# Compact visuals and USB-video follow-up (2026-11-08)

Latest branch work: default native now uses lossless compact portrait+dance artwork, saving32512B linked image space. Optional `native/build.sh --video` builds MIDI+UVC160x120 MJPEG with incremental integer JPEG, one8KiB frame and no extra framebuffer/core1 job; update loader retains original USB code. Video profile excludes UAC/CDC and has not been enumerated on a physical USB host. Default and optional packages pass native/emulator smoke;25 host tests plus patched-driver fake-SIE tests pass.

Read [NEXTGEN_MEMORY.md](NEXTGEN_MEMORY.md) for exact linked budgets/reserves and [NEXTGEN_VIDEO_NATIVE.md](NEXTGEN_VIDEO_NATIVE.md) for adapter validation/limits. `--video-bench` is an emulator-only3-frame encoder benchmark, not a camera-stream build. Wardrobe revision3 corrects07 to Signal Suit and adds11 Moon Circuit witch; concepts only, matched production atlases still pending. Keep all work on the feature branch/draft PR; no release, merge, deployment or flash.

---

# Next-generation feature branch handoff (2026-11-08)

Work is on `feature/miku-nextgen-performance`, based on main `88b0ef3`; keep it separate until owner integration approval.
Read [NEXTGEN_DEVELOPMENT.md](NEXTGEN_DEVELOPMENT.md) for per-phase implementation, measurements, limitations and owner decisions.

Native-ready: silent STAGE/DANCE SELECT; slower adjustable ANIMAL; added bass/drum presets; shared default dance state and fixed odd-width sprite decoding. Voice compensation is opt-in API-only. Host prototypes: editable lyric compiler, five GEN candidates, procedural visual concepts, stereo pedal DSP, MIDI-safe performance codec and recorded-state companion. Live USB audio/performance transmission and UVC are not implemented.

Validation: host Release 19/19; tables/sprites unchanged; native image185208B/static35540B/pool254784B; final fm1-emu smoke and targeted SELECT/held-note/rate/mixer/dance checks pass. Emulator ISR8.6% smoke/10.3% DANCE, not device measurements. Source hashes and generated audio/screens under `build/evidence/`; `python3 tools/nextgen_review.py` creates a local review index. No release, main merge, live-site change or hardware flash occurred.

Next: owner listening/design review; move experimental GEN scratch from5032B stack to explicit workspace before native integration; implement bounded live lyric player; prototype Dinghy UAC1 input/feedback/rings before pedal DSP with the performance-events companion as recommended first renderer. See individual NEXTGEN documents for exact interfaces and unfinished work. Physical measurements require explicit owner authorization.

---

# Handoff (2026-11-04, release 0.6)

## Where things stand

- **Release v0.6** (2026-11-04): published on GitHub, built and attested by zvenson/fm1-build (`miku-fm-0.6.fwsc`,
  identity `FM-1_39000600`, update loader in). Tested by the owner on a real FM-1: installed from stock V15, from another
  firmware, and back to another firmware; saves work; CPU well under budget. The site carries the latest release's
  attested package (pages.yml: a push to main uses it; a dev build only while no release exists).
- Listing: `fmhub.json` and `docs/card.png` current; mail to zven@thefirmwarehub.com and an fm1-editor.com issue drafted
  for the owner. Licensing: code GPL-3.0-only, the singer is Hatsune Miku fan art under the PCL (`LICENSING.md`).

**Read [PROJECT_STATE.md](PROJECT_STATE.md) too:** every asset and its intended use (e.g. the packed but unused dancing
sprites), and the original master prompt checked item by item (done / partial / missing).


- **main** at the merge of the Step 4 PR (generator), after PR #9 (crackle fix). Firmware **0.5** (identity `FM-1_39000500`), built on Dinghy `f77bf03`
  (GPL-3.0-only). Not yet tested on a physical FM-1 and never flashed. No GitHub release made yet.
- Site (GitHub Pages, dev build from main, labelled "not attested"): https://michaelaperry.github.io/MIKU-FM1/
  with fm1-emu at `/fm1-emu/`.
- Done: Step 0 (identity, licence, release/pages workflows, docs), Step 1 (integer formant voice `fvoice`, singer
  sprites, STAGE face), 0.3 (level, OCT transpose), Step 2 (four-track songwriter: `firmware/song/`,
  `firmware/audio/fmband/`, `firmware/app/engine.c`, SONG/PATTERN/MIXER pages, 8 song slots), Step 3 (Japanese
  consonants in `fvoice`, ん hum, lips-shut face), crackle fix (plosive bursts), Step 4 (song generator, GEN page:
  `firmware/song/gen.c`, `song_render --gen`).
- Owner decisions: Dinghy platform; name Miku-FM; pitch variant A (transpose 0); the ChatGPT Miku-inspired art is the
  main art (unofficial, non-commercial fan work, `assets/NOTICE.md`); the agent merges PRs itself.

## Crackle bug: fixed (2026-11-03, this branch)

Owner report: white-noise crackle in the vocal solo only, on roughly every other syllable. Measured on `song_render --solo vocal`
with an instrumented scratch copy of `fvoice.c` (logs note-ons, phase entries and filter resets with sample positions) and
three detectors (>16 kHz ticks, 2-10 kHz 1 ms energy vs a 30 ms median, LPC-residual outliers vs block phase):
- All 20 broadband events were inside consonants, none in vowels; 15 of them in the release-burst phase of k and t, the rest
  nasal shape changes. Turning the noise band off removed 20 of the 21 strong >16 kHz events; breath noise and aspiration
  did not matter. Every demo note starts over the previous note's release (none start fresh).
- Ruled out: clamping (0 hits at every stage in the demo), block edges (outliers evenly spread over the 16 block positions),
  note-start filter resets, the `nbc` swap with carried `ny1/ny2` (no event at the ts T->S switch), the gated breath noise
  (only the per-period glottal edge at about -40 dB, normal buzz).
- Cause: each k/t/p release burst was a 6-7 ms block of flat noise at the vowel's own level (0 dB; up to +5 dB on high
  notes, where the vowel is quieter; +8 dB for "ka" at note 96), with a 0.8 ms fade-in so it even grew over the burst.
- Fix (`gen_fvoice_tables.py` `bst=1`, `fvoice.c`): a burst phase reaches its level within one block, then decays
  exponentially to the next phase's noise (to 25% when a vowel follows) by its end; k/t/p burst starts 0.55/0.50/0.50 -> 0.32.
  Demo bursts now -6 to -11 dB re the vowel. Regression test `t_burst` (k t p x 5 vowels x notes 48/72/96 x fresh / over a
  release): the old voice fails it 67 times.
- Open, separate: the vowel level swings with pitch ("a" is 19 dB down at note 96, 8 dB at 78: harmonics miss F1), so "ta"
  on note 76 in the demo is still only -2 dB under its vowel. A per-vowel loudness table against pitch would fix that.
  Bass and drum crackle counts (naive squares) are unchanged; the owner hears them as clean.

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

## Next

1. Step 4 generator: done (GEN page, `firmware/song/gen.c`, see VALIDATION). Owner listening pass on the generated songs
   (`song_render --gen STYLE KEY SCALE SEED`); ideas: more styles and progressions, a bridge, melody motifs, real lyrics.
2. Owner listening pass on consonants (h, fu, z, j, nasals, fricative level); consonant look-ahead so the vowel lands
   on the beat (the sequencer can start VOCAL notes `fv_cons_ms()` early).
3. First hardware test with the owner (install from V15, from another firmware, install another firmware from it;
   measure CPU% on STAGE; save across power cycles). Then a tagged release (v0.x → attested via zvenson/fm1-build),
   the `github-pages` environment tag rule `v*`, and the Firmware Hub mail (fmhub.json is in place).
4. Later: browser companion editor (MKS2 import/export, piano roll, lyrics), MIDI, pan in the song format.
