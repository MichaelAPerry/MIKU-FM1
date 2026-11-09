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
the final block to zero and retire once every carrier is quiet (gain below 32768, a
peak of 8 LSB or -72 dBFS at full mix) instead of waiting for the envelope to end; a
slow release therefore frees its voice well before the envelope floor. A carrier that
holds an audible L4 after key-off keeps its voice, as on a DX7, until the voice is stolen
or dx_hush is called. dx_hush (used by SAVE) ramps every sounding voice to zero within two
blocks and frees it. Stealing a sounding voice fades its next 64 samples out through a
small per-synth tail buffer (DX_FADE) instead of cutting it. The LFO delay ramp detects
its 32-bit wrap against the previous state, like MSFA's 64-bit overflow test.

This is DX7-compatible synthesis, not a hardware-bit-exact claim. Original MSFA
frequency/detune/envelope scaling remains approximate, the sine table differs by
small rounding amounts, and AMD calibration is approximate. Sync-off phase follows
a deterministic sample-clock model. MPE and arbitrary USB bank transfer are not added by this backend. Native BANK edits real operator
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
tests/test_dx7_fixes.c covers the LFO delay for every value 0-99, quiet retirement, dx_hush,
the steal fade and L4 sustain.
Host ASan+UBSan runs use ASAN_OPTIONS=detect_leaks=0 because this environment does
not support LeakSanitizer tracing. Actual target image/state/timing results belong
in docs/DX7_BANK_HANDOFF.md; standalone object sizes are not final device budgets.

## Performance controls

dx_note_legato retunes a gated voice (the newest one when there are several; in the
engine's one-voice bass that is the bass note) without touching envelopes, LFO or
phases. Every ratio operator's log-frequency base moves by the semitone difference
and an offset ramps linearly back to zero over glide_ms (updated once per 64-sample
block, 0 = jump, capped at 60 s); fixed-frequency operators stay put. The original
note's velocity, key-level and rate scaling are kept (a slide is the same note
moving, as on a mono synth), and the patch transpose captured at note-on is reused,
so a patch edit never changes the interval. With no gated voice it is dx_note_on.
The engine uses a 60 ms glide for sequenced bass notes that carry the slide flag.

dx_set_bend (signed 14 bit) times dx_set_bend_range (semitones, default 2) is a
log-frequency offset added to every sounding voice, held or releasing, next to the
pitch envelope and LFO pitch; it takes effect at each voice's next 64-sample block.
dx_set_sustain keeps voices that received dx_note_off sounding until the pedal is
released, when those whose key is up are released. dx_all_off and dx_hush ignore the
pedal. Stealing order: free, released, pedal-held, oldest. Tests: tests/test_dx7_perform.c.
