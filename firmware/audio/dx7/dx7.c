/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2012-2013 Google Inc.
 * Freestanding C adaptation 2026 MIKU-FM1 contributors.
 * Based on Google MSFA f67d41d313b7dc85f6fb99e79e515cc9d208cfff;
 * see README.md for changes, provenance and compatibility limits. */
#include "dx7.h"
#include "dx7_tables.h"
#include "dx7_constants.h"
#include "firmware/audio/fmband/fmband.h" /* existing quadratic gain/equal-power pan */
/* Carrier gain below which a released voice is finished: the renderer divides every sample by 4096 * carriers, so 32768
 * is a peak of 8 LSB (-72 dBFS) at full mix. The 1120 mixing threshold is too low for this (a slow release only reaches
 * it when its envelope ends, minutes later), while a deliberate sustain via L4 stays well above 32768. */
#define DX_QUIET 32768
static void dx_zero(void *p,uint32_t n){uint8_t *b=p;while(n--)*b++=0;}
static int32_t dx_min(int32_t a,int32_t b){return a<b?a:b;}
static int32_t dx_max(int32_t a,int32_t b){return a>b?a:b;}
static int32_t dx_sin(uint32_t phase){
    uint32_t ix=(phase&0xffffffu)>>14,frac=phase&16383u;
    return dx_sine[ix]+(int32_t)(((int64_t)(dx_sine[ix+1]-dx_sine[ix])*frac)>>14);
}
static int32_t dx_exp(int32_t x){
    uint32_t ix=((uint32_t)x&0xffffffu)>>14,frac=(uint32_t)x&16383u;
    int shift=6-(x>>24);int64_t y;
    if(shift>=32)return 0;
    if(shift<0)return 0x7fffffff;
    y=dx_exponential[ix]+(((int64_t)dx_exponential[ix+1]-dx_exponential[ix])*frac>>14);
    return (int32_t)(y>>shift);
}
static int32_t dx_freq(int32_t x){
    uint32_t ix=((uint32_t)x&0xffffffu)>>14,frac=(uint32_t)x&16383u;
    int shift=20-(x>>24);int64_t y;
    if(shift>=32)return 0;
    if(shift<0)shift=0;
    y=dx_frequency[ix]+(((int64_t)dx_frequency[ix+1]-dx_frequency[ix])*frac>>14);
    return (int32_t)(y>>shift);
}
static int32_t dx_level(int32_t v){
    static const uint8_t lut[20]={0,5,9,13,17,20,23,25,27,29,31,33,35,37,39,41,42,43,45,46};
    return v>=20?28+v:lut[v<0?0:v];
}
static void dx_env_advance(DxEnv *e,uint32_t stage){
    int32_t level,rate;e->stage=(uint8_t)stage;if(stage>=4)return;
    level=((dx_level(e->levels[stage])>>1)<<6)+e->output-4256;
    e->target=dx_max(16,level)*65536;e->rising=(uint8_t)(e->target>e->level);
    rate=dx_min(((e->rates[stage]*41)>>6)+e->scale,63);
    e->inc=(4+(rate&3))<<(8+(rate>>2));
}
static int32_t dx_env_sample(DxEnv *e){
    if(e->stage<3||(e->stage<4&&!e->down)){
        if(e->rising){
            if(e->level<1716*65536)e->level=1716*65536;
            e->level+=((17*(1<<24)-e->level)>>24)*e->inc;
            if(e->level>=e->target){e->level=e->target;dx_env_advance(e,e->stage+1u);}
        }else{e->level-=e->inc;if(e->level<=e->target){e->level=e->target;dx_env_advance(e,e->stage+1u);}}
    }
    return e->level;
}
static void dx_pitch_advance(DxPitchEnv *e,uint32_t stage){
    e->stage=(uint8_t)stage;if(stage>=4)return;
    e->target=(int32_t)dx_pitchtab[e->levels[stage]]*524288;
    e->rising=(uint8_t)(e->target>e->level);e->inc=dx_ratetab[e->rates[stage]]*1143;
}
static int32_t dx_pitch_sample(DxPitchEnv *e){
    if(e->stage<3||(e->stage<4&&!e->down)){
        if(e->rising){e->level+=e->inc;if(e->level>=e->target){e->level=e->target;dx_pitch_advance(e,e->stage+1u);}}
        else{e->level-=e->inc;if(e->level<=e->target){e->level=e->target;dx_pitch_advance(e,e->stage+1u);}}
    }return e->level;
}
static int32_t dx_curve(int32_t group,int32_t depth,uint32_t curve){
    int32_t value=curve==0||curve==3?(group*depth*329)>>12:(dx_exp_scale_data[dx_min(group,32)]*depth*329)>>15;
    return curve<2?-value:value;
}
static void dx_lfo_init(DxLfo *l,const uint8_t *p){
    int32_t sr=p[112]?165*p[112]>>6:1,a;
    dx_zero(l,sizeof *l);sr*=sr<160?11:11+((sr-160)>>4);l->delta=(uint32_t)sr*36558u;
    a=99-p[113];
    if(a==99){l->delayinc=l->delayinc2=0xffffffffu;}
    else{a=(16+(a&15))<<(1+(a>>4));l->delayinc=(uint32_t)a*36558u;a&=0xff80;l->delayinc2=(uint32_t)dx_max(128,a)*36558u;}
    l->wave=(p[116]>>1)&7u;l->sync=p[116]&1u;l->random=1;l->phase=l->sync?0x7fffffffu:0;
}
static int32_t dx_lfo_sample(DxLfo *l){
    uint32_t x;l->phase+=l->delta;
    switch(l->wave){
    case 0:x=l->phase>>7;x^=0u-(l->phase>>31);return (int32_t)(x&0xffffffu);
    case 1:return (int32_t)((~l->phase^0x80000000u)>>8);
    case 2:return (int32_t)((l->phase^0x80000000u)>>8);
    case 3:return (int32_t)((~l->phase>>7)&0x1000000u);
    case 4:return (1<<23)+(dx_sin(l->phase>>8)>>1);
    default:if(l->phase<l->delta)l->random=(uint8_t)(l->random*179u+17u);return ((int32_t)(l->random^0x80u)+1)*65536;
    }
}
static int32_t dx_lfo_delay(DxLfo *l){
    uint32_t delta=l->delay<0x80000000u?l->delayinc:l->delayinc2,d=l->delay+delta;
    if(d<l->delay)return 1<<24; /* 32-bit wrap of the ramp, same as MSFA's 64-bit d > ~0u */
    l->delay=d;
    return d<0x80000000u?0:(int32_t)((d>>7)&0xffffffu);
}
uint32_t dx_patch_valid(const uint8_t p[DX_PACKED]){
    uint32_t o,i;if(!p)return 0;
    for(i=0;i<128;i++)if(p[i]>127)return 0;
    for(o=0;o<6;o++){const uint8_t *q=p+17u*o;
        for(i=0;i<11;i++)if(q[i]>99)return 0;
        if(q[11]>15||q[12]>119||q[13]>31||q[14]>99||q[15]>63||q[16]>99)return 0;
    }
    for(i=102;i<110;i++)if(p[i]>99)return 0;
    if(p[110]>31||p[111]>15||p[112]>99||p[113]>99||p[114]>99||p[115]>99||((p[116]>>1)&7)>5||p[117]>48)return 0;
    return 1;
}
void dx_set_mix(DxSynth *s,uint8_t level,uint8_t pan){s->level=level;s->pan=pan;mx_gain(level,pan,&s->gl,&s->gr);}
void dx_init(DxSynth *s,uint32_t voices){
    uint32_t o;dx_zero(s,sizeof *s);s->voices=(uint8_t)(voices<1?1:voices>DX_VOICES?DX_VOICES:voices);
    for(o=0;o<6;o++){uint8_t *p=s->patch+17u*o;p[0]=p[1]=p[2]=p[3]=99;p[4]=p[5]=p[6]=99;p[12]=56;p[15]=2;}
    s->patch[5u*17u+14u]=99;s->patch[110]=31;s->patch[117]=24;
    for(o=106;o<110;o++)s->patch[o]=50;
    for(o=118;o<128;o++)s->patch[o]=' ';
    s->bend_range=2;dx_set_mix(s,100,64);
}
uint32_t dx_patch(DxSynth *s,const uint8_t patch[128]){
    uint32_t i;if(!s||!dx_patch_valid(patch))return 0;
    for(i=0;i<128;i++)s->patch[i]=patch[i];
    return 1;
}
static void dx_voice_block(DxSynth *s,DxVoice *v);
/* Stealing must not hard-cut a sounding tail: take what the voice would play next (its buffered rest, then one more
 * block), fade it linearly over DX_FADE samples and add it to the synth's outgoing-tail buffer. */
