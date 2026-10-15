"""One-minute song, flexible kits, decoder comparison, real upload/NOR/DSP tests."""
import math
import os
import struct
import subprocess
import sys
import wave
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import ko_bank as bank
import ko_codec_host

out=ROOT/'build/ko-fixtures';out.mkdir(parents=True,exist_ok=True)


def wav(path,n,rate=22050,freq=220):
    with wave.open(str(path),'wb') as w:
        w.setparams((1,2,rate,0,'NONE','not compressed'))
        for start in range(0,n,2205):
            vals=[int(10000*math.sin(2*math.pi*freq*i/rate)+5000*math.sin(2*math.pi*3120*i/rate)+
                      (12000*math.exp(-(i%(rate//2))/150) if i%(rate//2)<1000 else 0))
                  for i in range(start,min(start+2205,n))]
            w.writeframes(struct.pack(f'<{len(vals)}h',*vals))


song=out/'minute.wav';wav(song,60*22050)
hits=[];short=[]
for i in range(16):
    h=out/f'pad{i}.wav';s=out/f'short{i}.wav';wav(h,13230);wav(s,2205);hits.append(h);short.append(s)
mp3=bank.build(song,hits);ima=bank.build(song,short,'adpcm')
f=bank.validate(mp3);assert f[6]==60*22050 and f[10]>bank.sio.SLOT_MAX_DATA and f[11]==16
assert bank.validate(ima)[6]==60*11025
for name,image in [('minute-mp3.kob',mp3),('minute-ima.kob',ima)]:
    (out/name).write_bytes(image)
    damaged=bytearray(image);damaged[-1]^=1
    try:bank.validate(damaged)
    except ValueError:pass
    else:raise AssertionError('corrupt payload accepted')
assert sum(size for _,size in bank.EXTENTS)==bank.SIZE
for base,size in bank.EXTENTS:
    assert not (base<0xA0000 and base+size>0x97000)
    assert (base>=0x78000 and base+size<=0xFC000) or (base==0x60000 and base+size==0x76000)
    assert not (base<0x78000 and base+size>0x76000), "boot marker sectors overlap the bank"
second_song=out/'second-song.wav';wav(second_song,8*22050,freq=880)
dual=bank.combine(mp3,bank.build(second_song,(),bitrate=32))
(out/'dual.kob').write_bytes(dual)
assert bank.validate(dual)[1]==3
metadata=bytes((i*17)%256 for i in range(4096))
assert bank.combine(mp3+metadata,bank.build(short[0]))[-4096:]==metadata
try: bank.combine(mp3,mp3)
except ValueError: pass
else: raise AssertionError('second song kit accepted')
class Fake:
    def __init__(self,supported=True,fail_commit=False):self.calls=[];self.supported=supported;self.fail_commit=fail_commit
    def req(self,cmd,args,timeout=5):
        self.calls.append((cmd,args))
        if cmd==90:return [0x4B,2]+bank.u21(bank.SIZE)+bank.u21(bank.HEAD)+[0,3] if self.supported else []
        return [3] if cmd==93 and self.fail_commit else [0]
link=Fake()
try: bank.upload(link,dual)
except ValueError: pass
else: raise AssertionError('two-song upload accepted by old firmware')
assert len(link.calls)==1
link=Fake();bank.upload(link,mp3)
assert link.calls[0][0]==90 and link.calls[1][0]==91 and link.calls[-1][0]==93
for link in (Fake(False),Fake(True,True)):
    try:bank.upload(link,mp3)
    except ValueError:pass
    else:raise AssertionError('unsupported firmware/failed commit accepted')
    if not link.supported:assert len(link.calls)==1
for sanitize in (False,True):
    host,objs,flags=ko_codec_host.objects(sanitize)
    linkflags=['-O1' if sanitize else '-O2','-g','-w','-Ibuild/gen','-Ifirmware/src']
    if os.environ.get('FELUCCA_SAMPLER_ONLY')=='1':
        linkflags+=['-DFELUCCA_SAMPLER_ONLY=1']
    if os.environ.get('FELUCCA_KO_DUAL')=='1': linkflags+=['-DFELUCCA_KO_DUAL=1']
    if sanitize:linkflags+=['-fsanitize=address,undefined','-fno-sanitize=shift,signed-integer-overflow,pointer-overflow',
                            '-fno-sanitize-recover=undefined']
    for test,args in [('ko_mp3_test',[str(out/'minute-mp3.kob')]),
                      ('ko_storage_test',[str(out/'minute-mp3.kob'),str(out/'minute-ima.kob')]+([str(out/'dual.kob')] if os.environ.get('FELUCCA_KO_DUAL')=='1' else [])),
                      *([('pigeon_expanded_bank_test',[])] if os.environ.get('FELUCCA_SAMPLER_ONLY')=='1' and os.environ.get('FELUCCA_KO_DUAL')=='1' else [])]:
        exe=host/test
        subprocess.run([os.environ.get('CC','cc'),*linkflags,f'tests/{test}.c',*objs,'-lm','-o',str(exe)],check=True,cwd=ROOT)
        subprocess.run([str(exe),*args],check=True,cwd=ROOT,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('KO storage/codec host tests PASSED')
