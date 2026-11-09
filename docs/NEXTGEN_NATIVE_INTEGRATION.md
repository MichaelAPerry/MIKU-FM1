# Native performance, wardrobe and song-library milestone

> Current device UI follow-up: [selectable per-song LOOK controls, electronic scenes, recordings and resource measurements](DEVICE_LOOK_HANDOFF.md). This supersedes earlier global-look settings and pre-follow-up audio/resource figures below.


This is a working development milestone on `feature/miku-nextgen-performance`.
It supersedes the earlier “host-only GEN/lyrics, original outfit only” status.
PR21 remains a draft. Nothing in this document authorizes merging, deploying,
releasing, or flashing a physical FM-1.

## Owner decisions carried into this implementation

The owner asked to finish production outfit animation and native lyric/GEN
integration, requested additional reusable effects sheets including fireworks,
and confirmed a song library with ten editable songs: four preloaded songs and
six free slots. They selected complete first verses rather than excerpts, then
clarified that these should be actual songs in a library, not listening clips.
The factory songs therefore contain complete melody forms with vocal first
verses, editable accompaniment and instrumental endings. Additional verses have
not been silently added. The owner should review the arrangements by listening.

Signal Suit is a technical bodysuit, not the earlier mistaken “zettai” concept.
Summer Beat is the requested sporty bikini. Moon Circuit is the witch option.
The eleven outfit options remain marked prototype pending final creative review.
All original singer source sheets and original default pixels remain intact.

## Native song library

HOME now cycles STAGE, DANCE, SONG, PATTERN, MIXER, GEN, LYRICS, VISUALS,
SONG LIBRARY. The original first six page indices are unchanged.

In SONG LIBRARY:

- SELECT or PRESETS chooses one of ten slots; four entries appear at a time.
- PLAY toggles the loaded song. If another populated entry is selected, PLAY
  loads it and starts it after the existing unsaved-change confirmation.
- EDIT loads the selected entry. EDIT on an empty slot creates a new editable
  song in memory for that slot. Nothing is written until explicitly saved.
- REC saves the current song into the selected slot. Replacing a different
  populated entry requires a second REC within three seconds. Changing the
  target invalidates that confirmation.
- FX also loads; SCL starts a new song; ENV restores the selected factory song
  for entries 1–4. Unsaved replacements require a second matching action.
- SEQ opens the existing SONG arrangement editor. On the other pages it keeps
  its previous stop behavior. SAVE still uses the existing save workflow.
- Status includes PLAYING and unsaved `*`; confirmation/error toasts are visible.

Slots 1–4 contain Sakura Sakura, Tōryanse, Furusato and Kōjō no Tsuki on a fresh
installation. Slots 5–10 are free. The firmware reconstructs a factory song only
when no saved record is present. A saved project always takes precedence.
Restoring a factory is an in-memory action and never erases a saved record.
The original Otonohako demo remains available through the previous demo action.

### Storage safety

The pinned Dinghy store map is patched from eight to ten slots, preserving
`SLOTS_AT=0x97000` and the existing four-sector/16 KiB stride. Slots 1–8 retain
exactly their previous addresses. Slot 9 starts at `0xB7000`, slot 10 at
`0xBB000`, and the new end is `0xBF000`, below `FL_DATA_HI=0xE0000` and the
blob/settings areas. No partition shrinks and no existing record is relocated.
The patch fails closed if the pinned declaration changes. Upstream compile-time
flash allow-list/protected-range checks still run during the real native build.

The save format remains 4508 bytes. MKS2 v4 assigns existing high vocal flag bits
for lyric articulation; v1–v3 files remain readable, with previously ignored
high bits cleared to preserve their old playback. Original mora IDs 0–69 retain
their meanings. Appended `sha`, `ja`, `rya` use existing consonant machinery;
`rya` remains an R→A approximation, not a new palatalized consonant model.

Settings version 2 retains the same twelve-byte record and migrates v1 pan/slot
settings. Two formerly reserved bytes hold outfit, scene, effect and coarse
intensity. SAVE persists these settings. The selected visual defaults to the
original outfit and effects off. Older firmware can still read the old eight
slot locations; it does not expose the new slots or understand MKS2 v4.

## Factory-song construction and limits

