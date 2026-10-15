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
    pixels = indexed.tobytes()
    # Pack each row independently: high bit = repeated byte, otherwise literals.
    # Palette indices and RGB565 colours remain byte-for-byte identical.
    packed = bytearray()
    offsets = [0]
    for y in range(indexed.height):
        row = pixels[y*indexed.width:(y+1)*indexed.width]
        at = 0
        while at < len(row):
            run = 1
            while at+run < len(row) and row[at+run] == row[at] and run < 128:
                run += 1
            if run >= 3:
                packed.extend((0x80 | (run-1), row[at]))
                at += run
            else:
                start = at
                at += run
                while at < len(row) and at-start < 128:
                    if at+2 < len(row) and row[at] == row[at+1] == row[at+2]:
                        break
                    at += 1
                packed.append(at-start-1)
                packed.extend(row[start:at])
        offsets.append(len(packed))
    assert len(packed) < 65536
    # Independently decode every row before emitting the asset.
    decoded = bytearray()
    for a,b in zip(offsets, offsets[1:]):
        while a < b:
            control = packed[a]; a += 1
            count = (control & 127)+1
            if control & 128:
                decoded.extend([packed[a]]*count); a += 1
            else:
                decoded.extend(packed[a:a+count]); a += count
    assert bytes(decoded) == pixels
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
/* Decode one row into the existing canvas, with the LCD byte order applied. */
static void brand_splash_row(uint32_t y, uint16_t *out)
{
    uint32_t at=brand_splash_row_offsets[y], end=brand_splash_row_offsets[y+1], x=0;
    while(at<end) {
        uint32_t control=brand_splash_rle[at++], n=(control&127u)+1u;
        if(control&128u) {
            uint16_t c=brand_splash_palette[brand_splash_rle[at++]];
            c=(uint16_t)((c<<8)|(c>>8));
            while(n--) out[x++]=c;
        } else while(n--) {
            uint16_t c=brand_splash_palette[brand_splash_rle[at++]];
            out[x++]=(uint16_t)((c<<8)|(c>>8));
        }
    }
}
''')
    print(f'splash: {indexed.width}x{indexed.height}, {len(packed)+len(offsets)*2+512} bytes (lossless RLE, was {len(pixels)+512})')
