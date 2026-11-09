# Actual DX7 renderer and dedicated BANK editor

Implementation: `2bb1662`. Branch: feature/miku-nextgen-performance. PR21 remains
a draft. This supersedes the earlier DX7-shortlist-only status. BANK is a DX7
editor, not a renamed editor for the existing two-operator band.

## What works

An embedded integer C renderer reuses Google's MSFA synthesis implementation
(original source f67d41d313b7dc85f6fb99e79e515cc9d208cfff), with six operators,
all32 DX7 algorithm routes, feedback, per-operator envelopes, ratio/fixed frequency,
coarse/fine/detune, velocity/key scaling, pitch envelope and LFO. There is no
JUCE/plugin wrapper, heap, runtime floating-point table generation or binary
M-VAVE SDK engine. Module source/license/provenance: firmware/audio/dx7/.

The factory bank is **ten original voices** authored for this project in
`tools/dx7_candidates.py` (two per GEN style) and compiled from
`assets/dx7/factory.packed` into `bank.h` by `tools/gen_dx7_bank.py`. They are
GPL-3.0-only project data with no third-party patch bytes. This replaced an earlier
shortlist of eight voices taken from Dexed's bundled cartridges, which had no clear
redistribution licence (review finding F1, resolved by this change). The old
shortlist is mentioned only in historical documents. Patch and engine licences are
separate; see LICENSING.md and assets/dx7/README.md.

Chords use four DX7 voices and bass uses one. The original formant singer,
synthesized drums and four-track arrangement remain. Existing classic instrument
IDs are retained, and existing songs keep their source unless explicitly changed.
MIXER's ALGORITHM encoder can cycle the classic and DX7 families for chords/bass.
Once a track has an edited patch, CUSTOM is its own last stop in that cycle. Leaving
CUSTOM keeps a one-level copy (2 x 128 bytes of RAM, dropped on song load/new), so
cycling back restores it. Moving from CUSTOM into a factory DX7 patch needs a
second press (REPLACE EDIT? AGAIN); moving into a classic preset does not.
No factory demo arrangement or approved genre lineup was silently replaced.

## Review actual firmware and audio

The ten original voices: [auditions](evidence/dx7-candidates/) (same bytes as the compiled bank).
The links below are historical evidence of the BANK UI and persistence, recorded with the
superseded eight-voice Dexed-based bank. Their voice names and audio are not the current bank.

- [Native BANK walkthrough, with actual keyboard and full-band audio](evidence/dx7-bank/device-walkthrough.mp4)
- [Native bank/operator/envelope/bass/stage gallery](evidence/dx7-bank/dx7-device-pages.png)
- [Edited OP1 after a fresh emulator reboot](evidence/dx7-bank/restored-operator.png)
- [Three-row full band: singer + DX7 chords/bass + drums](evidence/dx7-bank/audio/full-band.mp3)
- [Original bells, same first-row chord part](evidence/dx7-bank/audio/classic-8.mp3)
- [DX7 PLUCVIBE 1, same first-row chord part](evidence/dx7-bank/audio/dx7-1.mp3)
- [DX7 PLUCVIBE1 matched to original-bell RMS for listening](evidence/dx7-bank/audio/dx7-1-level-matched.mp3)
- [Edited DX7 chord patch, three rows](evidence/dx7-bank/audio/edited.mp3)
- [Portable edited project exported from native emulator flash](evidence/dx7-bank/edited-native.mks)

All audio runs through the actual firmware engine. Raw MP3/AAC copies add no
normalization; the separately labeled level-matched PLUCVIBE1 copy applies a
constant measured RMS gain for fairer timbre comparison, without changing device
parameters or hiding the raw clipping measurements. The video uses actual native LCD frames, nearest-neighbor enlarged,
with explanatory captions outside the display. The browser is not synthesizing a
replacement instrument. Musical preference is for owner listening review; code
presence and spectral similarity alone do not establish a better arrangement.

