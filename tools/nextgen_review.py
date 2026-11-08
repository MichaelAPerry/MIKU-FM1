#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a local listening/visual review index from generated actual-firmware evidence."""
from pathlib import Path
from html import escape
import json
import wave

root = Path(__file__).resolve().parents[1]
out = root/'build/evidence/review.html'
out.parent.mkdir(parents=True, exist_ok=True)
parts = ['<!doctype html><meta charset="utf-8"><title>MIKU next-generation review</title>',
         '<style>body{background:#0c1428;color:#e9f7ff;font:16px system-ui;max-width:1000px;margin:40px auto}a{color:#61ded2}audio{display:block;width:90%;margin:8px 0 24px}img{image-rendering:pixelated;margin:12px}h2{margin-top:40px}</style>',
         '<h1>MIKU-FM1 development review</h1><p>Actual firmware engine renders and emulator screenshots. Musical and visual choices await owner review. No live USB or hardware validation is claimed. A/B files are not loudness matched.</p>']
measurements = []
def audio(label, path):
    full=root/path
    if not full.exists(): return
    relative='../nextgen/'+full.name if full.parent.name=='nextgen' else full.name
    if full.parent != out.parent and full.parent.name!='nextgen':
        relative=full.relative_to(out.parent).as_posix()
    with wave.open(str(full),'rb') as w:
        seconds=w.getnframes()/w.getframerate()
    parts.append(f'<p>{escape(label)} ({seconds:.2f}s)</p><audio controls preload="none" src="{escape(relative)}"></audio>')
    measurements.append({'label':label,'path':path,'seconds':seconds})
parts.append('<h2>Live performance</h2>')
audio('ANIMAL before','build/nextgen/animal-before.wav')
audio('ANIMAL after: default 140–220ms','build/nextgen/animal-after.wav')
parts.append('<h2>Voice and lyrics</h2>')
audio('Original vowels','build/evidence/voice-before.wav')
audio('Opt-in bounded compensation','build/evidence/voice-after.wav')
audio('Word/phrase compiler through real formant voice','build/evidence/lyrics-phrase.wav')
parts.append('<h2>Backing band: same score</h2>')
for name,label in [('demo','Original'),('subdrop','Digital pluck / sub drop / punch'),('hardacid','Glass pad / hard acid / digital')]:
    audio(label,f'build/nextgen/band_{name}.wav')
parts.append('<h2>GEN candidates: two contrasting settings each</h2><p>Seed1 energy45/complexity30; seed42 energy90/complexity85. Editable MKS2 projects accompany each WAV.</p>')
for name in ['hyperpop','future-bass','jersey-club','digicore','idol-pop']:
    for seed in [1,42]: audio(f'{name}, seed {seed}',f'build/nextgen/{name}-{seed}.wav')
parts.append('<h2>Pedal DSP: recorded-input processing</h2>')
audio('Input','build/evidence/pedal-input.wav');audio('Processed','build/evidence/pedal-processed.wav')
parts.append('<h2>Visual review concepts</h2>')
for name in ['stage-original','stage-grid','stage-horizon','companion']:
    if (out.parent/(name+'.png')).exists(): parts.append(f'<p>{name}</p><img width="240" src="{name}.png" alt="{name}">')
for image in ['final/smoke/work/lcd_2500.png','final/dance/work/lcd_1200.png','final/mixer/work/lcd_2850.png']:
    if (out.parent/image).exists(): parts.append(f'<p>Actual firmware emulator: {image}</p><img width="240" src="{image}" alt="emulator">')
parts.append('<p>Hatsune Miku fan art under the Piapro Character License. © Crypton Future Media, INC. www.piapro.net. Unofficial, free and non-commercial. See LICENSING.md.</p>')
out.write_text('\n'.join(parts))
(out.parent/'review-audio.json').write_text(json.dumps(measurements,indent=2)+'\n')
print(out)
