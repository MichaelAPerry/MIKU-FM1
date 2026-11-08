/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/app/performance_wire.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    uint8_t bytes[PW_SIZE],before[PW_SIZE];PwState s={0},t={0},prior;
    s.sample=0xffffffffu;s.sequence=16383;s.mouth=5;s.vocal_level=127;s.mora=69;
    s.section=5;s.row=63;s.step=15;s.playing=1;s.sprite=7;s.audio_level=127;
    s.outfit=127;s.background=127;s.frame=127;s.palette=127;s.transient=127;
    assert(pw_encode(bytes,sizeof bytes,&s)==PW_SIZE);
    for(unsigned i=1;i<PW_SIZE-1;i++)assert(bytes[i]<128);
    assert(pw_decode(&t,bytes,sizeof bytes)==PW_SIZE);
    assert(t.sample==s.sample&&t.sequence==s.sequence&&t.mora==69&&t.transient==127&&t.sprite==7);
    prior=t;
    for(unsigned i=0;i<PW_SIZE;i++){
        uint8_t x=bytes[i];bytes[i]^=1;assert(!pw_decode(&t,bytes,sizeof bytes));assert(!memcmp(&prior,&t,sizeof t));bytes[i]=x;
    }
    assert(!pw_decode(&t,bytes,PW_SIZE-1));assert(!pw_decode(&t,0,PW_SIZE));
    memcpy(before,bytes,sizeof bytes);s.mora=70;
    assert(!pw_encode(bytes,sizeof bytes,&s));assert(!memcmp(bytes,before,sizeof bytes));
    s.mora=0;assert(!pw_encode(bytes,PW_SIZE-1,&s));assert(!memcmp(bytes,before,sizeof bytes));
    puts("performance wire: bounds, 7-bit encoding, checksum, roundtrip, transactionality OK");return 0;
}
