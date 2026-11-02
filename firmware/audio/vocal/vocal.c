/* SPDX-License-Identifier: MIT */
#include "firmware/audio/vocal/vocal.h"
#include <math.h>
#include <string.h>
#define PI 3.14159265358979323846f
/* Original coarse Japanese vowel formant centers, Hz (illustrative synthesis
   targets, not a recorded voice or a copy of another synth). */
static const float formants[5][3]={
    {800,1250,2800}, {310,2200,3000}, {350,1450,2700},
    {500,1900,2700}, {500,900,2600}
};
int miku_voice_init(MikuVoice *v, uint32_t sr) {
    if(!v || sr<8000 || sr>192000) return 0;
    memset(v,0,sizeof(*v));v->sample_rate=(float)sr;
    v->noise_state=0x12345678u;return 1;
}
void miku_voice_note_on(MikuVoice *v,uint8_t pitch,uint8_t velocity,MikuVowel vowel) {
    if(!v || vowel>MIKU_O || pitch>127) return;
    v->frequency=440.f*powf(2.f,((int)pitch-69)/12.f);
    v->amplitude=(float)velocity/127.f;v->vowel=(uint8_t)vowel;v->active=1;
    /* Three resonant sections; coefficients computed on note entry, not per sample. */
    for(int i=0;i<3;i++) {
        float hz=formants[vowel][i];
        if(hz>v->sample_rate*.45f) hz=v->sample_rate*.45f;
        float radius=expf(-PI*140.f/v->sample_rate);
        float omega=2.f*PI*hz/v->sample_rate;
        v->a1[i]=2.f*radius*cosf(omega);v->a2[i]=-radius*radius;
        v->gain[i]=(1.f-radius)*((i==0)?1.f:(i==1)?.6f:.3f);
        v->z1[i]=v->z2[i]=0.f;
    }
}
void miku_voice_note_off(MikuVoice *v){if(v)v->active=0;}
float miku_voice_render(MikuVoice *v) {
    if(!v || !v->active) return 0.f;
    v->phase+=v->frequency/v->sample_rate;
    if(v->phase>=1.f) v->phase-=floorf(v->phase);
    /* Band-unlimited harmonic source: intentionally low gain; anti-aliasing TODO. */
    float source=2.f*v->phase-1.f;
    v->noise_state^=v->noise_state<<13;
    v->noise_state^=v->noise_state>>17;
    v->noise_state^=v->noise_state<<5;
    float noise=((v->noise_state&65535u)/32767.5f-1.f)*v->breath;
    float sum=0.f;
    for(int i=0;i<3;i++){
        float y=v->gain[i]*(source+noise)+v->a1[i]*v->z1[i]+v->a2[i]*v->z2[i];
        v->z2[i]=v->z1[i];v->z1[i]=y;sum+=y;
    }
    return tanhf(sum*.6f)*v->amplitude;
}
