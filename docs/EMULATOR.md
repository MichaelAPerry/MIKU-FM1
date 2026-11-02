# Native FM-1 emulator acceptance gate

Target: https://joakimeriksson.github.io/fm1-emu/
Source: https://github.com/joakimeriksson/fm1-emu
Automation: https://github.com/DimmKirr/setup-fm1-emulator

The emulator runs **actual .fwsc firmware packages** (JieLi pi32v2) unchanged. MONK-1's separate WebAssembly web preview is *not* a substitute for a native emulated boot.

Source inspection of https://github.com/charlesvestal/fm1-monk confirms a 240x240 display and 256-frame 44.1 kHz stereo (24-bit PCM in int32) audio half buffers. Its board implementation, packager, and linker are GPL-3.0-only and require a JieLi toolchain and AC79 SDK.

MIKU-FM components now provide:
- `firmware/ui/stage.c`: original screen renderer to a 240x240 RGB565 pixel buffer, with play/stop/page/vowel controls.
- `firmware/platform/fm1_runtime.c`: four-part synthesis into 44.1 kHz stereo half buffers.
- Host tests of both components.

**Not yet a device firmware.** A real native board entry point must call the UI, update it via mapped panel buttons, and transfer its framebuffer to the LCD. The audio ISR must call the runtime, with XIP/stack/CPU budgets measured. We must generate a valid .fwsc, boot in fm1-emu, and confirm the device UI and sound. No such boot has been performed. Do not substitute a static framebuffer test or a WebAssembly reimplementation for this milestone.

Potential command line once the binary exists: `fm1-emu --fwsc build/miku.fwsc --ms 5000 --snap build/screen.png --wav build/boot.wav`. Confirm exact flags with the installed emulator version. Physical flashing should follow only after emulation and on-device risk review.

Licensing: a device integration derived from MONK-1/Felucca must respect GPL-3.0-only and other notices. The independently authored MIT engine may remain MIT as a component, but the linked derived firmware must satisfy GPL distribution requirements.