static void dx_ghost_add(DxSynth *s,DxVoice *v){
    int32_t t[DX_FADE];uint32_t i,n=v->left,rem=(uint32_t)s->glen-s->gpos;int32_t div;
    if(!v->active||(v->active==2&&!v->left))return;
    div=(int32_t)(4096u*(v->carriers?v->carriers:1u));
    for(i=0;i<DX_FADE;i++)t[i]=0;
    if(n>DX_FADE)n=DX_FADE;
    for(i=0;i<n;i++)t[i]=v->audio[DX_BLOCK-v->left+i]/div;
    if(n<DX_FADE&&v->active!=2){dx_voice_block(s,v);for(i=n;i<DX_FADE;i++)t[i]=v->audio[i-n]/div;}
    for(i=0;i<DX_FADE;i++){
        int32_t old=i<rem?s->ghost[s->gpos+i]:0;
        s->ghost[i]=old+(int32_t)(((int64_t)t[i]*(int32_t)(DX_FADE-i))/(int32_t)DX_FADE);
    }
    s->gpos=0;s->glen=DX_FADE;
}
void dx_note_on(DxSynth *s,uint8_t note,uint8_t velocity){
    uint32_t i,o,k=s->voices;DxVoice *v;int32_t midinote,offset,out,rate;const uint8_t *p=s->patch;
    if(!velocity){dx_note_off(s,note);return;}
    for(i=0;i<s->voices;i++)if(!s->voice[i].active){k=i;break;}
    if(k==s->voices){for(i=0;i<s->voices;i++)if(!s->voice[i].gated&&(k==s->voices||s->voice[i].age<s->voice[k].age))k=i;}
    if(k==s->voices){for(i=0;i<s->voices;i++)if(!s->voice[i].keyed&&(k==s->voices||s->voice[i].age<s->voice[k].age))k=i;} /* pedal-held */
    if(k==s->voices){k=0;for(i=1;i<s->voices;i++)if(s->voice[i].age<s->voice[k].age)k=i;}
    v=s->voice+k;if(v->active)dx_ghost_add(s,v);
    dx_zero(v,sizeof *v);v->active=v->gated=v->keyed=1;v->note=note;v->xpose=(uint8_t)(p[117]-24);v->age=++s->age;
    v->algorithm=p[110];v->fbshift=(uint8_t)((p[111]&7u)?8u-(p[111]&7u):16u);
    v->pmd=(uint8_t)((p[114]*165u)>>6);v->pms=dx_pitchmodsenstab[p[116]>>4];v->amd=p[115];
    midinote=dx_min(127,dx_max(0,(int32_t)note+(int32_t)p[117]-24));v->midi=(uint8_t)midinote;
    for(o=0;o<6;o++){
        const uint8_t *q=p+17u*o;DxEnv *e=v->env+o;uint32_t mode=q[15]&1u,coarse=q[15]>>1,detune=q[12]>>3;
        for(i=0;i<4;i++){e->rates[i]=q[i];e->levels[i]=q[4+i];}
        offset=midinote-q[8]-17;
        out=dx_level(q[14])+dx_curve((offset<0?-offset:offset)/3,offset<0?q[9]:q[10],offset<0?q[11]&3u:q[11]>>2);
        out=dx_min(127,out)*32+(((int32_t)((q[13]>>2)*(dx_velocity_data[velocity>127?63:velocity>>1]-239)+7)>>3)*16);
        e->output=dx_max(0,out);e->down=1;
        rate=dx_min(31,dx_max(0,midinote/3-7));e->scale=(uint8_t)((q[12]&7u)*(uint32_t)rate>>3);dx_env_advance(e,0);
        if(!mode)v->base[o]=50857777+1398101*midinote+dx_coarsemul[coarse]+dx_fine[q[16]]+12606*((int32_t)detune-7);
        else{v->fixed|=(uint8_t)(1u<<o);v->base[o]=(int32_t)(((int64_t)4458616*((coarse&3u)*100u+q[16]))>>3)+(detune>7?13457*((int32_t)detune-7):0);}
        if(!(p[111]&8u))v->phase[o]=(uint32_t)dx_freq(v->base[o])*s->clock;
        v->ams[o]=q[13]&3u;
        if((dx_algorithms[v->algorithm][o]&7u)==4u)v->carriers++;
    }
    for(i=0;i<4;i++){v->pitch.rates[i]=p[102+i];v->pitch.levels[i]=p[106+i];}
    v->pitch.level=(int32_t)dx_pitchtab[p[109]]*524288;v->pitch.down=1;dx_pitch_advance(&v->pitch,0);
    dx_lfo_init(&v->lfo,p);if(!v->lfo.sync)v->lfo.phase=(s->clock/64u)*v->lfo.delta;
}
static void dx_release(DxVoice *v){
    uint32_t o;v->gated=0;v->keyed=0;
    for(o=0;o<6;o++){v->env[o].down=0;dx_env_advance(v->env+o,3);}v->pitch.down=0;dx_pitch_advance(&v->pitch,3);
}
void dx_note_off(DxSynth *s,uint8_t note){
    uint32_t i;for(i=0;i<s->voices;i++){DxVoice *v=s->voice+i;if(v->active&&v->gated&&v->keyed&&v->note==note){
        if(s->sustain)v->keyed=0;else dx_release(v);
    }}
}
void dx_set_sustain(DxSynth *s,uint8_t on){
    uint32_t i;s->sustain=on!=0;
    if(!s->sustain)for(i=0;i<s->voices;i++){DxVoice *v=s->voice+i;if(v->active&&v->gated&&!v->keyed)dx_release(v);}
}
void dx_set_bend_range(DxSynth *s,uint8_t semis){s->bend_range=semis>48?48:semis;}
void dx_set_bend(DxSynth *s,int32_t bend){
    bend=bend<-8192?-8192:bend>8191?8191:bend;s->bend=(int32_t)(((int64_t)bend*(int32_t)s->bend_range*1398101)>>13);
}
void dx_all_off(DxSynth *s){uint32_t i;for(i=0;i<s->voices;i++)if(s->voice[i].active&&s->voice[i].gated)dx_release(s->voice+i);}
void dx_note_legato(DxSynth *s,uint8_t note,uint8_t velocity,uint32_t glide_ms){
    uint32_t i,o,k=s->voices,blocks;DxVoice *v;int32_t midinote,delta;
    if(!velocity){dx_note_off(s,note);return;}
    for(i=0;i<s->voices;i++)if(s->voice[i].active&&s->voice[i].gated&&(k==s->voices||s->voice[i].age>s->voice[k].age))k=i;
    if(k==s->voices){dx_note_on(s,note,velocity);return;}
    v=s->voice+k;
    midinote=dx_min(127,dx_max(0,(int32_t)note+(int32_t)(int8_t)v->xpose));
    delta=1398101*(midinote-(int32_t)v->midi);
    for(o=0;o<6;o++)if(!(v->fixed&(1u<<o)))v->base[o]+=delta;
    v->gofs-=delta;v->midi=(uint8_t)midinote;v->note=note;v->keyed=1;
    if(glide_ms>60000u)glide_ms=60000u;
    blocks=(glide_ms*441u/10u+DX_BLOCK-1u)/DX_BLOCK;
    if(!blocks||!v->gofs){v->gofs=0;v->gleft=0;v->ginc=0;}
    else{v->gleft=(uint16_t)blocks;v->ginc=-(v->gofs/(int32_t)blocks);}
}
void dx_hush(DxSynth *s){
    uint32_t i;for(i=0;i<s->voices;i++){DxVoice *v=s->voice+i;if(v->active==1){v->gated=0;v->active=3;}}
}
uint32_t dx_active(const DxSynth *s){uint32_t i,n=0;for(i=0;i<s->voices;i++)n+=s->voice[i].active!=0;return n;}
/* Algorithms 4 and 6 feed a later operator back into an earlier one.
 * They require sample-major evaluation; the original MSFA block-major path
 * explicitly left this case unimplemented. */
