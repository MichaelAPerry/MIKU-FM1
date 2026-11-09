Playback with DX7 patches currently checksums both patches every audio block, and several wardrobe singing poses are larger than idle poses. This draft moves patch checks to the locked edit boundary, reuses decoded DANCE rows, preserves redraw cadence phase, and normalizes upright wardrobe pose heights while keeping the bow distinct.

It also fixes BANK/LIBRARY hint layout, adds checked user-bank name editing, and extends CC64 to classic chords/bass and the singer. Native draw/audio telemetry and a deterministic host benchmark support further measurement. MKS2 format and the flash HAL are unchanged.

Validation: 52/52 host CTest checks pass, including 729 text-layout states; generator check and patched native UVC fake-SIE tests pass. A 60-second DX7 host render has identical PCM hash before/after and zero desktop deadline misses (mean callback 21.904 to 19.320 us). These are host results, not physical callback guarantees. Wardrobe accounting is 339850/340000 bytes.

Before/after host UI images and decoded sprite contact sheets: [review evidence](evidence/smooth-and-seen/README.md). They are explicitly not emulator screenshots.

Still required before completion:
- Authentic JieLi/AC79 default and --video builds, linked image/static/pool checks, changed-package emulator smoke and emulator page captures. Toolchain download is currently denied by environment outbound policy.
- Sustained UVC fps/JPEG size/frame-time and macOS/Windows/Linux enumeration evidence. Keep the isolated MIDI+UVC profile; USB-audio coexistence is unproven.
- Owner listening review before changing CLUB STAB, DIGI GROWL and DIGI PLUCK trims.
- Claude review before main integration; owner hardware tests occur only after that process. No device has been accessed or flashed. Main and release tags were not pushed.

Full verified/unverified record: [VALIDATION.md](VALIDATION.md).