| Bank | Factory ID | Voice | GEN style | Track | Algorithm | Listen |
|---:|---:|---|---|---|---:|---|
|1|64|HYPER GLAS|HYPERPOP|chords|5|[MP3](evidence/dx7-candidates/cand-0.mp3)|
|2|65|HYPER 808|HYPERPOP|bass|2|[MP3](evidence/dx7-candidates/cand-1.mp3)|
|3|66|FUTURE SAW|FUTURE BASS|chords|22|[MP3](evidence/dx7-candidates/cand-2.mp3)|
|4|67|FUTURE SUB|FUTURE BASS|bass|1|[MP3](evidence/dx7-candidates/cand-3.mp3)|
|5|68|CLUB STAB|JERSEY CLUB|chords|5|[MP3](evidence/dx7-candidates/cand-4.mp3)|
|6|69|CLUB BOUNC|JERSEY CLUB|bass|1|[MP3](evidence/dx7-candidates/cand-5.mp3)|
|7|70|DIGI PLUCK|DIGICORE|chords|9|[MP3](evidence/dx7-candidates/cand-6.mp3)|
|8|71|DIGI GROWL|DIGICORE|bass|5|[MP3](evidence/dx7-candidates/cand-7.mp3)|
|9|72|IDOL KEYS|IDOL POP|chords|5|[MP3](evidence/dx7-candidates/cand-8.mp3)|
|10|73|IDOL BASS|IDOL POP|bass|5|[MP3](evidence/dx7-candidates/cand-9.mp3)|

Design notes per voice are in [DX7_PATCH_PROPOSAL.md](DX7_PATCH_PROPOSAL.md). The voices are
first drafts that nobody has heard on an FM-1 speaker; some are quiet (CLUB STAB, DIGI GROWL,
DIGI PLUCK) and may need a per-voice level trim after listening.

**GEN mapping.** An unlocked GEN pass (`firmware/song/gen_next.c`) sets CHORDS to factory voice
0/2/4/6/8 and BASS to 1/3/5/7/9 for HYPERPOP, FUTURE BASS, JERSEY CLUB, DIGICORE and IDOL POP,
and copies the 128-byte snapshot into the song (the engine plays snapshots). Drums keep their
classic preset. Locked tracks keep preset and snapshot. GEN songs therefore encode as MKS2 v5.
The older classic generator (`gen.c`) is unchanged and still picks classic presets.

## Native controls

BANK is appended as page10; the previous nine page indices are unchanged.

- MIXER REC opens BANK; HOME also cycles to it.
- SCL selects CHORDS and ENV selects BASS. FX/LFO show that DX7 applies to these
  two tracks; they do not replace the singer or drum engine.
- SELECT in BROWSE chooses one of the ten factory voices (`DX7_BANK_N`, shown as "/ 10") **silently**. The screen names
  the candidate and separately names the loaded sound. Browsing alone does not
  change a sounding patch or create a note-on.
- REC in BROWSE loads the displayed DX7 patch. KEYS play the loaded sound.
  If the track holds an edited patch (CUSTOM), REC shows REPLACE EDIT? AGAIN and
  needs a second press within 3 s.
  A patch load affects subsequent notes; a held voice keeps its patch snapshot.
- EDIT advances parameter pages. PRESETS selects pages directly once a DX7 patch
  is loaded. REC from an edit page returns to BROWSE.
- SELECT or ALGORITHM in edit pages chooses Yamaha OP1–OP6. K1–K4 edit the four
  labeled fields. OP1 maps to the final operator in standard packed DX7 ordering.
- ARP on OP FREQUENCY toggles ratio/fixed mode. ARP on MODULATION cycles LFO wave.
- SAVE while stopped writes the song, actual patch bytes and existing visual
  preferences. SAVE first hushes the engine (EN_HUSH: panic plus a fast fade and
  retirement of every DX7 voice, so release tails of seconds or minutes do not
  matter) and waits for silence before the flash write. Only if the engine is
  still not quiet within 800 ms does SAVE defer with TAIL PLAYING - SAVE AGAIN.
  If the song saves but the look preferences do not, the message is SONG SAVED -
  LOOK NOT SAVED.

