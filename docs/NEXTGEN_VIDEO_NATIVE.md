# Isolated native MIDI+UVC profile

Current 0.8 status: [review handoff](0_8_REVIEW.md) and
[validation](VALIDATION.md). They supersede the historical integration figures
at the end of this document. No physical host enumeration is claimed.

This is an isolated Dinghy development profile, selected explicitly with `native/build.sh --video`.
It retains the existing MIDI interface and updater SysEx on EP1 and adds UVC1.0 MJPEG on EP4 IN.
It does not add USB audio or CDC, and does not establish physical host enumeration, camera-app compatibility,
video throughput or simultaneous audio/MIDI/UVC operation. No physical device has been flashed.

## Build isolation and descriptors

`tools/patch_dinghy_video.py DINGHY_ROOT` accepts only the SHA-256 of `firmware/src/usb.c` from pinned
Dinghy `f77bf03283bbb2c05294b01cdad68a30b410f93d`. It refuses changed source rather than guessing anchors.
The generated source selects the patched driver only for `MIKU_UVC=1` and a non-loader build; its other branch is
an exact copy of the original source. The rescue/update loader therefore retains its original MIDI-only driver.
Default builds never invoke this patch. UAC/CDC combinations are compile-time errors in this video profile.

The development descriptor keeps the existing project test VID/PID and changes `bcdDevice` to4.11 to distinguish
host descriptor caches and advertises `MIKU-FM VIDEO`. The 18-byte camera
terminal declares its three-byte control bitmap consistently. It is not a newly allocated production USB identity. Configuration is252bytes/four interfaces:
audio-control0/MIDI-streaming1 (unchanged EP1 OUT/IN), video-control2/video-streaming3, alt0 without endpoints,
alt1 EP4 IN isochronous512bytes/1ms. Advertised mode is160×120 MJPEG, nominal15fps, maximum8192bytes/frame.
An audio/MIDI IAD and a video IAD group their respective interfaces. Raw configuration traversal is checked by tests.
The shared UVC class core is included into the driver's translation unit; it must not also be compiled there twice.

## Ownership and interrupt work

`native/video_usb.inc` exposes `video_usb_active()`, `video_usb_busy()`,
`video_usb_generation()`, `video_usb_publish(jpeg,length,generation)` and
`video_usb_take_free()`. Main owns two fixed8192-byte compressed buffers in the
pool, with one encoder and an immutable scene snapshot. It encodes only into a
free buffer. Publish transfers that buffer through a single-frame mailbox;
`busy()==0` means mailbox capacity, not permission to overwrite the cached image.
A buffer is writable again only when `take_free()` returns its pointer, or a
stream generation change has invalidated all ownership.

TIMER5 accepts a mailbox only between class frames, swaps its cached pointer,
and returns the old pointer through one release slot. Publish refuses another
mailbox until that slot is drained, so a release cannot be overwritten. The
cached image stays immutable through final packet completion and may repeat on
the nominal15fps schedule while main encodes the other buffer. Each packet
copies at most510JPEG bytes plus a2byte header to the aligned512byte DMA buffer.
There is no JPEG-sized copy, rendering or encoding in TIMER5, which outranks
audio. The default offline benchmark retains one buffer and measures encoding;
physical USB-ISR cost remains unverified.

`session_fresh` counts newly published JPEGs actually transmitted, and
`session_repeated` counts cached replays. Fresh means a new encoder publication,
not a different pixel hash. Neither count alone proves physical USB throughput.
The consumer checks publication generations again, rejecting a mailbox
interrupted by stop/reset/resume. Main and TIMER5 each own one side of the
mailbox/release handoff; only TIMER5 operates the UVC core, EP0 and endpoint DMA.

Alt0, configuration, reset, detach and suspension flush EP4 before releasing
class ownership and invalidate the cached image. A late publication is cancelled
on the next consumer pass. A queued payload unconsumed for40timer polls is flushed
and its frame dropped; the selected stream restarts without a host alt1 reselect.
Resume restarts the selected stream with a fresh generation/image. Recovery,
DMA coherency and physical timing still need the owner's later hardware testing.

