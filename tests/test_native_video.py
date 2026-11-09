#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compile the actual patched pinned USB source against a fake SIE, then drive EP0/EP4.
Usage: python3 tests/test_native_video.py [DINGHY_ROOT]
No register addresses are used by the test; the protocol source is unchanged.
"""
from pathlib import Path
import importlib.util
import subprocess
import sys
import tempfile
root=Path(__file__).resolve().parents[1]
dinghy=Path(sys.argv[1]) if len(sys.argv)>1 else root/'build/dinghy'
spec=importlib.util.spec_from_file_location('patch_video',root/'tools/patch_dinghy_video.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
# Read exact pinned source from Git, not another mode's mutable build worktree.
original=subprocess.check_output(['git','show','f77bf03283bbb2c05294b01cdad68a30b410f93d:firmware/src/usb.c'],cwd=dinghy,text=True)
patched=module.patch(original)
try:module.patch(original+'\n')
except ValueError:pass
else:raise AssertionError('unrecognized source accepted')
mock=r'''
#pragma once
#include <stdint.h>
#include <string.h>
static uint32_t regs[8][32],global_regs[16],index_now,sie_value;
static uint8_t control[1024],payload[512];static uint32_t control_n,payload_n,sends;
static void fm1_usb_reset(void){}
static void fm1_usb_attach(void *p){(void)p;}
static void fm1_usb_off(void){}
static uint32_t fm1_usb_sie_on(void){return 1;}
static uint32_t fm1_usb_sie_done(void){return 1;}
static uint32_t fm1_usb_sie_data(void){return sie_value;}
static void fm1_usb_sie_wr_start(uint32_t r,uint32_t v){if(r==14)index_now=v;else if(r<16)global_regs[r]=v;else regs[index_now][r]=v;}
static void fm1_usb_sie_rd_start(uint32_t r){sie_value=r<16?global_regs[r]:regs[index_now][r];if(r>=2&&r<=6)global_regs[r]=0;}
static void fm1_usb_ep0_buf(void *p){(void)p;}
static void fm1_usb_ep_txbuf(uint32_t e,void *p){(void)e;(void)p;}
static void fm1_usb_ep_rxbuf(uint32_t e,void *p){(void)e;(void)p;}
static void fm1_usb_ep0_send(void *p,uint32_t n){memcpy(control+control_n,p,n);control_n+=n;}
static void fm1_usb_ep_send(uint32_t e,void *p,uint32_t n){(void)e;(void)p;(void)n;}
static void fm1_usb_ep4_txbuf(void *p){(void)p;}
static void fm1_usb_ep4_send(void *p,uint32_t n){assert(n<=512);memcpy(payload,p,n);payload_n=n;sends++;}
static void fm1_usb_rx_sync(void){}
static void fm1_usb_ep_enable(uint32_t n){(void)n;}
static uint32_t fm1_usb_sof_take(void){return 1;}
'''
harness=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define MIKU_UVC 1
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
#define FELUCCA_OTA 0
#define HALF_FRAMES 256u
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
static uint32_t fm1_ms;
static void fm1_delay_ms(uint32_t n){(void)n;}
#include "firmware/src/usb.c"
static void request(uint8_t kind,uint8_t req,uint16_t value,uint16_t iface,uint16_t len){
    control_n=0;regs[0][S_CSR0]=1;regs[0][S_COUNT0]=8;
    ep0buf[0]=kind;ep0buf[1]=req;ep0buf[2]=value;ep0buf[3]=value>>8;
    ep0buf[4]=iface;ep0buf[5]=iface>>8;ep0buf[6]=len;ep0buf[7]=len>>8;
    ep0_service();
}
static void data(const uint8_t *p,uint32_t n){memcpy(ep0buf,p,n);regs[0][S_COUNT0]=n;regs[0][S_CSR0]=1;ep0_service();}
static void finish_in(void){for(unsigned n=0;usb.e0_tx&&n<16;n++){regs[0][S_CSR0]=0;ep0_service();}assert(!usb.e0_tx);}
int main(void){
    uint8_t probe[26],jpeg[1200];
    usb.up=1;
    request(0x80,6,0x100,0,18);finish_in();
    assert(control_n==18&&control[4]==0xef&&control[5]==2&&control[6]==1&&control[12]==0x11&&control[13]==4);
    request(0x80,6,0x302,0x409,64);finish_in();assert(control_n==28&&control[0]==28);
    const char *product="MIKU-FM VIDEO";for(unsigned i=0;i<13;i++)assert(control[2+2*i]==product[i]&&!control[3+2*i]);
    request(0x80,6,0x200,0,512);finish_in();assert(control_n==252&&control[4]==4);
    unsigned off=0,ep1=0,ep4=0,ifs=0;
    while(off<control_n){unsigned n=control[off];assert(n>=2&&off+n<=control_n);
        if(control[off+1]==4&&control[off+3]==0)ifs++;
        if(control[off+1]==36&&control[off+2]==2&&control[off+3]==1&&n>=8&&control[off+4]==1&&control[off+5]==2){
            /* UVC camera terminal: descriptor length must include bmControls. */
            assert(n==18&&control[off+14]==3&&n==15u+control[off+14]);
            assert(!control[off+15]&&!control[off+16]&&!control[off+17]);
        }
        if(control[off+1]==5){if(control[off+2]==1||control[off+2]==0x81)ep1++;
            if(control[off+2]==0x84){ep4++;assert(control[off+4]==0&&control[off+5]==2);}}
        off+=n;
    }
    assert(off==252&&ifs==4&&ep1==2&&ep4==1);
    request(0,9,1,0,0);assert(usb.config==1);
    request(1,11,1,3,0);assert(regs[0][S_CSR0]==0x60); /* no COMMIT */
    request(0xA1,0x85,0x100,3,2);assert(control_n==2&&control[0]==26);finish_in();
    request(0xA1,0x81,0x100,3,26);assert(control_n==26);memcpy(probe,control,26);finish_in();
    request(0x21,1,0x100,3,26);assert(video_class.control_pending==1);data(probe,25);assert(!video_class.probe_set&&regs[0][S_CSR0]==0x60);
    request(0x21,1,0x100,3,26);data(probe,26);assert(video_class.probe_set);
    request(0x21,1,0x200,3,26);data(probe,26);assert(video_class.committed);
    request(1,11,1,3,0);assert(video_usb_active());
    memset(jpeg,42,sizeof jpeg);jpeg[0]=255;jpeg[1]=216;jpeg[1198]=255;jpeg[1199]=217;
    assert(video_usb_publish(jpeg,sizeof jpeg,video_usb_generation()));assert(video_usb_busy());assert(!video_usb_publish(jpeg,sizeof jpeg,video_usb_generation()));
    video_usb_service();assert(sends==1&&payload_n==512&&payload[0]==2&&!(payload[1]&2));
    uint8_t snapshot[512];memcpy(snapshot,payload,sizeof snapshot);
    video_usb_service();assert(sends==1&&!memcmp(snapshot,payload,sizeof snapshot));
    regs[4][S_TXCSR1]=0;video_usb_service();assert(sends==2&&video_usb_busy());
    regs[4][S_TXCSR1]=0;video_usb_service();assert(sends==3&&payload[1]&2&&video_usb_busy());
    regs[4][S_TXCSR1]=0;video_usb_service();assert(!video_usb_busy()&&video_usb.completed==1);
    /* Alt0 flush releases an in-flight frame; next alt1 starts clean. */
    assert(video_usb_publish(jpeg,sizeof jpeg,video_usb_generation()));fm1_ms=100;video_usb_service();assert(video_usb_busy());
    request(1,11,0,3,0);assert(!video_usb_active()&&!video_usb_busy()&&!video_class.source);
    request(1,11,1,3,0);assert(video_usb_active());
    /* Abandoned SET_CUR must not consume the next request as data. */
    request(0x21,1,0x100,3,26);regs[0][S_CSR0]=0x10;ep0_service();assert(!video_class.control_pending);
    request(0x80,8,0,0,1);assert(control_n==1&&control[0]==1);finish_in();
    /* Class request on wrong interface/high index is stalled. */
    request(0xA1,0x81,0x100,2,26);assert(regs[0][S_CSR0]==0x60);
    request(0xA1,0x81,0x100,0x103,26);assert(regs[0][S_CSR0]==0x60);
    request(1,11,2,3,0);assert(regs[0][S_CSR0]==0x60);
    /* Consumer cancels late publication interrupted by alt0. */
    request(1,11,0,3,0);video_usb.owner=1;video_usb_service();assert(!video_usb_busy());
    /* Suspended stream and bus reset release DMA/source before re-use. */
    request(1,11,1,3,0);assert(video_usb_publish(jpeg,sizeof jpeg,video_usb_generation()));fm1_ms=200;video_usb_service();
    usb.suspended=1;video_usb_service();assert(!video_usb_busy()&&!video_usb_active());usb.suspended=0;
    video_usb_service();assert(video_usb_active()); /* resume needs no new alt1 */
    uint32_t stale=video_usb_generation();
    request(1,11,0,3,0);request(1,11,1,3,0);
    assert(!video_usb_publish(jpeg,sizeof jpeg,stale));
    /* Simulate preemption after publisher's first generation check. */
    video_usb.source=jpeg;video_usb.length=sizeof jpeg;video_usb.source_generation=stale;video_usb.owner=1;
    unsigned old_sends=sends;video_usb_service();assert(!video_usb_busy()&&sends==old_sends);
    /* A timed-out packet recovers on the same alternate setting. */
    fm1_ms=250;assert(video_usb_publish(jpeg,sizeof jpeg,video_usb_generation()));video_usb_service();
    for(unsigned i=0;i<40;i++)video_usb_service();
    assert(video_usb_active()&&!video_usb_busy()&&video_usb.stalls==1);
    request(1,11,1,3,0);assert(video_usb_publish(jpeg,sizeof jpeg,video_usb_generation()));fm1_ms=300;video_usb_service();
    global_regs[S_INTRUSB]=4;usb_poll();assert(!usb.config&&!video_usb_busy()&&!video_class.committed);
    /* Ten seconds of delivery across 150 frames: retain the entire payload,
       EOF exactly once, alternating FID, bounded packet sizes and timer cadence. */
    request(0,9,1,0,0);
    request(0x21,1,0x100,3,26);data(probe,26);
    request(0x21,1,0x200,3,26);data(probe,26);
    request(1,11,1,3,0);
    unsigned start_completed=video_usb.completed,previous_start=0,first_fid=video_class.fid;
    for(unsigned frame=0;frame<150;frame++){
        uint8_t received[1200];unsigned bytes=0,packet=0,fid=0;
        assert(video_usb_publish(jpeg,sizeof jpeg,video_usb_generation()));
        for(unsigned turns=0;video_usb_busy()&&turns<100;turns++){
            unsigned before=sends;fm1_ms++;regs[4][S_TXCSR1]=0;video_usb_service();
            if(sends!=before){
                assert(payload_n>=2&&payload_n<=512&&payload[0]==2&&!(payload[1]&0x40));
                if(!packet){fid=payload[1]&1;assert(fid==(first_fid^(frame&1)));
                    if(previous_start&&!(fm1_ms-previous_start>=66&&fm1_ms-previous_start<=67)){fprintf(stderr,"frame %u at %u previous %u clock %u\n",frame,fm1_ms,previous_start,video_class.last_start_us);assert(0);}
                    previous_start=fm1_ms;
                }
                assert((payload[1]&1)==fid&&bytes+payload_n-2<=sizeof received);
                memcpy(received+bytes,payload+2,payload_n-2);bytes+=payload_n-2;packet++;
                assert(!!(payload[1]&2)==(bytes==sizeof received));
            }
        }
        assert(!video_usb_busy()&&bytes==sizeof jpeg&&!memcmp(received,jpeg,sizeof jpeg));
    }
    assert(video_usb.completed-start_completed==150&&video_usb.session_frames==150);
    assert(video_usb.session_bytes==150*sizeof jpeg&&video_usb.interval_min_ms==66&&video_usb.interval_max_ms==67);
    printf("UVC transport simulation: %u frames, %u bytes, frame intervals %u..%u ms\n",video_usb.session_frames,video_usb.session_bytes,video_usb.interval_min_ms,video_usb.interval_max_ms);
    /* Existing MIDI rescue command still works. */
    const uint8_t key[]={0xf0,0x22,0x24,0x35,0x7d,0xf7};for(unsigned i=0;i<6;i++)sysex_byte(key[i]);assert(usb.uboot_req);
    puts("native UVC: descriptors/EP0/probe/commit/alt/payload/ownership/reset/MIDI rescue OK");
}
'''
with tempfile.TemporaryDirectory() as td:
    work=Path(td);(work/'firmware/src').mkdir(parents=True);(work/'firmware/hal').mkdir()
    (work/'firmware/src/usb.c').write_text(patched);(work/'firmware/hal/fm1_usb.h').write_text(mock)
    (work/'test.c').write_text(harness)
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Wno-unused-function','-I'+str(work),'-I'+str(root),'-I'+str(root/'native'),str(work/'test.c'),'-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test')],check=True)