| Edit page | K1 | K2 | K3 | K4 |
|---|---|---|---|---|
| GLOBAL |Algorithm1–32|Feedback|LFO speed|Track level|
| OP FREQUENCY |Operator output|Coarse|Fine|Detune|
| OP ENV RATES |R1|R2|R3|R4|
| OP ENV LEVELS |L1|L2|L3|L4|
| KEYBOARD SCALE |Breakpoint|Left depth|Right depth|Rate scaling|
| MODULATION |LFO delay|Pitch depth|Amplitude depth|Pitch sensitivity|
| PITCH RATES |R1|R2|R3|R4|
| PITCH LEVELS |L1|L2|L3|L4|
| MIX / TUNING |Track level|Device pan|Patch transpose|Oscillator sync|
| CURVES / TOUCH |Left curve|Right curve|Velocity sensitivity|Amplitude sensitivity|

Operator/envelope/LFO edits affect future onsets, so audition the next key to hear
changes. Level/pan update the track mix immediately. An edit does not add an attack
or retune an already sounding oscillator. Keyboard note-off ownership is now
recorded per physical key, so changing pages/tracks while holding a note sends its
release to the instrument that started it. Release checks cover chord, bass,
vocal and one-shot drum behavior.

## Song and storage compatibility

Classic songs still export unchanged 4508-byte MKS2 v4 records. The reader continues
to accept v1/v2/v3/v4. DX7 songs use **MKS2 v5,4764 bytes**, with the same prefix and
an appended pair of128-byte packed snapshots before the CRC. Both track snapshots
are stored; only an enabled DX7 track requires a valid packed patch. CRC/field
validation is transactional and malformed input does not mutate the loaded song.

Classic preset IDs0–7 retain their meaning. Chord/bass IDs 64..73 are the ten factory
voices (64 + bank index), 74 (`SG_DX7_CUSTOM`) an edited patch, 75 (`SG_DX7_USER`) a
voice from a future imported user bank (reserved; played like CUSTOM); `SG_DX7_LAST` is 75.
v5 was never released, so the first-draft numbering (64..71, CUSTOM 72) was replaced; see
SONG_FORMAT.md. Playback uses the embedded snapshots, so
changing a future factory bank cannot silently change an existing saved song.
Old firmware rejects v5; it cannot reproduce newly embedded DX7 patches. Classic
exports remain v4-compatible. Full DX7 global-bank import/export and named user
bank slots are not provided by this project-save implementation.

Ten slot addresses and16 KiB slot strides are unchanged. 4764 bytes fit within the
existing slot capacity. Actual native save at0x97000 produced a v5 record with valid
outer CRC and both patch snapshots; settings remain44 bytes at0xFC000. A fresh
emulator boot restored edited PLUCVIBE1 OP1 output79/coarse2/fine5 and TIGHT BASS.
Both Puya0x856014 and UC25HQ80 0xB36014 emulator configurations saved/restored the
same v5 payload; no physical chip was flashed. GEN track-lock tests also preserve
both selected DX7 preset identity and the actual locked patch snapshot.

## Validation and measurements

Host test results, image/static/pool numbers and the MSFA comparison were rerun for the
original ten-voice bank. The emulator smoke scripts, recordings, callback timings and the JPEG
benchmark below were recorded with the superseded eight-voice bank and have **not** been
repeated for the original voices (no emulator in this environment).

Release host CMake build and **46/46 CTests pass** (47 minus the removed Dexed extraction test). Original table/sprite regeneration
and new DX7 bank/table checks pass. Renderer ASan+UBSan passes with LeakSanitizer
trace detection disabled for this environment. Original-MSFA reference vectors
regenerate exactly using tools/gen_dx7_reference.py on the pinned upstream jni
checkout. Its host-only adapter supplies controllers/patch-transport stubs and a
stddef include; it does not substitute the new renderer into the reference.

