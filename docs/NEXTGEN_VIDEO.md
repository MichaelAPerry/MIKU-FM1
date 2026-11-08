# USB video feasibility core (2026-11-08)

Working code: `firmware/usb/video/uvc.c/h` implements a bounded UVC 1.0 MJPEG class and payload core. It is freestanding,
integer-only, contains no hardware register access, and runs independently of the audio engine. It is **not proof that
an FM-1 enumerates as a camera**. Native HAL attachment, JPEG production, and physical-host testing are separate gates.
No hardware was flashed. The owner authorized video feasibility engineering while keeping Dinghy and preserving overhead.

## Profile and state machine

The initial profile offers one 160x120 MJPEG frame at nominal 15 fps (`dwFrameInterval=666667`, in 100 ns units), at most
8,192 bytes per encoded frame. EP4 IN uses at most 512 bytes per 1 ms packet, including a two-byte UVC payload header.
The independent descriptor contains only video control and video streaming interfaces; its VID/PID is a test placeholder
with PID zero, intentionally unsuitable for publishing. Never replace the updater/rescue descriptors with it. A native
MIDI+video descriptor adapter must rebase interface numbers and select a legitimate project identity/revision; the core's
`MUVC_VS_IF` is overridable for that adapter. Audio duplex is not promised by either profile.

PROBE supports GET_CUR/MIN/MAX/RES/DEF, GET_LEN and GET_INFO. It accepts a supported or unspecified format/frame index and
normalizes suggestions to the single advertised interval, frame cap and packet cap. Invalid indexes and malformed EP0
lengths stall. COMMIT requires a preceding PROBE and explicit acceptance of the canonical format, interval and caps;
unchecked host-provided lengths never control memory allocation or copies. SET_INTERFACE alt1 is accepted only after a
valid COMMIT. Unsupported alternate settings leave current state intact. A new setup cancels pending OUT control data.

The producer offers one immutable complete JPEG plus an opaque token. Accepted ownership lasts until the producer consumes
a release notification. New frames cannot overwrite a held or unacknowledged frame; this provides bounded backpressure
without a queue. SOI/EOI and the size cap are checked, but JPEG dimensions/content must be validated by the encoder. The
producer renders/encodes in main context, then briefly serializes mailbox operations against the USB timer. Encoding does
not occur with interrupts disabled.

`muvc_prepare()` copies one packet to the caller's DMA buffer. A second prepare cannot alter it until `muvc_ack()`.
Successful completion advances the source offset; a failed completion leaves the offset unchanged for retry. EOF is set
only on the last packet; one frame uses one FID, toggled after successful EOF completion. Buffer ownership releases only
then. An error signaled during a frame finishes any already queued payload and then emits a header-only ERR|EOF packet;
its completion releases the frame as failed. Once a final EOF packet is already on the wire, its bits cannot be changed
retroactively. Stop/reset must first flush the hardware endpoint, then release source ownership and clear pending DMA
state. Reset clears negotiation/stream state while preserving diagnostic counters. No frame is replayed after reset.

Frames are paced from their first prepared packet using unsigned microsecond clock differences, including timer wrap.
The minimum interval is 66,667 us (14.999925 fps); actual frame delivery depends on producer and host demand. A too-small
caller packet buffer fails without starting a frame or advancing state. The core never blocks while waiting for a frame,
USB completion, a deadline, or a producer. Counters track completed frames, dropped frames, packets, rejected offers and
control stalls. It does not claim that each caller ack represents a real USB transmission: the HAL adapter must enforce
that contract from endpoint status.

## Source review and precise Dinghy hooks

Reviewed Synthesthetic at `589bd0f30b68b66f78dab84debb42909131f4cad`, particularly `firmware/src/{usb.c,uvc_desc.h,vcore.c}`.
Its source demonstrates UVC1 probe/commit and FID/EOF payloads on AC79 EP4; its 320x240, 30 fps, 1,016-byte packet profile
and dedicated core-1 renderer are deliberately not copied as resource assumptions. This core uses a smaller independent
profile and explicit acknowledgement/ownership tests. See `NEXTGEN_USB_RESEARCH.md` for the broader comparison.

Reviewed the actual pinned Dinghy checkout (`build/dinghy`, revision selected by `native/build.sh`):

- `firmware/src/usb.c` owns every SIE INDEX transaction in `usb_poll()` / TIMER5. Keep all EP4 setup, packet send and
  completion work there. Do not add a competing USB ISR or register access from the audio callback.
- `uac_ep4_reset()` shows the controller's EP4 FIFO reset and `S_TXCSR2=0x40` isochronous mode. Video uses EP4 only in a
  profile where optional UAC capture is disabled. Never let both services select or arm EP4.
- `firmware/hal/fm1_usb.h` supplies `fm1_usb_ep4_txbuf()`, `fm1_usb_ep4_send()` and `fm1_usb_ep_enable(1<<4)`; EP4 has
  dedicated DMA address/count registers. `_send()` publishes memory with `csync` before DMA. Generic EP1-3 send helpers
  must not be used for EP4. DMA packet storage must remain stable until endpoint completion.
