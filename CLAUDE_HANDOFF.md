# Current task: 0.8 smooth and seen

Review [the 0.8 handoff](docs/0_8_REVIEW.md) and the latest branch/evidence SHA.
The PR21 material below is historical context, not the current assignment.

# MIKU-FM1 — current handoff for Claude Code

**Start with [CLAUDE_CODE_REVIEW.md](CLAUDE_CODE_REVIEW.md).** It is the independent
review brief for [PR21](https://github.com/MichaelAPerry/MIKU-FM1/pull/21), including
risk areas, reproduction commands, evidence limits and the requested report format.

Branch: `feature/miku-nextgen-performance`; target: `main`; PR remains **draft**.
Current implemented DX7 code: `2bb1662`; evidence milestone: `c394043`.
Read the actual current HEAD and record it in the review. No main merge/push,
release, live-site deployment or physical FM-1 flash is authorized by this handoff.

The DX7 renderer and BANK/operator editor are implemented. Earlier “DX7 plan only”
notes are historical. The owner removed the arbitrary64 KiB flash floor; actual
hardware limits and configured RAM/pool checks still apply.

| Current detail | Start here |
|---|---|
| Actual six-operator renderer, bank controls, v5 snapshots, audio/visual proof | [DX7 handoff](docs/DX7_BANK_HANDOFF.md) |
| Independent PR review assignment and source/test map | [Claude Code review brief](CLAUDE_CODE_REVIEW.md) |
| Native lyrics/GEN, ten-song library and paired wardrobe integration | [Native integration](docs/NEXTGEN_NATIVE_INTEGRATION.md) |
| Per-song outfit/background/effect selection and settings migration | [Device LOOK](docs/DEVICE_LOOK_HANDOFF.md) |
| Current measured resources/evidence hashes | [Snapshot](docs/evidence/NEXTGEN_NATIVE_INTEGRATION_SNAPSHOT.json) |
| Actual DX7 device walkthrough, auditions, editable projects and metrics | [DX7 evidence](docs/evidence/dx7-bank/README.md) |
| Previous longer handoff, preserved as historical context | [Archived handoff](docs/archive/CLAUDE_HANDOFF_BEFORE_PR21_REVIEW.md) |

Current validation baseline:46 local CTests pass; actual default/video/benchmark
native builds pass; default/video emulator smoke passes. The video build uses
538444 image bytes with43120 free;40720 static RAM;266080 pool. Emulator callback
worst2.718 ms in diagnostic high load versus5.805 ms deadline, zero overruns.
These are emulator measurements, not physical or sustained USB-video qualification.

This remains a development milestone. Arbitrary USB bank management, independent
named user banks, DX7 bend/sustain/MPE routing, physical USB/video testing, pedal
USB input/routing and live companion telemetry remain unfinished. GEN's candidate
presets are not automatically remapped to DX7 yet. Final musical/art/UI approval
belongs to the owner. Read the review brief rather than treating this index as
an approval to integrate.
