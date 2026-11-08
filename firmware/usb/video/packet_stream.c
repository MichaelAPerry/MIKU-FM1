/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual core integration evidence: packetize a JPEG then reconstruct byte-for-byte.
 * Usage: muvc_packet_stream input.jpg packets.uvcp reconstructed.jpg */
#include "firmware/usb/video/uvc.h"
#include <stdio.h>
#include <string.h>
static uint8_t jpeg[MUVC_FRAME_CAP+1],packet[MUVC_PACKET_CAP],roundtrip[MUVC_FRAME_CAP];
int main(int argc,char **argv){
    if(argc!=4)return 1;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;uint32_t bytes=(uint32_t)fread(jpeg,1,sizeof jpeg,f);fclose(f);
    if(bytes>MUVC_FRAME_CAP)return 3;
    Muvc s;uint8_t b[8]={0x21,1,0,1,MUVC_VS_IF,0,26,0},p[26]={0};uint32_t n,token,reason,offset=0,packets=0;
    muvc_init(&s);if(muvc_control_setup(&s,b,p,sizeof p,&n)!=0 || muvc_control_data(&s,p,26)!=1)return 4;
    b[0]=0xA1;b[1]=0x81;if(muvc_control_setup(&s,b,p,sizeof p,&n)!=1)return 4;
    b[0]=0x21;b[1]=1;b[3]=2;if(muvc_control_setup(&s,b,p,sizeof p,&n)!=0 || muvc_control_data(&s,p,26)!=1)return 4;
    if(!muvc_stream(&s,1)||!muvc_offer(&s,jpeg,bytes,123))return 4;
    f=fopen(argv[2],"wb");if(!f)return 2;
    while((n=muvc_prepare(&s,0,packet,sizeof packet))!=0){
        if(n<2 || packet[0]!=2 || (packet[1]&0x41) || offset+n-2>bytes)return 5;
        fputc((int)n&255,f);fputc((int)(n>>8),f);fwrite(packet,1,n,f);
        for(uint32_t i=2;i<n;i++)roundtrip[offset++]=packet[i];
        packets++;if(!muvc_ack(&s,1))return 5;
    }
    fclose(f);if(offset!=bytes || memcmp(jpeg,roundtrip,bytes) || !muvc_take_release(&s,&token,&reason) || reason!=MUVC_RELEASE_SENT)return 6;
    f=fopen(argv[3],"wb");if(!f)return 2;fwrite(roundtrip,1,bytes,f);fclose(f);
    printf("UVC stream: %u JPEG bytes, %u packets, FID0 EOF final, reconstructed identical\n",bytes,packets);return 0;
}
