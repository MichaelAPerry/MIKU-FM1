#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Export computer-view RGBA sprites from the exact packed firmware rows."""
from pathlib import Path
import json,re,io,argparse
from PIL import Image
import sprite_compact as codec
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'companion/media'
def atlas(text,prefix,pal,w,h,count):
    ids=codec.parse(text,prefix+'_ROWS');offsets=codec.parse(text,prefix+'_OFFSETS');data=codec.parse(text,prefix+'_DATA');palette=codec.parse(text,pal)
    packed=(ids,offsets,data,[[0]*w]*(len(offsets)-1));im=Image.new('RGBA',(w*count,h))
    for f in range(count):
        tile=[]
        for y in range(h):
            for i in codec.decode_python(packed,ids[f*h+y]):
                p=palette[i];r=p>>11;g=(p>>5)&63;b=p&31
                tile.append(((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2),255 if i else 0))
        cell=Image.new('RGBA',(w,h));cell.putdata(tile);im.paste(cell,(f*w,0))
    result=io.BytesIO();im.save(result,format='PNG');return result.getvalue()
def generate():
    original=(ROOT/'firmware/ui/singer_compact.h').read_text();wardrobe=(ROOT/'firmware/ui/wardrobe_sprites.h').read_text()
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
            files[e['id']+'-'+kind.lower()+'.png']=atlas(text,stem+'_'+kind,pal,w,h,count)
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