`assets/songs/factory/songs.json` is the editable transcription source.
`tools/gen_factory_songs.py` compiles sparse note/mora/duration records.
`firmware/song/factory.c` reconstructs a normal Song with the existing four
tracks. There is no sample bank, extra Song-sized scratch allocation, extra
track, heap, or new language engine.

A source quarter uses two grid steps and the stored tempo is half the source
quarter tempo. This fits the complete melodies into 6–10 vocal patterns without
changing the song architecture. Furusato's three-beat phrasing is continuous:
its accents recur every six steps, even across 16-step storage-row boundaries.
It does not acquire an extra beat or padding silence after every musical bar.

The factory songs have one instrumental ending row, stop naturally, and may be
looped using the existing SONG control. The source files document first-verse
underlay, hold marks, rests, reference URLs and pronunciation limitations.
The accompaniment is a new restrained firmware arrangement, not a copied modern
score or recording. Musical suitability and pronunciation still need owner
listening review. All four render through the actual device engine, with zero
full-scale samples in the recorded review WAVs.

## Native lyric editor and articulation

LYRICS edits the vocal pattern of the selected arrangement row:

- SELECT chooses a unit; ALGORITHM selects its mora, rest `/`, or held vowel `_`.
- K1 changes duration; K2 changes note; K3 selects vocal pattern; K4 selects row.
- EDIT inserts a unit; REC deletes it. The editor consumes available trailing
  silence and rejects phrases longer than sixteen grid steps without mutation.
- FX marks a word boundary; SCL marks a phrase end; ENV toggles separation;
  LFO chooses a sustained vowel. PLAY loops the selected row.
- ARP copies on release. Hold ARP and press EDIT to paste articulation while
  retaining destination pitches. A paste cannot overwrite its own clipboard.
- Keyboard notes audition normally and set the chosen unit's pitch/mora.
  GLO restores the previous vocal-pattern edit through the existing undo ring.

The editor and reusable compiler use `ly_apply`/`ly_read`, normal patterns and
saved-song I/O. Legacy overlapping/cross-bar vocal material is refused by this
particular editor and remains editable in PATTERN. It is not destroyed or
silently converted. The current native editor spans one storage row; reusable
host text compilation supports phrase construction, but a multi-row text-entry
interface is still future work.

The sequencer and engine carry outgoing legato, incoming sustained-vowel and
separated onset flags through the on-beat queue and harmony voices. Explicit
rests end phrases. Authored separated syllables receive a fresh onset even when
consonant pre-start overlaps a previous queued release. Unmarked legacy steps
keep their original timing. Focused engine tests cover held vowels, delayed
queues, releases and final silence.

## Native GEN integration

GEN offers five contemporary candidate engines followed by the four retained
legacy choices. K1 chooses a style. K2/K3 choose key/scale. Hold EDIT while
turning K2/K3 for energy/complexity; a short EDIT release still keeps a take.
REC makes a song, ARP regenerates a phrase, track buttons retain locking, and
GLO drops the take. These are working candidates, not an owner-approved final
genre lineup. Stored generated songs remain ordinary editable projects.

`gen_next_song_ws` uses caller-owned 840-byte pool workspace. The old host wrapper
is omitted from native compilation, removing the earlier roughly 5 KB stack
scratch. Host seed comparisons, locking and phrase-context failure tests pass.

## Wardrobe and shared stage effects

The build contains ORIGINAL plus ENCORE, OFF DUTY, RETRO WAVE, PRISM, NEON PUNK,
CITY BEAT, SIGNAL SUIT, SUMMER BEAT, SNOW GLOW, FESTIVAL and MOON CIRCUIT.
Each new option includes twelve portrait expressions and eight genuine dance
poses. Switching an outfit changes portrait and dance atlases, palettes and
portrait dirty rectangles together. The USB JPEG renderer uses the selected
body atlas; it no longer always renders the original outfit.

`tools/wardrobe_pack.py` handles recovered magenta gutters, source-grid border
margins, explicit reviewed grid bounds when needed, common body baselines, stable portrait heads and lossless row encoding.
A two-pixel fitting inset provides room around generated artwork and improves
compression. It does not fabricate poses or recolour the original as a costume.
The final additional wardrobe accounting is 335463 bytes against a 340000-byte
packing budget. Oversized packs fail before replacing a checked-in header.
An optional nearby-row search saves 4304 further bytes without changing a
single decoded pixel or adding delta chains. Original compact generation is
unchanged by default. Geometry/labels and count are generated into `wardrobe_meta.h`, so extending the
manifest updates runtime registration too. Native asserts verify count parity.

