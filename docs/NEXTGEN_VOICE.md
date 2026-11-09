# Voice evaluation: bounded pitch/vowel compensation

This is an opt-in DSP evaluation API, not a newly selected product sound. `FvVoice.level_comp` is 0 by default (the original voice); 100 selects the full correction. It is not mapped to a device knob or saved in MKS2. The owner has not listened to or approved this sound.

## Implementation

Measured machine-exact MIKU at every MIDI note 60–96, each AIUEO, velocity 127, brightness 100, no breath/human/scoop/vibrato. Each fresh note renders 160 blocks of 256 frames; RMS and peak use the last 80 blocks. The generated Q12 table contains notes 72–96, matched to each vowel's note-72 RMS, with no attenuation and a maximum +6.02 dB lift. Notes <=72 remain exact. Log-pitch interpolation follows portamento. Gain changes use the existing per-sample ramp; no additional oscillator, filter or sample bank.

Only the MIKU character with neutral fine formant shift uses this calibration. Other characters retain baseline output. Compensation applies after consonant phases; the consonant noise band is separate and unchanged. It does not change formant positions, excitation, pitch or timing. The +6 dB gain path halves the multiplier before multiplying to avoid signed 32-bit overflow; neutral gains use the original multiply exactly.

## Reproduce audio and measurements

```
mkdir -p build/evidence
cc -O2 -Wall -Wextra -Wpedantic -I. tools/voice_compare.c firmware/audio/fvoice/fvoice.c -lm -o build/voice_compare
build/voice_compare build/evidence/voice > build/evidence/voice.csv
```

This writes `voice-before.wav` and `voice-after.wav` (35 matching fresh notes, notes 48/60/72/78/84/90/96, AIUEO within each note, 100 ms silence between entries). CSV records RMS, peaks, cap hits and normalized waveform error. Files live under ignored `build/`; regenerate them from the committed tool.

| Note/vowel | Baseline RMS | Compensated RMS | Change | Peak after |
| --- | ---: | ---: | ---: | ---: |
| 78 A | 1768 | 3535 | +6.02 dB | 9539 |
| 84 A | 6167 | 6560 | +0.54 dB | 10331 |
| 90 O | 2653 | 5306 | +6.02 dB | 12823 |
| 96 A | 614 | 1228 | +6.02 dB | 3564 |
| 96 I | 4109 | 5969 | +3.24 dB | 11234 |
| 96 U | 2547 | 4703 | +5.33 dB | 12113 |
| 96 E | 6180 | 6180 | 0 dB | 10753 |
| 96 O | 941 | 1881 | +6.02 dB | 4638 |

All 35 paired examples have no output cap hits. Normalized steady waveform error (`1-correlation^2`) is below 0.000001, supporting unchanged spectral shape and pitch under constant gain. This is a loudness lift, not a demonstrated improvement in perceived vowel identity.

## Tests and limitations

`tests/test_fvoice.c` now renders all notes 48–96 × five vowels at both settings: oscillator phase/increment must match, samples <=72 must match, power gain stays between unity and 4×, normalized waveform error <0.00001, and no newly clipped sample is allowed. Fresh consonant phases in every class must match the baseline exactly. The focused fvoice test passes; parent validation must relink and run the whole suite.

There is an existing raw U-vowel overload at note 92, velocity 127: steady RMS 27401, peak 32767. The correction is unity there, so it neither creates nor fixes it. Do not describe this voice as globally free of clipping. Additional character, source-parameter, legato and effect combinations still need measurement.

The severe A loss at note 96 is only reduced by 6 dB; it remains much quieter than note 72. More aggressive make-up gain could increase noise, phrase jumps and mix overload. A listening pass and a separate investigation of formant/excitation behavior are required before selecting a product control or default. Nasality, diphthongs, word/phrase format and expression are still unfinished.

Host `cc -Os` object code+constants: 14459 → 14921 bytes (+462); generated table is 250 bytes. `sizeof(FvVoice)` is unchanged at 392 bytes on this host (new parameter occupies padding). No pool allocation. This is not a JieLi flash or audio-ISR measurement. Physical device and emulator timing must be recorded separately by the coordinator; no hardware was flashed.
