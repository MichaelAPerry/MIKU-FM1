# Device LOOK controls and electronic stage follow-up

> Latest UI pass: [visual wardrobe and song-library menus, native walkthrough and current build measurements](evidence/menu-redesign/README.md).


This is the current follow-up to the original native integration milestone.
Implementation commit: `71035d9`. PR21 stays draft on
`feature/miku-nextgen-performance`; no merge, deployment, release or physical
FM-1 flash was performed. The owner selected electronic showcase stages.
The twelve scene pairings are reviewable prototypes, not final genre approval.

## Review the actual interface

- [Native device UI walkthrough](evidence/device-scenes/device-ui-walkthrough.mp4)
- [Sakura with backing band and default stage](evidence/device-scenes/sakura-scene.mp4)
- [All twelve style scenes](evidence/device-scenes/twelve-style-scenes.png)
- [Four factory scenes](evidence/device-scenes/four-demo-scenes.png)
- [Saved custom LOOK after a fresh boot](evidence/device-scenes/saved-look-after-reboot.png)

These are LCD captures from the actual compiled firmware running in fm1-emu.
They are not a replacement browser synthesizer or interface mockups. Video is
nearest-neighbor scaled to 720 pixels, without audio normalization. The UI
walkthrough adds explanatory captions outside the native 240x240 screen.

## User operation

1. From DANCE press EDIT to open LOOK. STAGE/DANCE ARP opens SONG LIBRARY.
2. Turn SELECT to choose a complete STYLE / OUTFIT bundle: matching portrait,
   dancing body, background geometry, stage colors, framing and default effect.
3. K1 selects ORIGINAL, GRID, HORIZON, CONCERT or LED WALL. K2 selects NONE,
   CORNERS or RAILS. K3 (or ALGORITHM) selects OFF, FIREWORKS, SPARKLES,
   CONFETTI, SMOKE or BEAMS. K4 changes effect intensity.
4. PRESETS switches STYLE/WARM coloring. REC explicitly triggers the effect.
5. EDIT returns to DANCE. Stop playback with SEQ, then SAVE to keep the song
   and its look. An asterisk marks unsaved changes. Saving while playing uses
   the existing STOP FIRST guard.
6. In SONG LIBRARY turn SELECT to an entry and press LFO to load/open its LOOK.
   Existing unsaved-song confirmations apply when changing loaded songs.

The LOOK screen shows actual paired portrait/body thumbnails and current
background/effect names. This is usable without a computer. HOME still cycles
all nine pages, with the original first six page indices retained.

Factory scene defaults are Sakura/Encore/concert/sparkles,
Toryanse/Neon Punk/LED/confetti, Furusato/Prism/horizon/beams,
and Kojo/Moon Circuit/grid/fireworks. All twelve outfits have distinct stage
colors and rendered backgrounds. This uses shared procedural geometry and
palettes, not twelve additional illustrated bitmap sheets. Background and
effect choices can be overridden independently of the outfit.

## Persistence and compatibility

`firmware/ui/scene_prefs.c/h` owns a 44-byte version-3 settings record, including
three bytes per song slot for outfit, background, frame, effect, intensity and
palette. Intensity is quantized to 16 persistent values. All ten slots retain
independent looks. There is no additional graphics pool allocation.

The old 12-byte version-1/version-2 preference record migrates at boot. The
version-2 global look is imported into the previously selected slot; other slots
receive their own defaults. Existing song save addresses, original eight slots,
4508-byte MKS v4 files and v1-v3 readers remain unchanged. **Visual choices are
per-device slot settings and are not embedded in exported .mks files.** Exporting
portable complete scene projects would require a sidecar or approved format
extension. Do not describe existing .mks exports as carrying their stage look.

SAVE writes the selected slot's scene after successfully writing its song.
Restoring a factory in memory also restores that factory's scene; persistent
replacement still requires SAVE. Existing saved songs take priority over the
factory templates. No owner projects were erased.

## Audible accompaniment correction

