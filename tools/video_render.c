/* SPDX-License-Identifier: GPL-3.0-only */
/* Host validation of actual integer firmware JPEG and original character pixels.
 * cc -O2 -std=c11 -I. tools/video_render.c firmware/usb/jpeg/{mjpeg,performance_frame}.c firmware/ui/performance.c -o build/video_render */
#define _POSIX_C_SOURCE 200809L
#include "firmware/usb/jpeg/mjpeg.h"
#include "firmware/usb/jpeg/performance_frame.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
static MjEncoder a,b;
static uint8_t output[MJ_FRAME_CAP],reference[MJ_FRAME_CAP];
static uint32_t calls;
static uint16_t noise_pixel(void *ctx,uint32_t x,uint32_t y){
    uint32_t z=x*374761393u+y*668265263u+(uint32_t)*(uint8_t*)ctx;
    calls++;z=(z^(z>>13))*1274126177u;return (uint16_t)(z^(z>>16));
}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (double)t.tv_sec+t.tv_nsec/1e9;}
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"FAIL:%s\n",why);exit(1);}}
static void save(const char *dir,const char *stem,const uint8_t *bytes,uint32_t n){
    char name[1024];snprintf(name,sizeof name,"%s/%s.jpg",dir,stem);FILE *f=fopen(name,"wb");require(f!=0,"open JPEG");require(fwrite(bytes,1,n,f)==n,"write JPEG");require(!fclose(f),"close JPEG");
}
static void ppm(const char *dir,const char *stem,PerfState *s){
    char name[1024];snprintf(name,sizeof name,"%s/%s.ppm",dir,stem);FILE*f=fopen(name,"wb");require(f!=0,"open PPM");fprintf(f,"P6\n160 120\n255\n");
    for(unsigned y=0;y<120;y++)for(unsigned x=0;x<160;x++){
        uint16_t p=mj_perf_pixel(s,x,y);unsigned r=p>>11,g=(p>>5)&63u,bv=p&31u;
        fputc((r<<3)|(r>>2),f);fputc((g<<2)|(g>>4),f);fputc((bv<<3)|(bv>>2),f);
    }require(!fclose(f),"close PPM");
}
int main(int argc,char**argv){
    PerfState s;uint8_t seed=1;double total=0,worst=0;uint32_t largest=0,smallest=8192;char stem[80];
    if(argc!=2){fprintf(stderr,"usage:video_render EXISTING_OUTPUT_DIR\n");return 1;}
    perf_init(&s);
    for(unsigned bg=0;bg<3;bg++)for(unsigned frame=0;frame<8;frame++){
        uint32_t steps=0;int rc;double begin=now();perf_select(&s,0,bg,bg,bg==2);s.sprite=(uint8_t)frame;s.light=180;s.beat_q8=320;
        require(mj_begin(&a,160,120,65,output,sizeof output,mj_perf_pixel,&s),"begin");require(mj_step(&a,0)==0&&!mj_size(&a),"zero budget");
        do{double t=now();rc=mj_step(&a,1);double dt=now()-t;if(dt>worst)worst=dt;steps++;}while(rc==0);
        total+=now()-begin;require(rc==1&&steps==80,"80 MCUs");uint32_t n=mj_size(&a);require(n>600&&n<=8192,"size cap");
        require(mj_begin(&b,160,120,65,reference,sizeof reference,mj_perf_pixel,&s),"second begin");require(mj_step(&b,80)==1,"whole frame");
        require(n==mj_size(&b)&&!memcmp(output,reference,n),"chunk invariant");require(mj_step(&a,1)==1&&mj_size(&a)==n,"complete stable");
        if(n>largest)largest=n;
        if(n<smallest)smallest=n;
        snprintf(stem,sizeof stem,"stage-bg%u-frame%u",bg,frame);save(argv[1],stem,output,n);ppm(argv[1],stem,&s);
    }
    /* Buffer guard: an incomplete overflow may never be published. */
    memset(output,0xa5,sizeof output);require(mj_begin(&a,160,120,100,output,700,noise_pixel,&seed),"small buffer begin");
    int rc;do{rc=mj_step(&a,1);}while(rc==0);require(rc==-1&&!mj_size(&a),"overflow rejected");
    for(unsigned i=700;i<sizeof output;i++)require(output[i]==0xa5,"output guard");
    calls=0;require(!mj_begin(&a,0,120,65,output,8192,noise_pixel,&seed),"zero width");require(!mj_begin(&a,161,120,65,output,8192,noise_pixel,&seed),"width bound");
    require(!mj_begin(&a,160,121,65,output,8192,noise_pixel,&seed),"height bound");require(!mj_begin(&a,160,120,101,output,8192,noise_pixel,&seed),"quality bound");
    require(!mj_begin(&a,160,120,65,output,8193,noise_pixel,&seed),"capacity bound");require(!calls&&mj_step(&a,1)==-1,"invalid no pixels");
    /* Edge replication and independent caller-owned contexts. */
    require(mj_begin(&a,159,119,40,output,8192,noise_pixel,&seed),"odd dimensions");
    require(mj_begin(&b,1,1,65,reference,8192,mj_perf_pixel,&s),"independent encoder");
    require(mj_step(&b,1)==1,"single pixel");save(argv[1],"one-pixel",reference,mj_size(&b));
    do{rc=mj_step(&a,3);}while(rc==0);require(rc==1,"odd encode");save(argv[1],"odd-noise",output,mj_size(&a));
    printf("24 actual-scene JPEGs Q65: %u..%u bytes; host mean %.3f ms/frame,worst observed MCU %.3f us; sizeof(MjEncoder)=%zu; overflow/chunk/guard/edge/independence tests PASS\n",smallest,largest,total/24*1000,worst*1e6,sizeof(MjEncoder));
    return 0;
}
