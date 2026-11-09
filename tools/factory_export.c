/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/factory.h"
#include <stdio.h>
static Song song;static uint8_t data[SONG_ENC_SIZE];
int main(int argc,char **argv){
    if(argc!=2)return 1;
    for(unsigned i=0;i<SF_COUNT;i++){
        char path[1024];sf_load(&song,i);snprintf(path,sizeof path,"%s/%s.mks",argv[1],sf_name(i));
        FILE *f=fopen(path,"wb");if(!f)return 2;
        if(song_encode(&song,data,sizeof data)!=sizeof data||fwrite(data,1,sizeof data,f)!=sizeof data)return 3;
        if(fclose(f))return 4;
    }return 0;
}
