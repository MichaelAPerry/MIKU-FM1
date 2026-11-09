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
    uint8_t pending,wait;
    uint32_t completed,cancelled,stalls;
} video_usb;
static __attribute__((unused)) uint32_t video_usb_busy(void){return video_usb.owner!=0;}
static __attribute__((unused)) uint32_t video_usb_active(void){return video_usb.active;}
static __attribute__((unused)) uint32_t video_usb_publish(const uint8_t *data,uint32_t bytes){
    if(!video_usb.active || video_usb.owner || !data || bytes<4u || bytes>MUVC_FRAME_CAP ||
       data[0]!=255 || data[1]!=216 || data[bytes-2]!=255 || data[bytes-1]!=217)return 0;
    video_usb.source=data;video_usb.length=bytes;
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
        if(reason==MUVC_RELEASE_SENT)video_usb.completed++;else video_usb.cancelled++;
        RING_PUBLISH();video_usb.owner=0;
    }
}
static void video_usb_stop(uint32_t reset){
    video_usb.active=0;video_ep4_flush();
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
    video_usb.active=(uint8_t)alt;return 1;
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
    if(usb.suspended){video_usb_stop(0);return;}
    sie_wr(S_INDEX,4);csr=sie_rd(S_TXCSR1);
    if(video_usb.pending){
        if(csr&1u){
            if(++video_usb.wait>=40u){video_usb.stalls++;video_usb_stop(0);}
            return;
        }
        video_usb.pending=video_usb.wait=0;
        muvc_ack(&video_class,1);video_collect_release();
    }
    if(video_usb.owner==1u){
        RING_PUBLISH();
        if(muvc_offer(&video_class,video_usb.source,video_usb.length,1))video_usb.owner=2;
        else{video_usb.cancelled++;RING_PUBLISH();video_usb.owner=0;}
    }
    n=muvc_prepare(&video_class,fm1_ms*1000u,video_dma,sizeof video_dma);
    if(!n)return;
    fm1_usb_ep4_send(video_dma,n);sie_wr(S_TXCSR1,1);
    video_usb.pending=1;video_usb.wait=0;
}
