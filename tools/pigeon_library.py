#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""PIGEON local sample library and complete archives. No device writes except upload/restore."""
import argparse, base64, json, shutil, struct, subprocess, tempfile, wave, zlib
from pathlib import Path
import ko_bank as bank
from fm1_sample_upload import Link, pack7
ROOT=Path(__file__).resolve().parents[1]

def component(image,slot):
    f=bank.validate(image)
    if slot==2 and f[1]!=3:return None
    off=0 if slot==1 else bank.HEADER
    h=list(struct.unpack_from('<20I',image,off))
    return dict(h=h,tail=image[off+80:off+bank.HEADER-4],song=image[h[7]:h[7]+h[8]],wave=image[h[14]:h[14]+h[15]*2])

def repack(original,replacement=None,slot=1,kit=None):
    """Relocate encoded blocks without decoding/re-encoding the untouched song or kit."""
    f=bank.validate(original);songs=[component(original,1),component(original,2)]
    if replacement is not None:songs[slot-1]=component(replacement,1)
    src=original if kit is None else kit;g=bank.validate(src)
    kitdata=src[g[9]:g[9]+g[10]];zones=src[208:656]
    payload=bytearray(kitdata);payload+=bytes(-len(payload)%256)
    headers=[]
    for i,s in enumerate(songs):
        if s is None:continue
        h=s['h'].copy();h[1]=3 if songs[1] is not None and not i else 2
        h[9]=bank.HEAD if not i else bank.HEAD+len(payload);h[10]=len(kitdata) if not i else 0;h[11]=g[11] if not i else 0
        h[7]=bank.HEAD+len(payload);payload+=s['song'];h[14]=bank.HEAD+len(payload);payload+=s['wave']
        h[17:20]=[bank.HEADER,0,0] if h[1]==3 else [0,0,0]
        tail=bytearray(s['tail']);tail[128:576]=zones if not i else bytes(448)
        headers.append((h,tail))
        if i==0 and songs[1] is not None:payload+=bytes(-(bank.HEAD+len(payload))%256)
    used=bank.HEAD+len(payload)
    if used>bank.SIZE:raise ValueError(f'Need {used-bank.HEAD:,} payload bytes; capacity {bank.SIZE-bank.HEAD:,}. Nothing written.')
    crc=zlib.crc32(payload);head=bytearray()
    for h,tail in headers:
        h[2]=used;h[3]=crc;raw=struct.pack('<20I',*h)+tail;head+=raw+struct.pack('<I',zlib.crc32(raw))
    metadata=original[f[2]:]
    image=bytes(head).ljust(bank.HEAD,b'\xff')+payload+metadata
    bank.validate(image);return image

def ffmpeg():
    found=shutil.which('ffmpeg');local=ROOT/'build/audio-python/imageio_ffmpeg/binaries/ffmpeg-macos-aarch64-v7.1'
    if found:return found
    if local.exists():return str(local)
    raise ValueError('Install ffmpeg to import non-WAV audio or render previews')

def audio_wav(source,dest,start=0,seconds=None):
    args=[ffmpeg(),'-v','error','-y','-ss',str(start),'-i',str(source)]
    if seconds is not None:args+=['-t',str(seconds)]
    subprocess.run(args+['-ac','1','-ar','22050','-c:a','pcm_s16le',str(dest)],check=True)

