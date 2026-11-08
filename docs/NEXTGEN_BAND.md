# Next-generation backing band: opt-in prototype (2026-11-08)

The four-track architecture and original eight chord, four bass and two drum presets remain intact.
The original presets pass frozen sample hashes captured from `origin/main`; neutral added parameters bypass their new DSP.
These are audition candidates, not an approved sound palette or a finished workstation redesign.

## Implemented

- Bass preset **4 SUB DROP**: six-semitone decaying pitch onset over the existing stable sine bass, mild saturation,
  independent pitch envelope and the existing slide/glide. A slide retains the running pitch envelope; a fresh note
  restarts it. Pitch reaches its exact base increment within 100 ms in the regression fixture.
- Bass preset **5 HARD ACID**: sharper filter-envelope articulation and symmetric saturation. New `BsPreset` fields
  `drop` (quarter semitones), `pdec` (existing decay index) and `drive` (0..127) are editable through the DSP API.
- Drum preset **2 PUNCH**: low sine kick, pitch sweep, brighter clap/snare noise and short noise hats.
- Drum preset **3 DIGITAL**: triangle kick, short square snare, metallic noninteger-ratio FM percussion and clustered hats.
  Both use compact existing synthesis parameters; no samples or extra audio buffers.
- `ch_set_modern(s, 0)` selects **DIGITAL PLUCK** and `ch_set_modern(s, 1)` selects **GLASS PAD**. These are explicit
  API profiles, outside the full eight-entry stored chord preset bank. An optional second detuned carrier shares each
  voice's FM modulator and amplitude envelope. Four voices remain four voices. The digital pluck has a fast index fall;
  the pad has a slower spectral envelope. Existing single-carrier presets bypass the second carrier.

No new track, song version, modulation interface or automatic genre-specific sound choice is introduced by this module.
Native MIXER now exposes the new bass/drum indices through its existing preset encoder (all within the existing 0..7 file bounds). The chord API profiles are
host-audition infrastructure until a reviewed selection/persistence interface is added.

## Reproduce and compare

```
cmake --build build/host
ctest --test-dir build/host --output-on-failure
mkdir -p build/nextgen
build/host/firmware/audio/fmband/band_demo build/nextgen
```

The same four-bar score at 148 BPM renders to `band_demo.wav` (original bells/FM bass/808), `band_subdrop.wav`
(digital pluck/sub drop/punch), and `band_hardacid.wav` (glass pad/hard acid/digital). `presets.wav` auditions every
stored preset plus both chord API profiles. The audio is the actual firmware DSP compiled for the host.

| Same-score render, 7.4 s including release | Peak | RMS | RMS dBFS | Full-scale samples |
| --- | ---: | ---: | ---: | ---: |
| Original | 26685 | 5192 | -16.00 | 0 |
| Sub drop / punch | 25203 | 5884 | -14.92 | 0 |
| Hard acid / digital | 20111 | 3344 | -19.82 | 0 |

These comparisons are not loudness matched. They show changed timbre/envelopes and safe output for these fixtures;
they do not establish subjective improvement or safety under every possible mix. The existing extreme full-band stress
fixture reaches the limiter's full-scale cap; the singer still needs mix-specific gain staging.

## Validation and cost

`fmband_tests` passes. It covers pitch, glide, accent, drum velocity/decay/choke, deterministic random stress, silence
following release, chunk-size invariance and limiter behavior. Added coverage freezes all original preset renders,
checks sub-drop settling and legato non-retrigger, measures new basses across MIDI 24..96 with accent/full velocity,
and checks both chord profiles' release and single-sample versus 256-sample rendering. Saturation's third-harmonic /
fundamental power rises from below 0.000001 to 0.023573 in the sine-bass fixture (drive 0 versus 100).

Host Release best of 200 256-frame blocks, four chord voices plus bass and all six drums: original heavy acid mix
7.8 us; digital pluck/sub drop/punch 7.1 us; glass pad/hard acid/digital 8.2 us. These noisy host timings are comparative
only: they are **not device ISR measurements**. New bass pitch work occurs every 16 frames; optional saturation adds
one multiply, a shift and a branch per bass sample. Unison adds one phase accumulator, lookup and averaging per active
chord voice/sample, without doubling the FM modulators.

Host `sizeof`: `ChSynth` 272 -> 304 B, `BsSynth` 112 -> 116 B, `DrKit` remains 304 B. No pool buffers are added.
Chord presets remain 16 B (the new byte occupies padding), bass presets 22 -> 24 B, drum presets remain 68 B.
Device flash/static RAM totals, audio deadline, emulator load, and hardware sound require the coordinator's full native
build and emulator gate. No device has been flashed or benchmarked for these changes.

## Remaining decisions and work

Owner listening review should select or reject these sounds before genre defaults are finalized. There is no claim of
a supersaw, new operator-routing topology, dedicated lead track, stereo unison, per-track effects, oversampling or
anti-aliasing improvements. High-register FM/square/saw aliasing remains inherited work. The chord track can already
play lead notes with these envelopes, but allocating a fifth track would change the product architecture and is not done.

Next engineering tasks: measured high-register alias control, optional stereo unison with preserved center image,
small envelope/timbre editing UI, and vocal-aware gain/ducking experiments using full singer-plus-band renders.
