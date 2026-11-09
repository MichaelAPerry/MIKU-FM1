/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/gen_next.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static Song a,b,copy,decoded;
static GenNextWorkspace workspace;
static uint8_t bytes[SONG_ENC_SIZE];
int main(void) {
    GenNextCfg c={1,0,0,0,0,75,70};
    for(int style=0;style<GX_N;style++)for(int seed=1;seed<=7;seed+=3)for(int key=0;key<12;key++)for(int minor=0;minor<2;minor++) {
        c.style=(uint8_t)style;c.seed=(uint16_t)seed;c.key=(uint8_t)key;c.minor=(uint8_t)minor;c.lock=0;
        sg_init(&a);sg_init(&b);CHECK(gen_next_song(&a,&c));CHECK(gen_next_song_ws(&b,&c,&workspace));CHECK(!memcmp(&a,&b,sizeof a));
        CHECK(a.nrows>=16 && a.nrows<=NROW);CHECK(song_encode(&a,bytes,sizeof bytes)==sizeof bytes);
        CHECK(song_decode(&decoded,bytes,sizeof bytes)==SIO_OK);CHECK(!memcmp(&a,&decoded,sizeof a));
        for(int t=0;t<NTRK;t++)for(int p=0;p<NPAT;p++)for(int s=0;s<NSTEP;s++)CHECK(sg_step_ok(t,&a.pat[t][p][s]));
        /* Locked tracks and all rows remain bit-exact, including non-generated step data. */
        copy=a;c.lock=1u<<TR_CHORDS;c.seed+=19;CHECK(gen_next_song(&a,&c));
        CHECK(!memcmp(copy.row,a.row,sizeof a.row));CHECK(!memcmp(copy.pat[TR_CHORDS],a.pat[TR_CHORDS],sizeof a.pat[TR_CHORDS]));
        CHECK(!memcmp(&copy.trk[TR_CHORDS],&a.trk[TR_CHORDS],sizeof(Trk)));
        CHECK(!gen_next_phrase(&a,&c,2,TR_CHORDS,1));
        /* Phrase rewrite must alter only the referenced pattern. */
        c.lock=0;copy=a;b=a;int row=3,t=TR_DRUMS,p=a.row[row].pat[t];
        CHECK(gen_next_phrase(&a,&c,row,t,5));CHECK(gen_next_phrase(&b,&c,row,t,5));CHECK(!memcmp(&a,&b,sizeof a));
        CHECK(memcmp(copy.pat[t][p],a.pat[t][p],sizeof a.pat[t][p]));
        memcpy(a.pat[t][p],copy.pat[t][p],sizeof a.pat[t][p]);CHECK(!memcmp(&a,&copy,sizeof a));
    }
    CHECK(sizeof(GenNextWorkspace)<=1024);
    copy=a;
    CHECK(!gen_next_song_ws(&a,&c,0));CHECK(!memcmp(&a,&copy,sizeof a));
    CHECK(!gen_next_song_ws(&a,&c,(GenNextWorkspace *)(void *)&a));CHECK(!memcmp(&a,&copy,sizeof a));
    CHECK(!gen_next_song_ws(&a,&c,(GenNextWorkspace *)(void *)&c));CHECK(!memcmp(&a,&copy,sizeof a));
    CHECK(!gen_next_song_ws(0,&c,&workspace));CHECK(!gen_next_song_ws(&a,0,&workspace));
    c=(GenNextCfg){1,GX_HYPERPOP,0,1,0,75,70};sg_init(&a);CHECK(gen_next_song(&a,&c));copy=a;c.style=GX_N;
    CHECK(!gen_next_song(&a,&c));CHECK(!memcmp(&a,&copy,sizeof a));
    /* Distinct styles differ in arrangement, drums, bass, harmony and vocal onset patterns. */
    uint32_t hashes[GX_N][5];
    for(int style=0;style<GX_N;style++) {
        c.style=(uint8_t)style;sg_init(&a);CHECK(gen_next_song(&a,&c));
        hashes[style][0]=sg_crc32((const uint8_t *)a.row,sizeof a.row);
        for(int t=0;t<NTRK;t++)hashes[style][t+1]=sg_crc32((const uint8_t *)a.pat[t],sizeof a.pat[t]);
    }
    for(int i=0;i<GX_N;i++)for(int j=0;j<i;j++)for(int field=0;field<5;field++)CHECK(hashes[i][field]!=hashes[j][field]);
    /* Refuse capacity overflow atomically in a highly varied hand-edited locked harmony. */
    sg_init(&a);a.nrows=32;
    for(int r=0;r<32;r++){a.row[r].sect=r<16?SEC_VERSE:SEC_CHORUS;a.row[r].pat[TR_CHORDS]=(uint8_t)(r%16);
        Step s={{(uint8_t)(48+r%16),CQ_MAJ,16,10}};sg_set_step(&a,TR_CHORDS,r%16,0,&s);}
    copy=a;c.lock=1u<<TR_CHORDS;CHECK(!gen_next_song(&a,&c));CHECK(!memcmp(&a,&copy,sizeof a));
    puts("gen_next: 360 wrapper/workspace crosschecks/roundtrips/locks/phrases; style distinction and atomic overflow passed");return 0;
}
