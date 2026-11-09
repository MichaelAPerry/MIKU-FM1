# PR21 independent review (Claude Code)

PR: https://github.com/MichaelAPerry/MIKU-FM1/pull/21 — `feature/miku-nextgen-performance` → `main`, draft.
Review date: 2026-11-09. This document is review output only. No implementation file on the PR branch was
changed, and nothing was merged, pushed to `main`, released, deployed or flashed.

## 1. What was reviewed

| Item | Value |
|---|---|
| Reviewed head | `33d2115c6d0146a67322b300f4faca6cdcf350f5` ("Add independent Claude Code review handoff for PR21…") |
| Latest implementation commits | `2bb1662` (DX7 renderer/BANK), `c394043` (evidence/handoff) |
| Base | `88b0ef3e568cf615eb5dfaa42d170d8c0f75d431` (current `main`; merge base equals it) |
| Size | 139 commits, 348 files, +38159/−343 against the base |
| Working tree | Clean detached worktree of the head. After every generator the tree was still clean (`git status` empty) |
| Remote state | Branch head still `33d2115` at the end of the review. GitHub `test` check runs on the head: 2/2 success (host CMake/CTest only) |

Read first, as asked: `CLAUDE_CODE_REVIEW.md`, `docs/DX7_BANK_HANDOFF.md`, `firmware/audio/dx7/README.md`,
`assets/dx7/README.md`, `LICENSING.md`. Then the authored source: `firmware/audio/dx7/*`, `firmware/app/engine.{c,h}`,
`firmware/song/{song.h,song_io.c,dx7_edit.c,seq.c,gen.c,gen_next.c}`, `native/dinghy.c` (storage, keys, MIXER, BANK),
`native/bank_ui.inc`, `tools/patch_dinghy_slots.py`, `native/build.sh`, and the workflow changes. Also read: the pinned
Dinghy `f77bf03` storage and flash-ID code (`platform.c`, `store.h`, `fm1_flash_map.h`, `recovery.c`, `loader.c`) and the
pinned Dexed `fea2f0f` `README.md`, `LICENSE` and `assets/builtin_pgm.zip`.

Generated sprite tables, wardrobe and art pipelines, the companion, the site, and the UVC/JPEG code were covered only
by their generator checks and CTest. I did not audit them line by line (see §6).

## 2. Findings, ranked

Severity: **High** = blocks integration until the owner decides or it is fixed. **Medium** = wrong behaviour that users
can reach, or data loss. **Low** = edge case, polish, or documentation accuracy. File/line references are at `33d2115`.

### F1 — High (licensing/evidence): no documented redistribution basis for the eight DX7 programs

- **Where:** `assets/dx7/README.md:7`, `LICENSING.md:80`; data in `assets/dx7/factory.packed` and `firmware/audio/dx7/bank.h`.
- **Claim:** the programs are "GPLv3-distributed" because they sit inside Dexed's GPLv3 repository.
- **What I checked:** Dexed's `README.md` at `fea2f0f` licenses *Dexed* under GPLv3 and MSFA under Apache-2.0. For the
  programs it gives only credits: "DX7 program compilation: Jean-Marc Desprez (SynprezFM)" and a list of eleven program
  authors. `builtin_pgm.zip` holds `SynprezFM_01.syx` … `SynprezFM_32.syx` and nothing else. There is no license file,
  readme or grant inside it. The selected voices come from those cartridges (for example `glassbell` = `SynprezFM_01.syx`
  program 9).
- **Impact:** the patch bytes are compiled into every firmware image and released package. "It ships in a GPL repository"
  is an inference. It is not a license from the named program authors. The handoff itself says engine licensing is not
  proof for the patch archive, and the evidence does not close that gap.
- **What is fine:** provenance is exact. `tools/prepare_dx7_review_bank.py` on the pinned ZIP (sha256
  `6ab2487b…cbb8d`) reproduces `factory.packed` byte for byte, and `gen_dx7_bank.py --check` passes.
- **Fix:** the owner decides on one of these. (a) Get or cite an explicit grant from SynprezFM/Dexed covering
  redistribution of these programs in firmware. (b) Replace them with programs whose license is explicit, or author
  original ones. (c) Keep the bank out of release images until (a) or (b) is done. Until then, change the wording from
  "GPLv3-distributed" to "bundled in Dexed's GPLv3 repository; no separate program license found."