Core tests compare 320 sampled raw Q24 points (32 per patch, regenerated for the ten original voices) with the
independent original renderer (note60,velocity100,44100 Hz, PMD/AMD disabled),
allowing128 units for sine-LUT rounding. Tests separately cover32 algorithms,
algorithm4/6 feedback, actual full-patch LFO/polyphony,17-vs256-frame exact output,
A4 frequency, malformed-patch rejection, held snapshot isolation, stealing and
natural release. Integration checks all ten actual routed presets, the GEN style mapping, every editor
field/range, v5 round trips, legacy isolation, held pitch and GEN track locks.

Actual default/video emulator smoke scripts pass:124928 audio frames, RMS2738,
32 screen colors in each profile. The full46-second native BANK/song recording
has2021888 stereo frames, peak11381 and zero full-scale samples. The unnormalized
host full-band example peaks29273 with zero full-scale samples.

| Target profile | Image used/free | Static RAM used/free | Pool used/free |
|---|---:|---:|---:|
| Default | 532584 / 48980 | 43280 / 55024 | 255624 / 88440 |
| MIDI+UVC | 539244 / 42320 | 41488 / 56816 | 266080 / 77984 |
| Offline JPEG benchmark | not rebuilt for the bank swap (previous: 538328 / 43236) | previous: 40720 / 57584 | previous: 266080 / 77984 |

The default and MIDI+UVC rows were rebuilt with the original ten-voice bank (real device toolchain, placeholder SDK files, so no valid package). Actual limits remain581564 image /98304 static /344064 pool bytes. The owner's
removed64 KiB flash floor stays removed. Static/pool margins remain16384/32768.
Relative to the neon UI milestone, DX7 + editor + persistence/diagnostics cost
23956 default /23928 video image bytes,6752/6736 static bytes and zero pool bytes.
Each DxSynth is2948 bytes; four chord voices and one bass voice are configured.
The bass synth's fixed-capacity allocation leaves some voice storage unused.

Native callback timing excludes first-time initialization and records an actual
monotonic maximum, not only a decaying moving peak:

- Normal46-second BANK/keyboard/full-band exercise: **1.881 ms worst**, zero overruns.
- A6.5-second high-load run with vocal harmony, chorus/delay/reverb80,drive60,
  on-beat50, DX7 chords/bass and all six drum sounds on each sixteenth:
  **2.718 ms worst**, zero overruns; output peak5548 at diagnostic master60.
- Callback deadline:256/44100 = **5.805 ms**. Initialization measured1.063 ms.

These are emulator timer measurements, not physical ISR qualification. The target
JPEG benchmark with the saved DX7 song encoded three frames, zero encoder failures,
4779-byte final JPEG, maximum tile2.307 ms, maximum frame wall101.068 ms,17.9%
emulated ISR share and zero audio overruns (worst callback1.864 ms). This is an
offline three-frame measurement including UI transitions; sustained UVC rate and
actual USB delivery are unverified. Do not advertise a proven15 fps DX7+video mode.

For cross-profile emulator tests the local diagnostic flash restore copies **only
song slots/settings** (FM1_FLASH_DATA_ONLY=1). Restoring a whole flash dump from a
different firmware profile also replaces its code and invalidates that benchmark.
The updated recorded helper source is committed beside device-scene evidence.

## Remaining scope and next task

