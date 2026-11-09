# Current MIKU-FM1 handoff

Start with **[CLAUDE_HANDOFF.md](../CLAUDE_HANDOFF.md)**. The detailed current
implementation is [NEXTGEN_NATIVE_INTEGRATION.md](NEXTGEN_NATIVE_INTEGRATION.md).

- Branch `feature/miku-nextgen-performance`; PR21 remains a draft for review.
- Native integration includes LYRICS/GEN, twelve paired outfits, bounded stage
  effects and a ten-slot song library with four editable factory songs.
- 38 host checks, actual native builds, emulator library/save/reboot, companion
  browser checks and target offline JPEG/audio evidence are recorded there.
- Final default image/static/pool: 504416/35728/255624 bytes. MIDI+UVC:
  510900/33924/266080 bytes, retaining 70664 image bytes free and passing the
  unchanged 64 KiB image /16 KiB static /32 KiB pool free floors.
- Owner authorized pushing the feature branch. No main merge/push, release,
  live-site deployment or physical-device flash is authorized or performed.
- Creative approval, physical USB testing, native pedal input transport,
  live telemetry and full-body mouth compositing remain outstanding.

[Durable review evidence](evidence/native-integration/README.md) contains actual
screenshots, packed-art contact sheets, listening copies, editable factory
songs and measurements. Earlier handoffs are archived and do not override the
owner's current instructions.
