# Experimental contemporary GEN candidates (2026-11-08)

Current integration status: [NEXTGEN_NATIVE_INTEGRATION.md](NEXTGEN_NATIVE_INTEGRATION.md)
and [CLAUDE_HANDOFF.md](../CLAUDE_HANDOFF.md) supersede earlier original-only,
host-only, provisional source-grid and memory figures in this milestone document.

The POP/BALLAD/DANCE/CHIP engine remains available as the legacy baseline. `gen_next.c/h` implements a separate,
integer-only composition engine for native GEN and host review. Native selection/controls are integrated separately in
`native/dinghy.c`; the five modern directions remain candidates, not a finalized musical lineup.
No assertion of genre authenticity substitutes for listening approval.

## Implemented behavior

Every candidate produces normal editable four-track Song patterns and existing arrangement section labels. The seed,
style, key, major/minor selection, energy and complexity determine output. Repeated harmonic/density contexts share
patterns and recurring short melodic contours; section transitions add drum fills and style-specific kick cutouts.
Unlocked regeneration follows a locked chord track's actual root and quality. Locked tracks retain their steps and
settings; locked-arrangement generation preserves the existing section structure, name, tempo and master mix.

| Candidate | Distinctive engineering choices | Form in bars; BRIDGE is an existing label |
| --- | --- | --- |
| Hyperpop | 164 BPM, bright pluck/acid/chip kit, octave-displaced syncopated bass, high hook register, chopped chord stabs, busy digital fill | intro 1, verse 4, bridge 2, hook 4, breakdown 2, hook 4, outro 1 |
| Future Bass | 148 BPM, pad/sub kit, seventh harmonies, syncopated sustained chord gates, half-time clap, connected bass with slide flags, longer vocal tones | intro 2, verse 4, bridge 2, drop 4, verse 2, bridge 2, drop 4, outro 2 |
| Jersey Club | 140 BPM, sparse organ/sub, repeating syncopated five-kick grid, one-step vocal chops and bass, restrained harmony | intro 1, hook 4, verse 2, hook 4, breakdown 2, hook 4, outro 1 |
| Digicore / Glitch Pop | 172 BPM, chip arpeggiator/square kit, irregular hat holes, displaced octave bass, single-step syllables and 32nd chord arps | intro 1, verse 3, hook 4, break 1, hook 4, verse 3, outro 1 |
| Electropop / Idol Pop | 132 BPM, electric-piano/FM/808 kit, seventh movement, root/fifth bass motion, stepwise recurring melody, four-on-floor hook | intro 2, verse 4, bridge 2, chorus 4, verse 4, bridge 2, chorus 4, outro 2 |

Density has sparse/verse/hook roles. Energy controls percussion openness and onset density; complexity controls fills,
hat rolls and percussion accents. These are bounded controls, not an unconstrained randomness amount. Existing section
labels represent proposed breakdowns/prechoruses/drops, so no song-file enum changes are needed. Sparse sections currently
leave vocals out; owner review should decide whether to add prechorus vocals or call-and-response there.

`gen_next_phrase()` rewrites one referenced pattern with a reproducible variation and refuses locked or silent rows.
Shared rows deliberately hear that edit, matching the existing pattern model. More than 16 required contexts per unlocked
track is refused atomically, leaving the song unchanged, rather than reusing an unrelated harmony. This can occur in a
highly varied hand-written locked arrangement. The caller must display the failure when native integration is approved.

## Review examples and regeneration

The host tool writes both a `.mks` project and a stereo WAV through the actual `Engine`, at 44.1 kHz. Files live under
`build/nextgen/` and are generated artifacts, not committed assets. Seed 1 uses energy 45 / complexity 30; seed 42 uses
energy 90 / complexity 85. Both use A minor. Candidate defaults use master 80 for mix headroom; locked-arrangement
regeneration keeps the existing master value. An initial Future Bass render at master 100 hit full scale twice; reducing
the candidate's new-song master to 80 removed those hits. This is measured gain staging, not an engine limiter claim.

```sh
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host
mkdir -p build/nextgen
# STYLE: 0 hyperpop, 1 future bass, 2 jersey club, 3 digicore, 4 idol pop
build/host/firmware/app/gen_next_render build/nextgen/hyperpop-1 0 1 45 30
build/host/firmware/app/gen_next_render build/nextgen/hyperpop-42 0 42 90 85
# Repeat for styles 1..4 with suitable output prefixes.
```

Measured with the branch's firmware sound engines at render time:

| Files under build/nextgen | Rows | Seconds including tail | Peak seed 1 / 42 | RMS seed 1 / 42 | Full-scale samples |
| --- | --- | --- | --- | --- | --- |
| `hyperpop-{1,42}.{wav,mks}` | 18 | 29.338 | 23089 / 24757 | 2561.8 / 2555.6 | 0 / 0 |
| `future-bass-{1,42}.{wav,mks}` | 22 | 38.673 | 29144 / 29478 | 4219.0 / 4152.5 | 0 / 0 |
| `jersey-club-{1,42}.{wav,mks}` | 18 | 33.855 | 28208 / 30211 | 2627.4 / 2709.9 | 0 / 0 |
| `digicore-{1,42}.{wav,mks}` | 17 | 26.720 | 24391 / 22516 | 2551.0 / 2558.8 | 0 / 0 |
| `idol-pop-{1,42}.{wav,mks}` | 24 | 46.637 | 29058 / 27864 | 3157.7 / 3152.0 | 0 / 0 |

These ten examples are listening material. They have not received owner listening approval; tests cannot measure musical
quality. Timbres are the existing playable preset IDs (including later branch improvements where present), not promises
of supersaws, samples or new DSP. Lead roles use the chord track; no fifth song track was introduced.

## Tests, resource evidence and limits

`miku_gen_next_tests` checks 360 generated configurations (five styles x three seeds x twelve keys x two scales):
bit-exact determinism, valid steps, encode/decode equality, locked chord steps/settings/rows, deterministic localized
phrase edits, pairwise distinct arrangement and all four track hashes, invalid settings, and atomic context overflow.
Every configuration crosschecks host-wrapper and explicit-workspace output byte for byte. Null and overlapping workspace
arguments are refused before mutation; ASan/UBSan passes with environment leak detection disabled. All ten historical
review `.mks` artifacts remain byte-identical after the workspace refactor.
The full host suite passed 15/15 at the first milestone; the focused GEN test passed again after the final freestanding
copy correction. Other agents add tests independently, so the final branch's total may be larger.

The initial prototype reserved 5,032 local stack bytes by copying a full Song; that implementation is replaced.
`gen_next_song_ws(g, cfg, workspace)` uses **840 bytes of explicit caller-owned workspace**, containing only rows,
context keys and pattern indices. It preflights the complete result before changing the Song, preserving transactional
capacity refusal. Native code should allocate one `GenNextWorkspace` in its fixed pool and call `_ws` from main/UI
context under the existing structural-edit lock; it never runs in the audio ISR. Do not alias the workspace with the
Song or settings. The host convenience `gen_next_song()` remains compatible and uses an 840-byte local workspace;
define `GEN_NEXT_NO_STACK_WRAPPER` before native inclusion to omit that wrapper entirely.

The updated isolated pi32v2 `-Os -ffunction-sections -fno-builtin -Wall -Wextra` compile, with the host wrapper omitted,
reports **3,368 bytes text/rodata**, zero .data/.bss. The compiler reserves **20 local stack bytes** for
`gen_next_song_ws`, plus saved registers and callees, rather than the former 5,032 bytes. Target assembly confirms
`sizeof(GenNextWorkspace)=840`. `nm -u` lists only `mora_find`, `sg_clear_pat`, `sg_step_ok`: no libc, floating point or
division helper. The caller-owned native pool allocation adds 840 bytes, with no heap or audio ISR work. Final linked
firmware image, UI generation time, full call-stack high-water, emulator and physical measurements belong to the native
integration validation; isolated-object measurements cannot substitute for those. Baseline preset IDs and all generated
notes/rows remain unchanged by this memory refactor.

Limitations: harmony changes once per bar, the grid remains sixteen sixteenths (only the existing chord arps can use
32nds), no triplet extension or fine-grained drum rolls, no new genre-specific drum kit parameters, no meaningful words
(the melody uses reproducible mora syllables), no sidechain DSP added here, and no visual concept is chosen. A locked
chord row with several chord events currently contributes its first chord to accompaniment; intrabar harmonic following
is unfinished. Energy/complexity are experimental settings and are not persisted. Candidate variation and genre contrast
are demonstrable, but further listening/tuning is needed before calling this a replacement workstation generator.

Owner decisions: which of the five directions to keep, desired aggression/brightness, preferred forms and vocal density,
and acceptable native control mapping. Next concrete task: validate the native selector and locked-regeneration flow using the explicit workspace, collect
approval on these ten examples, and refine the chosen rhythmic templates and motifs.