The engine is an MSFA-compatible reconstruction, not hardware-bit-exact Yamaha
emulation. AMD calibration and sync-off phase modeling are approximate. Cached
operator blocks introduce up to63 samples (~1.43 ms) before a note-off envelope
update. The DX7 engines now implement mono legato/glide (the sequenced bass slide flag
glides 60 ms instead of retriggering), pitch bend and the sustain pedal
(dx_note_legato, dx_set_bend, dx_set_sustain; engine commands EN_BEND and EN_SUSTAIN,
ignored by the classic chord/bass engines). Routing external MIDI notes, CC64 and
bend from native/dinghy.c into those commands, MPE and USB SysEx bank management
are not added.
User naming/independent custom bank storage remain future UI work.

The owner should listen to the ten original voices on the device and tune balance. GEN
mapping to the bank is done. Next coding task: profile and optimize sustained native
video under that musical load, and optionally add user DX7 bank import (`SG_DX7_USER`). Existing genre
lineup approval and physical-device/USB qualification remain open. Pedal USB audio
routing and live companion telemetry remain unfinished. No main merge/release,
website deployment or physical flash was performed. Keep PR21 draft for review.

## User bank, DX7 SysEx import and external MIDI (owner-requested additions)

Status: implemented and compiled with the device toolchain (placeholder SDK; no valid package, emulator not run, nothing
tried on a device or with Dexed). Host-tested: the parser (`tests/test_dx7_sysex.c`, also under ASan/UBSan), the patch script
(`tests/test_patch_dinghy_sysex.py`) and the real MIDI-in routing code extracted from `native/dinghy.c`
(`tests/test_native_midi_in.py`).

**Receiving.** Dinghy's USB layer assembles one SysEx frame at a time (`sx_frame`, the 7-bit bytes between F0 and F7) and
`ota_service()` decoded it as M-UPGRADE and threw away everything else. `tools/patch_dinghy_sysex.py` (exact-match, refuses if
the pinned text changed, run by `native/build.sh` after the optional video patch, which hashes the pristine `usb.c`) (1)
enlarges `sx_frame` from 640 to 4104 bytes in the app (the update loader, which also compiles `usb.c`, keeps 640) and (2) lets
`ota_service()` offer a frame whose first byte is 0x43 (Yamaha) to `app_sysex()` in `native/dinghy.c` and release it. M-UPGRADE
frames never start with 0x43 (their first decoded byte is 0, so the first 7-bit group is 0). The M-UPGRADE handshake and
session, the soft-key/UBOOT paths and recovery mode are unchanged (no hook in recovery or the loader). Frames arriving while
the app has not yet released the previous one are dropped by Dinghy as before; `app_sysex` only validates and returns.

**Parsing** (`firmware/song/dx7_sysex.{c,h}`, GPL-3.0-only, no heap/libm): the 32-voice bulk dump (VMEM, `43 0n 09 20 00`
+ 4096 + checksum, the same 128-byte packed voice the firmware uses) and the single voice (VCED, `43 0n 00 01 1B` + 155 +
checksum, packed OP6-first). Any channel nibble is accepted. Checksum and every voice (`dx_patch_valid`) are checked; a bulk
dump with any bad voice is rejected whole and the toast says which ("INVALID VOICE 12"). Bits the DX7 does not use in a
few flag bytes are masked before validation. Toasts: IMPORT 32 VOICES? REC, BAD CHECKSUM, INVALID VOICE n, BAD DX7 LENGTH.
Other Yamaha messages (parameter changes, requests) are ignored silently.

**Applying** (nothing changes before REC; the prompt lasts 10 s and REC on it works on any page):
- bulk dump: replaces the whole user bank (IMPORT 32 VOICES? REC), selects USER 1;
- single voice: if the BANK page is on an edit page (a DX7 CHORDS/BASS patch open) it goes into that track's edit buffer
  (preset becomes CUSTOM; the previous edit is kept in the usual one-level copy); anywhere else it goes into the currently
  selected user slot ("VOICE TO USER n? REC"); with no bank yet, the other 31 slots are filled with INIT VOICE.

