/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/ui/sprite_codec.h"
#include "firmware/ui/singer_compact.h"
#include "firmware/ui/singer_sprites.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static int fails;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#c);fails++;}}while(0)
static void exact(const SprAtlas*a,const uint8_t *const *original){
    CHECK(spr_validate(a));
    for(unsigned f=0;f<a->frames;f++)for(unsigned y=0;y<a->height;y++){
        uint8_t row[257];memset(row,0xa5,sizeof row);CHECK(spr_decode_row(a,f,y,row+1,255));CHECK(row[0]==0xa5&&row[a->width+1]==0xa5);
        for(unsigned x=0;x<a->width;x++){
            unsigned p=y*a->width+x,b=original[f][p>>1],want=p&1u?b&15u:b>>4;uint8_t one=0xa5;
            CHECK(row[x+1]==want);CHECK(spr_pixel(a,f,x,y,&one)&&one==want);
        }
    }
}
static void untouched(SprAtlas*a){
    uint8_t row[256],old[256],pixel=0xa5;memset(row,0xa5,sizeof row);memcpy(old,row,sizeof row);
    CHECK(!spr_decode_row(a,0,0,row,sizeof row));CHECK(!memcmp(row,old,sizeof row));
    CHECK(!spr_pixel(a,0,0,0,&pixel)&&pixel==0xa5);CHECK(!spr_validate(a));
}
static void corrupt(void){
    uint16_t ids[]={0},off[]={0,3,6};uint8_t data[]={1,0x31,0x32,0,0x12,0x34};
    SprAtlas a={8,1,1,1,3,ids,off,data};uint8_t row[256];
    CHECK(spr_validate(&a));CHECK(spr_decode_row(&a,0,0,row,sizeof row));
    CHECK(row[0]==1&&row[3]==1&&row[4]==2&&row[7]==2);
    ids[0]=1;untouched(&a);ids[0]=0;
    off[1]=4;untouched(&a);off[1]=3;
    data[0]=3;untouched(&a);data[0]=1;
    data[1]=0xf1;untouched(&a);data[1]=0x31;
    data[2]=0x12;untouched(&a);data[2]=0x32;
    a.width=0;untouched(&a);a.width=8;
    a.data_size=2;untouched(&a);a.data_size=3;
    memset(row,0xa5,sizeof row);CHECK(!spr_decode_row(&a,0,0,row,7)&&row[0]==0xa5);
    CHECK(!spr_decode_row(&a,1,0,row,sizeof row));CHECK(!spr_decode_row(&a,0,1,row,sizeof row));
    CHECK(!spr_pixel(&a,0,8,0,row));
    /* Valid one-level patch, then invalid self/base/bounds/mode/truncation. */
    uint16_t di[]={1},df[]={0,3,10};uint8_t dd[]={1,0x31,0x32,2,0,0,2,2,0,0xab};
    SprAtlas d={8,1,1,2,10,di,df,dd};CHECK(spr_validate(&d));CHECK(spr_decode_row(&d,0,0,row,sizeof row));
    CHECK(row[0]==1&&row[1]==1&&row[2]==10&&row[3]==11&&row[4]==2);
    dd[4]=1;untouched(&d);dd[4]=0;
    dd[4]=2;untouched(&d);dd[4]=0;
    dd[6]=7;untouched(&d);dd[6]=2;
    dd[7]=0;untouched(&d);dd[7]=2;
    dd[8]=2;untouched(&d);dd[8]=0;
    df[2]=9;untouched(&d);df[2]=10;
    /* Exhaustive single-byte mutations of a small delta record, with guards.
       Valid mutations must agree between row and pixel paths; invalid ones
       must never partially change the destination. */
    for(unsigned at=0;at<sizeof dd;at++)for(unsigned value=0;value<256u;value++){
        uint8_t saved=dd[at],guard[10],pixel=0xa5;dd[at]=(uint8_t)value;memset(guard,0xa5,sizeof guard);
        uint32_t ok=spr_decode_row(&d,0,0,guard+1,8);
        CHECK(guard[0]==0xa5&&guard[9]==0xa5);
        if(ok){for(unsigned x=0;x<8;x++)CHECK(spr_pixel(&d,0,x,0,&pixel)&&pixel==guard[x+1]);}
        else {for(unsigned x=0;x<10;x++)CHECK(guard[x]==0xa5);CHECK(!spr_pixel(&d,0,0,0,&pixel)&&pixel==0xa5);}
        dd[at]=saved;
    }
    /* Odd-width raw padding is canonical; don't read the unused nibble. */
    uint16_t ri[]={0},ro[]={0,3};uint8_t rd[]={0,0x12,0x30};SprAtlas r={3,1,1,1,3,ri,ro,rd};
    CHECK(spr_validate(&r));rd[2]=0x3f;untouched(&r);
}
static void cost(const SprAtlas*a){
    uint8_t row[255];unsigned iterations=500;clock_t start=clock();
    for(unsigned i=0;i<iterations;i++)for(unsigned y=0;y<a->height;y++)CHECK(spr_decode_row(a,i%a->frames,y,row,sizeof row));
    printf("%ux%u row decode: %.2f us/frame host (includes validation)\n",a->width,a->height,1e6*(clock()-start)/CLOCKS_PER_SEC/iterations);
}
int main(void){
    exact(&SC_FACE_ATLAS,SINGER_FACES);exact(&SC_STAGE_ATLAS,SINGER_STAGE);
    CHECK(!memcmp(SC_FACE_PAL,SINGER_FACE_PAL,sizeof SC_FACE_PAL));CHECK(!memcmp(SC_STAGE_PAL,SINGER_STAGE_PAL,sizeof SC_STAGE_PAL));CHECK(!memcmp(SC_FACE_DIRTY,SINGER_FACE_DIRTY,sizeof SC_FACE_DIRTY));
    corrupt();cost(&SC_FACE_ATLAS);cost(&SC_STAGE_ATLAS);
    printf("sprite codec: %s\n",fails?"FAILED":"ok");return fails!=0;
}
