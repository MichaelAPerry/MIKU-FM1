/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/factory.h"
#undef NDEBUG
#include <assert.h>
#include <string.h>
#include <stdio.h>
static Song a,b;static uint8_t buf[SONG_ENC_SIZE];
int main(void){
    for(unsigned id=0;id<SF_COUNT;id++){
        assert(sf_name(id)&&sf_title(id));assert(sf_load(&a,id));assert(sf_load(&b,id));assert(!memcmp(&a,&b,sizeof a));
        assert(a.nrows<=NPAT&&a.nrows>=7&&!a.loop);
        for(unsigned t=0;t<NTRK;t++)for(unsigned p=0;p<NPAT;p++)for(unsigned s=0;s<NSTEP;s++)assert(sg_step_ok(t,&a.pat[t][p][s]));
        assert(song_encode(&a,buf,sizeof buf)==sizeof buf);assert(song_decode(&b,buf,sizeof buf)==SIO_OK);assert(!memcmp(&a,&b,sizeof a));
        unsigned vocals=0;for(unsigned p=0;p<NPAT;p++)for(unsigned s=0;s<NSTEP;s++)vocals+=a.pat[0][p][s].v[0]!=0;assert(vocals>35);
        printf("%s: %u rows, %u sung units, CRC %08x\n",sf_title(id),a.nrows,vocals,sg_crc32(buf,sizeof buf));
    }
    a=b;assert(!sf_load(&a,SF_COUNT)&&!memcmp(&a,&b,sizeof a));assert(!sf_load(0,0));assert(!sf_name(SF_COUNT)&&!sf_title(SF_COUNT));return 0;
}