**Storage.** The user bank is device data, not song data: songs still embed the snapshot they use and the song format is
unchanged. 32 x 128 B = 4096 B in Dinghy blobs 0 and 1 (16 voices, 2048 B each; blobs were unused by MIKU-FM). The RAM copy
is loaded at boot; the bank exists only if both blobs pass Dinghy's CRC and all 32 voices pass `dx_patch_valid`. Writes
follow `do_save`: refused while a song plays ("STOP FIRST"), then `EN_HUSH` and a wait (up to 800 ms) for `en.act`, then
the erase/program (the sound drops out for that moment). A bulk import writes blob 0 then 1; if the second fails the old
bank is written back. A power cut between the two writes leaves a mixed (but valid) bank; a failed write mid-sector can
leave the half invalid, in which case the bank reads as "no user bank" at the next start.

**BANK UI.** BROWSE shows FACTORY n / 10 or USER n / 32 with the voice name. **ARP** in BROWSE switches the source (USER only
once a bank exists: "NO USER BANK - SEND DX7 SYSEX"). SELECT browses silently as before. **REC** loads the displayed voice
into the track: preset SG_DX7_USER with the 128-byte snapshot copied; if the track holds CUSTOM or USER the existing second
press (REPLACE EDIT? AGAIN, 3 s) applies, and leaving USER stashes it like CUSTOM. Entering BANK opens on USER when the track
holds a USER voice. **Store:** on an edit page **GLO** picks a user slot (starts at the selected slot, each further press
within 8 s goes to the next), the page shows "REC STORE USER n  GLO NEXT", **REC** writes the track's snapshot into the slot
with its 10-character name. Naming the slot after the existing content is the overwrite confirmation (there is no separate
prompt). Name editing is future work: the project has no name-entry helper (song names cannot be edited on the device
either); imported and stored voices keep the name in the patch bytes. **GLO in BROWSE** sends the user bank to the
computer as a DX7 bulk dump on channel 1 (`ota_wire_send`, which blocks the main loop while the USB ring drains; the audio
engine keeps running).

**MIDI in.** The main loop drains Dinghy's `midi_get` (USB and TRS share the ring), at most 48 messages per pass.
Note-on/off on any channel plays the current track exactly where the keys do (same page-to-track rule: STAGE/DANCE/LIBRARY
sing, otherwise the selected track; same `EN_ON` arguments; drums map note mod 12 onto the 12 keys' drum map). A 128-entry table
records which track started a note so its note-off goes there after a page or track change; mono tracks (vocal, bass) release
when the last MIDI or physical key held on them is released; a note-off that finds the command ring full is retried every loop. CC64
is `EN_SUSTAIN` and pitch bend is `EN_BEND` for the current chords/bass track; a track that stops being current gets pedal up and
centre. CC120/123 release every held note. Clock and other realtime are ignored. MIDI does not write pattern steps. Mixing
physical keys and MIDI on one mono track at once is not specially handled. Silent BANK browsing creates no notes.

**Cost** (default build, measured with the real toolchain, baseline = parent commit): image 533,292 -> 538,220 B (+4,928),
static RAM 43,424 -> 51,424 B (+8,000: `sx_frame` +3,464, user bank 4,096, the rest MIDI/prompt state), pool 255,624 ->
263,824 B (+8,200: the pending-import and export buffers). Resource budgets pass. The update loader is byte-identical in size.

**Not done / unverified:** device USB behaviour (a 4.1 KB SysEx through the pinned EP1 path, host NAK/hold-back), interop with
Dexed and real cartridges (parser tested only with constructed dumps and the ten original voices), flash writes on a device,
the on-screen look of the new strings (layout follows `bank_ui.inc`, not seen on the LCD), export receivable by a DX7/Dexed.
The website's DX7 bank tool (`site/bank.html`, `site/js/dx7syx.js`; `syx.html` redirects to it) opens, sends, receives and saves .syx files through Web MIDI; its packed/VCED conversion is tested against this C code (`tests/test_bank_tool.py`), the page itself is untested against hardware.
