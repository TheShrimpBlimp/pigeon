"""Compare the compiled row decoder with every original quantized RGB565 pixel."""
from pathlib import Path
import os, subprocess, tempfile
from PIL import Image, ImageOps
root=Path(__file__).resolve().parents[1]
with Image.open(root/'assets/branding/splash.png') as im:
    photo=ImageOps.exif_transpose(im).convert('RGB')
    photo.thumbnail((240,240),Image.Resampling.LANCZOS)
    image=photo.quantize(colors=256,method=Image.Quantize.MEDIANCUT)
palette=image.getpalette()
colors=[((palette[i]>>3)<<11)|((palette[i+1]>>2)<<5)|(palette[i+2]>>3) for i in range(0,768,3)]
expected=b''.join(colors[i].to_bytes(2,'big') for i in image.tobytes())
with tempfile.TemporaryDirectory() as tmp:
    source=Path(tmp)/'decode.c';exe=Path(tmp)/'decode'
    source.write_text('#include <stdint.h>\n#include <stdio.h>\n#include "brand_splash.h"\n'
        'int main(void){uint16_t row[BRAND_SPLASH_W]={0};for(uint32_t y=0;y<BRAND_SPLASH_H;y++){'
        'brand_splash_row(y,row);for(uint32_t x=0;x<BRAND_SPLASH_W;x++){putchar(row[x]&255);putchar(row[x]>>8);}}}\n')
    subprocess.run([os.environ.get('CC','cc'),'-O1','-fsanitize=address,undefined','-I'+str(root/'build/gen'),str(source),'-o',str(exe)],check=True)
    actual=subprocess.check_output([str(exe)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
assert actual==expected, 'Splash pixels or LCD byte order changed'
print(f'Splash: compiled decoder preserves all {image.width*image.height} RGB565 pixels; sanitizers PASS')