def render(image,dest,slot):
    s=component(image,slot)
    if s is None:raise ValueError('Song slot is empty')
    h=s['h'];data=s['song']
    with tempfile.TemporaryDirectory(dir=ROOT/'build') as tmp:
        tmp=Path(tmp)
        if h[4]==2:
            raw=tmp/'encoded.mp3';raw.write_bytes(data[(h[12]+1)*4:])
            # Raw stream has no Xing gapless tags: trim firmware's exact encoder skip.
            subprocess.run([ffmpeg(),'-v','error','-y','-i',str(raw),'-af',f'atrim=start_sample={h[13]}:end_sample={h[13]+h[6]},asetpts=PTS-STARTPTS','-ac','1','-ar',str(h[5]),'-c:a','pcm_s16le',str(dest)],check=True)
        else:
            pred=idx=0;values=[]
            for i in range(h[6]):
                nib=(data[i//2]>>(4*(i&1)))&15;step=bank.sio.IMA_STEP[idx]
                diff=(step>>3)+(step if nib&4 else 0)+(step>>1 if nib&2 else 0)+(step>>2 if nib&1 else 0)
                pred=max(-32768,min(32767,pred+(-diff if nib&8 else diff)));idx=max(0,min(88,idx+bank.sio.IMA_IDX[nib&7]));values.append(pred)
            with wave.open(str(dest),'wb') as w:w.setparams((1,2,h[5],0,'NONE','not compressed'));w.writeframes(struct.pack('<%dh'%len(values),*values))

def u32(v):return [(v>>(7*i))&(15 if i==4 else 127) for i in range(5)]
def r32(a):
    if len(a)!=5 or any(v<0 or v>127 for v in a) or a[4]>15:raise ValueError('Malformed protocol number')
    return sum(v<<(7*i) for i,v in enumerate(a))
def capture(link):
    a=link.req(65,[],5)
    if a[:2]!=[1,0] or len(a)!=3+11*a[2]:raise ValueError('Backup manifest failed; stop playback first')
    objects=[]
    for i in range(a[2]):
        p=3+11*i;ident=a[p];size=r32(a[p+1:p+6]);crc=r32(a[p+6:p+11])
        if ident>=32:continue # combined KO bank replaces legacy sample objects
        if ident>9 or size>3840:raise ValueError('Unexpected backup object')
        data=bytearray()
        for off in range(0,size,256):
            n=min(256,size-off);r=link.req(66,[ident]+u32(off)+[n&127,n>>7])
            if r[:2]!=[ident,0] or r32(r[2:7])!=off or r[7:9]!=[n&127,n>>7]:raise ValueError('Backup read failed')
            block=bank.unpack7(r[9:]);assert len(block)==n;data+=block
        if zlib.crc32(data)!=crc:raise ValueError('Backup changed during capture')
        objects.append(dict(id=ident,size=size,crc=crc,data=base64.b64encode(data).decode()))
    image=bank.export(link)
    return dict(format='pigeon-archive',version=1,objects=objects,bank=base64.b64encode(image).decode(),bank_crc=zlib.crc32(image))

def validate_archive(a):
    if a.get('format')!='pigeon-archive' or a.get('version')!=1 or not isinstance(a.get('objects'),list):raise ValueError('Invalid PIGEON archive')
    if [o['id'] for o in a['objects']]!=list(range(10)):raise ValueError('Incomplete archive')
    image=base64.b64decode(a['bank'],validate=True);bank.validate(image)
    if zlib.crc32(image)!=a['bank_crc']:raise ValueError('Sample bank CRC mismatch')
    for o in a['objects']:
        data=base64.b64decode(o['data'],validate=True)
        if len(data)!=o['size'] or len(data)>3840 or zlib.crc32(data)!=o['crc']:raise ValueError('Project archive CRC mismatch')
    if not a['objects'][0]['size'] or not a['objects'][1]['size']:raise ValueError('Current project/settings missing')
    return image

def restore(link,a):
    image=validate_archive(a) # validate every object before writes
    bank.upload(link,image)
    for o in a['objects'][2:]+[a['objects'][1],a['objects'][0]]:
        def put(args):
            r=link.req(67,args,5)
            if r!=[args[0],o['id'],0]:raise ValueError(f'Restore object {o["id"]} failed: {r}')
        put([0,o['id']]+u32(o['size'])+u32(o['crc']))
        try:
            data=base64.b64decode(o['data'])
            for off in range(0,len(data),256):put([1,o['id']]+u32(off)+pack7(data[off:off+256]))
            put([2,o['id']])
        except Exception:
            link.req(67,[3,o['id']]);raise

def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='cmd',required=True)
    r=sub.add_parser('replace-song');r.add_argument('out');r.add_argument('bank');r.add_argument('audio');r.add_argument('--slot',type=int,choices=(1,2),required=True);r.add_argument('--start',type=float,default=0);r.add_argument('--seconds',type=float);r.add_argument('--bitrate',type=int,default=64)
    r=sub.add_parser('replace-kit');r.add_argument('out');r.add_argument('bank');r.add_argument('pads',nargs='+')
    r=sub.add_parser('preview');r.add_argument('bank');r.add_argument('out');r.add_argument('--slot',type=int,choices=(1,2),default=1);r.add_argument('--play',action='store_true')
    for name in ('backup','restore','upload','info'):
        r=sub.add_parser(name);r.add_argument('file')
    a=p.parse_args();(ROOT/'build').mkdir(exist_ok=True)
    if a.cmd=='replace-song':
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as tmp:
            w=Path(tmp)/'song.wav';audio_wav(a.audio,w,a.start,a.seconds);new=bank.build(w,bitrate=a.bitrate,name=Path(a.audio).stem)
            image=repack(Path(a.bank).read_bytes(),new,a.slot);Path(a.out).write_bytes(image);bank.preview(image)
    elif a.cmd=='replace-kit':
        if len(a.pads)>16:raise ValueError('Kit supports at most 16 pads')
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as tmp:
            pads=[]
            for i,source in enumerate(a.pads):
                w=Path(tmp)/f'pad{i}.wav';audio_wav(source,w);pads.append(w)
            # A tiny dummy song is discarded by repack; only the new encoded kit is used.
            w=Path(tmp)/'dummy.wav'
            with wave.open(str(w),'wb') as f:f.setparams((1,2,22050,0,'NONE','not compressed'));f.writeframes(bytes(4410))
            image=repack(Path(a.bank).read_bytes(),kit=bank.build(w,pads));Path(a.out).write_bytes(image);bank.preview(image)
    elif a.cmd=='preview':
        render(Path(a.bank).read_bytes(),a.out,a.slot)
        if a.play:subprocess.run(['afplay',a.out],check=True)
    elif a.cmd=='info':bank.preview(Path(a.file).read_bytes())
    elif a.cmd=='backup':
        archive=capture(Link());validate_archive(archive);Path(a.file).write_text(json.dumps(archive)+'\n');print('Complete PIGEON archive saved')
    elif a.cmd=='restore':
        archive=json.loads(Path(a.file).read_text());validate_archive(archive)
        # Preserve an automatic recovery archive before the first restore write.
        link=Link();backup=capture(link);Path(a.file+'.before-restore.pigeon').write_text(json.dumps(backup)+'\n');restore(link,archive);print('PIGEON archive restored')
    elif a.cmd=='upload':
        image=Path(a.file).read_bytes();bank.validate(image);link=Link();backup=bank.export(link);Path(a.file+'.before-upload.kob').write_bytes(backup);bank.upload(link,image)
        actual=bank.export(link)
        if actual[:bank.validate(image)[2]]!=image[:bank.validate(image)[2]]:raise ValueError('Upload verification failed')
        if len(image)>bank.validate(image)[2] and actual[bank.validate(image)[2]:]!=image[bank.validate(image)[2]:]:raise ValueError('Chop metadata verification failed')
        print('Bank uploaded and verified')
if __name__=='__main__':
    try:main()
    except (ValueError,TimeoutError,subprocess.CalledProcessError) as e:raise SystemExit(f'PIGEON library: {e}')