### F2 — Medium (audio correctness): LFO modulation drops to zero every ~2 s for LFO delay 85 or 93

- **Where:** `firmware/audio/dx7/dx7.c:88-93` (`dx_lfo_delay`), wrap test on line 90: `if(d<l->delayinc)`.
- **Cause:** MSFA detects the end of the delay ramp with a 64-bit sum (`d > ~0u`). The port uses a 32-bit sum and compares
  it with `delayinc`. After the ramp crosses 2^31 the increment is `delayinc2`. When `delayinc2 > delayinc` (long delays,
  `a < 128`), a wrapped sum can land in `[delayinc, delayinc2)`. The test misses the wrap, stores the small value, and the
  delay output returns to 0 and ramps up again.
- **Reproduce:** run `docs/pr21-review-probes/p_lfo.c` and `p_lfo2.c`. With LFO delay 85: off until 1.42 s, ramps, full at
  2.08 s, **off again at 2.09 s**, then the cycle repeats (3.51 s ramp, 4.17 s full, …). LFO delay 93 does the same.
  On the device: BANK → MODULATION, set K1 DELAY = 85 and K2 PMD or K3 AMD > 0, then hold a chord. The vibrato or tremolo
  cuts out periodically.
- **Impact:** wrong sound for editable, legal values. None of the eight factory patches uses a delay in the affected range.
- **Fix:** test the wrap against the old state, e.g. `if(d<l->delay)return 1<<24;` (or use a `uint64_t` sum as MSFA does).
  Add a test that runs every delay value 0–99 for more than 60 s and checks that the delay output never falls after it
  reaches full.

### F3 — Medium (persistence/UX): SAVE is refused while DX7 tails run, which is several seconds for factory patches and minutes for legal edits

- **Where:** `native/dinghy.c:383-391` (`do_save`: PANIC, wait 800 ms on `en.act`, else "TAIL PLAYING - SAVE AGAIN").
  `engine.c:93` (PANIC is only `dx_all_off`, a normal release). `dx7.c:193,213` (a voice stays active until every carrier
  envelope reaches stage 4).
- **Measured** (`p_rel.c`, time from PANIC to all voices inactive after a held note): GLASSBELL 3.53 s, PLUCVIBE 1 2.59 s,
  TIGHT BASS 1.86 s, SLOW3D PAD 6.35 s, SPARKLE 1 7.41 s. With an edited carrier R4 of 40/30/20/10/0: 4.4 s / 12.7 s /
  44 s / 118 s / **355 s**. "Active" lasts long after the tail stops being audible, because the release targets a level
  far below the 1120 gain threshold.
- **Impact:** five of the eight factory patches already exceed the 800 ms window. `DX7_BANK_HANDOFF.md:85` calls this
  case "an unusually long DX7 tail", which the measurements contradict. With a slow-release edit the user cannot save for
  minutes, which invites a power-off that loses the work.
- **Fix:** let SAVE silence DX voices quickly (e.g. a `dx_kill` that ramps every voice to 0 within one or two blocks)
  before erasing. Separately, retire a released voice once every carrier's gain stays below the audibility threshold.
  Test: a factory pad, then a patch with R4=0, can be saved within about 100 ms of pressing SAVE.

### F4 — Medium (data loss): one MIXER encoder detent destroys an edited DX7 patch, with no confirmation or undo

- **Where:** `native/dinghy.c:1245-1246` (MIXER ALGORITHM on CHORDS/BASS) and `:1106` (REC in BANK BROWSE).
- **Trigger:** edit a patch in BANK, so the preset becomes 72 (CUSTOM). Go to MIXER on that track and turn ALGORITHM one
  step. `pos` maps CUSTOM to bank index 0, so +1 calls `db_select(…,1)`. That overwrites `sg.dx7[t]` with factory patch 2.
  −1 switches the track to the last classic preset, and the next +1 then overwrites the snapshot with factory patch 1.
  In BANK, REC means BACK on an edit page and LOAD in BROWSE. A double press from an edit page therefore replaces the
  edit with `dx_cursor`, which is factory patch 1 for a CUSTOM track.
- **Impact:** unsaved operator edits are lost with no warning. Undo covers patterns and rows, not patches.
- **Fix:** keep CUSTOM as its own stop in the MIXER cycle, or ask for a second press ("REPLACE EDIT? AGAIN") before
  `db_select` overwrites a CUSTOM snapshot. Optionally keep one-level patch undo. Test: from CUSTOM, ±1 detent and back
  restores identical bytes.