VISUALS controls SELECT outfit, K1 background, K2 frame, K3 effect, K4 intensity.
ALGORITHM also selects effects. REC explicitly triggers an effect. DANCE and
native UVC display the scene; STAGE displays the matching vocal portrait.

A reusable 4×4 effects sheet supplies four frames each of fireworks, sparkles,
confetti and smoke. Six modes include OFF and procedural lighting beams.
The packed atlas is 2441 bytes before small C descriptor overhead. Rendering
uses at most three 32×32 sprites, no heap or full-frame buffer, and integer
absolute-time animation. Automatic bursts start on four-beat musical events;
beams continue while playing. Manual bursts expire after 960 ms. All effects
run in the main/display/JPEG path, never the audio ISR.

The choreography still uses eight poses per outfit. A separate vocal portrait
shows actual mouth state; per-phoneme mouth compositing onto every full-body
pose is not implemented. The source art and native view need visual review,
particularly differing head sizes and garment motion across generated poses.

## Computer companion

The local companion review supports all twelve paired outfits, backgrounds,
frames and the reusable effects. Its RGBA images are exported from the exact
firmware-packed rows by `tools/export_companion_art.py`, including the original
portrait/dancer. It plays a matching firmware-rendered WAV and recorded actual
engine state. The portrait follows actual vocal-engine mouth output.

These scene selectors are review controls. This page does not claim live USB
telemetry or synthesize substitute audio. The optional native UVC profile
renders the device-selected scene through the real incremental JPEG encoder.
Real USB enumeration/frame delivery, pedal audio transport and simultaneous
UAC/MIDI/UVC still require separate implementation/testing; none is implied by
a successful offline JPEG benchmark.

## Validation and evidence

See `CLAUDE_HANDOFF.md` and the durable integration snapshot for exact current
memory/timing measurements. Ignored `build/evidence/` contains:

- `library-tests.log`: the full Release host suite.
- `wardrobe-audit/report.json`: eleven real source pairs, originals unchanged.
- `stage-effects-review.gif`: shared renderer animation for five effects.
- `companion-wardrobe-effects.png`: actual browser review screenshot.
- `library-native-screen/work/`: library browsing/loading/playing screenshots.
- `slot-ten-save/`: actual target save, extracted length/address/CRC report.
- `slot-ten-reboot/`: fresh emulator boot from saved flash and playback.
- `visuals-final/work/`: selected native portrait and full-body/effects.
- `gen-native/`, `lyrics-native/`: real firmware controls and audio.
- `factory-songs/`: four .mks files, full-song WAVs and clipping/RMS hashes.
- `wardrobe-video-bench/`: target JPEG counters and recovered native JPEG.

The emulator flash round trip uses a local diagnostic patch adding
FM1_FLASH_IN/OUT. That emulator checkout is ignored and is not part of the
firmware delivery. The patch runs no operation on an owner's physical device.
Host flash mocks independently cover all ten records, unsaved target changes,
factory restoration and visual preference packing.

### Remaining work and review decisions

1. Listen to the four complete first-verse song arrangements, including source
   tempo/grid display and the limited palatalized consonant approximation.
2. Approve or revise the twelve wardrobe choices, hairstyle variants, backgrounds
   and effects. All source artwork is retained for revision.
3. Review the added native pages/control bindings and genre shortlist; no final
   genre lineup or new character design is silently frozen.
4. Authorize and perform physical-device timing, audio continuity, display/DMA
   contention, flash-save and USB recovery tests before production integration.
5. Finish actual computer-to-FM-1 USB audio routing/pedal transport and live
   performance telemetry. The host pedal DSP is still a prototype.
6. Investigate full-body mouth compositing and multi-row native lyric entry as
   follow-on presentation/editor work. Neither is described as finished.
7. Review existing high-register clipping risks and opt-in voice compensation
   using the earlier A/B evidence; the baseline vocal architecture remains.

The next concrete engineering task after this milestone is physical validation
of the native USB video profile, followed by an input-buffer/UAC prototype that
exposes incoming computer audio to pedal DSP before final output mixing. That
work must preserve the measured memory reserves and Dinghy recovery path.
