/* SPDX-License-Identifier: Apache-2.0
 * MSFA reference adapter for regression fixtures. Host only. */
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "synth.h"
#include "controllers.h"
#include "dx7note.h"
#include "exp2.h"
#include "sin.h"
#include "freqlut.h"
int main(int argc,char **argv){
 if(argc!=3)return 2;FILE *f=fopen(argv[1],"rb");unsigned char packed[128];fread(packed,1,128,f);fclose(f);
 char patch[156]={0};
 for(int op=0;op<6;op++){unsigned char *p=packed+op*17;char *q=patch+op*21;for(int j=0;j<11;j++)q[j]=p[j];q[11]=p[11]&3;q[12]=p[11]>>2;q[13]=p[12]&7;q[14]=p[13]&3;q[15]=p[13]>>2;q[16]=p[14];q[17]=p[15]&1;q[18]=p[15]>>1;q[19]=p[16];q[20]=p[12]>>3;}
 for(int j=0;j<8;j++)patch[126+j]=packed[102+j];patch[134]=packed[110];patch[135]=packed[111]&7;patch[139]=patch[143]=0;
 Exp2::init();Sin::init();Freqlut::init(44100);PitchEnv::init(44100);
 Dx7Note note;memset(&note,0,sizeof note);note.init(patch,60+packed[117]-24,100);Controllers controls;controls.values_[0]=8192;
 f=fopen(argv[2],"wb");for(int b=0;b<64;b++){int32_t out[64]={0};note.compute(out,1<<23,0,&controls);fwrite(out,4,64,f);}fclose(f);
 return 0;
}
