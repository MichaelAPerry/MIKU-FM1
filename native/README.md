# Native FM-1 firmware

`dinghy.c` builds the actual MIKU-FM1 device application on pinned Dinghy. It uses
the formant singer, classic band and MSFA-derived six-operator DX7 backend; the
native song library, lyrics/GEN, wardrobe/stage controls and DX7 BANK editor are
integrated. The same device engine is used by host rendering tools. This is no
longer the initial five-vowel/eight-step native prototype.

Start review at [CLAUDE_CODE_REVIEW.md](../CLAUDE_CODE_REVIEW.md). Current DX7
controls, source provenance, save format and native evidence are documented in
[DX7_BANK_HANDOFF.md](../docs/DX7_BANK_HANDOFF.md). Setup and verification are in
[VALIDATION.md](../docs/VALIDATION.md).

Set JIELI_TOOLCHAIN and AC79_SDK to the documented local dependencies, then run
`bash native/build.sh`. `--video` builds the experimental MIDI+UVC profile;
`--video-bench` builds its offline encoder benchmark. Run builds sequentially;
they reuse/reset the ignored build/dinghy checkout. Preserve actual resource and
HAL checks. Device DSP remains freestanding and integer-only.

No physical hardware qualification or sustained USB-video result is claimed.
Do not flash hardware, merge main, release or deploy without owner authorization.
See LICENSING.md for GPL application, Apache MSFA and separate character-art terms.
