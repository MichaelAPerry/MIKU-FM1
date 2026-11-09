/* SPDX-License-Identifier: GPL-3.0-only */
/* Included by opt-in patched Dinghy usb.c, after sie_rd/sie_wr. All Muvc and
 * SIE access belongs to TIMER5; main only writes the single-frame mailbox.
 * The accepted JPEG stays immutable until video_usb_busy()==0. */
static void e0_send(const uint8_t *p,uint16_t len,uint16_t wlen);
static Muvc video_class;
static uint8_t video_dma[MUVC_PACKET_CAP] __attribute__((aligned(32)));
static uint8_t video_reply[MUVC_PROBE_LEN];
static struct {
    const uint8_t * volatile source;
    volatile uint32_t length;
    volatile uint8_t owner; /* 0 free; main publishes1; timer owns2 until DMA completion/flush */
    volatile uint8_t active;
    volatile uint32_t generation,source_generation;
    uint8_t paused;
    uint8_t pending,wait;
    volatile uint32_t completed,cancelled,stalls,bytes_completed;
    volatile uint32_t first_frame_ms,last_frame_ms,interval_min_ms,interval_max_ms,session_frames,session_bytes;
    uint8_t measured;
} video_usb;
static __attribute__((unused)) uint32_t video_usb_busy(void){return video_usb.owner!=0;}
static __attribute__((unused)) uint32_t video_usb_active(void){return video_usb.active&&!usb.suspended;}
static __attribute__((unused)) uint32_t video_usb_generation(void){return video_usb.generation;}
static __attribute__((unused)) uint32_t video_usb_publish(const uint8_t *data,uint32_t bytes,uint32_t generation){
    if(generation!=video_usb.generation || usb.suspended || !video_usb.active || video_usb.owner || !data || bytes<4u || bytes>MUVC_FRAME_CAP ||
       data[0]!=255 || data[1]!=216 || data[bytes-2]!=255 || data[bytes-1]!=217)return 0;
    video_usb.source=data;video_usb.length=bytes;video_usb.source_generation=generation;
    RING_PUBLISH();video_usb.owner=1;return 1;
}
static void video_ep4_flush(void){
    sie_wr(S_INDEX,4);sie_wr(S_TXCSR1,0x48);sie_wr(S_TXCSR2,0);
    sie_wr(S_TXCSR1,0);sie_wr(S_TXCSR2,0x40);
    video_usb.pending=video_usb.wait=0;
}
static void video_collect_release(void){
    uint32_t token,reason;
    if(muvc_take_release(&video_class,&token,&reason)){
        (void)token;
        if(reason==MUVC_RELEASE_SENT){
            uint32_t now=fm1_ms,interval=now-video_usb.last_frame_ms;
            if(video_usb.measured){
                if(!video_usb.interval_min_ms||interval<video_usb.interval_min_ms)video_usb.interval_min_ms=interval;
                if(interval>video_usb.interval_max_ms)video_usb.interval_max_ms=interval;
            }else{video_usb.first_frame_ms=now;video_usb.measured=1;}
            video_usb.last_frame_ms=now;video_usb.bytes_completed+=video_usb.length;video_usb.completed++;
            video_usb.session_frames++;video_usb.session_bytes+=video_usb.length;
        }else video_usb.cancelled++;
        RING_PUBLISH();video_usb.owner=0;
    }
}
static void video_usb_stop(uint32_t reset){
    video_usb.active=0;video_usb.paused=0;video_usb.generation++;video_ep4_flush();
    if(reset)muvc_reset(&video_class);else muvc_stream(&video_class,0);
    video_collect_release();
    /* Also cancel a main-loop publication not yet offered to the class core. */
    RING_PUBLISH();video_usb.owner=0;
}
static void video_usb_config(void){
    video_usb_stop(1);fm1_usb_ep4_txbuf(video_dma);
    sie_wr(S_INDEX,4);sie_wr(S_TXMAXP,0xff);fm1_usb_ep_enable(1u<<4);
}
static int video_usb_alt(uint32_t alt){
    if(alt>1u || (alt && !video_class.committed))return 0;
    video_usb_stop(0);
    if(!muvc_stream(&video_class,alt))return 0;
    video_usb.active=(uint8_t)alt;
    video_usb.measured=0;video_usb.interval_min_ms=video_usb.interval_max_ms=0;
    video_usb.session_frames=video_usb.session_bytes=0;
    return 1;
}
static void video_usb_abort_control(void){video_class.control_pending=0;}
static int video_usb_control(const uint8_t *setup,uint16_t length){
    uint32_t n=0;int action;
    if(!usb.config)return -1;
    action=muvc_control_setup(&video_class,setup,video_reply,sizeof video_reply,&n);
    if(action==MUVC_CTRL_REPLY){e0_send(video_reply,(uint16_t)n,length);return 1;}
    if(action==MUVC_CTRL_OUT){sie_wr(S_INDEX,0);sie_wr(S_CSR0,0x40);return 1;}
    return -1;
}
static int video_usb_control_data(void){
    uint32_t n=sie_rd(S_COUNT0);
    return muvc_control_data(&video_class,ep0buf,n)==MUVC_CTRL_REPLY;
}
static void video_usb_service(void){
    uint32_t csr,n;
    if(!usb.up || !usb.config || !video_usb.active){
        /* A publication can be interrupted by alt0 before owner=1 is stored.
           It has not reached the class/DMA, so the consumer can cancel it. */
        if(video_usb.owner==1u){RING_PUBLISH();video_usb.owner=0;}
        return;
    }
    if(usb.suspended){
        if(!video_usb.paused){video_usb_stop(0);video_usb.active=1;video_usb.paused=1;}
        return;
    }
    if(video_usb.paused){video_usb.paused=0;if(!video_usb_alt(1))return;}
    sie_wr(S_INDEX,4);csr=sie_rd(S_TXCSR1);
    if(video_usb.pending){
        if(csr&1u){
            if(++video_usb.wait>=40u){
                video_usb.stalls++;
                /* Drop this stalled frame; a camera app need not reselect alt1
                   merely because one endpoint transaction failed. */
                video_usb_alt(1);
            }
            return;
        }
        video_usb.pending=video_usb.wait=0;
        muvc_ack(&video_class,1);video_collect_release();
    }
    if(video_usb.owner==1u){
        RING_PUBLISH();
        if(video_usb.source_generation==video_usb.generation && muvc_offer(&video_class,video_usb.source,video_usb.length,1))video_usb.owner=2;
        else{video_usb.cancelled++;RING_PUBLISH();video_usb.owner=0;}
    }
    n=muvc_prepare(&video_class,fm1_ms*1000u,video_dma,sizeof video_dma);
    if(!n)return;
    fm1_usb_ep4_send(video_dma,n);sie_wr(S_TXCSR1,1);
    video_usb.pending=1;video_usb.wait=0;
}