### F5 — Medium (audio quality): four-voice stealing hard-cuts release tails, giving clicks on chord changes

- **Where:** `dx7.c:122-125` (steal oldest released voice, else oldest) and `:125` (`dx_zero` discards the stolen
  voice's current block and state immediately).
- **Measured** (`p_steal.c`: 4-note chord for 1 s, release, then a new 4-note chord 100 ms later). Peak tail amplitude
  that is cut instantly / largest sample step during the release / largest step across the steal:
  GLASSBELL 2842 / 451 / 4429, PLUCVIBE 1 1610 / 110 / 4099, TIGHT BASS 5508 / 261 / 3324, SLOW3D PAD 3641 / 239 / 1951.
  Mono DX bass (1 voice) steals on every new note while the previous release is still sounding.
- **Impact:** chord parts using 7th/maj7/min7/add9 voicings (4 notes) steal all four tails on every change. These steps
  are 10–35× the normal release slope, which is an audible click risk. Part of the step is the new note's attack, so
  confirm by listening on the device.
- **Fix:** fade a stolen voice over a short window (e.g. 32–64 samples) before reuse, or use a spare "ghost" slot for the
  outgoing tail. Revisit the voice count if the budget allows: the measured worst callback is 2.7 of 5.8 ms (emulator).

### F6 — Low (DX7 semantics): a carrier with L4 > 0 is cut off right after key-off instead of holding at L4

- **Where:** `dx7.c:193` and `:213` (stage 4 = done → `active=3` → gain forced to 0 within one block).
- **Reproduce:** `p_steal.c`, last line. Carrier with L3 = L4 = 99: the output ends 3.5 ms after note-off from full
  level. A DX7 keeps sounding at L4 after release.
- **Impact:** not audible in the factory bank: the carriers' L4 values are 0 or ≤16, which is inaudible, and Slow3D Pad's
  L4=99 operators are modulators. It is reachable through BANK → OP ENV LEVELS K4, with an abrupt ending.
- **Fix:** only retire a voice whose carriers end below the audibility threshold. Otherwise keep it, so it can be stolen
  with a fade (see F5). Document the choice.

### F7 — Low (concurrency): BANK knob edits write the song without `lk()`

- **Where:** `native/dinghy.c:1216` (`db_set`), `:1100` (`db_toggle`). The ISR reads and CRCs `g->dx7` every block
  (`engine.c:242`).
- **Analysis:** each edit changes one byte, and every possible byte value is a valid patch byte. The ISR may copy a patch
  in the middle of a burst of edits, but every copy passes `dx_patch_valid`, and held notes are unaffected. Low risk.
  However, it breaks the threading rule written at the top of `dinghy.c`, which says structural edits run under the lock.
- **Fix:** wrap these edits in `lk()/ulk()` like `db_select`, or document the single-byte argument next to the existing
  single-step argument.

### F8 — Low (test gap): the native library test cannot exercise a v5 save

- **Where:** `tests/test_native_library.py:37`: the mock `slot_save` asserts `n==SONG_ENC_SIZE` (4508).
- **Impact:** the library save, load and overwrite logic is only tested with v4 records. v5 has host round-trip tests
  (`test_dx7_bank.c`) and the claimed emulator save, but nothing tests library logic with a 4764-byte record. I confirmed
  by reading the code that this is safe: Dinghy's `area_load` requires the exact header length, so `sload()`'s
  MAX → SIZE → V2 → V1 probing cannot misread a shorter record.
- **Fix:** let the mock accept `SONG_ENC_MAX` and add a DX7 save → slot name → reload case.

### F9 — Low (licensing hygiene): the Apache-2.0 text is not shipped with release binaries

- **Where:** `.github/workflows/release.yml:62-78` copies only `.fwsc` and `-app.bin` into release assets.
- **Impact:** Apache-2.0 §4(a) requires giving recipients of object form a copy of the License. The repository has it
  (`firmware/audio/dx7/LICENSE`), and Apache-2.0 → GPL-3.0 combination is compatible. But a release-asset download carries
  no notice. Also, `dx7.c` is labelled Apache-2.0 while it includes GPL `fmband.h`, so the module is not separable as
  Apache-only. Harmless in the combined work, but worth a comment.
- **Fix:** attach `LICENSE`, `LICENSING.md` and the MSFA `LICENSE` to the release assets, or link them from an
  about/credits screen.

### Checked with no defect found (limits stated)

- **DX7 math vs MSFA** (`dx7.c`, `dx7_constants.h`): I compared the following line by line with MSFA `dx7note.cc`,
  `env.cc`, `pitchenv.cc`, `lfo.cc` and `fm_core.cc`: packed byte layout (OP6 first, bit fields for curves, RS/detune,
  KVS/AMS, coarse/mode, sync/wave/PMS), the 32-route table, envelope advance/getsample (jump target 1716, `inc` shift
  2+LG_N), velocity and rate scaling, level-scaling curves, ratio and fixed frequency, LFO waves and init, the pitch-env
  unit (1143 @ 44.1 kHz), the LFO unit (36558 @ 44.1 kHz), and the gain threshold 1120. Feedback for algorithms 4/6 is
  sample-major with the bus semantics matching the block path. The 44.1 kHz constants match the 256/44100 callback.
  Remaining differences are the documented approximations (AMS/AMD calibration, sync-off phase model, per-voice LFO
  instead of MSFA's global LFO, carrier-count normalisation). This is a code comparison: I did not run the upstream
  reference generator, because the MSFA jni sources were not fetched.
- **Arithmetic safety:** `p_ext.c` under ASan + UBSan (no recover). All 32 algorithms with maximal legal values (OL 99,
  KVS 7, level-scaling depth 99, feedback 7, PMD/AMD 99, PMS 7, notes up to 127, level 127 hard-panned), plus 3000 random
  valid patches with random notes, velocities and block sizes 1–256: no UB, no out-of-bounds. Peak per DX engine:
  59039 at the extreme, 15915 for random patches. The engine clamps the mix at ±131071 before master gain and `mx_limit`,
  so there is no wrap-around clipping. The `pmd·delay` and `pms·(lfo−2^23)` products stay below 2^32 and 2^31 (checked
  bounds).
- **Patch validation** (`dx_patch_valid`): every packed field limit is correct. `db_field`/`db_limit` (`dx7_edit.c`)
  map all 10 pages × 4 knobs to the right byte, mask and shift with neighbours preserved, and OP1 → offset 85.
- **Held notes:** a patch load or edit changes only `s->patch`. Voice parameters are latched at note-on, so held notes are
  not re-attacked or retuned. Note-offs on CHORDS/BASS go to both the classic and DX engines, so classic↔DX switches while
  notes sound leave nothing stuck. Physical keys record their owning track (`kn_track`, `keys_release`). The sequencer
  emits bass note-off before note-on at the same time (`seq.c:144`). Remaining edge cases, unchanged from the classic
  engine: one `EN_OFF` releases every DX voice of that pitch, including a key and sequencer note of the same pitch.
  An `en_post` that fails on a full 32-entry ring drops a note-off.
- **Song format:** v4 stays at 4508 bytes when neither CHORDS nor BASS uses DX7. v5 is 4764 bytes. `song_decode` pairs
  version and length strictly, checks the length field and CRC first, parses twice (validate, then store), validates only
  active snapshots, and allows DX IDs only in v5 on tracks 1–2. Every consumer that can see a v5 record uses
  `SONG_ENC_MAX` (`dinghy.c` buffer and `sload`, `song_render.c`, `dx7_examples.c`). Tools that only write classic songs
  use `SONG_ENC_SIZE`. No JS or site code parses MKS2. `en_pre` and the MIXER/BANK labels guard `pnm[]` against IDs ≥ 64.
- **Ten slots:** `patch_dinghy_slots.py` only changes `NSLOT 8u → 10u`. `SLOTS_AT 0x97000`, `SLOT_SECT 4`, the 16 KiB
  stride and the 8 existing addresses are unchanged. 0xBF000 ≤ 0xE0000 and 0xF4000 (blobs) as asserted. Saves are
  erase-then-write with the header written last, so an interrupted save loses the old song in that slot (it is not kept).
  This is documented, not a regression. `do_save` reports "SAVE FAILED" if `settings_save` fails after the song was
  already written, which is misleading but safe.
- **Flash variants:** pinned Dinghy `fm1_flash_map.h:22` accepts exactly `0x856014` and `0xB36014`. The same macro gates
  the app (`platform.c:472`), recovery (`recovery.c:59`) and the loader (`loader.c:99`). `native/dinghy.c` adds static
  asserts for both IDs and rejects `0xFFFFFF`. Erase/program timing, sector geometry and OTA interruption on the
  UC25HQ80 part are **not** verified by anything in this PR (emulator only).
- **Generated content:** `gen_fvoice_tables.py` and `gen_fmband_tables.py` regenerate with no diff.
  `sprite_pack.py --check`, `gen_dx7_bank.py --check` and `gen_dx7_tables.py --check` all pass.
- **GEN:** regenerating an unlocked CHORDS/BASS track sets a classic preset (`gen.c:209`, `gen_next.c:188`). That
  silently drops a DX7 or CUSTOM selection for that track: the snapshot bytes stay in RAM but no UI path brings them back.
  This is documented ("GEN not remapped"). The owner should know that an unlocked GEN pass discards patch edits, like
  F4. Track locks preserve preset and snapshot (covered by CTest).

## 3. Checks actually run (all in this container, on `33d2115`)

| Check | Result |
|---|---|
| `pip install -r tools/requirements-dev.txt`; CMake Release configure and build | OK |
| `ctest --test-dir build/host --output-on-failure` | **46/46 passed** (14.3 s) |
| `gen_fvoice_tables.py`, `gen_fmband_tables.py` | regenerated, `git status` clean |
| `sprite_pack.py --check`, `gen_dx7_bank.py --check`, `gen_dx7_tables.py --check` | pass (exit 0) |
| Documented ASan + UBSan `tests/test_dx7.c` build | pass |
| `prepare_dx7_review_bank.py` on pinned Dexed `builtin_pgm.zip` vs `assets/dx7/factory.packed` | byte-identical |
| Review probes (`docs/pr21-review-probes/`) | results quoted in F2, F3, F5, F6 and §2 |
| Host relative render cost (`p_cost.c`, 4+1 voices) | alg 4/6 sample-major path costs about 10 % more than other routes on the host. Not a device timing |
| GitHub check runs on the head | 2 × `test` success (host only) |

**Not run, and why:**

- `native/build.sh` (all three profiles) and `tools/emu_smoke.sh`: the Jieli toolchain
  (`jieli-linux-toolchains-20260730.1`) and AC79 SDK are not in this container. Image/RAM/pool numbers, native
  callback timings (1.881 / 2.718 ms), and the JPEG benchmark are therefore **reported, not reproduced**.
- `tools/gen_dx7_reference.py --check`: the pinned MSFA jni sources were not fetched. The 32-points-per-patch,
  ±128 tolerance fixture in CTest passes, but it is a weak oracle: one sample every other block, PMD/AMD off, one note.
- Physical FM-1 (either flash chip), USB enumeration, UVC throughput and listening on device speakers: not performed and
  not permitted. **Emulator results remain emulator results.** No claim in this PR is physically qualified.

## 4. Conclusions by area

- **Compatibility / data loss:** the MKS2 v4/v5 format, readers and slot layout are sound, and old songs keep their
  sound. Data-loss risks are in the UI, not the format: F4 (an encoder detent overwrites edits), F3 (save blocked for
  seconds to minutes), and GEN dropping unlocked patch edits. Power loss during a save destroys that slot's previous song
  (pre-existing, documented).
- **Audio / deadlines:** the renderer is a faithful MSFA port with documented approximations, and it is arithmetically
  safe. One real defect (F2), one quality risk (F5) and one semantic gap (F6). No heap, libm or blocking calls in the ISR
  path. Per-block CRC32 of 2×128 bytes adds steady ISR cost (included in the emulator figure). The deadline margin is
  reported only from the emulator, and the factory bank contains no algorithm 4/6 patch, so the sample-major path did not
  contribute to the measured worst case.
- **UI:** BANK paging, operator mapping, masks and silent browse are correct. Destructive actions need guards (F4), and
  F7 is a hygiene issue. I did not judge whether the 240×240 labels are legible or the advanced controls understandable
  on the real panel. That needs the device and the owner.
- **Licensing:** MSFA Apache-2.0 → GPL-3.0-only is compatible and attributed (F9 is a packaging nit). The patch-data
  basis (F1) is **unsupported by evidence** and blocks a release containing the bank. PCL art notices are retained and
  extended to the new sprite headers.

## 5. Gaps against the original request, and open owner decisions

- Not implemented (stated by the PR, confirmed): GEN mapping to the DX7 bank, USB pedal/audio routing, simultaneous
  USB audio + MIDI + UVC (the UVC profile disables UAC/CDC), live companion telemetry, sustained UVC frame rate, MIDI
  sustain/bend/SysEx bank transfer, named user patch slots.
- Unverified claims: physical behaviour of both flash chips, device audio deadlines and output level, the native image
  and RAM numbers, and "15 fps"-type video rates (the PR itself says not to advertise them).
- Owner decisions: the patch-data license route (F1); the bank shortlist, genre roles and balance; voice count vs
  clicks (F5); the outfits, stage looks and UI style; whether to bring DX7 bass to the classic slide/glide behaviour.

## 6. Recommendation

**Not ready to integrate.** Keep PR21 draft. Before the owner considers merging:

1. Resolve F1, or exclude the eight programs from release images.
2. Fix F2 and F4. Fix F3, or at least have SAVE silence DX voices quickly.
3. Rebuild all three native profiles and rerun the emulator smoke checks. Add a timing run that uses an algorithm 4/6
   feedback patch on CHORDS and BASS.
4. Listen to F5 on hardware and decide.

F6–F9 can follow. Physical-device and USB qualification remain open regardless, and this review does not count the
original multi-phase assignment as finished. Areas covered only by automated checks: sprite/wardrobe packing, stage
effects, companion, site, UVC/JPEG and lyric/GEN internals beyond the DX7 interactions above.

## 7. Fix status (follow-up on `claude/zealous-ramanujan-31p6l5`, not on the PR branch)

At the owner's request, F2–F9 were fixed on this review branch, which contains the PR21 head plus the review. Merging
the fixes into PR21 is the owner's decision. Host CTest: **47/47** (adds `dx7_fixes`). The ASan+UBSan DX7 test is
clean. Nothing was compiled with the native toolchain or run on a device.

| Finding | Fix | Verified on host | Still unverified |
|---|---|---|---|
| F1 licensing | Resolved by original voices: the owner approved the ten voices of `docs/DX7_PATCH_PROPOSAL.md`; they are now the factory bank (IDs 64..73, CUSTOM 74, USER 75 reserved) and the Dexed bytes, the extraction tool and the review fixtures are removed | host CTest 46/46; reference vectors regenerated from unmodified MSFA at `f67d41d` for the new bank (±128 tolerance holds); ASan+UBSan clean; native compile and link of the default and video profiles with the device toolchain (placeholder SDK, no valid package) | on-device listening and level trim of the ten voices; emulator smoke, timing and JPEG benchmark not rerun for the new bank |
| F2 LFO delay | wrap test against the previous state | delays 0–99 for 65 s never drop | — |
| F3 SAVE tails | new `EN_HUSH` / `dx_hush` (≤2 blocks) used by SAVE; quiet-gain retirement; "SONG SAVED - LOOK NOT SAVED" | hush clears `act` in 2 blocks; factory tails retire about 35 % sooner | native compile; device feel |
| F4 edit loss | CUSTOM is its own MIXER stop with a one-level copy; second press required to replace an edit (MIXER and BANK REC) | syntax-checked extracts only | native compile; inline main-loop logic not unit-tested |
| F5 steal clicks | 64-sample faded tail buffer per synth (`sizeof(DxSynth)` 2948 → 3208) | step across steal: SLOW3D PAD 1951 → 94, TIGHT BASS 3324 → 684 | device listening; a chord-change block costs about 29 % more DX render time on the host (`p_stealcost.c`) |
| F6 L4 cut | a voice with an audible carrier L4 keeps sounding until stolen (with fade) or hushed | probe | — |
| F7 lock | `db_set`/`db_toggle` under `lk()` | reading | audibility of a lock per knob step while playing |
| F8 test gap | mock accepts v5; v5 save, name, reload, then v4 overwrite | CTest | — |
| F9 notices | release assets include LICENSE, LICENSING.md and the MSFA Apache text | reading | external reusable workflow's checksum step |

Retirement uses a −72 dBFS gain floor (`DX_QUIET`), so very slow edited releases (R4 = 0, about 4 minutes) still hold a
voice until stolen or hushed. SAVE no longer waits on them.

**F1 resolved by original voices:** the compiled bank is now ten original voices authored in `tools/dx7_candidates.py` (GPL-3.0-only project data); the eight Dexed/SynprezFM voices no longer ship in firmware, data or fixtures. Sections 1–6 above describe the state at the reviewed head and are left unchanged as history.
