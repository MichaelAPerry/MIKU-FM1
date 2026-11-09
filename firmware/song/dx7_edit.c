/* SPDX-License-Identifier: GPL-3.0-only */
#include "dx7_edit.h"
#include "firmware/audio/dx7/bank.h"
_Static_assert(SG_DX7_FIRST+DX7_BANK_N<=SG_DX7_CUSTOM,"factory preset IDs collide with CUSTOM");
uint32_t db_active(const Song *g,uint32_t t){return g&&(t==1||t==2)&&g->trk[t].preset>=SG_DX7_FIRST&&g->trk[t].preset<=SG_DX7_LAST;}
uint32_t db_select(Song *g,uint32_t t,uint32_t bank){
    uint32_t i;if(!g||(t!=1&&t!=2)||bank>=DX7_BANK_N)return 0;
    for(i=0;i<128;i++)g->dx7[t-1][i]=dx7_bank[bank][i];
    g->trk[t].preset=(uint8_t)(SG_DX7_FIRST+bank);return 1;
}
/* Packed DX7 fields retain unrelated bits when editing. op0 is Yamaha OP1. */
static uint32_t db_field(uint32_t op,uint32_t page,uint32_t k,uint32_t *shift,uint32_t *mask){
    uint32_t off=(5u-op)*17u;*shift=0;*mask=127;
    if(page==1){if(k==0){*mask=31;return 110;}if(k==1){*mask=7;return 111;}return 112;}
    if(page==2){if(!k)return off+14;if(k==1){*shift=1;*mask=31;return off+15;}if(k==2)return off+16;*shift=3;*mask=15;return off+12;}
    if(page==3)return off+k;
    if(page==4)return off+4+k;
    if(page==5){if(k<3)return off+8+k;*mask=7;return off+12;}
    if(page==6){if(k<3)return 113+k;*shift=4;*mask=7;return 116;}
    if(page==7)return 102+k;
    if(page==8)return 106+k;
    if(page==9){if(k==2)return 117;*shift=3;*mask=1;return 111;}
    if(page==10){if(k<2){*shift=k*2;*mask=3;return off+11;}*shift=k==2?2:0;*mask=k==2?7:3;return off+13;}
    return 0;
}
uint32_t db_limit(uint32_t page,uint32_t k){
    if(k>3||!page||page>=DB_PAGES)return 0;
    if(page==1)return k==0?31:k==1?7:k==3?127:99;
    if(page==2)return k==1?31:k==3?14:99;
    if(page==5)return k==3?7:99;
    if(page==6)return k==3?7:99;
    if(page==9)return k<2?127:k==2?48:1;
    if(page==10)return k==2?7:3;
    return 99;
}
uint32_t db_value(const Song *g,uint32_t t,uint32_t op,uint32_t page,uint32_t k){
    uint32_t sh,mask,off;if(!db_active(g,t)||op>=6||!page||page>=DB_PAGES||k>3||(page==9&&k==1))return 0;
    if((page==1&&k==3)||(page==9&&k==0))return g->trk[t].level;
    off=db_field(op,page,k,&sh,&mask);return (g->dx7[t-1][off]>>sh)&mask;
}
uint32_t db_set(Song *g,uint32_t t,uint32_t op,uint32_t page,uint32_t k,uint32_t value){
    uint32_t sh,mask,off;if(!db_active(g,t)||op>=6||!page||page>=DB_PAGES||k>3||value>db_limit(page,k)||(page==9&&k==1))return 0;
    if((page==1&&k==3)||(page==9&&k==0)){g->trk[t].level=(uint8_t)value;return 1;}
    off=db_field(op,page,k,&sh,&mask);g->dx7[t-1][off]=(uint8_t)((g->dx7[t-1][off]&~(mask<<sh))|(value<<sh));
    g->trk[t].preset=SG_DX7_CUSTOM;return 1;
}
uint32_t db_toggle(Song *g,uint32_t t,uint32_t op,uint32_t page){
    uint32_t off;if(!db_active(g,t)||op>=6)return 0;
    if(page==2){off=(5u-op)*17u+15u;g->dx7[t-1][off]^=1u;}
    else if(page==6){off=116;uint32_t w=((g->dx7[t-1][off]>>1)&7u)+1u;g->dx7[t-1][off]=(uint8_t)((g->dx7[t-1][off]&~14u)|((w%6u)<<1));}
    else return 0;
    g->trk[t].preset=SG_DX7_CUSTOM;return 1;
}
