/* SPDX-License-Identifier: MIT */
#include "firmware/audio/band/band.h"
#include <math.h>
#include <string.h>
#define TAU 6.2831853071795864769f
static float freq(uint8_t n){return 440.f*powf(2.f,((int)n-69)/12.f);}
int miku_band_init(MikuBand *b,uint32_t sr){if(!b||sr<8000)return 0;memset(b,0,sizeof(*b));b->sample_rate=sr;return 1;}
void miku_band_note(MikuBand *b,const MikuNote *n,int on){
    if(!b||!n)return;
    if(n->track==MIKU_BASS){
        if(!on){if(b->bass.active&&b->bass.pitch==n->pitch)b->bass.active=0;return;}
        b->bass=(MikuTone){1,n->pitch,0,0,freq(n->pitch),n->velocity/127.f,0};return;
    }
    if(n->track!=MIKU_CHORDS)return;
    if(!on){for(int i=0;i<MIKU_CHORD_VOICES;i++)
        if(b->chords[i].active && b->chords[i].pitch==n->pitch){b->chords[i].active=0;break;}return;}
    int slot=-1;float oldest=-1;
    for(int i=0;i<MIKU_CHORD_VOICES;i++){
        if(!b->chords[i].active){slot=i;break;}
        if(b->chords[i].age>oldest){oldest=b->chords[i].age;slot=i;}
    }
    b->chords[slot]=(MikuTone){1,n->pitch,0,0,freq(n->pitch),n->velocity/127.f,0};
}
float miku_band_render(MikuBand *b){
    float out=0.f,sr=(float)b->sample_rate;
    for(int i=0;i<MIKU_CHORD_VOICES;i++){
        MikuTone *v=&b->chords[i];if(!v->active)continue;
        v->phase+=v->frequency/sr;v->phase-=floorf(v->phase);
        v->mod_phase+=v->frequency*2.f/sr;v->mod_phase-=floorf(v->mod_phase);
        float env=fminf(1.f,v->age*15.f);
        out+=sinf(TAU*v->phase+1.8f*sinf(TAU*v->mod_phase))*.11f*v->level*env;
        v->age+=1.f/sr;
    }
    MikuTone *v=&b->bass;
    if(v->active){
        v->phase+=v->frequency/sr;v->phase-=floorf(v->phase);
        v->mod_phase+=v->frequency/sr;v->mod_phase-=floorf(v->mod_phase);
        out+=sinf(TAU*v->phase+2.5f*expf(-v->age*12.f)*sinf(TAU*v->mod_phase))*.26f*v->level;
        v->age+=1.f/sr;
    }
    return out;
}
void miku_drums_init(MikuDrums *d){if(d){memset(d,0,sizeof(*d));d->noise=0xa51c03f1u;}}
/* MIDI note 36 kick, 38 snare, 42 closed hat, 46 open hat */
void miku_drums_trigger(MikuDrums *d,uint8_t note,uint8_t vel,uint32_t sr){
    if(!d||!sr)return;
    int kind=note==36?1:note==38?2:note==42?3:note==46?4:0;
    if(!kind)return;
    d->kind=(uint8_t)kind;d->level=vel/127.f;d->phase=0;
    d->remaining=(kind==1?.28f:kind==2?.19f:kind==3?.075f:.3f);
    d->frequency=(kind==1?105.f:190.f);
}
float miku_drums_render(MikuDrums *d,uint32_t sr){
    if(!d||!d->kind||d->remaining<=0)return 0;
    d->noise^=d->noise<<13;d->noise^=d->noise>>17;d->noise^=d->noise<<5;
    float noise=((d->noise&65535u)/32767.5f-1.f);
    float lifespan=(d->kind==1?.28f:d->kind==2?.19f:d->kind==3?.075f:.3f);
    float progress=1.f-d->remaining/lifespan;
    float envelope=(1-progress)*(1-progress);
    d->phase+=(d->frequency*(d->kind==1?(1.5f-progress):1.f))/(float)sr;
    d->phase-=floorf(d->phase);
    float sample=d->kind==1?sinf(TAU*d->phase)*.6f:
                 d->kind==2?noise*.35f+sinf(TAU*d->phase)*.1f:noise*.18f;
    d->remaining-=1.f/(float)sr;
    if(d->remaining<=0)d->kind=0;
    return sample*d->level*envelope;
}
