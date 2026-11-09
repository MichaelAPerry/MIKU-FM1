# SPDX-License-Identifier: GPL-3.0-only
"""Run the actual native MIDI-in routing (note ownership, retried note-offs, pedal / wheel) against a fake engine ring."""
from pathlib import Path
import subprocess, sys, tempfile
root = Path(sys.argv[1]); source = (root / 'native/dinghy.c').read_text()
start = source.index('static uint8_t mi_own[128]'); end = source.index('\nstatic void guarded(', start)
code = '''#include <stdint.h>
#include <assert.h>
enum {TR_VOCAL, TR_CHORDS, TR_BASS, TR_DRUMS};
enum {EN_ON = 4, EN_OFF, EN_BEND = 10, EN_SUSTAIN};
static int en; static uint8_t kn_track[27]; static struct { uint32_t notes; } fm1_in; static uint32_t ed_mora;
static unsigned n_ev, ev[64][5]; static int accept = 1;
static unsigned en_post(void *e, unsigned k, unsigned a, unsigned b, unsigned c) {
    (void)e; if (!accept) return 0; ev[n_ev][0] = k; ev[n_ev][1] = a; ev[n_ev][2] = b; ev[n_ev][3] = c; n_ev++; return 1; }
static uint8_t q[64][3]; static unsigned qh, qt;
static int midi_get(uint8_t *s, uint8_t *a, uint8_t *b) { if (qt == qh) return 0; *s = q[qt][0]; *a = q[qt][1]; *b = q[qt][2]; qt++; return 1; }
static void push(unsigned s, unsigned a, unsigned b) { q[qh][0] = (uint8_t)s; q[qh][1] = (uint8_t)a; q[qh][2] = (uint8_t)b; qh++; }
''' + source[start:end] + '''
int main(void) {
    /* a note-on on any channel plays the current track; the note-off goes to the track that started it */
    push(0x93, 60, 100); midi_service(TR_CHORDS);
    assert(n_ev == 1 && ev[0][0] == EN_ON && ev[0][1] == TR_CHORDS && ev[0][2] == 60);
    push(0x83, 60, 0); midi_service(TR_BASS);                               /* the track changed meanwhile */
    assert(n_ev == 2 && ev[1][0] == EN_OFF && ev[1][1] == TR_CHORDS && ev[1][2] == 60);
    push(0x90, 62, 90); midi_service(TR_BASS);
    push(0x90, 62, 0); midi_service(TR_CHORDS);                             /* velocity-0 note-on is a note-off */
    assert(n_ev == 4 && ev[3][0] == EN_OFF && ev[3][1] == TR_BASS && ev[3][2] == 0);
    push(0x80, 99, 0); midi_service(TR_CHORDS); assert(n_ev == 4);          /* a stray note-off posts nothing */
    /* mono track: released with the last of two held notes */
    push(0x90, 40, 90); push(0x90, 43, 90); midi_service(TR_BASS);
    push(0x80, 40, 0); midi_service(TR_BASS); assert(n_ev == 6);
    push(0x80, 43, 0); midi_service(TR_BASS); assert(n_ev == 7 && ev[6][0] == EN_OFF && ev[6][1] == TR_BASS);
    /* drums: one-shot, no ownership */
    push(0x90, 38, 100); midi_service(TR_DRUMS);
    assert(n_ev == 8 && ev[7][0] == EN_ON && ev[7][1] == TR_DRUMS && ev[7][2] == 2);
    push(0x80, 38, 0); midi_service(TR_DRUMS); assert(n_ev == 8);
    /* a full ring: the note-off is retried, not dropped */
    push(0x90, 70, 100); midi_service(TR_CHORDS); assert(n_ev == 9);
    accept = 0; push(0x80, 70, 0); midi_service(TR_CHORDS); assert(n_ev == 9 && mi_npend);
    midi_service(TR_CHORDS); assert(n_ev == 9 && mi_npend);
    accept = 1; midi_service(TR_CHORDS); assert(n_ev == 10 && ev[9][0] == EN_OFF && ev[9][2] == 70 && !mi_npend);
    midi_service(TR_CHORDS); assert(n_ev == 10);
    /* a refused note-on is simply not owned */
    accept = 0; push(0x90, 71, 100); midi_service(TR_CHORDS); accept = 1; push(0x80, 71, 0); midi_service(TR_CHORDS); assert(n_ev == 10);
    /* pedal and wheel follow the current chords/bass track; a track that stops being current is reset */
    n_ev = 0;
    push(0xB0, 64, 127); midi_service(TR_CHORDS);
    assert(n_ev == 1 && ev[0][0] == EN_SUSTAIN && ev[0][1] == TR_CHORDS && ev[0][2] == 1);
    midi_service(TR_BASS);
    assert(n_ev == 3 && ev[1][0] == EN_SUSTAIN && ev[1][1] == TR_CHORDS && ev[1][2] == 0 && ev[2][1] == TR_BASS && ev[2][2] == 1);
    push(0xB0, 64, 0); midi_service(TR_BASS); assert(n_ev == 4 && ev[3][2] == 0);
    n_ev = 0; push(0xE0, 0x00, 0x60); midi_service(TR_BASS);               /* 14-bit 0x3000 */
    assert(n_ev == 1 && ev[0][0] == EN_BEND && ev[0][1] == TR_BASS && ev[0][2] == 0 && ev[0][3] == 0x60);
    midi_service(TR_VOCAL); assert(n_ev == 2 && ev[1][1] == TR_BASS && ev[1][2] == 0 && ev[1][3] == 0x40);   /* centre again */
    /* the singing track and the clock: no effect on the ring beyond notes */
    n_ev = 0; push(0xF8, 0, 0); push(0xFA, 0, 0); push(0xC0, 5, 0); midi_service(TR_VOCAL); assert(n_ev == 0);
    /* all notes off */
    push(0x90, 50, 100); push(0x90, 52, 100); midi_service(TR_CHORDS); n_ev = 0;
    push(0xB0, 123, 0); midi_service(TR_CHORDS); assert(n_ev == 2);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp); (p / 'midi.c').write_text(code)
    subprocess.run([sys.argv[2], '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', str(p / 'midi.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
