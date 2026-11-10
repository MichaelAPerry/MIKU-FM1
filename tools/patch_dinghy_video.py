#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Patch only pinned Dinghy USB source for isolated MIDI+UVC development builds.
Usage: patch_dinghy_video.py DINGHY_ROOT. Default builds must not call this.
"""
from pathlib import Path
import hashlib
import sys
EXPECTED='0aa9af8620a7c193dd3c3376db58201b006201fa37bd497194346545167707c0'

def patch(source):
    if hashlib.sha256(source.encode()).hexdigest()!=EXPECTED:
        raise ValueError('video patch refuses USB source differing from pinned Dinghy f77bf03')
    original=source
    def sub(a,b,n=1):
        nonlocal source
        if source.count(a)!=n: raise ValueError('video patch anchor mismatch: '+a[:80])
        source=source.replace(a,b)
    sub('enum { S_FADDR', '''#if !defined(MIKU_UVC) || !MIKU_UVC
#error "video patch requires explicit MIKU_UVC=1"
#endif
#if FELUCCA_CDC || FELUCCA_UAC || defined(FELUCCA_LOADER)
#error "MIKU UVC prototype is isolated from UAC, CDC and update-loader builds"
#endif
#define MUVC_VC_IF 2u
#define MUVC_VS_IF 3u
#include "firmware/usb/video/uvc.c"
enum { S_FADDR''')
    sub('#define CFG_LEN (101 + 74 * FELUCCA_CDC + 74 * FELUCCA_UAC + 3 * FELUCCA_UAC_48K)', '#define CFG_LEN (252) /* MIDI101 + audioIAD8 + video143 */')
    sub('#define USB_NIF (2 + FELUCCA_UAC + 2 * FELUCCA_CDC)', '#define USB_NIF 4')
    sub('18, 1, 0x10, 0x01, 0, 0, 0, 64, 0x09, 0x12, FELUCCA_USB_PID & 0xFF,','18, 1, 0x00, 0x02, 0xEF, 2, 1, 64, 0x09, 0x12, FELUCCA_USB_PID & 0xFF,')
    sub('FELUCCA_USB_PID >> 8, 0x00 + 0x10 * (FELUCCA_UAC + FELUCCA_UAC_48K), 0x03,','FELUCCA_USB_PID >> 8, 0x11, 0x04, /* development video descriptor revision4.11 */')
    sub('    9, 4, 0, 0, 0, 1, 1, 0, 0,','    8, 11, 0, 2, 1, 0, 0, 0, /* audio/MIDI IAD; EP1 unchanged */\n    9, 4, 0, 0, 0, 1, 1, 0, 0,')
    sub("static const uint8_t STR2[] = {16, 3, 'F', 0, 'e', 0, 'l', 0, 'u', 0, 'c', 0, 'c', 0, 'a', 0};",
        "static const uint8_t STR2[] = {28, 3, 'M', 0, 'I', 0, 'K', 0, 'U', 0, '-', 0, 'F', 0, 'M', 0, ' ', 0, 'V', 0, 'I', 0, 'D', 0, 'E', 0, 'O', 0};")
    marker='#if FELUCCA_CDC\n    8, 0x0B, CDC_IF, 2, 2, 2, 1, 0,'
    video='''    /* UVC1.0 MJPEG160x120, nominal15fps; isolated EP4 IN512. */
    8,11,2,2,14,3,0,2,
    9,4,2,0,0,14,1,0,0,
    13,36,1,0,1,40,0,0,108,220,2,1,3,
    18,36,2,1,1,2,0,0,0,0,0,0,0,0,3,0,0,0,
    9,36,3,2,1,1,0,1,0,
    9,4,3,0,0,14,2,0,0,
    14,36,1,1,61,0,0x84,0,2,0,0,0,1,0,
    11,36,6,1,1,0,1,0,0,0,0,
    30,36,7,1,0,160,0,120,0,64,31,0,0,0,0,15,0,
      0,32,0,0,43,44,10,0,1,43,44,10,0,
    6,36,13,1,1,4,
    9,4,3,1,1,14,2,0,0,
    7,5,0x84,5,0,2,1,
'''
    sub(marker,video+marker)
    sub('static void ep1_config(void)','''#include "video_usb.inc"
static void ep1_config(void)''')
    sub('    fm1_usb_ep_enable(1u << 1);','    fm1_usb_ep_enable(1u << 1);\n    video_usb_config();')
    sub('        usb.e0_tx = 0;\n#if FELUCCA_CDC','        usb.e0_tx = 0;\n        video_usb_abort_control();\n#if FELUCCA_CDC',2)
    sub('    fm1_usb_rx_sync();\n#if FELUCCA_CDC','''    fm1_usb_rx_sync();
    if(video_class.control_pending){
        if(video_usb_control_data())goto ack;
        goto stall;
    }
    if(sie_rd(S_COUNT0)!=8u)goto stall;
#if FELUCCA_CDC''')
    sub('    switch ((uint32_t)s[0] << 8 | s[1]) {','''    if((s[0]==0x21u || s[0]==0xA1u) && s[4]>=2u){
        if(video_usb_control(s,wlength)>0)return;
        goto stall;
    }
    switch ((uint32_t)s[0] << 8 | s[1]) {''')
    sub('        usb.config = s[2];','''        if(s[3] || s[4] || s[5] || wlength)goto stall;
        video_usb_stop(1);
        usb.config = s[2];''')
    sub('    case 0x010B:                                        /* SET_INTERFACE: alt 0 (alt 1 for the audio stream) */','''    case 0x010B:                                        /* SET_INTERFACE */
        if(s[5] || wlength || !usb.config || s[4]>=USB_NIF)goto stall;
        if(s[4]==MUVC_VS_IF){if(video_usb_alt(wvalue))goto ack;goto stall;}''')
    sub('    case 0x810A:\n','''    case 0x810A:
        if(s[5] || s[4]>=USB_NIF || !usb.config)goto stall;
        if(s[4]==MUVC_VS_IF){
            static uint8_t alt;alt=video_usb.active;e0_send(&alt,1,wlength);return;
        }
''')
    sub('        if (wvalue != 0 || ep > last)','''        if(wvalue==0 && s[4]==0x84u)goto ack;
        if (wvalue != 0 || ep > last)''')
    sub('        usb.resets++;','        usb.resets++;\n        video_usb_stop(1);')
    sub('    if (usb.config)\n        ep1_tx();','    if (usb.config)\n        ep1_tx();\n    video_usb_service();')
    sub('    usb.timeouts = 0;\n    fm1_usb_reset();','    video_usb_stop(1);\n    usb.timeouts = 0;\n    fm1_usb_reset();')
    sub('    usb.detached = 1;','    video_usb_stop(1);\n    usb.detached = 1;')
    return "#if defined(MIKU_UVC) && MIKU_UVC && !defined(FELUCCA_LOADER)\n"+source+"\n#else /* exact pinned USB implementation for default and rescue loader */\n"+original+"\n#endif\n"

if __name__=='__main__':
    p=Path(sys.argv[1])/'firmware/src/usb.c'
    try:p.write_text(patch(p.read_text()))
    except ValueError as error:raise SystemExit(str(error))