static void dx_multi_feedback(DxVoice *v,const uint32_t frequency[6],int32_t gain[6],const int32_t delta[6],const uint8_t audible[6]){
    uint32_t i,o;int32_t fb0=v->feedback[0],fb1=v->feedback[1];
    for(i=0;i<DX_BLOCK;i++){
        int32_t bus[3]={0,0,0};uint8_t has[3]={1,0,0};
        for(o=0;o<6;o++){
            uint32_t flags=dx_algorithms[v->algorithm][o],in=(flags>>4)&3u,out=flags&3u,phase=v->phase[o];
            int32_t sample=0;gain[o]+=delta[o];
            if(audible[o]){
                if((flags&0x40u)&&v->fbshift<16)phase+=(uint32_t)((fb0+fb1)>>(v->fbshift+1u));
                else if(in&&has[in])phase+=(uint32_t)bus[in];
                sample=(int32_t)(((int64_t)dx_sin(phase)*gain[o])>>24);
                if((flags&4u)&&has[out])bus[out]+=sample;else bus[out]=sample;
                has[out]=1;
            }else if(!(flags&4u))has[out]=0;
            if(flags&0x80u){fb0=fb1;fb1=sample;}
            v->phase[o]+=frequency[o];
        }
        v->audio[i]=bus[0];
    }
    v->feedback[0]=fb0;v->feedback[1]=fb1;
}
static void dx_voice_block(DxSynth *s,DxVoice *v){
    uint32_t o,i,has[3]={1,0,0},done=1,special=(v->algorithm==3u||v->algorithm==5u)&&v->fbshift<16u;
    uint32_t frequencies[6];int32_t starts[6],deltas[6];uint8_t audible[6];
    int32_t lfo=dx_lfo_sample(&v->lfo),delay=dx_lfo_delay(&v->lfo),pitch=dx_pitch_sample(&v->pitch);
    int32_t amp=(int32_t)(((int64_t)((v->amd*128u+49u)/99u)*delay)>>24); /* amplitude depth, shared 0..99 */
    pitch+=(int32_t)(((int64_t)((uint32_t)v->pmd*(uint32_t)delay)*(v->pms*(lfo-(1<<23))))>>39);
    pitch+=s->bend;
    if(v->gleft){v->gofs+=v->ginc;if(!--v->gleft)v->gofs=0;}
    for(i=0;i<DX_BLOCK;i++)v->audio[i]=0;
    for(o=0;o<6;o++){
        uint32_t flags=dx_algorithms[v->algorithm][o],in=(flags>>4)&3u,out=flags&3u,add=(flags&4u)!=0;
        int32_t *dst=out?s->bus[out-1]:v->audio;const int32_t *src=in?s->bus[in-1]:0;
        int32_t level=dx_env_sample(v->env+o),level0=level,g1=v->gain[o],g2,dg,gain,fb0=v->feedback[0],fb1=v->feedback[1];
        uint32_t phase=v->phase[o],frequency=(uint32_t)dx_freq(v->base[o]+pitch+((v->fixed>>o&1u)?0:v->gofs));
        /* DX7 AMS levels approximate 0 / 1.5 / 3 / 6 dB modulation at full depth. */
        if(v->ams[o])level-=(int32_t)(((int64_t)(v->ams[o]==1?1:v->ams[o]==2?2:4)*amp*(0xffffff-lfo))>>9);
        g2=dx_exp(level-14*(1<<24));
        /* A released voice retires once every carrier is below DX_QUIET (the unmodulated level bounds the gain; a carrier
         * still rising towards an audible L4 keeps it alive). A carrier that settles audibly at L4 never retires. */
        if(!v->gated&&(flags&7u)==4u){const DxEnv *e=v->env+o;
            if(dx_exp(level0-14*(1<<24))>=DX_QUIET||(e->stage<4u&&e->rising&&dx_exp(e->target-14*(1<<24))>=DX_QUIET))done=0;}
        if(!v->gated&&v->active==3)g2=0;
        v->gain[o]=g2;dg=(g2-g1+32)>>6;gain=g1;
        if(special){frequencies[o]=frequency;starts[o]=g1;deltas[o]=dg;audible[o]=(uint8_t)(g1>=1120||g2>=1120);continue;}
        if(g1>=1120||g2>=1120){
            if(!has[out])add=0;
            for(i=0;i<DX_BLOCK;i++){
                uint32_t ph=phase;int32_t sample;gain+=dg;
                if(in&&has[in])ph+=(uint32_t)src[i];
                else if((flags&0xc0u)==0xc0u&&v->fbshift<16){ph+=(uint32_t)((fb0+fb1)>>(v->fbshift+1u));fb0=fb1;}
                sample=(int32_t)(((int64_t)dx_sin(ph)*gain)>>24);
                if(!(in&&has[in])&&(flags&0xc0u)==0xc0u&&v->fbshift<16)fb1=sample;
                if(add)dst[i]+=sample;else dst[i]=sample;phase+=frequency;
            }
            has[out]=1;
        }else if(!add)has[out]=0;
        if((flags&0xc0u)==0xc0u){v->feedback[0]=fb0;v->feedback[1]=fb1;}
        v->phase[o]+=frequency*DX_BLOCK;
    }
    if(special)dx_multi_feedback(v,frequencies,starts,deltas,audible);
    if(!v->gated&&(done||v->active==3))v->active=(uint8_t)(v->active==3?2:3);
    v->left=DX_BLOCK;
}
void dx_render(DxSynth *s,int32_t *stereo,uint32_t frames){
    uint32_t v,n,i,at;
    for(i=0;i<frames&&s->gpos<s->glen;i++,s->gpos++){
        int32_t x=s->ghost[s->gpos];stereo[2u*i]+=(x*s->gl)>>15;stereo[2u*i+1u]+=(x*s->gr)>>15;
    }
    if(s->gpos>=s->glen)s->gpos=s->glen=0;
    for(v=0;v<s->voices;v++){DxVoice *voice=s->voice+v;if(!voice->active)continue;
        at=0;while(at<frames){
            if(!voice->left){if(voice->active==2){voice->active=0;break;}dx_voice_block(s,voice);}
            n=frames-at;if(n>voice->left)n=voice->left;
            for(i=0;i<n;i++){
                int32_t x=voice->audio[DX_BLOCK-voice->left+i]/(int32_t)(4096u*(voice->carriers?voice->carriers:1u));
                stereo[2u*(at+i)]+=(x*s->gl)>>15;stereo[2u*(at+i)+1u]+=(x*s->gr)>>15;
            }
            voice->left=(uint8_t)(voice->left-n);at+=n;
            if(!voice->left&&voice->active==2){voice->active=0;break;}
        }
    }
    s->clock+=frames;
}