Probe/Commit requests route only to streaming interface3 with exact26byte SET_CUR data stages. Wrong interface/entity,
unsupported control, invalid length and malformed Commit stall. SetupEnd and SentStall clear pending control OUT state.
Alt1 requires a valid negotiated Commit. Short GET replies use the existing EP0 sender. The adapter does not expose
unsupported camera/processing controls or promise compatibility with applications that require them.

## Validation

Run `python3 tests/test_native_video.py [DINGHY_ROOT]` after cloning the pinned Dinghy repository. It obtains the pinned
source with `git show`, applies the exact production patch, compiles that real driver against a fake SIE HAL and drives
its actual EP0/EP4 functions. Tested and passing:

-252byte descriptor traversal, four interfaces, unchanged MIDI endpoint pair and512byte video endpoint;
-GET_LEN, GET_CUR, Probe and Commit, short OUT rejection, alt1-before-Commit rejection;
-wrong/high interface indices and unsupported alt rejection;
-payload boundaries, EOF, immutable pending DMA contents and source release only after hardware completion;
-alt0 flush, abandoned-control SetupEnd, late mailbox publication, suspension and USB reset;
-the existing MIDI rescue SysEx command;
-150 consecutive fresh reconstructed frames (180,000 bytes), then150 frames
 during an encoder pause (1 fresh/149 repeated), all at66–67 ms simulated
 intervals with exact payload/FID/EOF and safe producer-buffer reuse;
-suspend/resume, stalled endpoint recovery, queued mailbox ownership and stale generations.

This is a software protocol/ownership test. Fake SIE completion is not a measured host IN transaction, and it does not
reproduce the PHY, DMA coherency, bus timing or OS camera stack. Actual native link/resource figures, main-loop JPEG
cost and emulator evidence are recorded by the coordinator in the development/validation log. The opt-in
`--video-bench` build encodes bounded main-loop frames for emulator inspection without pretending an enumerated host
is present. It is separate from transport validation.

Encoding advances one MCU per main-context service, including LCD DMA waits after
platform initialization. A twelve-row decoded sprite cache avoids repeated per-pixel
atlas traversal; a separate fixed three-row effect cache also avoids per-pixel
sprite decoding. All 720 cached/reference fixtures produce identical JPEG bytes.
CI compiles with `-DMIKU_VIDEO_BENCH_FRAMES=0 -DMIKU_VIDEO_BENCH_END_MS=39000`:
encoding continues across the full generated DX7 arrangement, starts no new frame
after the cutoff, and finishes the last JPEG before the 40 s emulator dump.
The report requires at least 150 completed frames and 29 s between first/last
completion, timed LCD transfers, both DX7 tracks and zero audio overruns.
The default bounded benchmark remains three frames. Encoder FPS is distinct
from completed USB FPS.
Delivered counters and formulas are in the [evidence guide](evidence/smooth-and-seen/README.md).

Hardware traces, camera-app compatibility on macOS/Windows/Linux and sustained physical
throughput remain unverified. The owner permits experimental installation only after
Claude review and main integration; no device access or installation is part of this work.

## Actual native integration result (2026-11-08)

Root integration now builds `native/build.sh --video` as a development-only MIDI+UVC package. Actual image159080B/static33732B/pool265216B; loader package9067B is preserved. Resource margins and tradeoffs are in [NEXTGEN_MEMORY.md](NEXTGEN_MEMORY.md). The package passes fm1-emu smoke with audio and display; no real USB host enumeration has been tested. Main-loop encoding is incremental, one MCU/pass, with one immutable JPEG buffer. An independent `--video-bench` profile measures encoding without pretending a USB host is present; three target JPEGs completed, recovered image matches the host reference exactly. Do not use the benchmark profile as camera-stream evidence.
