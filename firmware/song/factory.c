/* SPDX-License-Identifier: GPL-3.0-only */
#include "factory.h"
typedef struct {uint8_t tick,note,length,mora,flags;} SfNote;
typedef struct {const char *name,*title;uint8_t bpm,rows,meter;const SfNote *notes;uint16_t count;const uint8_t *roots;} SfSong;
#include "factory_tables.h"
const char *sf_name(uint32_t id){return id<SF_COUNT?SF_SONGS[id].name:0;}
const char *sf_title(uint32_t id){return id<SF_COUNT?SF_SONGS[id].title:0;}
uint32_t sf_load(Song *g,uint32_t id){
    const SfSong *s;uint32_t i,r,t;
    if(!g||id>=SF_COUNT)return 0;
    s=SF_SONGS+id;
    sg_init(g);g->bpm=s->bpm;g->loop=0;
    for(i=0;i<8u;i++)g->name[i]=' ';
    for(i=0;i<8u&&s->name[i];i++)g->name[i]=s->name[i];
    g->trk[TR_VOCAL].level=120;g->trk[TR_CHORDS].level=108;
    g->trk[TR_BASS].level=96;g->trk[TR_DRUMS].level=96;
    g->trk[TR_CHORDS].preset=0;g->trk[TR_BASS].preset=0;g->trk[TR_DRUMS].preset=0;
    for(r=0;r<=s->rows;r++){
        uint32_t p=r<s->rows?r:s->rows-1u;uint8_t root=s->roots[p];
        sg_row_insert(g,(int)r);
        sg_row_sect(g,(int)r,r==s->rows?SEC_OUTRO:r<s->rows/2u?SEC_VERSE:SEC_CHORUS);
        for(t=0;t<NTRK;t++)sg_row_set(g,(int)r,(int)t,t==TR_VOCAL?(r<s->rows?(int)r:(int)ROW_OFF):(int)r);
        /* Rearticulate decaying bells: one hit per long row left almost the whole
         * phrase without audible harmony. Preserve the original voice settings. */
        for(t=0;t<16u;t+=4u){
            Step c={{root,(uint8_t)((CM_STRUM<<4)|(id==2u?CQ_MAJ:CQ_MIN)),(uint8_t)(r==s->rows?16u:4u),10}};
            if(r==s->rows&&t)continue;
            sg_set_step(g,TR_CHORDS,(int)r,(int)t,&c);
        }
        for(t=0;t<16u;t+=2u){
            uint32_t beat=(r*16u+t)%(s->meter*2u);Step b={{(uint8_t)(root-12u),2,0,9}};
            Step d={{(uint8_t)(D_CHAT|(beat==0?D_KICK:0)|(beat==4?D_SNARE:0)),(uint8_t)(beat==0?D_KICK:beat==4?D_SNARE:0),0,0}};
            if(r==s->rows){if(t)continue;b.v[1]=12;d.v[0]=D_KICK;}
            sg_set_step(g,TR_BASS,(int)r,(int)t,&b);sg_set_step(g,TR_DRUMS,(int)r,(int)t,&d);
        }
    }
    for(i=0;i<s->count;i++){
        const SfNote *n=s->notes+i;Step v={{n->note,n->length,n->mora,n->flags}};
        sg_set_step(g,TR_VOCAL,n->tick/16u,n->tick%16u,&v);
    }
    return 1;
}