The previous recording had backing parts, but their levels were buried. Solo
first-row analysis found vocal RMS 6325, chords 153, bass 680 and drums 104.
Factory levels now use vocal 120 (unchanged), chords 108, bass 96, drums 96.
Quarter-row chord strikes and stronger bass/drum accents improve sustained
accompaniment. New solo RMS is approximately chords 898, bass 1802, drums 752.
The vocal-only PCM comparison was byte-identical before/after. Voice DSP,
melodies and lyrics were preserved. Four complete host renders peak at
30338-31512 with zero full-scale samples. The committed factory MP3/MKS
listening examples were refreshed; no samples were normalized to hide clipping.

## Validation and measured resources

All 40 Release host CTests pass. Added scene preferences coverage checks per-slot
isolation, migration, transactional invalid input, twelve distinct background
hashes and save/reboot decoding. Native library extraction tests cover actual
save/load functions. Factory mix tests require audible backing RMS relative to
the voice and check full-mix headroom with actual default pan settings.

Host JPEG validation covers 720 outfit/background/effect/pose combinations;
maximum encoded size is 5967/8192 bytes. Original fvoice/fmband generated tables
and original singer sprite regeneration pass. Local Chromium tests pass all
twelve paired outfits and companion scene controls. The companion continues to
play recorded performance state; this is not live USB telemetry integration.

| Native profile | Image used/free | Static RAM used/free | Pool used/free |
|---|---:|---:|---:|
| Default |506540 / 75024|35760 / 62544|255624 / 88440|
| MIDI + UVC |513180 / 68384|33984 / 64320|266080 / 77984|
| Offline JPEG benchmark |513108 / 68456|33984 / 64320|266080 / 77984|

The unchanged minimum-free floors are 65536 image, 16384 static and 32768 pool.
Relative to the prior 6d14510 milestone, MIDI+UVC image grew 2280 bytes and static
RAM 60 bytes; pool usage is unchanged. Only 2848 image bytes remain above the
video reserve floor. Additional substantial features need measured savings.

The walkthrough changes Sakura to Off Duty/concert/beams/warm/intensity 60,
saves it, plays another slot, returns to Sakura, and verifies restoration. A
separate fresh emulator process restored the 44-byte flash record with valid
outer CRC at 0xFC000. No physical device was touched. Recorder input scripts and
an offline helper source are committed beside the evidence. The helper uses the
official emulator's dual-core JIT machine backend. Its PCM was checked exactly
against the reference CLI for a scripted three-second playback. Its explicit
key-down/key-up grammar differs from the CLI's ranged-key syntax.

## Remaining work and owner review

The owner should review the actual LOOK workflow, electronic scene pairings,
outfit artwork, effects intensity and audible backing balance. Final musical
genre assignment remains open; these scene presets do not replace genre-specific
composition engineering. The four old melodies now have audible accompaniment,
not newly approved contemporary arrangements.

Physical USB enumeration, real delivered UVC frame rate, combined USB bandwidth,
hardware ISR timing and USB pedal input remain unverified or unfinished. The
experimental video profile disables UAC/CDC; never promise simultaneous USB
audio/MIDI/UVC. Full-body per-phoneme facial compositing and additional bespoke
choreography are not claimed by this scene/UI change. Native portrait mouth
states continue using the existing actual vocal engine state.

Next coding task after owner review: address specific UI/art changes and profile
scene rendering under native video load before spending the remaining image
reserve on USB routing. Keep PR21 draft until the full review scope is settled.

## Current target-side JPEG timing

The three-frame offline emulator benchmark completed without encoder failure: final
JPEG 4779 bytes, maximum tile 1577.125 us, maximum frame wall time 86064.167 us,
emulated ISR share 8.9%, audio-render moving peak 308.083 us, audio peak 10293
and zero full-scale samples. This sample includes native UI transitions and is
not a sustained frame-rate measurement; the 86 ms frame wall maximum needs
further profiling before claiming a delivered USB video frame rate. These are
emulator counters, not physical device timing or USB throughput.
