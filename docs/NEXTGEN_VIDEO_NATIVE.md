# Optional native UVC integration prototype

Current integration status: [NEXTGEN_NATIVE_INTEGRATION.md](NEXTGEN_NATIVE_INTEGRATION.md)
and [CLAUDE_HANDOFF.md](../CLAUDE_HANDOFF.md) supersede earlier original-only,
host-only, provisional source-grid and memory figures in this milestone document.

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

The development descriptor keeps the existing project test VID/PID and changes `bcdDevice` to4.10 to distinguish
host descriptor caches. It is not a newly allocated production USB identity. Configuration is252bytes/four interfaces:
audio-control0/MIDI-streaming1 (unchanged EP1 OUT/IN), video-control2/video-streaming3, alt0 without endpoints,
alt1 EP4 IN isochronous512bytes/1ms. Advertised mode is160×120 MJPEG, nominal15fps, maximum8192bytes/frame.
An audio/MIDI IAD and a video IAD group their respective interfaces. Raw configuration traversal is checked by tests.
The shared UVC class core is included into the driver's translation unit; it must not also be compiled there twice.

## Ownership and interrupt work

`native/video_usb.inc` exposes three main-loop helpers: `video_usb_active()`, `video_usb_busy()` and
`video_usb_publish(jpeg,length)`. Publish accepts one complete JPEG; the caller must retain its immutable storage until
busy becomes0. A single-producer mailbox publishes pointer/length before its volatile ownership flag. The TIMER5
consumer alone operates the UVC core, EP0 class state, packetization and endpoint DMA. There is no shared core mutation
from main, no busy waiting, no heap, no frame rendering or JPEG encoding in either USB or audio ISR.

The consumer copies at most510JPEG bytes plus a2byte UVC header to one aligned512byte DMA buffer. It does not prepare
another payload until the previous TxPktRdy clears; only then does it acknowledge completion and advance the core.
The JPEG is released after final payload completion, not merely after preparing EOF. Alt0, new configuration, bus reset,
detach and observed suspension flush EP4 before releasing ownership. A late publication interrupted by alt0 is cancelled
on the next consumer pass. A queued payload unconsumed for40timer polls (approximately20ms) stops/flushed the stream;
the host must reopen alt1. This bounded timeout policy needs real-host evaluation. It prevents a disconnected host from
holding the caller's single JPEG indefinitely while the native instrument continues independently.

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
-the existing MIDI rescue SysEx command.

This is a software protocol/ownership test. Fake SIE completion is not a measured host IN transaction, and it does not
reproduce the PHY, DMA coherency, bus timing or OS camera stack. Actual native link/resource figures, main-loop JPEG
cost and emulator evidence are recorded by the coordinator in the development/validation log. The opt-in
`--video-bench` build encodes bounded main-loop frames for emulator inspection without pretending an enumerated host
is present. It is separate from transport validation.

Next concrete task: capture real descriptor/control traces and MJPEG frames on an authorized FM-1 connected to a host,
measure frame throughput under simultaneous native synthesis and MIDI traffic, and verify recovery/update behavior.
No broader USB combination or platform migration should be inferred from this isolated profile.

## Actual native integration result (2026-11-08)

Root integration now builds `native/build.sh --video` as a development-only MIDI+UVC package. Actual image159080B/static33732B/pool265216B; loader package9067B is preserved. Resource margins and tradeoffs are in [NEXTGEN_MEMORY.md](NEXTGEN_MEMORY.md). The package passes fm1-emu smoke with audio and display; no real USB host enumeration has been tested. Main-loop encoding is incremental, one MCU/pass, with one immutable JPEG buffer. An independent `--video-bench` profile measures encoding without pretending a USB host is present; three target JPEGs completed, recovered image matches the host reference exactly. Do not use the benchmark profile as camera-stream evidence.