- At SET_CONFIGURATION, reset the video state and hardware FIFO. At SET_INTERFACE for the video interface, flush FIFO
  before calling `muvc_stream()`. Route class requests to `muvc_control_setup()`, collect exactly 26 bytes for SET_CUR,
  and apply `muvc_control_data()` before acknowledging status. Preserve EP1 MIDI, OTA SysEx and recovery logic.
- When TXPKTRDY clears, acknowledge the previously queued payload in the core, then prepare/arm one new payload. Never
  acknowledge merely because another timer tick arrived. On bus reset, suspend/disconnect or explicit alt0, flush before
  dropping ownership. A host stopping midway must not strand the producer forever.
- Construct descriptors once before attach; do not mutate a configuration already cached by a host. A video-enabled
  build needs its own device revision/layout and tests traversing every descriptor, interface and endpoint reference.

The optional integration is a build-time Dinghy extension, not a platform migration. Keep a baseline build and rescue
path. The separate native adapter work is documented in `NEXTGEN_VIDEO_NATIVE.md` when available; this file does not
claim the adapter or hardware validation is complete.

## Memory and bandwidth feasibility

Measured isolated pi32v2 `-Os -ffunction-sections -fno-builtin -Wall -Wextra` object: **1,118 bytes text/rodata**, zero
.data/.bss, `nm -u` empty (no libc or arithmetic helpers). Target `sizeof(Muvc)=60` bytes (host x86_64 is 64). The class
core allocates no frame or packet buffers. Caller storage for one JPEG (8,192), one DMA packet (512), core (60) and EP0
reply (26) totals **8,790 bytes**, before encoder state/row scratch, alignment, stacks and native adapter state. Double
JPEG buffering adds another 8,192 bytes and is optional; one buffer is sufficient for a first working stream at lower
producer throughput. No full frame image buffer is required if the renderer supplies encoder rows from a coherent
performance snapshot. The JPEG/renderer peer must report its actual scratch and stack separately.

The last measured native baseline in this branch used pool 254,784 / 344,064 (89,280 free) and static RAM 35,540 / 98,304.
Those figures do not include video. The conservative 8,790-byte class+single-JPEG subtotal leaves 80,490 pool bytes before
all remaining requirements **if** those caller buffers are placed in the pool. This is capacity arithmetic, not a linked
video build measurement. Native image/static/pool and CPU must be measured again once the adapter and encoder are linked.
No buffer should silently borrow song storage, alter saved projects or duplicate the existing 147 KB effect state.

At maximum JPEG size, 17 payload packets carry one frame; at 15 fps the JPEG payload is at most 122,880 bytes/s plus UVC
headers. The endpoint reserves up to 512,000 bytes/s on the full-speed periodic schedule, so average payload savings do not
justify an untested audio+video composite. Uncompressed YUY2 at 160x120x2x15 is 576,000 bytes/s, exceeding this profile's
510,000-byte/s image payload ceiling. YUY2 would require a larger packet reservation or reduced frame rate; raw RGB/YUY2
is not silently substituted for MJPEG. The 5.805 ms audio deadline and its safety margin remain unchanged.

## Tests and packet evidence

`test_uvc.c` exercises PROBE/COMMIT caps and invalid indexes/lengths, GET control replies, alt-setting refusal, bounded
source acceptance, immutable ownership, failed-send retry identity, buffer-capacity backpressure, exact payload assembly,
FID/EOF, ERR termination, mid-frame stop/reset, release tokens, microsecond wrap, frame pacing, and descriptor lengths.
The normal host test passes. ASan/UBSan passes with `ASAN_OPTIONS=detect_leaks=0`; LeakSanitizer itself fails under this
environment's ptrace, so leak detection is disabled (the core does not allocate). No hardware inference follows.

The host `muvc_packet_stream` tool uses this exact core, negotiates it, writes length-prefixed USB payload records, and
reconstructs JPEG bytes. A 160x120 gradient JPEG (host fixture, 1,609 bytes) produced four payloads, final EOF, and an
identical reconstructed JPEG decoded by Pillow as 160x120. Evidence: `build/evidence/video/{uvc-test.jpg,test.uvcp,
reassembled.jpg}`. This confirms framing and reconstruction; it is not a rendering performance measurement. Repeat on
the real integer renderer's output to validate the complete proposed producer pipeline.

```sh
# Parent CMake adds add_subdirectory(firmware/usb/video).
cmake --build build/host
ctest --test-dir build/host -R '^uvc$' --output-on-failure
build/host/firmware/usb/video/muvc_packet_stream input.jpg packets.uvcp reconstructed.jpg
```

Remaining gates: integrate reduced-memory JPEG producer; compile opt-in native profile; run actual package in fm1-emu;
verify graceful host stop/unplug and normal MIDI/update service; measure worst callback time including nested USB traffic,
frame throughput and producer time; then seek explicit hardware-testing authorization for camera enumeration, host latency,
USB reservation, underruns and OCT-minus rescue. Until those pass, video is a tested protocol core and feasibility path.
