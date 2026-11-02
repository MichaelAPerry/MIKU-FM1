# Initial FM-1 research (2026-11-02)

## Directly inspected
- [Official fm1_open README](https://github.com/m-vave/fm1_open/blob/main/README.md): V14 SDK-based firmware, browser preview, Windows JieLi PI32v2 toolchain 2.5.3 with r3 target. GPLv3 with FM1 SDK linking exception; binary SDK has separate terms. No claimed measured device budgets.
- [MONK-1 README](https://github.com/charlesvestal/fm1-monk/blob/main/README.md): vocal DSP uses formant-wave-function synthesis and 5 vowels. Upstream DSP attributed MIT, firmware as a whole GPL-3.0-only with separately MPL-licensed modules. Its 12 KB voice state is an upstream-reported figure, not a MIKU-FM measurement. Browser emulator exists.
- [Melodee licensing document](https://github.com/keremimo/melodee/blob/main/LICENSING.md): GPL-3.0-only derived from Felucca, many separately licensed DSP ports. Source exists; do not copy blindly.
- [FM1 custom firmware catalog](https://github.com/aroum/fm1-custom-fw): catalog and reverse-engineering material exists; specific MOONDAW/MAKU/Forge Phoenix sources have not yet been verified.

## Decision
Start with a clean-room host-testable C11 core under MIT. Keep FM-1 board integration as an adapter; do not incorporate GPL firmware while repository is MIT. Separately inspect actual SDK HAL, memory map, audio scheduling and third-party licensing before selecting a hardware base. MIKU-FM1 contains no derivative code from inspected projects at this point.

## Unknowns / next inspections
Device flash/SRAM allocation, available filesystem/storage, safe song persistence, input and display drivers, hardware scheduler and genuine executable emulator compatibility. No flash images are yet produced. Official simulator availability does not prove compatibility with arbitrary third-party firmware.
