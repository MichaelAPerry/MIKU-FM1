#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Export computer-view RGBA sprites from the exact packed firmware rows."""
from pathlib import Path
import json,re,io,argparse
from PIL import Image
import sprite_compact as codec
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'companion/media'


def atlas_frames(text, prefix, w, h, count):
    ids, offsets, data = (codec.parse(text, prefix + suffix)
                          for suffix in ('_ROWS', '_OFFSETS', '_DATA'))
    packed = (ids, offsets, data, [[0] * w] * (len(offsets) - 1))
    for f in range(count):
        yield bytes(i for row in ids[f*h:(f+1)*h] for i in codec.decode_python(packed, row))


def rgba(palette):
    colors = []
    for i, p in enumerate(palette):
        r, g, b = p >> 11, (p >> 5) & 63, p & 31
        colors.append(((r << 3) | (r >> 2), (g << 2) | (g >> 4),
                       (b << 3) | (b >> 2), 255 if i else 0))
    return colors


def tile(indices, colors, size):
    image = Image.new('RGBA', size)
    image.putdata([colors[i] for i in indices])
    return image


class JapaneseMouths:
    def __init__(self, text):
        def dimension(name):
            return int(re.search(r'^#define MOUTH_' + name + r' (\d+)u$', text, re.M)[1])
        self.w, self.h = dimension('W'), dimension('H')
        frames = list(atlas_frames(text, 'MOUTH_ATLAS', self.w, self.h, dimension('COUNT')))
        order = re.search(r'FVV_\* order: ([A-Z ]+)\.', text)[1].split()
        colors = rgba(codec.parse(text, 'MOUTH_PAL'))
        self.overlays = [tile(frames[order.index(name)], colors, (self.w, self.h))
                         for name in ('AH', 'EE', 'UH', 'EH', 'OH', 'MBP')]
        self.anchors = codec.parse(text, 'MOUTH_ANCHORS')
        self.offsets = codec.parse(text, 'MOUTH_CLEAN_OFFSETS')
        self.cleanup = codec.parse(text, 'MOUTH_CLEAN_PIXELS')

    def faces(self, neutral, colors, outfit):
        x, y = self.anchors[2*outfit:2*outfit+2]
        clean = neutral.copy()
        for k in range(self.offsets[outfit], self.offsets[outfit+1]):
            position, index = self.cleanup[2*k:2*k+2]
            dy, dx = divmod(position, self.w)
            clean.putpixel((x + dx, y + dy), colors[index])
        for overlay in self.overlays:
            face = clean.copy()
            face.alpha_composite(overlay, (x, y))
            yield face


def atlas(text, prefix, pal, w, h, count, mouths=None, outfit=0):
    colors = rgba(codec.parse(text, pal))
    image = Image.new('RGBA', (w * count, h * (2 if mouths else 1)))
    for f, frame in enumerate(atlas_frames(text, prefix, w, h, count)):
        cell = tile(frame, colors, (w, h))
        image.paste(cell, (f*w, 0))
        if f == 0 and mouths:
            # Second row follows the review trace's A I U E O CLOSED order.
            for m, face in enumerate(mouths.faces(cell, colors, outfit)):
                image.paste(face, (m*w, h))
    result = io.BytesIO()
    image.save(result, format='PNG')
    return result.getvalue()


def generate():
    original=(ROOT/'firmware/ui/singer_compact.h').read_text();wardrobe=(ROOT/'firmware/ui/wardrobe_sprites.h').read_text()
    mouths=JapaneseMouths((ROOT/'firmware/ui/mouth_sprites.h').read_text())
    entries=[{'id':'original','name':'ORIGINAL'}]+json.loads((ROOT/'assets/singer/outfits/manifest.json').read_text())['outfits']
    visual=(ROOT/'firmware/ui/performance.c').read_text()
    def style_table(name):
        body=re.search(r'\b'+name+r'\[[^\]]+\]\[3\]\s*=\s*\{(.*?)\};',visual,re.S).group(1)
        return [int(n,0) for n in re.findall(r'0x[0-9a-fA-F]+|\b\d+\b',body)]
    styles=style_table('settings');colours=style_table('style_colours')
    files={};manifest=[]
    for i,e in enumerate(entries):
        stem='SC' if i==0 else 'WD_'+re.sub('[^A-Z0-9]','_',e['id'].upper());text=original if i==0 else wardrobe
        for kind,w,h,count in [('FACE',90,90,12),('STAGE',61,71,8)]:
            pal=stem+('_'+kind+'_PAL' if i==0 else '_'+('FPAL' if kind=='FACE' else 'SPAL'))
            files[e['id']+'-'+kind.lower()+'.png']=atlas(text,stem+'_'+kind,pal,w,h,count,mouths if kind=='FACE' else None,i)
        manifest.append({'id':e['id'],'name':e['name'],'faces':e['id']+'-face.png','stage':e['id']+'-stage.png','scene':{'background':styles[i*3],'frame':styles[i*3+1],'effect':styles[i*3+2],'intensity':80,'colours':colours[i*3:i*3+3]}})
    fx=(ROOT/'firmware/ui/stagefx_sprites.h').read_text();files['effects.png']=atlas(fx,'FX','FX_PAL',32,32,16)
    files['wardrobe.json']=(json.dumps(manifest,indent=2)+'\n').encode();return files
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');a=ap.parse_args();files=generate();OUT.mkdir(parents=True,exist_ok=True)
    for name,data in files.items():
        path=OUT/name
        if a.check:
            if name.endswith('.png'):
                with Image.open(path) as actual,Image.open(io.BytesIO(data)) as expected:
                    assert actual.size==expected.size and actual.convert('RGBA').tobytes()==expected.convert('RGBA').tobytes(),name+' pixels are stale'
            else:assert path.read_bytes()==data,name+' is stale'
        else:path.write_bytes(data)
    print(f'companion: {len(files)} files from actual firmware atlases')
