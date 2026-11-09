# Native visual menu redesign

Implementation: `d880f21`, feature/miku-nextgen-performance. PR21 stays draft.

[Watch the actual firmware emulator walkthrough](menu-walkthrough.mp4).
[Wardrobe and library contact sheet](menus.png).
[Fresh boot restored custom scene](restored-look.png).

The previous text-list LOOK screen now has a charcoal surface, a selected-outfit
header/counter, a 140x96 stage preview, matching portrait, theme card, four
parameter cards, intensity meter, twelve outfit position marks and button hints.
Character aspect ratio is preserved inside the preview; stage geometry is fitted
to its card. PRESETS changes the theme; REC triggers the effect; EDIT returns to
DANCE; SAVE keeps the scene using the existing stop-first rule. The stage preview
refreshes when settings change; it is not a continuously animated preview.

The song library now uses inset cards, numbered slot badges, a teal selection
rail/underline and a pink loaded-song marker. Unsaved songs retain the asterisk.
Existing library navigation, confirmation and save semantics are unchanged.
The original STAGE/DANCE presentation is unchanged. Pattern, mixer, GEN and lyric
editor layouts have not received this visual redesign; do not claim an all-page
UI replacement. Additional artwork and full-screen menu buffers were unnecessary.

## Validation

- All 40 Release host CTests pass (10.82 seconds).
- Actual default and MIDI+UVC firmware build with unchanged resource guards.
- Actual default firmware booted in fm1-emu; changed outfit, background, effect,
  intensity and palette; saved; navigated library; loaded a second scene and played.
- A fresh emulator process restored the saved custom scene from emulator flash.
- Native LCD screenshots were inspected for clipping, readability and paired art.
- Video uses native LCD captures at 10 fps, nearest-neighbor enlarged to 720x720,
  with explanatory captions outside the screen. Audio is unnormalized emulator PCM
  encoded as AAC. Intro is silent until PLAY at 19 seconds.

| Profile | Image used/free | Static RAM used/free | Pool used/free |
|---|---:|---:|---:|
| Default |506700 / 74864|35760 / 62544|255624 / 88440|
| MIDI+UVC |513340 / 68224|33984 / 64320|266080 / 77984|

This pass costs 160 image bytes, zero static RAM and zero pool bytes relative to
71035d9. The video image retains 2688 bytes above the unchanged 65536-byte reserve.
The earlier three-frame offline JPEG benchmark was not rerun for this UI-only
change; its figures remain historical. No physical hardware, USB throughput or
new CPU deadline result is claimed. No release, main merge or website deployment.

## Owner review / next step

Review this actual device workflow and visual hierarchy before applying the same
card treatment to the remaining editors. Full original feature completion,
physical USB tests and sustained video performance remain outstanding as listed
in [the device LOOK engineering handoff](../../DEVICE_LOOK_HANDOFF.md).
