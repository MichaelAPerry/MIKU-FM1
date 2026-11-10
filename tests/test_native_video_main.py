#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise actual native incremental video producer with bounded transport ownership."""
import pathlib
import subprocess
import sys
import tempfile
root, cc=pathlib.Path(sys.argv[1]),sys.argv[2]
s=(root/'native/dinghy.c').read_text()
a=s.index('static MjEncoder video_encoder');b=s.index('\n}\n#endif',a)+2
producer=s[a:b]
a=s.index('static void app_lcd_idle(void){');b=s.index('{',a)+1;depth=1
while depth:
    depth+=(s[b]=='{')-(s[b]=='}');b+=1
idle=s[a:b]

harness=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "firmware/usb/jpeg/mjpeg.h"
#include "firmware/usb/jpeg/performance_frame.h"
#include "firmware/usb/video/uvc.h"
#include "firmware/ui/stagefx.h"
#include "firmware/song/song.h"
static uint32_t active,busy,ticks,published,generation;
#if MIKU_VIDEO_BENCH
static void video_copy_into_hold(const uint8_t *p,uint32_t n){assert(n>=4&&n<=8192&&p[0]==255&&p[1]==216&&p[n-2]==255&&p[n-1]==217);}
#endif
static uint32_t fm1_ticks(void){ticks+=100;return ticks;}
static uint32_t video_usb_generation(void){return generation;}
static uint32_t video_usb_active(void){return active;}
static uint32_t video_usb_busy(void){return busy;}
static uint32_t video_usb_publish(const uint8_t*p,uint32_t n,uint32_t epoch){
    assert(epoch==generation);
    assert(active&&!busy&&p&&n<=8192&&p[0]==255&&p[1]==216&&p[n-2]==255&&p[n-1]==217);
    busy=1;published=n;return 1;
}
PRODUCER
static uint32_t app_idle_ready,page,trk,polls,midi_polls;
static PerfState performance;
#define PG_DANCE 1u
#define PG_LIBRARY 8u
static void app_lcd_idle(void);
static void platform_poll(void){polls++;app_lcd_idle();} /* intentional reentry must be blocked */
static void midi_service(uint32_t t){assert(t<4);midi_polls++;}
IDLE
static uint8_t prior[8192],want[8192];static MjEncoder reference;
int main(void){
    app_lcd_idle();assert(!polls&&!midi_polls&&!video_encoded); /* pre-platform init */
    PerfState scene;perf_init(&scene);scene.sprite=6;scene.background=1;
    for(unsigned i=0;i<100;i++)video_update(&scene);
    assert(!video_encoding&&!video_encoded&&!published);
    active=1;
    for(unsigned i=0;i<79;i++){video_update(&scene);assert(!published);}
    video_update(&scene);assert(video_encoded==1&&busy&&published);
    assert(video_cache.decoded_rows<=71u);
    memcpy(prior,video_jpeg,published);uint32_t n=published;
    assert(mj_begin(&reference,160,120,65,want,sizeof want,mj_perf_pixel,&scene));
    assert(mj_step(&reference,80)==1&&mj_size(&reference)==n&&!memcmp(prior,want,n));
    scene.sprite=7;
    for(unsigned i=0;i<100;i++)video_update(&scene);
    assert(video_encoded==1&&!memcmp(prior,video_jpeg,n)); /* DMA-owned frame immutable */
    busy=0;published=0;
    for(unsigned i=0;i<10;i++)video_update(&scene);
    assert(video_encoding&&!published);
    active=0;video_update(&scene);assert(!video_encoding); /* disconnect aborts unfinished frame */
    active=1;generation++;
    for(unsigned i=0;i<80;i++)video_update(&scene);
    assert(busy&&video_encoded==2&&published);
    assert(mj_begin(&reference,160,120,65,want,sizeof want,mj_perf_pixel,&scene));
    assert(mj_step(&reference,80)==1&&mj_size(&reference)==published&&!memcmp(video_jpeg,want,published));
    busy=0;published=0;for(unsigned i=0;i<10;i++)video_update(&scene);
    assert(video_encoding);generation++;scene.sprite=2;
    for(unsigned i=0;i<79;i++){video_update(&scene);assert(!published);}
    video_update(&scene);assert(published&&video_encoded==3);
    assert(video_min_bytes&&video_max_bytes>=video_min_bytes&&video_total_bytes>=published);
    /* Entire JPEG can progress during LCD DMA waits, without main-loop passes. */
    busy=0;published=0;performance=scene;app_idle_ready=1;
    for(unsigned i=0;i<80;i++)app_lcd_idle();
    assert(busy&&published&&video_encoded==4&&polls==80&&midi_polls==80);
    puts("native video main: incremental budget, immutable USB ownership, disconnect/restart, exact JPEG");
}
'''.replace('PRODUCER',producer).replace('IDLE',idle)
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'main.c').write_text(harness)
    subprocess.run([cc,'-std=c11','-O2','-Wall','-Wextra','-DMIKU_UVC=1','-DMIKU_COMPACT_SPRITES=1','-I'+str(root),str(p/'main.c'),
        str(root/'firmware/usb/jpeg/mjpeg.c'),str(root/'firmware/usb/jpeg/performance_frame.c'),
        str(root/'firmware/ui/performance.c'),str(root/'firmware/ui/stagefx.c'),str(root/'firmware/ui/sprite_codec.c'),'-o',str(p/'main')],check=True)
    subprocess.run([str(p/'main')],check=True)
    bench=harness[:harness.index('int main(void)')]+r'''
