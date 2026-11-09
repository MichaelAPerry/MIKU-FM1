# Bounded stereo pedal DSP prototype (2026-11-08)

`firmware/audio/pedal/pedal.c,h` implements the application-side routing needed before a future USB playback endpoint
can be integrated. It accepts incoming stereo PCM samples directly. It does not implement USB descriptors, a USB ring,
clock feedback, enumeration or real-device streaming. Dinghy and standalone MIKU remain intact.

```text
caller input L/R -> clamp -> peak / mean-square / block-rise analysis
                       -> dry -------------------------+
                       -> independent stereo drive/LPF +-> wet/dry -> output gain --+
caller synth L/R ---------------------------------------> synth gain -------------+-> mix headroom -> clamp
```

The caller owns all buffers and one `Pedal` per stream; no buffers or state are shared with the singer's effect chain.
The routine processes 1..256 stereo frames, rejects oversized/empty blocks without changing state/output, never blocks,
and uses only integer arithmetic. Input, generated audio and output may alias. Analysis observes incoming input before
mute, gain, effects or mixing, so setting output level to zero still permits visual analysis. `transient` reports the
positive increase in block peak, not a musical beat detector. `mean_square` is a quantized incoming mean square, not RMS.

`PedalSet` controls are prototype engineering parameters, not an approved hardware interface:

| Parameter | Range/default | Behavior |
| --- | --- | --- |
| wet | 0..100 / 100 | interpolation between original stereo signal and processed stereo signal |
| drive | 0..100 / 0 | bounded rational saturation; zero skips saturation exactly |
| tone | 0..100 / 100 | independent per-channel one-pole low-pass; 100 skips filtering |
| level | 0..127 / 127 | pedal output gain; 127 unity |
| synth | 0..127 / 0 | optional already-rendered MIKU gain; 127 unity |
| bypass | 0..1 / 0 | wet target zero; preserves stereo input |
| mix_headroom | 0..100 / 50 | gain applied to both routes when connected input and enabled synth are combined |

The first call applies requested gains exactly. Subsequent wet/bypass, level and synth gain changes use a per-frame
integer ease to avoid gain steps and reach their exact target. Drive and tone changes are currently immediate.
At bypass/unity, with no synth route and after a bypass transition settles, samples equal input exactly. With both
paths enabled, default 50% headroom keeps full-scale aligned signals inside the output range; selecting 100% headroom
can deliberately hit the clamp and increments an output clipping counter. Generated audio continues at its selected
gain if input is absent. Null input resets filters, clears input analysis and produces no pedal tail or stale signal.
Transport disconnect/underrun fade behavior remains a separate integration choice: immediate zero can click at a
nonzero input boundary, so the future input-ring adapter should ramp before passing null input.

## Validation

`pedal_tests` verifies exact bypass stereo routing, analysis while muted, effects on both channels, in-place processing,
independent instances, disconnect clearing, continued synth on disconnect, default mix headroom, clipping counters,
INT32 extremes, invalid-control clamps, invalid-block transactionality, and sample-identical processing with 256-frame
versus sixteen 16-frame blocks, including control smoothing. Direct GCC AddressSanitizer/UndefinedBehaviorSanitizer
run passes (`ASAN_OPTIONS=detect_leaks=0`: the environment's ptrace setup prevents LeakSanitizer operation; no heap is used).

The `pedal_render` host tool reads an actual canonical stereo PCM16 44.1 kHz WAV and routes it through the same module:

```sh
build/host/firmware/app/song_render build/evidence/pedal-input.wav --rows 2 3
build/host/firmware/audio/pedal/pedal_render build/evidence/pedal-input.wav build/evidence/pedal-processed.wav 60 65 75
build/host/firmware/audio/pedal/pedal_render build/evidence/pedal-input.wav build/evidence/pedal-unity.wav 0 0 100
cmp build/evidence/pedal-input.wav build/evidence/pedal-unity.wav
```

Evidence generated during development uses the current actual MIKU engine render as the external WAV fixture; it is
not a claim of a live DAW or USB connection. Input and unity files match byte-for-byte. Processed audition is deliberately
an effect comparison, not a loudness-matched sound-quality claim. Both files have 143027 stereo frames (3.24 seconds):

| Render | Peak | RMS | Full-scale samples |
| --- | ---: | ---: | ---: |
| input | 27623 | 5457 | 0 |
| drive60, wet65, tone75 | 28655 | 10377 | 0 |

No listening approval, latency or physical-output quality is claimed.

## Resource measurements and limits

`Pedal` is 44 bytes and `PedalSet` seven bytes on the host; no static RAM, pool buffers or heap allocation. Caller input
and output blocks require up to 2048 bytes each if stored separately, and may reuse existing buffers. Host freestanding
`cc -Os -fno-builtin` object: 1136 bytes text, zero data/BSS, no unresolved symbols or libc calls. Host `-O2` timing of
100000 full stereo 256-frame blocks, drive100/tone50/wet75 with synth mixing: approximately 7.05 microseconds per block
in this environment. These are x86 figures, not FM-1 instruction cost or ISR measurements.

The module is currently a host-validated routing prototype, not linked into `native/dinghy.c`. Native flash/RAM/pool
cost is therefore zero until an explicitly tested integration adds it. No USB throughput, underrun or hardware deadline
measurement exists for this module. There is no delay/reverb/chorus allocation and no hidden reuse of mono voice FX.

Next concrete engineering task: an isolated integer playback-ring adapter that consumes incoming USB frames before
this pipeline, ramps starvation/reset transitions, counts underruns/overruns, and exposes host-tested reset and clock
feedback behavior; then build and benchmark the actual Dinghy USB extension. Native capture should initially exclude
pedal input to avoid implicit DAW feedback. Hardware testing still requires separate authorization. Pedal control mapping,
tail/bypass semantics, capture routing and computer presentation remain owner decisions.
