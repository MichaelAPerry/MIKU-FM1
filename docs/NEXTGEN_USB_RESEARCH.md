# USB pedal and performance research (2026-11-08)

Status: source review and engineering proposal, **not an implemented USB feature**. No physical FM-1 was accessed,
no firmware was flashed, and no claim of combined USB audio/MIDI/video operation in MIKU follows from this review.

## Sources inspected

Pinned revisions make the findings reproducible. Links below refer to source, not assumptions from product names.

| Project | Revision | Relevant source |
| --- | --- | --- |
| fm1-base | `b0976f295ccaeaf77a8053a5a785a9cc21f5345f` | [README](https://github.com/charlesvestal/fm1-base/blob/b0976f295ccaeaf77a8053a5a785a9cc21f5345f/README.md), [uac_mix.h](https://github.com/charlesvestal/fm1-base/blob/b0976f295ccaeaf77a8053a5a785a9cc21f5345f/device/uac_mix.h), `device/main_fm1.c` |
| fm1-base's felucca-bsp submodule | `5b0879bfadc69a17dbebb917fb6b86d516f19c44` | [chip/usb.c](https://github.com/hugelton/felucca-bsp/blob/5b0879bfadc69a17dbebb917fb6b86d516f19c44/chip/usb.c) |
| Melodee | `165d177a425944f3d30b0b669255a6ed0050c3a5` | [USB stream engine](https://github.com/keremimo/melodee/blob/165d177a425944f3d30b0b669255a6ed0050c3a5/firmware/src/usb_audio_stream.c), `usb_audio.c`, `usb_audio_desc.h`, `audio.c`, `audio_worker.h` |
| Synthesthetic | `589bd0f30b68b66f78dab84debb42909131f4cad` | [USB](https://github.com/michaeljsmalley/synthesthetic/blob/589bd0f30b68b66f78dab84debb42909131f4cad/firmware/src/usb.c), [UVC descriptors](https://github.com/michaeljsmalley/synthesthetic/blob/589bd0f30b68b66f78dab84debb42909131f4cad/firmware/src/uvc_desc.h), `vcore.c`, `audio.c`, `DEVELOPMENT.md` |
| MIKU's pinned Dinghy platform | `f77bf03283bbb2c05294b01cdad68a30b410f93d` | [USB](https://github.com/zvenson/dinghy/blob/f77bf03283bbb2c05294b01cdad68a30b410f93d/firmware/src/usb.c), selected by `native/build.sh` |

USB direction terminology is from the host's perspective: OUT is computer to FM-1 (playback); IN is FM-1 to computer
(capture). An upstream README calling a feature an "audio input" can therefore mean computer recording, not pedal input.

## What the projects actually implement

### fm1-base

The BSP implements UAC1 capture on EP4 IN and optional stereo PCM16, 44.1 kHz computer playback on EP4 OUT.
Playback advertises an adaptive isochronous endpoint. `uo_service()` copies packets into a 2,048-frame packed-stereo
ring (8,192 bytes). `uac_play()` primes to `2 * HALF_FRAMES`, normally 512 frames, then linearly resamples using a
ring-fill servo. Its nominal Q16 step accounts for an approximately 44,117.6 Hz I2S clock rather than assuming the USB
and codec clocks match. Ring starvation increments an underrun counter and re-primes. Service may nest in audio render
to avoid losing a 1 ms USB packet while rendering a 5.8 ms audio half.

`device/uac_mix.h` makes the routing explicit: first tap app audio for computer recording, then add computer playback
for the speaker/headphones, then apply physical MASTER if recording is configured at full level. Computer playback is
excluded from capture, avoiding a direct DAW monitoring loop. The playback ring is consumed even at gain zero.

This is a useful reference for monitoring and clock handling, but **its application interface is too late for pedal DSP**:
the app has already rendered when computer audio is added. MIKU needs a separate input-consumption API before effects.
Also, the pinned BSP's `uac_play()` uses float interpolation/servo arithmetic and a `uint64_t` product; `uac_mix()` has a
float gain pass. These cannot be copied unchanged into MIKU's freestanding integer-only DSP. fm1-base app code is MPL-2.0;
retain relevant file notices and review the actual source license before reuse.

The platform keeps MIDI and updates; it can place MIDI after the audio streaming interfaces for macOS compatibility.
Its optional screen mirror demonstrates a state/display transport path, but is not UVC. No migration to fm1-base is
needed to use these architectural findings.

### Melodee

Melodee implements one UAC1 duplex function at a fixed 44.1 kHz: IF2 audio control, IF3 playback, IF4 capture. EP2 OUT
carries stereo playback; EP3 IN supplies explicit three-byte 10.14 feedback; EP2 IN carries capture. Alternate settings
select PCM16 or packed PCM24. Internally the rings are int16: the low byte of PCM24 input is discarded and recording
pads it with zero. Offering 24-bit transport does not make the internal path 24-bit.

The two stereo rings are 1,024 frames each (8,192 bytes total), with a 512-frame target (11.61 ms at 44.1 kHz).
Four mono capture stems are optional and require extra storage. A low-pass ring-fill servo adjusts capture packet size
and playback feedback. The native-rate stream does not require a sample-rate converter. `ua_audio()` captures the
post-FX/limiter synth before adding computer playback, again preventing a direct capture/playback feedback loop.
Standard playback mute/volume use a Feature Unit, and host gain changes are smoothed.

`usb_audio.c` keeps SIE access in TIMER5, including stream resets and SET_INTERFACE. Each short audio-block copy is
serialized against USB service. The source documents a controller-specific hazard: exact hardware MaxP activates
double buffering and loses every other OUT packet, so the hardware MaxP register is set to `0xFF` independently of the
descriptor. RX DMA has a four-byte guard. Capture packets are prepared ahead of refill time rather than packed after
the endpoint becomes empty. These details need controller tests, not a blind descriptor-only port.

Resets and underruns clear readiness and prime again rather than replaying stale audio. Counters distinguish playback
and capture overruns/underruns and bad packets. Melodee has USB descriptor, driver, stream and hardware CPU test tools;
their existence is useful validation infrastructure, not evidence that MIKU passes those tests. Its separate audio worker
supports core-1 retirement/fallback; UAC transport itself does not require dedicating core 1.

Melodee is the strongest integer transport reference for a Dinghy extension. It still adds playback after its own synth
effects; MIKU must change the insertion point for pedal processing.

### Synthesthetic

The video build is a composite device with MIDI on EP1, FM-1-to-computer stereo capture on EP3 IN, and video on EP4 IN.
It retains updates over USB-MIDI. This proves a concrete source design for **MIDI + capture + video**, not host playback
plus capture plus video. No computer-playback OUT audio path was found in the inspected `usb.c`.

UVC 1.0 offers MJPEG 320 x 240 at a 333,333 x 100 ns interval (nominal 30 fps), with 1,016-byte full-speed isochronous
packets: a two-byte header and up to 1,014 JPEG bytes per millisecond. EP4 has no optional status endpoint. Probe/commit,
alternate setting stop/reset, FID/EOF packet headers, and processing-unit controls are implemented. Closing the host stream
releases the in-flight frame so the renderer does not wait forever.

Core 1 runs a persistent render + integer software-JPEG job; core 0 retains audio, USB and UI. Two 160 x 90 indexed visual
buffers are reused inside the larger SCREEN mirror allocation, and two JPEG buffers each have a 16,384-byte cap.
The camera descriptor's 49,152-byte maximum is not the actual encoded-buffer allocation. SCREEN mode uses a 240 x 240
indexed copy. The main visual allocation is therefore 57,600 + 32,768 = 90,368 bytes, excluding palettes, mailboxes,
analysis state, encoder stack and other code/data. The job publishes complete buffers via one-writer mailboxes; USB
holds a buffer until finished. JPEG quality adapts to size/time.

Once video owns core 1, no audio jobs are assigned to it. Before flash operations core 1 must acknowledge parking in a
RAM-only loop; failure aborts flash access. This is a substantial integration requirement for song saving and updates.

## MIKU integration constraints

MIKU currently exposes `app_audio(out, n)` and `en_render()`; neither accepts incoming audio. Pinned Dinghy has optional
UAC capture, not the playback path needed for a pedal. Its timer-service/ring design is a starting point; adding an OUT
descriptor alone cannot provide sound. Keep Dinghy and make any USB transport extension isolated and build-selectable.

The existing `vx_render()` is a **mono-source, single-instance** voice effect chain: it reads the left channel as input
and regenerates stereo. Calling it on arbitrary stereo USB input would discard the original right channel. Calling it
again for input audio would also share/advance the singer's delay/reverb/chorus state. Its approximately 147 KB of fixed
buffers cannot simply be duplicated. Drive and tremolo are cheap candidates for a new stereo instance; chorus/delay/
reverb need deliberate sharing or a measured smaller independent design. Retain the singer's current FX path initially.

The last documented MIKU pool usage is 254,784 / 344,064 bytes, leaving 89,280 bytes; this is historical, not a new build
measurement. Synthesthetic's 90,368-byte main visual allocation alone exceeds that remainder by 1,088 bytes, before USB
rings and all overhead. Thus a direct full SCREEN/video transplant does not fit the documented configuration. A smaller
renderer without SCREEN mirroring, smaller FX allocation, or explicit mutually exclusive modes would need measurement
and owner approval for behavior tradeoffs. Stereo UAC16 duplex rings alone add about 8 KB, plus packets/state.

## Recommended architecture: pedal transport plus performance events first

Preserve the standalone synth. Add a native-rate UAC1 playback prototype based on explicit feedback and integer rings,
behind an opt-in build flag. Service endpoints in the platform USB timer; the audio callback only consumes bounded blocks.
Maintain the updater's MIDI endpoint and rescue descriptors. Do not replace the whole platform to obtain playback.

Proposed signal path:

```text
USB OUT packets -> bounded stereo input ring -> input trim -> level/transient analysis
                                                        -> dry path -----------------+
                                                        -> stereo pedal FX -> wet ---+-> pedal gain
MIKU voice + instruments (existing engine/FX) -----------------------------------------+-> headroom/limiter
                                                                                       -> physical MASTER/output
```

Place analysis before effects so a long reverb tail does not create false attacks; optionally publish wet level too.
Wet/dry, bypass and output gain must be bounded and smoothed. Bypass must preserve both input channels and keep the ring
consumed. On disconnect/reset/starvation, ramp input to zero, clear stale ring data and re-prime; standalone synthesis
continues. Do not repeatedly reinitialize the song or voice. Count every underrun, overrun and malformed packet.

Capture routing must be explicit. Start with generated MIKU audio only, before pedal input; processed-input capture would
be a later selectable route because host software monitoring can otherwise form a loop. A host route is not a fifth song
track and must not silently change MKS2 or overwrite saved mix settings.

For computer visuals, send versioned snapshots/events from real firmware state: sample-clock timestamp, sequence number,
mouth/vowel and consonant state, mora/lyric token, vocal level, note/gate, beat/row/section, selected visual IDs, input level
and transient flags. Publish a bounded mailbox from the audio engine; serialize/send in the main/timer context. Use the
existing USB-MIDI transport with a distinct SysEx namespace initially, rate-limit and coalesce snapshots, and give updates
and note messages priority. A 32-byte payload at 60 Hz is 1,920 bytes/s before MIDI packing/framing; budget approximately
3 KB/s and measure it. Reconnect requests a full state snapshot. The companion interpolates animation and lets the
computer render approved outfits/backgrounds at native screen resolution. It must not synthesize substitute audio.

| Choice | Performance events (recommended first) | Native UVC (separate prototype) |
| --- | --- | --- |
| Device work | State copy + serialization; measure cost | Full renderer + JPEG + USB video service; core 1 ownership |
| Bandwidth | Approx. 3 KB/s proposed at 60 Hz | Up to 1,016 bytes/ms reserved video endpoint, plus audio/MIDI |
| Resolution | Host rendering can exceed LCD resolution | Source example 320 x 240, nominal 30 fps |
| Latency | Event transport + host frame; timestamp/interpolate | Render + JPEG + packet transmission + host camera buffering |
| Host integration | Companion required | Standard camera consumers such as OBS |
| Memory | Small state/ring buffers | Tens of KB of frame/JPEG buffers plus stacks |
| Maintenance | Protocol/version + companion assets | USB class/descriptors, JPEG, multicore/flash coordination |

These are architectural comparisons, not benchmark results. Keep UVC as an optional feasibility branch until measured.
At 30 fps a 16,384-byte JPEG averages 491,520 bytes/s. A 1,014-byte/ms payload ceiling permits about 33,800 bytes/frame
at 30 fps before other traffic. Uncompressed RGB565 320 x 240 x 30 is 4,608,000 bytes/s and cannot fit full-speed USB.
Stereo PCM16 at 44.1 kHz is 176,400 bytes/s **per direction** (352,800 bytes/s duplex); stereo PCM24 duplex is 529,200
bytes/s. Adding nominal payload rates is insufficient: periodic bus reservation includes maximum packet sizes, token/
header overhead, feedback packets and scheduling. A 1,016-byte video endpoint plus duplex audio may exceed a host's
periodic allocation even when average JPEG payload looks small. Combined enumeration must be built and tested.

## Required prototype gates

1. Host-test ring wrap, exact stereo routing, gain/bypass ramps, starvation/reset recovery, malformed packets, saturation
   and analysis at all boundaries. Verify external input is available before effects and no right channel is dropped.
2. Build actual Dinghy firmware; record image, .data/.bss and pool deltas. Check no floating point, libc, heap or 64-bit
   division enters device DSP. Boot with fm1-emu; emulator USB coverage may be incomplete and cannot certify real hosts.
3. With separate hardware authorization, test enumeration, continuous playback, disconnect/reconnect, MIDI notes and
   update/rescue on Linux/macOS/Windows. Measure codec-clock drift, input-to-output latency, ring fill, peak callback
   time including nested USB work, underruns and clipping under worst-case singer/effects/UI load. Target callback
   remains 256 / 44,100 = 5.805 ms; reserve substantial margin rather than using the entire deadline.
4. Validate performance snapshots against the actual voice mouth/consonant state, reconnect state and dense MIDI/update
   traffic. Produce synchronized screen/audio evidence. No new character/outfit concept is finalized by this protocol.
5. If UVC is desired, prototype it independently with the existing MIKU renderer/art, reduced buffers and flash-park
   safeguards. Test frame rate, dropped frames, JPEG size distribution, host camera latency, USB reservation and all
   simultaneous stream combinations. Preserve OCT-minus rescue and update compatibility before considering integration.

Owner decisions still open: companion visual presentation and new interfaces, approved outfits/backgrounds, pedal control
mapping and bypass/tail behavior, capture routing choices, and whether driverless UVC is worth resource tradeoffs.
No such creative or major behavior choice is finalized here.
