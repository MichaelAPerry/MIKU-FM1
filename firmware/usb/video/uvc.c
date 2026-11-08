/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/usb/video/uvc.h"
/* Independently bounded profile, informed by Synthesthetic589bd0f UVC1 framing and AC79EP4 transport.
 * No optional processing/status interfaces: computer visual designs and controls remain separate. */
#define MU16(n) ((n)&255u),(((n)>>8)&255u)
#define MU32(n) MU16(n),MU16((n)>>16)
const uint8_t muvc_device_descriptor[18]={18,1,0x00,0x02,0xEF,2,1,64,0x09,0x12,0,0,0,1,0,0,0,1};
/* PID0 is intentionally unassigned. Integration must select a legitimate project PID/revision, not ship this test identity. */
const uint8_t muvc_configuration_descriptor[]={
    9,2,MU16(152),2,1,0,0x80,50,
    8,11,0,2,14,3,0,0,
    9,4,0,0,0,14,1,0,0,
    13,36,1,0,1,MU16(40),MU32(48000000),1,1,
    18,36,2,1,1,2,0,0,0,0,0,0,0,0,0,0,0,0,
    9,36,3,2,1,1,0,1,0,
    9,4,1,0,0,14,2,0,0,
    14,36,1,1,MU16(61),MUVC_EP,0,2,0,0,0,1,0,
    11,36,6,1,1,0,1,0,0,0,0,
    30,36,7,1,0,MU16(MUVC_WIDTH),MU16(MUVC_HEIGHT),MU32(8000),MU32(983040),
        MU32(MUVC_FRAME_CAP),MU32(MUVC_INTERVAL),1,MU32(MUVC_INTERVAL),
    6,36,13,1,1,4,
    9,4,1,1,1,14,2,0,0,
    7,5,MUVC_EP,5,MU16(MUVC_PACKET_CAP),1
};
const uint16_t muvc_configuration_length=sizeof muvc_configuration_descriptor;
static uint32_t mu_read32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void mu_write32(uint8_t *p,uint32_t n){for(uint32_t i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void mu_probe(uint8_t *p){
    for(uint32_t i=0;i<MUVC_PROBE_LEN;i++)p[i]=0;
    p[0]=1;p[2]=1;p[3]=1;mu_write32(p+4,MUVC_INTERVAL);mu_write32(p+18,MUVC_FRAME_CAP);mu_write32(p+22,MUVC_PACKET_CAP);
}
static void mu_release(Muvc *s,uint32_t reason){
    if(!s->source)return;
    s->released_token=s->token;s->release_reason=(uint8_t)reason;
    s->source=0;s->length=s->offset=0;s->started=s->packet_pending=s->pending_eof=s->pending_error=s->error=0;
    if(reason==MUVC_RELEASE_SENT)s->frames++;else s->dropped++;
}
void muvc_init(Muvc *s){
    /* Volatile prevents a compiler's zero-loop lowering to memset in freestanding firmware. */
    volatile uint8_t *p=(volatile uint8_t *)s;
    for(uint32_t i=0;i<sizeof *s;i++)p[i]=0;
}
int muvc_stream(Muvc *s,uint32_t alt){
    if(alt>1 || (alt && !s->committed))return 0;
    if(!alt){if(s->started)s->fid^=1;mu_release(s,MUVC_RELEASE_STOP);s->clocked=0;}
    s->streaming=(uint8_t)alt;return 1;
}
void muvc_reset(Muvc *s){
    if(s->started)s->fid^=1;
    mu_release(s,MUVC_RELEASE_RESET);s->streaming=s->committed=s->probe_set=s->clocked=s->control_pending=0;
}
int muvc_offer(Muvc *s,const uint8_t *jpeg,uint32_t bytes,uint32_t token){
    if(!s->streaming || s->source || s->release_reason || !jpeg || bytes<4 || bytes>MUVC_FRAME_CAP ||
       jpeg[0]!=255 || jpeg[1]!=216 || jpeg[bytes-2]!=255 || jpeg[bytes-1]!=217){s->rejected++;return 0;}
    s->source=jpeg;s->length=bytes;s->offset=0;s->token=token;s->started=s->error=0;return 1;
}
uint32_t muvc_prepare(Muvc *s,uint32_t now,uint8_t *out,uint32_t cap){
    uint32_t n,last;
    if(!s->streaming || !s->source || s->packet_pending || !out || cap<2)return 0;
    n=s->length-s->offset;
    if(n>MUVC_PACKET_CAP-2)n=MUVC_PACKET_CAP-2;
    if(s->error)n=0;
    if(n>cap-2)return 0; /* hardware buffer too small: never split according to accidental caller capacity */
    if(!s->started){
        if(s->clocked && (uint32_t)(now-s->last_start_us)<MUVC_FRAME_US)return 0;
        s->started=1;s->clocked=1;s->last_start_us=now;
    }
    last=s->error || s->offset+n==s->length;
    out[0]=2;out[1]=(uint8_t)(0x80u|s->fid|(last?2u:0u)|(s->error?0x40u:0u));
    for(uint32_t i=0;i<n;i++)out[i+2]=s->source[s->offset+i];
    s->pending_bytes=(uint16_t)n;s->pending_eof=(uint8_t)last;s->pending_error=s->error;s->packet_pending=1;
    return n+2;
}
int muvc_ack(Muvc *s,uint32_t sent){
    if(!s->packet_pending)return 0;
    s->packet_pending=0;
    if(!sent)return 1;
    s->packets++;s->offset+=s->pending_bytes;
    if(s->pending_eof){uint32_t reason=s->pending_error?MUVC_RELEASE_ERROR:MUVC_RELEASE_SENT;s->fid^=1;mu_release(s,reason);}
    return 1;
}
void muvc_frame_error(Muvc *s){if(s->source)s->error=1;}
int muvc_take_release(Muvc *s,uint32_t *token,uint32_t *reason){
    if(!s->release_reason)return 0;
    if(token)*token=s->released_token;
    if(reason)*reason=s->release_reason;
    s->release_reason=0;return 1;
}
static int mu_stall(Muvc *s){s->stalls++;return MUVC_CTRL_STALL;}
int muvc_control_setup(Muvc *s,const uint8_t b[8],uint8_t *reply,uint32_t cap,uint32_t *length){
    uint32_t selector=b[3],request=b[1],want=b[6]|(uint32_t)b[7]<<8;
    s->control_pending=0;*length=0;
    if(b[2] || b[4]!=MUVC_VS_IF || b[5] || (selector!=1 && selector!=2))return mu_stall(s);
    if(b[0]==0x21 && request==1){
        if(want!=MUVC_PROBE_LEN)return mu_stall(s);
        s->control_pending=(uint8_t)selector;return MUVC_CTRL_OUT;
    }
    if(b[0]!=0xA1 || !reply)return mu_stall(s);
    if(request==0x85){if(cap<2 || want!=2)return mu_stall(s);reply[0]=MUVC_PROBE_LEN;reply[1]=0;*length=2;return MUVC_CTRL_REPLY;}
    if(request==0x86){if(cap<1 || want!=1)return mu_stall(s);reply[0]=3;*length=1;return MUVC_CTRL_REPLY;}
    if(request!=0x81 && request!=0x82 && request!=0x83 && request!=0x84 && request!=0x87)return mu_stall(s);
    if(cap<MUVC_PROBE_LEN || !want || want>MUVC_PROBE_LEN)return mu_stall(s);
    mu_probe(reply);
    if(request==0x84)for(uint32_t i=0;i<MUVC_PROBE_LEN;i++)reply[i]=0;
    *length=want;return MUVC_CTRL_REPLY;
}
int muvc_control_data(Muvc *s,const uint8_t *p,uint32_t n){
    uint32_t selector=s->control_pending;s->control_pending=0;
    if(!selector || !p || n!=MUVC_PROBE_LEN)return mu_stall(s);
    if(p[2]>1 || p[3]>1)return mu_stall(s);
    if(selector==1){s->probe_set=1;return MUVC_CTRL_REPLY;} /* one advertised mode: normalize suggestions */
    /* COMMIT must explicitly accept the canonical mode, including caps returned by PROBE. */
    if(!s->probe_set || s->source || p[2]!=1 || p[3]!=1 || mu_read32(p+4)!=MUVC_INTERVAL ||
       mu_read32(p+18)!=MUVC_FRAME_CAP || mu_read32(p+22)!=MUVC_PACKET_CAP)return mu_stall(s);
    s->committed=1;return MUVC_CTRL_REPLY;
}
