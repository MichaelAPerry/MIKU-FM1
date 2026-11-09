# Current MIKU-FM1 handoff — 2026-11-09

The complete, current engineering/review handoff is **[CLAUDE_HANDOFF.md](../CLAUDE_HANDOFF.md)**. Start there. It covers owner authorization, feature readiness, file map, native/host distinctions, exact tests/builds, memory headroom, evidence reproduction, known limitations and the pre-main review checklist.

- Branch: `feature/miku-nextgen-performance`; [PR21](https://github.com/MichaelAPerry/MIKU-FM1/pull/21) remains a draft.
- Owner authorized the feature-branch push on2026-11-09 and will have Claude review before main integration. No merge/main push, release, website deployment or hardware flash is authorized by that instruction.
- Default native features: silent SELECT, slower session-adjustable ANIMAL, added bass/drum presets, shared default dance state, lossless compact portrait/dance drawing.
- Optional native development profile: MIDI+UVC160×120 MJPEG with incremental integer JPEG. Real camera enumeration/delivery is unverified; UAC/CDC disabled in this profile.
- Host prototypes: voice-compensation audition, word compiler, contemporary GEN, pedal DSP and recorded-state companion. Only original outfit is runtime-registered; eleven new wardrobe concepts are not production skins.
- Current host suite:26/26 passed. Generated checks and separate real patched-driver fake-SIE test passed. Native default/video and emulator milestones are detailed in the main handoff and `VALIDATION.md`.
- Actual default image/static/pool:152696/35540/254784B. Optional video:159080/33732/265216B. Reserves enforce64KiB image,16KiB static,32KiB pool free. See [memory report](NEXTGEN_MEMORY.md).
- Fresh GitHub clones do not contain ignored WAV/log/package evidence. The main handoff includes regeneration commands; [durable snapshot](evidence/NEXTGEN_REVIEW_SNAPSHOT.json) preserves measurements and hashes.

Earlier stacked handoffs were preserved in [the historical archive](archive/HANDOFF_BEFORE_CLAUDE_REVIEW.md). Their old workflow/automatic-merge instructions and superseded figures are historical only. Current owner instructions take precedence.
