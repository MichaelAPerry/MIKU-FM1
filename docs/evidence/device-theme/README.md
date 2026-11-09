# Pink / blue device-wide theme

Implementation: `2414571281b179cc4a6133e291cd1e1d025450e2`.
Branch: feature/miku-nextgen-performance. PR21 remains draft; this UI iteration
is not completion of the original USB/DSP/genre assignment.

[All device windows](all-device-pages.png) ·
[Actual firmware walkthrough](all-pages-walkthrough.mp4) ·
[Measured results](measurements.json).

## Owner direction and implemented scope

The owner requested stronger pink/blue styling across the entire device,
more borders and subtle gradients. This replaces the earlier menu-only charcoal
pass. No new outfit/genre creative direction was silently selected.

- Shared blue (#4DBEFF) and pink (#FF5BAC) accents, violet edge tones and dark
  blue/purple gradient panels. RGB565 naturally quantizes the shallow gradient.
- SONG: bordered arrangement rows, selected-row accents and pattern-cell edges.
- PATTERN: shared header/frame, violet cursor column and consistent dark grid.
  Existing track colors and playhead remain distinct for editing.
- MIXER: gradient channel cards, bordered selected channel and existing level/pan
  controls. Mute/solo and track colors retain their meaning.
- GEN: gradient style/seed/shape/lock panels and common framed header.
- LYRICS: bordered articulation rows and matching selected-unit treatment.
- LOOK: gradient parameter/theme/portrait cards, bright selected accents and
  common frame. Portrait/body pairing and per-slot persistence are unchanged.
- LIBRARY: matching gradient song cards, selection indicators and header.
- STAGE: pink/blue corners outside the existing portrait plus matching gradient
  control strip and voice-parameter popup. Original portrait pixels are intact.
- DANCE: matching bordered performance strip; stage artwork/effect choices remain
  independently selectable rather than being replaced by a menu background.
- Held-SAVE window: framed slot/name card, gradient action panel and common
  palette. It uses the existing canvas in bands instead of black text boxes.

No new navigation or save semantics were introduced. HOME still cycles nine
pages, EDIT from DANCE opens LOOK, and ARP opens the library. Existing unsaved
confirmations, intentional PATTERN preview, silent live syllable selection and
STOP FIRST behavior remain. Boot/update/recovery diagnostics are platform screens
and retain their established rendering; this pass covers the device's musical
pages, performance strips and save window.

## Engineering and validation

`ui_panel` and `ui_rim` in native/dinghy.c centralize the treatment. Panels use
integer-only procedural four-row gradient steps and clipped rectangle primitives.
There are no new bitmaps, heap allocations, framebuffers, pool buffers or audio
ISR operations. The existing 48-row canvas and one-band-per-main-loop redraw
remain; the held-SAVE window reuses that canvas for its five bands. Save-window
rendering remains a main-loop operation; audio interrupts continue independently.

The actual default firmware was booted and recorded in fm1-emu while the song
played and all nine pages were visited. The capture also exercised a voice knob
popup, stopping, holding SAVE, and releasing to save. Individual PNGs are native
240x240 LCD captures; the contact sheet adds only external labels. The MP4 uses
nearest-neighbor scaling and captions outside the LCD. No simulated replacement
synthesizer or browser UI was used.

All 40 Release host CTests pass (12.05 seconds). Native default and MIDI+UVC
builds pass unchanged resource guards. The 30.5-second capture has 1338112 stereo
audio frames, peak 12152 and zero full-scale samples. This clipping check is not
proof against every possible audible artifact. No new physical CPU, ISR, USB
throughput, JPEG timing or delivered video-frame-rate measurement is claimed.
The earlier target JPEG benchmark is historical, not rerun for this chrome pass.

| Profile | Image used/free | Static RAM used/free | Pool used/free |
|---|---:|---:|---:|
| Default |507396 / 74168|35760 / 62544|255624 / 88440|
| MIDI+UVC |514044 / 67520|33984 / 64320|266080 / 77984|

Compared with d880f21, the image grows 696 bytes for default and 704 bytes for
video, with zero additional static RAM or pool use. The video profile retains
1984 bytes above its unchanged 65536-byte image reserve. Further substantial
features require savings or an explicit resource tradeoff; do not relax the
reserve silently or drop an outfit. No physical FM-1 was flashed, main merged,
release published or live website changed.

## Review / next task

Review punch, border strength, gradient visibility and readability on these actual
screens. A physical LCD check is still needed when hardware testing is authorized.
Further UI refinement can use the shared helpers; additional large artwork should
be weighed against USB overhead. The outstanding USB audio routing, physical UVC
validation and broader musical-engine work remain in the engineering handoffs.
