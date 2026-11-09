/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/ui/wardrobe_sprites.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#c);fails++;}}while(0)
static uint32_t atlas_check(const SprAtlas *atlas,const uint16_t *palette){
    uint8_t row[255];uint32_t hash=2166136261u;
    CHECK(spr_validate(atlas));CHECK(palette[0]==0xf81fu);
    for(unsigned f=0;f<atlas->frames;f++){
        unsigned opaque=0;
        for(unsigned y=0;y<atlas->height;y++){
            CHECK(spr_decode_row(atlas,f,y,row,sizeof row));
            for(unsigned x=0;x<atlas->width;x++){
                CHECK(row[x]<16u);opaque+=row[x]!=0;
                hash=(hash^(row[x]?palette[row[x]]:0u))*16777619u;
            }
        }
        CHECK(opaque>16u);
    }
    return hash;
}
int main(void){
    CHECK(WD_OUTFIT_N>=1u);CHECK(WD_ADDITIONAL_BYTES<=330000u);
    CHECK(!strcmp(WD_OUTFITS[0].id,"original")&&WD_OUTFITS[0].approved);
    for(unsigned i=0;i<WD_OUTFIT_N;i++){
        const WardrobeSprites *w=WD_OUTFITS+i;const SprAtlas *face=w->faces,*body=w->stage;
        CHECK(w->id&&w->name&&w->face_dirty&&w->face_palette&&w->stage_palette);
        CHECK(face->frames==12u&&face->width==90u&&face->height==90u);
        CHECK(body->frames==8u&&body->width==61u&&body->height==71u);
        for(unsigned j=0;j<i;j++)CHECK(strcmp(w->id,WD_OUTFITS[j].id));
        uint32_t fh=atlas_check(face,w->face_palette),sh=atlas_check(body,w->stage_palette);
        for(unsigned f=0;f<12;f++){
            const uint8_t *r=w->face_dirty+4u*f;uint8_t base[90],row[90];
            CHECK((unsigned)r[0]+r[2]<=90u&&(unsigned)r[1]+r[3]<=90u);
            for(unsigned y=0;y<90;y++){
                CHECK(spr_decode_row(face,0,y,base,sizeof base));CHECK(spr_decode_row(face,f,y,row,sizeof row));
                for(unsigned x=0;x<90;x++)if(base[x]!=row[x])CHECK(x>=r[0]&&x<(unsigned)r[0]+r[2]&&y>=r[1]&&y<(unsigned)r[1]+r[3]);
            }
        }
        printf("wardrobe %s: faces12 hash%08x dance8 hash%08x approved%u\n",w->id,fh,sh,w->approved);
    }
    printf("wardrobe pack: %s (%u outfits, %u additional bytes)\n",fails?"FAILED":"ok",WD_OUTFIT_N,WD_ADDITIONAL_BYTES);
    return fails!=0;
}
