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

The eight source patches are unchanged packed voices from Dexed's bundled bank
archive at fea2f0f0f2c397799f59b27eb11c038cd34b22c5. Source identity, program indices
and patch hashes are preserved in the earlier verified shortlist. Packed patch
bytes are now included in assets/dx7/factory.packed and generated into bank.h;
this replaces the earlier offline-only selection status. Patch and engine
licenses are separate; see LICENSING.md and assets/dx7/README.md.

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

| Bank | Existing source patch | Provisional role / candidates | Listen |
|---:|---|---|---|
|1|glassbell|Glassy hook; Hyperpop/Digicore|[MP3](evidence/dx7-bank/audio/dx7-0.mp3)|
|2|PLUCVIBE 1|Digital pluck; Future Bass/Electropop|[MP3](evidence/dx7-bank/audio/dx7-1.mp3)|
|3|HYPE BASS2|Aggressive bass; Hyperpop/Digicore|[MP3](evidence/dx7-bank/audio/dx7-2.mp3)|
|4|TIGHT BASS|Short rhythmic bass; Jersey Club|[MP3](evidence/dx7-bank/audio/dx7-3.mp3)|
|5|Slow3D Pad|Evolving chord bed; Future Bass|[MP3](evidence/dx7-bank/audio/dx7-4.mp3)|
|6|SYN-LEAD32|Melodic lead; Hyperpop/Electropop|[MP3](evidence/dx7-bank/audio/dx7-5.mp3)|
|7|FmRhodes18|Expressive keys; Electropop|[MP3](evidence/dx7-bank/audio/dx7-6.mp3)|
|8|SPARKLE 1|Digital accent; Digicore/Future Bass|[MP3](evidence/dx7-bank/audio/dx7-7.mp3)|

These are a reviewable bank, not a final genre lineup or proof every patch is best
for its proposed use. Individual first-row auditions deliberately use identical
chord events for comparison, including the bass presets played as chords. In the
full mix TIGHT BASS is used on the bass track, with one-voice behavior.

## Native controls

BANK is appended as page10; the previous nine page indices are unchanged.

- MIXER REC opens BANK; HOME also cycles to it.
- SCL selects CHORDS and ENV selects BASS. FX/LFO show that DX7 applies to these
  two tracks; they do not replace the singer or drum engine.
- SELECT in BROWSE chooses one of eight candidates **silently**. The screen names
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

Classic preset IDs0–7 retain their meaning. Chord/bass IDs64–71 identify factory
origins;72 identifies an edited patch. Playback uses the embedded snapshots, so
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

Release host CMake build and **46/46 CTests pass**. Original table/sprite regeneration
and new DX7 bank/table checks pass. Renderer ASan+UBSan passes with LeakSanitizer
trace detection disabled for this environment. Original-MSFA reference vectors
regenerate exactly using tools/gen_dx7_reference.py on the pinned upstream jni
checkout. Its host-only adapter supplies controllers/patch-transport stubs and a
stddef include; it does not substitute the new renderer into the reference.

Core tests compare256 sampled raw Q24 points across the eight patches with the
independent original renderer (note60,velocity100,44100 Hz, PMD/AMD disabled),
allowing128 units for sine-LUT rounding. Tests separately cover32 algorithms,
algorithm4/6 feedback, actual full-patch LFO/polyphony,17-vs256-frame exact output,
A4 frequency, malformed-patch rejection, held snapshot isolation, stealing and
natural release. Integration checks all eight actual routed presets, every editor
field/range, v5 round trips, legacy isolation, held pitch and GEN track locks.

Actual default/video emulator smoke scripts pass:124928 audio frames, RMS2738,
32 screen colors in each profile. The full46-second native BANK/song recording
has2021888 stereo frames, peak11381 and zero full-scale samples. The unnormalized
host full-band example peaks29273 with zero full-scale samples.

| Target profile | Image used/free | Static RAM used/free | Pool used/free |
|---|---:|---:|---:|
| Default |531820 / 49744|42512 / 55792|255624 / 88440|
| MIDI+UVC |538444 / 43120|40720 / 57584|266080 / 77984|
| Offline JPEG benchmark |538328 / 43236|40720 / 57584|266080 / 77984|

Actual limits remain581564 image /98304 static /344064 pool bytes. The owner's
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

The owner should listen and choose bank/genre assignments and balance. Next coding
task: integrate chosen DX7 profiles into candidate GEN arrangements, then profile
and optimize sustained native video under that musical load. Existing genre
lineup approval and physical-device/USB qualification remain open. Pedal USB audio
routing and live companion telemetry remain unfinished. No main merge/release,
website deployment or physical flash was performed. Keep PR21 draft for review.
