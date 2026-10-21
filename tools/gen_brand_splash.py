#!/usr/bin/env python3
"""Encode the supplied splash logo as a compact indexed LCD asset."""
from pathlib import Path
import sys
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / 'assets/branding/splash.png'
target = Path(sys.argv[1])
target.parent.mkdir(parents=True, exist_ok=True)
if not source.exists():
    target.write_text('/* No local splash logo supplied. */\n#define BRAND_SPLASH_AVAILABLE 0\n')
    print('splash: no assets/branding/splash.png; using text splash')
else:
    with Image.open(source) as original:
        photo = ImageOps.exif_transpose(original).convert('RGB')
        photo.thumbnail((240, 240), Image.Resampling.LANCZOS)
        # No crop: preserve the entire supplied portrait.
        indexed = photo.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    palette = indexed.getpalette()
    colors = [((palette[i] >> 3) << 11) | ((palette[i+1] >> 2) << 5) | (palette[i+2] >> 3)
              for i in range(0, 768, 3)]
    # RGB565 can merge RGB888 palette entries; canonicalise only identical LCD
    # colours, preserving every displayed pixel while improving run compression.
    unique = list(dict.fromkeys(colors))
    remap = bytes(unique.index(c) for c in colors)
    pixels = indexed.tobytes().translate(remap)
    colors = unique
    # Lossless row codec: literals, repeated indices, or unchanged pixels above.
    # Dynamic programming chooses the smallest encoding; the LCD canvas retains
    # the previous row, so vertical copies need no additional RAM.
    packed = bytearray()
    offsets = [0]
    previous = None
    for y in range(indexed.height):
        row = pixels[y*indexed.width:(y+1)*indexed.width]
        cost = [0]*(len(row)+1)
        choice = [None]*len(row)
        for at in range(len(row)-1,-1,-1):
            options = [(n+1+cost[at+n], 0, n) for n in range(1,min(128,len(row)-at)+1)]
            for mode in (128,192):
                for n in range(1,min(64,len(row)-at)+1):
                    if mode==128 and row[at+n-1]!=row[at]: break
                    if mode==192 and (previous is None or row[at+n-1]!=previous[at+n-1]): break
                    options.append((1+(mode==128)+cost[at+n],mode,n))
            cost[at],mode,n = min(options)
            choice[at] = mode,n
        at=0
        while at<len(row):
            mode,n=choice[at]
            packed.append(mode+n-1)
            if mode==0: packed.extend(row[at:at+n])
            elif mode==128: packed.append(row[at])
            at+=n
        previous=row
        offsets.append(len(packed))
    assert len(packed) < 65536
    # Independently decode all rows and compare every palette index.
    decoded = bytearray()
    previous = bytearray(indexed.width)
    for a,b in zip(offsets,offsets[1:]):
        row=bytearray(previous);x=0
        while a<b:
            control=packed[a];a+=1
            if control>=192:
                x+=(control&63)+1
            elif control>=128:
                n=(control&63)+1;row[x:x+n]=bytes([packed[a]])*n;a+=1;x+=n
            else:
                n=control+1;row[x:x+n]=packed[a:a+n];a+=n;x+=n
        assert x==indexed.width
        decoded.extend(row);previous=row
    assert bytes(decoded)==pixels
    def array(name, kind, values):
        rows = [','.join(str(x) for x in values[i:i+24]) for i in range(0, len(values), 24)]
        return f'static const {kind} {name}[] = {{\n'+',\n'.join(rows)+'\n};\n'
    target.write_text('/* Generated from assets/branding/splash.png. */\n'
                      '#define BRAND_SPLASH_AVAILABLE 1\n'
                      f'#define BRAND_SPLASH_W {indexed.width}u\n'
                      f'#define BRAND_SPLASH_H {indexed.height}u\n'
                      + array('brand_splash_palette','uint16_t',colors)
                       + array('brand_splash_row_offsets','uint16_t',offsets)
                      + array('brand_splash_rle','uint8_t',packed)
                      + '''
/* Decode rows in order, retaining the prior row; apply LCD byte order. */
static void brand_splash_row(uint32_t y, uint16_t *out)
{
    uint32_t at=brand_splash_row_offsets[y], end=brand_splash_row_offsets[y+1], x=0;
    while(at<end) {
        uint32_t control=brand_splash_rle[at++], n=(control&63u)+1u;
        if(control>=192u) { x+=n; continue; }
        if(control&128u) {
            uint16_t c=brand_splash_palette[brand_splash_rle[at++]];
            c=(uint16_t)((c<<8)|(c>>8));
            while(n--) out[x++]=c;
        } else {
            n=control+1u;
            while(n--) {
                uint16_t c=brand_splash_palette[brand_splash_rle[at++]];
                out[x++]=(uint16_t)((c<<8)|(c>>8));
            }
        }
    }
}
''')
    print(f'splash: {indexed.width}x{indexed.height}, {len(packed)+len(offsets)*2+len(colors)*2} bytes (lossless row delta, was {len(pixels)+512})')
