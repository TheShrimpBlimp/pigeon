"""Compile the same fixed-point decoder for meaningful host tests (inside build/)."""
import os
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SOURCES=['bit','fixed','frame','huffman','layer12','layer3','stream','synth','timer']


def objects(sanitize=False):
    out=ROOT/'build'/('ko-host-san' if sanitize else 'ko-host')
    out.mkdir(parents=True,exist_ok=True)
    flags=['-O1' if sanitize else '-O2','-g','-w','-fwrapv','-DFPM_64BIT','-DNDEBUG','-DHAVE_LIMITS_H',
           '-DSIZEOF_INT=4','-DSIZEOF_LONG=8','-DSIZEOF_LONG_LONG=8',
           '-Dmalloc=ko_mad_malloc','-Dcalloc=ko_mad_calloc','-Dfree=ko_mad_free']
    if sanitize:
        flags+=['-fsanitize=address,undefined','-fno-sanitize=shift,signed-integer-overflow',
                '-fno-sanitize-recover=undefined']
    objs=[]
    for name in SOURCES+['ko_mp3']:
        source=ROOT/'firmware'/('src/ko_mp3.c' if name=='ko_mp3' else f'vendor/libmad/{name}.c')
        obj=out/f'{name}.o'
        subprocess.run([os.environ.get('CC','cc'),*flags,'-c',str(source),'-o',str(obj)],check=True)
        objs.append(str(obj))
    return out,objs,flags
