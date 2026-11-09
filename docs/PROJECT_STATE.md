## Current review handoff (2026-11-09)

> Current device UI follow-up: [selectable per-song LOOK controls, electronic scenes, recordings and resource measurements](DEVICE_LOOK_HANDOFF.md). This supersedes earlier global-look settings and pre-follow-up audio/resource figures below.


## Latest native integration milestone

Native lyrics and contemporary GEN are integrated. Twelve paired outfits,
fireworks/sparkles/confetti/smoke/beams, a ten-slot song library, and four
editable first-verse factory songs are implemented on the feature branch.
The current authoritative status is [CLAUDE_HANDOFF.md](../CLAUDE_HANDOFF.md),
with implementation details in [NEXTGEN_NATIVE_INTEGRATION.md](NEXTGEN_NATIVE_INTEGRATION.md).
Earlier host-only/original-outfit-only status statements below are superseded
for these features. Physical USB and pedal transport remain unfinished.

Read [CLAUDE_HANDOFF.md](../CLAUDE_HANDOFF.md) for current feature readiness and owner constraints. The older snapshots below are historical; optional native UVC code now exists but real camera operation is unverified, and the new wardrobe remains concept-only.

# Feature branch state update (2026-11-08)

The historical state below describes the original firmware. For the current development branch, see [NEXTGEN_DEVELOPMENT.md](NEXTGEN_DEVELOPMENT.md) and [HANDOFF.md](HANDOFF.md). Mandatory live fixes, native bass/drum additions and default visual-state integration are implemented and tested. Voice/lyrics/GEN/visual/pedal/companion experiments are explicitly separated from native-ready features. Final genres, new costumes, live USB and UVC remain unfinished/pending review; no full next-generation completion claim.

---

# Project state: what exists, what is unused, what is missing (2026-11-03, after Step 4)

Companion to [HANDOFF.md](HANDOFF.md) (state, recipe, workflow). This file is the product picture: every asset and
what it is for, and the owner's original master prompt checked item by item.

## Assets (`assets/`, see `assets/NOTICE.md`)

| Asset | Packed into | Used? | Intended use |
|---|---|---|---|
| `singer/faces.webp` (12 faces) | `firmware/ui/singer_sprites.h` (SF_*, 90x90, 4bpp, drawn 2x) | yes: NEUTRAL, BLINK, A I U E O, N (lips shut: m b p, ん), HAPPY (PLAY start) | **unused: SF_SURPRISED, SF_TILT_L, SF_TILT_R.** Ideas: SURPRISED on a big leap/accent or an undo, TILTs as idle head motion between phrases or on alternate beats while stopped |
| `singer/stage.webp` (8 full-body frames) | same header (ST_IDLE0..3, ST_SING0..1, ST_ARMS, ST_BOW; 61x71, 4bpp, own palette, 17 KB) | yes (DANCE page, 3x: sway per beat, SING while the vocal sounds, ARMS on CHORUS rows, BOW at stop) | A small **dancing singer** (the original prompt asks for idle motion and beat-synchronised motion): ST_IDLE0..3 one frame per beat while playing (sway), ST_SING0..1 while the vocal sounds, ST_ARMS on CHORUS rows, ST_BOW when the song ends or on stop. Where: not decided with the owner. Options: (a) an alternate STAGE view (full-body dancer at 2x–3x instead of the face; toggle with a button on STAGE), (b) a small 1x dancer in a corner of SONG/GEN, (c) the face stays and the dancer replaces the step strip during playback. **Ask the owner which; recommend (a).** Draw with dirty rects like the face; feet share a baseline already |
| `singer/reference.webp` | – | reference only | keeping new art consistent |
| `singer/banner.webp` (2000x750) | `site/media/banner.jpg` (1600 wide) | yes: site hero | site hero image on `site/index.html` (resize/compress to ≤200 KB, keep the dark theme) |
| `singer/icon.webp` (1254x1254) | `site/media/{icon-512.jpg,apple-touch-icon.png,favicon-32.png}` | yes: favicon, social image | site favicon / social preview image; NOT the Hub card (the Hub wants a real 240x240 screen PNG: `docs/card.png`) |
| `songs/otonohako.md` | `firmware/song/demo.c` | yes: the default demo song | – |

## The original master prompt, item by item

Legend: **done** (emulator/host-verified), **partial**, **missing**. Nothing is verified on hardware yet.

### Vocal (track 1)
- done: excitation with adjustable harmonics (brightness), 6 formant resonators, five vowels, consonants for 70 morae
  (Step 3; crackle fix), portamento, vibrato, onset scoop, breathiness, lip-sync output, ん hum.
- done (voice home, 2026-11-04): attack/release, scoop, human, vibrato depth/rate/delay, tremolo editable on STAGE (knob
  banks), effects (chorus, delay, reverb, drive), character (formant size, 5 steps), languages (JAPANESE, ANIMAL babble,
  SCAT, VOWELS), syllable wheel; all saved in the song (MKS2 v2).
