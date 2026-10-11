#!/usr/bin/env python3
"""Encode the supplied splash photograph as a compact indexed LCD asset."""
from pathlib import Path
import sys
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / 'assets/branding/splash.png'
target = Path(sys.argv[1])
target.parent.mkdir(parents=True, exist_ok=True)
if not source.exists():
    target.write_text('/* No local splash photo supplied. */\n#define BRAND_SPLASH_AVAILABLE 0\n')
    print('splash: no assets/branding/splash.png; using text splash')
else:
    with Image.open(source) as original:
        photo = ImageOps.exif_transpose(original).convert('RGB')
        photo.thumbnail((208, 192), Image.Resampling.LANCZOS)
        # No crop: preserve the entire supplied portrait.
        indexed = photo.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    palette = indexed.getpalette()
    colors = [((palette[i] >> 3) << 11) | ((palette[i+1] >> 2) << 5) | (palette[i+2] >> 3)
              for i in range(0, 768, 3)]
    pixels = indexed.tobytes()
    def array(name, kind, values):
        rows = [','.join(str(x) for x in values[i:i+24]) for i in range(0, len(values), 24)]
        return f'static const {kind} {name}[] = {{\n'+',\n'.join(rows)+'\n};\n'
    target.write_text('/* Generated from assets/branding/splash.png. */\n'
                      '#define BRAND_SPLASH_AVAILABLE 1\n'
                      f'#define BRAND_SPLASH_W {indexed.width}u\n'
                      f'#define BRAND_SPLASH_H {indexed.height}u\n'
                      + array('brand_splash_palette','uint16_t',colors)
                      + array('brand_splash_pixels','uint8_t',pixels))
    print(f'splash: {indexed.width}x{indexed.height}, {len(pixels)+512} bytes')
