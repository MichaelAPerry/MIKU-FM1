# Current MIKU-FM1 handoff

Start with **[CLAUDE_CODE_REVIEW.md](../CLAUDE_CODE_REVIEW.md)** for the independent
review of draft PR21 on `feature/miku-nextgen-performance`.
[CLAUDE_HANDOFF.md](../CLAUDE_HANDOFF.md) is the concise current index.

The actual six-operator DX7 renderer, BANK/operator editor, eight source patches
and embedded v5 project snapshots are implemented. Current commands, limitations,
listening examples and native proof are in [DX7_BANK_HANDOFF.md](DX7_BANK_HANDOFF.md).
Earlier plan-only status and the64 KiB flash-reserve requirement are superseded.

Current validation baseline:46 local host tests pass; default/video/benchmark
native builds and default/video emulator smoke pass. Video image538444 bytes,
43120 free; static40720; pool266080. Physical testing and sustained USB throughput
remain outstanding. No main merge, release, website deployment or physical flash
is authorized by this handoff. The original broader assignment remains unfinished.
