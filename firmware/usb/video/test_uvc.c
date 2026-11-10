/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/usb/video/uvc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define T(x) do{if(!(x)){fprintf(stderr,"uvc:%d %s\n",__LINE__,#x);exit(1);}}while(0)
static int setup(Muvc *s,int request,int selector,int in,int count,uint8_t *reply,uint32_t *n){
    uint8_t b[8]={(uint8_t)(in?0xA1:0x21),(uint8_t)request,0,(uint8_t)selector,MUVC_VS_IF,0,(uint8_t)count,(uint8_t)(count>>8)};
    return muvc_control_setup(s,b,reply,MUVC_PROBE_LEN,n);
}
static void commit(Muvc *s){uint8_t p[26]={0};uint32_t n;
    T(setup(s,1,1,0,26,p,&n)==0);T(muvc_control_data(s,p,26)==1);
    T(setup(s,0x81,1,1,26,p,&n)==1);T(n==26);
    T(setup(s,1,2,0,26,p,&n)==0);T(muvc_control_data(s,p,26)==1);
}
int main(void){
    Muvc s;uint8_t jpeg[MUVC_FRAME_CAP],packet[MUVC_PACKET_CAP],again[MUVC_PACKET_CAP],probe[26];uint32_t n,token,reason;
    for(unsigned i=0;i<sizeof jpeg;i++)jpeg[i]=(uint8_t)i;
    jpeg[0]=255;jpeg[1]=216;jpeg[sizeof jpeg-2]=255;jpeg[sizeof jpeg-1]=217;
    muvc_init(&s);T(!muvc_stream(&s,1));T(!muvc_stream(&s,2));T(!muvc_offer(&s,jpeg,sizeof jpeg,1));
    T(setup(&s,0x85,1,1,2,probe,&n)==1 && n==2 && probe[0]==26);
    T(setup(&s,0x86,2,1,1,probe,&n)==1 && n==1 && probe[0]==3);
    T(setup(&s,0x84,1,1,26,probe,&n)==1);for(unsigned i=0;i<26;i++)T(!probe[i]);
    T(setup(&s,1,1,0,25,probe,&n)==-1);T(muvc_control_data(&s,probe,26)==-1);
    T(setup(&s,1,1,0,26,probe,&n)==0);probe[2]=2;T(muvc_control_data(&s,probe,26)==-1);
    commit(&s);T(s.committed);T(muvc_stream(&s,1));T(!muvc_stream(&s,2));T(s.streaming);
    T(muvc_offer(&s,jpeg,sizeof jpeg,7));T(!muvc_offer(&s,jpeg,sizeof jpeg,8));
    T(!muvc_prepare(&s,50,packet,8));T(!s.started && !s.offset && !s.packet_pending);
    T(muvc_prepare(&s,100,packet,sizeof packet)==512);T(packet[0]==2 && packet[1]==0x80);T(!s.offset);
    T(!muvc_prepare(&s,200,again,sizeof again));T(muvc_ack(&s,0));
    T(muvc_prepare(&s,300,again,sizeof again)==512);T(!memcmp(packet,again,512));T(muvc_ack(&s,1));T(s.offset==510);
    unsigned offset=510,packets=1;
    while(s.source){unsigned got=muvc_prepare(&s,400,packet,sizeof packet);T(got>=2 && got<=512);T(!(packet[1]&0x41));
        T(!memcmp(packet+2,jpeg+offset,got-2));offset+=got-2;T(!!(packet[1]&2)==(offset==sizeof jpeg));
        T(muvc_ack(&s,1));packets++;}
    T(offset==sizeof jpeg && packets==17);T(s.frames==1 && s.fid==1);T(!muvc_offer(&s,jpeg,sizeof jpeg,8));
    T(muvc_take_release(&s,&token,&reason));T(token==7 && reason==MUVC_RELEASE_SENT);T(!muvc_take_release(&s,0,0));
    T(muvc_offer(&s,jpeg,sizeof jpeg,8));T(!muvc_prepare(&s,100+MUVC_FRAME_US-1,packet,sizeof packet));
    T(muvc_prepare(&s,100+MUVC_FRAME_US,packet,sizeof packet)==512);T((packet[1]&1)==1);
    muvc_frame_error(&s);T(muvc_ack(&s,1));T(muvc_prepare(&s,99999,packet,sizeof packet)==2);T(packet[1]==0xC3);
    T(muvc_ack(&s,1));T(muvc_take_release(&s,&token,&reason));T(token==8 && reason==MUVC_RELEASE_ERROR && s.dropped==1);
    T(muvc_offer(&s,jpeg,sizeof jpeg,9));T(muvc_stream(&s,0));T(muvc_take_release(&s,&token,&reason));T(reason==MUVC_RELEASE_STOP);
    T(muvc_stream(&s,1));T(muvc_offer(&s,jpeg,sizeof jpeg,10));T(muvc_prepare(&s,0xfffffff0u,packet,sizeof packet));
    muvc_reset(&s);T(!s.source && !s.packet_pending && !s.committed && !s.streaming);
    T(muvc_take_release(&s,&token,&reason));T(token==10 && reason==MUVC_RELEASE_RESET);T(!muvc_ack(&s,1));
    commit(&s);T(muvc_stream(&s,1));
    uint8_t short_jpeg[4]={255,216,255,217};T(muvc_offer(&s,short_jpeg,4,11));
    T(muvc_prepare(&s,0xfffffff0u,packet,sizeof packet)==6);T(packet[1]&2);T(muvc_ack(&s,1));T(muvc_take_release(&s,0,0));
    T(muvc_offer(&s,short_jpeg,4,12));T(!muvc_prepare(&s,0x10u,packet,sizeof packet));
    T(muvc_prepare(&s,0xfffffff0u+MUVC_FRAME_US,packet,sizeof packet)==6);T(muvc_ack(&s,1));T(muvc_take_release(&s,0,0));
    T(!muvc_offer(&s,jpeg,3,12));T(!muvc_offer(&s,jpeg,MUVC_FRAME_CAP+1,12));
    /* Late producer slots skip missed deadlines without accumulating phase drift. */
    muvc_stream(&s,0);muvc_stream(&s,1);
    T(muvc_offer(&s,short_jpeg,4,13));T(muvc_prepare(&s,1000,packet,sizeof packet));T(muvc_ack(&s,1));T(muvc_take_release(&s,0,0));
    T(muvc_offer(&s,short_jpeg,4,14));T(muvc_prepare(&s,1000+MUVC_FRAME_US+10000,packet,sizeof packet));
    T(muvc_ack(&s,1));T(muvc_take_release(&s,0,0));
    T(muvc_offer(&s,short_jpeg,4,15));T(!muvc_prepare(&s,1000+2*MUVC_FRAME_US-1,packet,sizeof packet));
    T(muvc_prepare(&s,1000+2*MUVC_FRAME_US,packet,sizeof packet));T(muvc_ack(&s,1));T(muvc_take_release(&s,0,0));
    /* Probe/commit caps are never accepted from host unchecked. */
    T(setup(&s,0x81,1,1,26,probe,&n)==1);T(probe[22]==0 && probe[23]==2);
    probe[23]=4;T(setup(&s,1,2,0,26,probe,&n)==0);T(muvc_control_data(&s,probe,26)==-1);
    T(setup(&s,0x81,3,1,26,probe,&n)==-1);
    /* Every descriptor length is self-consistent and advertised packet/frame fit profile. */
    T(muvc_configuration_length==152);T(muvc_configuration_descriptor[2]==152);
    unsigned at=0,eps=0;while(at<muvc_configuration_length){unsigned len=muvc_configuration_descriptor[at];T(len>=2);T(at+len<=muvc_configuration_length);
        if(muvc_configuration_descriptor[at+1]==36&&muvc_configuration_descriptor[at+2]==2&&muvc_configuration_descriptor[at+3]==1){
            T(len==18&&muvc_configuration_descriptor[at+14]==3);T(len==15u+muvc_configuration_descriptor[at+14]);
        }
        if(muvc_configuration_descriptor[at+1]==5){eps++;T(muvc_configuration_descriptor[at+2]==MUVC_EP);T(muvc_configuration_descriptor[at+4]==0 && muvc_configuration_descriptor[at+5]==2);}at+=len;}
    T(at==muvc_configuration_length && eps==1);
    printf("uvc: controls/caps/FID/EOF/ERR/reset/ownership/backpressure/pacing passed; sizeof state=%zu\n",sizeof(Muvc));return 0;
}
