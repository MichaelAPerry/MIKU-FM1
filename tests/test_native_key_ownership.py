# SPDX-License-Identifier: GPL-3.0-only
"""Run the actual native key-release helper across track/page ownership changes."""
from pathlib import Path
import re,subprocess,tempfile,sys
root=Path(sys.argv[1]);source=(root/'native/dinghy.c').read_text()
start=source.index('static void keys_release(');end=source.index('\nstatic void app_main',start)
code='''#include <stdint.h>
#include <assert.h>
enum {TR_VOCAL,TR_CHORDS,TR_BASS,TR_DRUMS};
#define EN_OFF 5
static int en;static uint8_t kn[27],kn_track[27];
static unsigned count,tracks[10],notes[10];
static unsigned en_post(void *e,unsigned kind,unsigned t,unsigned n,unsigned c){(void)e;(void)c;assert(kind==EN_OFF);tracks[count]=t;notes[count++]=n;return 1;}
'''+source[start:end]+'''
int main(void){
 kn_track[0]=TR_CHORDS;kn[0]=60;kn_track[1]=TR_VOCAL;kn_track[2]=TR_VOCAL;kn_track[3]=TR_BASS;kn_track[4]=TR_DRUMS;
 /* A chord key released after moving to another page must still release chords. */
 keys_release(1,2);assert(count==1&&tracks[0]==TR_CHORDS&&notes[0]==60);
 keys_release(2,4);assert(count==1); /* another owned vocal key remains held */
 keys_release(4,8);assert(count==2&&tracks[1]==TR_VOCAL); /* a bass key does not hold the singer */
 keys_release(8,0);assert(count==3&&tracks[2]==TR_BASS);
 keys_release(16,0);assert(count==3); /* one-shot drum key needs no release */
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'keys.c').write_text(code)
 subprocess.run([sys.argv[2],'-std=c11','-Wall','-Wextra','-Werror',str(p/'keys.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