int main(void){
    PerfState scene;perf_init(&scene);
    for(unsigned i=0;i<100;i++)video_update(&scene);
    assert(!video_encoded);
    scene.playing=1;scene.mouth=0;scene.sprite=4;scene.effect=STFX_FIREWORKS;scene.now_ms=479;
    for(unsigned i=0;i<10;i++)video_update(&scene);
    assert(!video_encoded&&video_encoding);scene.now_ms=480;
    for(unsigned i=0;i<240;i++)video_update(&scene);
    assert(video_encoded==3&&!video_failed&&video_last_bytes>0&&!busy);
    uint32_t n=video_last_bytes;memcpy(prior,video_jpeg,n);
    for(unsigned i=0;i<100;i++)video_update(&scene);
    assert(video_encoded==3&&!memcmp(prior,video_jpeg,n));
    puts("native video benchmark: playback gate and three frozen frames OK");
}
'''
    (p/'main.c').write_text(bench)
    subprocess.run([cc,'-std=c11','-O2','-Wall','-Wextra','-Wno-unused-function','-Wno-unused-variable',
        '-DMIKU_VIDEO_BENCH=1','-DMIKU_UVC=1','-DMIKU_COMPACT_SPRITES=1','-I'+str(root),str(p/'main.c'),
        str(root/'firmware/usb/jpeg/mjpeg.c'),str(root/'firmware/usb/jpeg/performance_frame.c'),
        str(root/'firmware/ui/performance.c'),str(root/'firmware/ui/stagefx.c'),str(root/'firmware/ui/sprite_codec.c'),'-o',str(p/'bench')],check=True)
    subprocess.run([str(p/'bench')],check=True)

    # An uncapped run must cover the whole interval and freeze a complete JPEG
    # even if STOP/cutoff arrives partway through its final frame.
    continuous=harness[:harness.index('int main(void)')]+r'''
int main(void){
    PerfState scene;perf_init(&scene);scene.playing=1;scene.now_ms=1000;
    for(unsigned i=0;i<80*200;i++)video_update(&scene);
    assert(video_encoded==200&&!video_failed);
    scene.now_ms=1999;
    for(unsigned i=0;i<10;i++)video_update(&scene);
    scene.now_ms=2000;scene.playing=0;
    for(unsigned i=0;i<100;i++)video_update(&scene);
    assert(video_encoded==201&&!video_encoding&&!video_failed);
    uint32_t n=video_last_bytes;memcpy(prior,video_jpeg,n);
    for(unsigned i=0;i<100;i++)video_update(&scene);
    assert(video_encoded==201&&!memcmp(prior,video_jpeg,n));
    puts("native video benchmark: whole-interval encoding, final JPEG frozen across cutoff/STOP OK");
}
'''
    (p/'main.c').write_text(continuous)
    subprocess.run([cc,'-std=c11','-O2','-Wall','-Wextra','-Wno-unused-function','-Wno-unused-variable',
        '-DMIKU_VIDEO_BENCH=1','-DMIKU_UVC=1','-DMIKU_COMPACT_SPRITES=1',
        '-DMIKU_VIDEO_BENCH_FRAMES=0','-DMIKU_VIDEO_BENCH_END_MS=2000','-I'+str(root),str(p/'main.c'),
        str(root/'firmware/usb/jpeg/mjpeg.c'),str(root/'firmware/usb/jpeg/performance_frame.c'),
        str(root/'firmware/ui/performance.c'),str(root/'firmware/ui/stagefx.c'),str(root/'firmware/ui/sprite_codec.c'),'-o',str(p/'continuous')],check=True)
    subprocess.run([str(p/'continuous')],check=True)