- done (2026-11-04): hold-a-button layers: SOURCE (tension, fry, whisper, fine formant), FX DEEP, ARTICULATION (consonant
  length/level, vowel speed, vowels on the beat), HARMONY (a second voice at an interval).
- partial: phoneme
  transitions (glides, nasals) are basic; syllable timing: the consonant starts on the beat, so the vowel lands 30–60 ms
  late (fix: start VOCAL notes `fv_cons_ms()` early in the sequencer).
- missing: pitch bends (beyond scoop/portamento), per-note vibrato/expression, pronunciation/articulation controls
  (the step's articulation bits are reserved), voice presets, a per-vowel loudness table vs pitch (see HANDOFF).

### Chords / Bass / Drums (tracks 2–4)
- done: 4-voice FM chords with 8 presets, block/arp/strum modes, velocity; bass with 4 presets, slide/portamento,
  accent; 6-piece kit (kick, snare, clap, closed/open hat, perc), 2 kits, accent, swing; shared clock.
- missing: editing envelopes/timbre of chords/bass/drums on the device (presets only), effects on the band (the voice has them), more drum kits.

### Sequencer
- done: step entry, note length, play/stop, patterns (16 per track), duplicate, chaining via 64 arrangement rows with
  section labels (patterns are referenced, not copied), tempo, swing, mute/solo, undo (8), save/load (8 slots, MKS2).
- missing: real-time recording + quantize, redo, time signatures other than 4/4, pan in the song format, song rename.

### Generative (Step 4)
- done: GEN page, 4 styles (POP, BALLAD, DANCE, CHIP), key/scale, seed, track locks, regenerate one phrase, keep/drop
  the take, deterministic.
- missing: the prompt's named styles "Denpa/Chiptune", "Dream Pop", "Generative Ambient", "Infinite Idol" (evolving
  endless mode with remembered motifs), energy/complexity controls, vocal scat lyrics, accompaniment-for-my-melody
  as an explicit mode (locks give most of it).

### UI pages
- done: STAGE, SONG, PATTERN (note editor; morae picked with ALGORITHM), MIXER, GEN.
- partial: LYRICS (inside PATTERN; no dedicated page for vowel length/consonant timing), PERFORMANCE (keys play live on
  any page; no page for vibrato/bend/expression performance).
- missing: animated idle/beat motion (the unused dance sprites above), expression changes.

### Browser companion editor (prompt section 8)
- missing entirely: piano roll, lyric entry, MKS2 import/export, MIDI file import/export, browser audio preview (the C
  engine `firmware/app/engine.c` could be compiled to WASM with Emscripten for a faithful preview), validation.

### Docs the prompt asked for
- present: RESEARCH, SONG_FORMAT, CONTROLS, MANUAL, FEATURES, VALIDATION, INSTALL, HANDOFF, this file.
- missing: ARCHITECTURE.md (module map), HARDWARE.md (resource budget: flash 152,924 of 581,564 B, RAM 34,544 of
  98,304 B, emulator CPU 11–13% — device CPU still to measure). `docs/ROADMAP.md` is stale (written before the Dinghy
  firmware existed): replace it with the list below.

### Release and Hub
- ready: release.yml (attested via zvenson/fm1-build@v1), pages.yml, fmhub.json, docs/card.png, identity FM-1_39XXYYZZ.
- missing: hardware tests (install from V15, from another firmware, back to another firmware), the first tag, the
  `github-pages` environment tag rule `v*` (owner, in Settings), the mail to zven@thefirmwarehub.com.

## Suggested order from here

1. Owner listening pass on the generator and consonants; fix what they hear.
2. First hardware test (only with the owner's explicit go-ahead): CPU% on STAGE, sound, saves across power cycles,
   install paths. Then tag v0.6 (attested release) and the Hub listing.
3. Dancing singer (ask where; recommend an alternate STAGE view), unused faces as expressions, site banner/icon.
4. Vowel-on-the-beat consonant look-ahead; voice loudness vs pitch; editable vibrato rate/delay and envelopes.
5. Browser companion editor (MKS2 + MIDI import/export, piano roll, lyrics, WASM preview of the same engine).
6. More generator styles from the prompt (Infinite Idol), real-time recording, effects, ARCHITECTURE/HARDWARE docs.

## Site (2026-11-04)

Modelled on the Mutable Dreams site, in Miku-FM's own look (navy grid, teal and pink, the pixel dancer):
`site/index.html` (hero with the banner and an animated dancer, 01 the six screens as native renders, 02 the emulator
embedded, 03 the trailer, 04 five songs to listen to, 05 features, install, credits) and `site/install.html` (Web MIDI
Install, return to official V15, USB rescue, installer log; English / 日本語). `tools/site_front.sh` fills both pages
and copies `site/{site.css,js,media}`. Media are made on a host and committed: `tools/site_trailer.py` (trailer +
screens from an fm1-emu capture), MP3s with `song_render` + ffmpeg. Not yet on the site: an illustrated guide (PDF),
update notes per version, a device test report (needs the hardware test), a source zip per release.

