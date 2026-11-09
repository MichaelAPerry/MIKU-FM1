/* SPDX-License-Identifier: GPL-3.0-only */
/* Export actual editable DX7-backed songs for song_render/native review. */
#include "firmware/song/factory.h"
#include "firmware/song/dx7_edit.h"
#include <stdio.h>
#include <string.h>
static Song song;static unsigned char data[SONG_ENC_MAX];
int main(int argc,char **argv){
 if(argc!=2)return 1;
 for(unsigned p=0;p<9;p++){
  char path[1024];sf_load(&song,0);if(p<8){db_select(&song,TR_CHORDS,p);db_select(&song,TR_BASS,3);}
  snprintf(path,sizeof path,"%s/%s%u.mks",argv[1],p<8?"dx7-":"classic-",p);
  unsigned n=song_encode(&song,data,sizeof data);FILE *f=fopen(path,"wb");if(!n||!f)return 2;
  if(fwrite(data,1,n,f)!=n){fclose(f);return 3;}fclose(f);
 }
 return 0;
}
