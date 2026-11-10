# Incremental JPEG rendering prototype (2026-11-08)

Current 0.8 integration uses all wardrobe atlases and a fixed decoded-row cache;
see [native profile](NEXTGEN_VIDEO_NATIVE.md) and [validation](VALIDATION.md).
The original-only timings below are historical.

The working host prototype renders MIKU's actual existing original atlas and shared
`PerfState`/`perf_pixel` scene into a baseline colour JPEG, without a framebuffer.
It is firmware C compiled for the host, not a captured screenshot or a replacement
browser synthesizer. Character art remains PCL, free/noncommercial, with its credit
in `LICENSING.md`; code provenance is in `firmware/usb/jpeg/NOTICE.md`.

## Implementation and ownership

`mjpeg.c` adapts Synthesthetic's GPL-3.0-only integer encoder at pinned revision
`589bd0f30b68b66f78dab84debb42909131f4cad`. It retains the integer DCT, standard
Huffman coding, quantisation and JFIF output, with 4:2:0 subsampling. It replaces
Synthesthetic's framebuffer/zoom paths with a RGB565 pixel callback and moves all
mutable data into a caller-owned `MjEncoder`.

`mj_begin()` accepts dimensions up to160x120,quality1..100 and output cap up to8192.
`mj_step(j,max_mcus)` processes at most that many16x16 tiles;zero budget does no
work. It returns0 pending,1 complete,-1 failed. `mj_size()` returns0 until a whole
JPEG,including EOI,is complete. Overflow never publishes a partial frame. The
caller must freeze its pixel context and own its output buffer throughout encoding.
After USB accepts a complete frame,that buffer remains immutable until the video
packetizer releases it. A second8192-byte buffer permits rendering the next frame.

The provided `mj_perf_pixel()` directly reads the existing odd-width packed sprite
with continuous nibble parity and samples the procedural background. It preserves
the original240x240 scene's aspect in a120x120 square centred in160x120 output.
No full frame,pixel row,heap,screen operation or USB operation is required. The
original eight body poses are used; no new costume or artwork is introduced.
The body atlas itself has no dynamic per-phoneme mouth overlay: this prototype
retains its original pose mouths; actual engine lip-sync data remains available
through `PerfState`,and a separately approved/engineered overlay is future work.

Call encoding from a preemptible main loop or a dedicated video worker,never the
audio or USB ISR. One-MCU steps provide an explicit scheduling boundary but do not
establish a device-time deadline. This module performs no multicore handoff.

## Reproduce

```
mkdir -p build/evidence/video
cc -O2 -std=c11 -Wall -Wextra -Wpedantic -I. tools/video_render.c \
  firmware/usb/jpeg/mjpeg.c firmware/usb/jpeg/performance_frame.c \
  firmware/ui/performance.c firmware/ui/stagefx.c firmware/ui/sprite_codec.c \
  -o build/video_render
build/video_render build/evidence/video
python3 firmware/usb/jpeg/check_decode.py build/evidence/video
```

The tool writes24 actual-scene JPEG/PPM pairs (three backgrounds x eight original
poses),one159x119 edge-replication fixture and one1x1 fixture. The PPMs are exact
pre-compression RGB565 source pixels for comparison; JPEGs decode with Pillow's
libjpeg backend26/26. Each160x120 frame requires80MCUs,including the final replicated
bottom edge. One-MCU and80-MCU scheduling produce byte-identical files. Independent
encoder contexts,overflow rejection,output guard bytes,invalid dimensions/quality/
capacity and completed-state stability are exercised. AddressSanitizer and UBSan
pass; this environment requires `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer
cannot run under its ptrace arrangement. No allocations occur in the encoder.

At quality65,the24 current scenes produce3475..5035bytes/frame. RGB PSNR versus the
exact PPM is24.2..27.2dB; 4:2:0 visibly softens saturated pixel-art edges. This is
compression evidence,not owner approval of image quality. One observed Release
host run averages0.36ms/frame; worst observed MCU59us on that run. Host timings are
noisy and are not physical-device or emulator deadlines.

At15fps,this fixture range implies52,125..75,525JPEG bytes/s. The8192-byte cap
limits encoded payload to122,880bytes/s at15fps. Actual USB scheduling,host camera
buffering,frame rate and coexistence still require the transport integration tests.

## Device compile/resource evidence

JieLi clang4.0.1,pi32v2,-Os,-ffreestanding,-fno-builtin,-Wall compiles warning-free:

| Standalone object | Text | Read-only data | Mutable globals | Undefined symbols |
| --- | ---: | ---: | ---: | --- |
| Integer encoder | 4634B | 3004B | 0 | none |
| Original-atlas scene callback | 164B | 17392B | 0 | perf_outfit,perf_pixel |

The callback object's read-only data includes the existing~17KB original body
atlas; a native unity build that already includes `singer_sprites.h` uses its
header guard and must avoid counting/copying this art twice. Its standalone table
cost is therefore not automatically incremental flash. The caller-owned encoder
is2240B on pi32v2 (2256B on this64-bit host); two output buffers require16384B,
for18624B of persistent encoder/output memory before transport/settings state.
No framebuffer or image row buffer is added. Put these buffers in the shared pool,
not automatically into scarce static RAM. Stack depth must be measured in any
native integration; DCT local scalars still use stack.

The current pinned Dinghy source has no existing CPU1/JPEG scheduler to reuse.
Synthesthetic's `vcore.c` uses a persistent CPU1 job,cache-line-separated mailboxes,
flash-operation parking and two16KB JPEG buffers plus indexed framebuffers.
Those facilities are not ported here. A future multicore port must coordinate
flash erases/programs and update recovery; merely running this encoder on CPU1
would not supply those safeguards. Preemptible main-loop stepping is the smaller
initial integration candidate.

This report validates host output and target compilation only. It does not claim
that native USB enumeration,simultaneous audio/MIDI/UVC,15fps,hardware ISR budget,
or physical recovery/update behavior has been established. See the transport
and native-video integration reports for their separate measured status.

## Compact atlas path

With `-DMIKU_COMPACT_SPRITES=1`,the scene callback reads the shared lossless compact
atlas through `spr_pixel()` instead of indexing the original raw body atlas.
Compile/link `firmware/ui/sprite_codec.c` as well. All26generated JPEG fixtures
are byte-identical between these paths; compact output decodes26/26 and passes
AddressSanitizer/UBSan with leak detection disabled as above. This confirms the
JPEG pixels match the original without introducing a duplicate raw body atlas.

The compact callback's standalone pi32v2 object is162B text and14499B read-only
atlas data (including the32-byte palette); its unresolved symbols are only
`perf_outfit`,`perf_pixel`,`spr_pixel`. A native unity build already owning the
compact atlas shares it through its header guard. Do not add this standalone
atlas cost a second time when reporting native incremental flash. A Release host
run measured0.59ms/frame versus0.36ms for raw pixels; worst observed MCU116us
versus59us. Random pixel access scans compact RLE records,so row caching may be
useful if native measurements show a problem; no additional row buffer is
allocated by this implementation. These are host observations,not timing promises.
