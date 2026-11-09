# Embedded DX7 renderer (MSFA-derived)

This is a freestanding integer C adaptation of Google's Music Synthesizer for
Android (MSFA), not a new two-operator model and not the Dexed desktop wrapper.
Original source revision: f67d41d313b7dc85f6fb99e79e515cc9d208cfff,
https://github.com/google/music-synthesizer-for-android/tree/f67d41d313b7dc85f6fb99e79e515cc9d208cfff/app/src/main/jni

Copyright 2012–2013 Google Inc. Apache-2.0; full LICENSE is included here.
The combined MIKU firmware remains GPL-3.0-only. The separate patch-bank source
and license/provenance are in assets/dx7/README.md; artwork retains its PCL terms.

Ported behavior comes from dx7note.cc (frequency/velocity/key scaling), env.cc,
pitchenv.cc, lfo.cc, fm_core.cc and the scalar fm_op_kernel.cc. The 32 algorithm
routes and calibration constants are retained in dx7_constants.h. Floating-point
LUT initialization is replaced by tools/gen_dx7_tables.py and immutable generated
constants. There is no device libm, heap, C++ runtime or desktop dependency.
Fixed-width 64-bit intermediate multiplies are used; native compilation emits no
soft-int64 helper calls. The existing fmband mx_gain supplies gain/pan.

All six operators have four envelope rates/levels, ratio/fixed tuning, coarse/fine
and detune, velocity, keyboard level/rate scaling and AMS. The renderer includes
pitch envelope, six LFO waveforms, delay, pitch modulation and amplitude modulation.
Carrier sum is normalized in the output adapter for four-voice backing headroom.
The original MSFA omission for multi-operator feedback in algorithms4/6 is handled
with sample-major evaluation for those two routes when feedback is enabled.
Oscillator/LFO sync-off phases follow the synth's sample clock; held voices retain
their own patch snapshot. Loading/editing a patch changes future note onsets,
without retriggering a sounding note or changing its oscillator pitch.

The caller owns all state. dx_init chooses 1–4 voices, dx_patch validates the full
128-byte Yamaha packed voice transactionally, note_on/off and all_off manage voices,
and dx_render adds interleaved stereo Q15 to a caller buffer. Arbitrary block sizes
are supported through per-voice64-frame caches. A note-off reaches a cached block's
next envelope update within63 samples (~1.43 ms). Released carrier envelopes ramp
the final block to zero and retire; no indefinitely allocated silent voices remain.

This is DX7-compatible synthesis, not a hardware-bit-exact claim. Original MSFA
frequency/detune/envelope scaling remains approximate, the sine table differs by
small rounding amounts, and AMD calibration is approximate. Sync-off phase follows
a deterministic sample-clock model. MIDI pitch bend, sustain/MPE routing and arbitrary
USB bank transfer are not added by this backend. Native BANK edits real operator
and global patch fields, and saved songs embed the actual packed patches.

## Independent reference and checks

reference_vectors.h was generated from original MSFA using note60/velocity100,
44100 Hz,64 blocks of64 samples per voice; every second block's first raw Q24
sample is retained (32 points per patch). PMD/AMD were disabled for this core
comparison; source packed transpose was applied. The host reference uses original
MSFA scalar kernels with host-only Controllers and patch-transport stubs. The port
must stay within128 raw Q24 units at every retained point across all eight voices.
This tolerance covers the direct-math sine LUT versus upstream's Q30 oscillator
LUT initialization. Full-patch LFO, three-note polyphony and release are separate
tests. Reference fixtures are not used to generate device audio.

Tests also exercise all32 algorithms, algorithm4/6 feedback0 versus7,17-frame versus
256-frame exact output, A4 pitch by crossings, strict malformed-patch rejection,
held-patch snapshot isolation, stealing, natural release and oscillator sync.
Host ASan+UBSan runs use ASAN_OPTIONS=detect_leaks=0 because this environment does
not support LeakSanitizer tracing. Actual target image/state/timing results belong
in docs/DX7_BANK_HANDOFF.md; standalone object sizes are not final device budgets.
