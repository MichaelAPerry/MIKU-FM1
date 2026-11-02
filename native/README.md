# Experimental native FM-1 application

This original **integer-only** test application uses the existing GPL-3.0-only
Dinghy FM-1 platform (https://github.com/zvenson/dinghy), rather than
creating a new emulator. Its application code draws an original singer and
responds to PLAY, SEQ, the ALGORITHM encoder and note keys. The five vowel
options are only distinct harmonic recipes, **not** Japanese phonemes or
formant filtering. Eight steps play at a fixed tempo.

The combined Dinghy-derived firmware must comply with GPL-3.0-only and
retain upstream copyright and notices. This file does not replace the
main MIT song engine; native integration of the complete engine is pending.

CI tries to build a real firmware package with the Linux JieLi compiler
and AC79 SDK. No device or emulator success is claimed unless verified.
Do not flash an untested development binary on physical hardware.
