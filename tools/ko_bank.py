#!/usr/bin/env python3
"""Build/upload/export an KO storage v2/v3 bank. No device access for build.

  ko_bank.py build OUT.kob SONG.wav [DRUM.wav ...] [--codec mp3|adpcm] [--bitrate 32] [--rate 11025] [--song2 SONG2.wav]
  ko_bank.py add-song OUT.kob BACKUP.kob SONG2.wav
  ko_bank.py upload BANK.kob
  ko_bank.py export OUT.kob
  ko_bank.py info
  ko_bank.py erase

MP3 encoding needs libmp3lame, or `python3 -m pip install --target build/codec-python lameenc`.
The device decoder only accepts the indexed mono/no-reservoir subset built here.
Upload replaces both songs AND kit as one bank; build takes 0..16 drums in pad order.
Export includes chop/compressor metadata. Old firmware is rejected before writes.
"""
import argparse
import array
import ctypes
import ctypes.util
import json
import math
import struct
import sys
import zlib
from pathlib import Path

import sampleio as sio
from fm1_sample_upload import Link, pack7

MAGIC, SIZE, HEAD, HEADER = 0x32424F4B, 0x7D000, 4096, 668
EXTENTS = ((0xA0000,0x13000),(0xB4000,0x28000),(0x78000,0x1A000),(0xEA000,0x12000),(0x60000,0x16000))


def physical(offset):
    if not 0 <= offset < SIZE:
        raise ValueError("bank offset outside pool")
    for base, size in EXTENTS:
        if offset < size:
            return base+offset
        offset -= size
    raise AssertionError("layout size mismatch")


def u21(v):
    if not 0 <= v < 1 << 21:
        raise ValueError("wire value outside 21 bits")
    return [v & 127,(v >> 7) & 127,(v >> 14) & 127]


def unpack7(data):
    out = bytearray()
    for i in range(0,len(data),8):
        group=data[i:i+8]
        if len(group)<2 or group[0]>>(len(group)-1):
            raise ValueError("malformed packed reply")
        out.extend(b|((group[0]>>j)&1)<<7 for j,b in enumerate(group[1:]))
    return bytes(out)


def pcm(path,rate):
    source,x=sio.read_any_wav(Path(path))
    x=sio.resample(x,source,rate)
    if not x:
        raise ValueError(f"{path}: empty audio")
    return sio.to_int16(x)


def encode_mp3(samples,bitrate):
    """Use LAME's C API to explicitly disable its bit reservoir and Xing frame."""
    library=ctypes.util.find_library("mp3lame")
    if not library:
        sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"build/codec-python"))
        try:
            import lameenc
            library=lameenc.__file__
        except ImportError:
            raise ValueError("MP3 encoding needs libmp3lame or lameenc (see tool help)") from None
    lib=ctypes.CDLL(library)
    lib.lame_init.restype=ctypes.c_void_p
    handle=lib.lame_init()
    if not handle:
        raise ValueError("cannot initialize LAME")
    try:
        for key,value in (("in_samplerate",22050),("out_samplerate",22050),("num_channels",1),
                          ("brate",bitrate),("quality",2),("disable_reservoir",1),
                          ("bWriteVbrTag",0),("write_id3tag_automatic",0)):
            fn=getattr(lib,f"lame_set_{key}"); fn.argtypes=[ctypes.c_void_p,ctypes.c_int]; fn.restype=ctypes.c_int
            if fn(handle,value)<0:
                raise ValueError(f"LAME rejected {key}")
        lib.lame_init_params.argtypes=[ctypes.c_void_p]
        if lib.lame_init_params(handle)<0:
            raise ValueError("LAME configuration failed")
        lib.lame_encode_buffer.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,
                                        ctypes.c_int,ctypes.c_void_p,ctypes.c_int]
        lib.lame_encode_flush.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int]
        lib.lame_get_encoder_delay.argtypes=[ctypes.c_void_p]
        a=array.array('h',samples)
        inp=(ctypes.c_short*len(a)).from_buffer(a)
        output=(ctypes.c_ubyte*(math.ceil(len(a)*1.25)+7200))()
        n=lib.lame_encode_buffer(handle,inp,inp,len(a),output,len(output))
        if n<0:
            raise ValueError(f"LAME encode failed: {n}")
        encoded=bytes(output[:n])
        n=lib.lame_encode_flush(handle,output,len(output))
        if n<0:
            raise ValueError(f"LAME flush failed: {n}")
        encoded+=bytes(output[:n])
        skip=lib.lame_get_encoder_delay(handle)+529
    finally:
        lib.lame_close.argtypes=[ctypes.c_void_p]; lib.lame_close(handle)
    offsets=[0]
    while offsets[-1]<len(encoded):
        p=offsets[-1]
        if p+13>len(encoded):
            raise ValueError("truncated MP3 frame")
        a,b,c,d=encoded[p:p+4]
        br=c>>4
        if a!=255 or b&254!=0xF2 or c&12 or not 1<=br<=8 or d>>6!=3:
            raise ValueError("encoder emitted unsupported MP3 format")
        size=72000*(0,8,16,24,32,40,48,56,64)[br]//22050+((c>>1)&1)
        if encoded[p+4+(0 if b&1 else 2)] or p+size>len(encoded):
            raise ValueError("MP3 reservoir enabled or truncated frame")
        offsets.append(p+size)
    frames=len(offsets)-1
    if frames<3 or frames>=16000 or len(samples)+skip>frames*576:
        raise ValueError("MP3 duration cannot be represented")
    table=len(offsets)*4
    return struct.pack(f'<{len(offsets)}I',*(p+table for p in offsets))+encoded,frames,skip


def onsets(samples,rate):
    """Same peak-rise detector as slc_scan; analysis stays on the computer for MP3."""
    steps=[abs(samples[i]-(samples[i-1] if i else 0)) for i in range(len(samples))]
    td,tr=max(steps)//12,max(map(abs,samples))//8
    envd=envr=last=previous=0
    starts=[0]; gap=rate//20
    for pos in range(0,len(samples),32):
        hd=max(steps[pos:pos+32]); hr=max(map(abs,samples[pos:pos+32]))
        if pos and ((hd>td and hd*4>envd*7) or (hr>tr and hr*4>envr*7)) and previous>=last+gap and len(starts)<32:
            starts.append(previous); last=previous
        envd=hd if hd>envd else envd-envd//16
        envr=hr if hr>envr else envr-envr//64
        previous=pos
    return starts


def build(song,drums=(),codec="mp3",bitrate=32,rate=11025,name=None,song2=None):
    if len(drums)>16 or codec not in ("mp3","adpcm") or rate not in (8000,11025,16000,22050):
        raise ValueError("unsupported bank configuration")
    if bitrate not in (24,32,40,48,56,64):
        raise ValueError("MP3 bitrate must be 24,32,40,48,56 or 64 kbps")
    rate=22050 if codec=="mp3" else rate
    audio=pcm(song,rate)
    if len(audio)>8000000:
        raise ValueError("song exceeds firmware sample count limit")
    kit=bytearray(); zones=[]
    for pad,path in enumerate(drums):
        hit=pcm(path,22050); encoded,state=sio.ima_encode(hit,0)
        zones.append(struct.pack('<5I2h4B',len(kit),len(hit),0,len(hit)-1,32768,
                                 (60+pad)*16,state[0],state[1],60+pad,60+pad,0))
        kit+=encoded
    if codec=="mp3":
        songdata,frames,skip=encode_mp3(audio,bitrate)
    else:
        songdata,_=sio.ima_encode(audio,0); frames=skip=0
    payload=bytearray(kit)
    payload+=b'\0'*(-len(payload)%256)
    song_off=HEAD+len(payload); payload+=songdata
    wave_off=HEAD+len(payload); bins=min(4096,len(audio))
    for i in range(bins):
        section=audio[i*len(audio)//bins:(i+1)*len(audio)//bins]
        payload+=struct.pack('bb',min(section)>>9,max(section)>>9)
    used=HEAD+len(payload)
    if used>SIZE:
        raise ValueError(f"bank needs {used:,} bytes; pool holds {SIZE:,}. Shorten audio, lower bitrate/rate, or shorten drums")
    starts=onsets(audio,rate)
    fields=[MAGIC,2,used,zlib.crc32(payload),2 if codec=="mp3" else 1,rate,len(audio),song_off,len(songdata),
            HEAD,len(kit),len(drums),frames,skip,wave_off,bins,len(starts),0,0,0]
    header=struct.pack('<20I',*fields)+struct.pack('<32I',*(starts+[0]*(32-len(starts))))
    header+=b''.join(zones)+b'\0'*(28*(16-len(zones)))
    header+=(name or Path(song).stem).encode('ascii','replace')[:8].ljust(8,b'\0')
    header+=struct.pack('<I',zlib.crc32(header))
    image=header.ljust(HEAD,b'\xff')+payload
    validate(image)
    return combine(image,build(song2,(),codec,bitrate,rate)) if song2 else image


def combine(first,second):
    """Two independently indexed songs in the same bounded flash pool."""
    f=validate(first);g=validate(second)
    if f[1]!=2 or g[1]!=2 or g[11]: raise ValueError('combine needs one-song base and a kit-free second song')
    metadata=first[f[2]:]
    first=first[:f[2]];second=second[:g[2]]
    pad=(-len(first))%256
    delta=len(first)+pad-HEAD
    payload=first[HEAD:]+b"\0"*pad+second[HEAD:]
    used=HEAD+len(payload)
    if used>SIZE: raise ValueError(f"two songs and kit need {used:,} bytes; capacity is {SIZE:,}")
    crc=zlib.crc32(payload)
    a=list(struct.unpack_from("<20I",first));b=list(struct.unpack_from("<20I",second))
    a[1]=3;a[2]=b[2]=used;a[3]=b[3]=crc;a[17]=HEADER
    for i in (7,9,14): b[i]+=delta
    def header(fields,old):
        h=struct.pack("<20I",*fields)+old[80:HEADER-4]
        return h+struct.pack("<I",zlib.crc32(h))
    image=(header(a,first)+header(b,second)).ljust(HEAD,b"\xff")+payload+metadata
    validate(image)
    return image


def validate(image):
    if len(image)<HEAD:
        raise ValueError("truncated bank")
    f=struct.unpack_from('<20I',image)
    magic,version,used,crc,codec,rate,samples,so,sl,ko,kl,kn,frames,skip,wo,wb,an,*reserved=f
    if magic!=MAGIC or version not in (2,3) or not HEAD<=used<=SIZE or len(image) not in (used,used+4096) or \
       zlib.crc32(image[:HEADER-4])!=struct.unpack_from('<I',image,HEADER-4)[0] or \
       zlib.crc32(image[HEAD:used])!=crc:
        raise ValueError("invalid bank header, length or CRC")
    if codec not in (1,2) or not 4000<=rate<=44100 or not 0<samples<=8000000 or not 0<=kn<=16 or \
       not 1<=an<=32 or (reserved!=([HEADER,0,0] if version==3 else [0,0,0])) or not 1<=wb<=4096 or \
       not (HEAD<=ko<=ko+kl<=so<so+sl<=wo<wo+wb*2<=used):
        raise ValueError("invalid bank ranges")
    starts=struct.unpack_from('<32I',image,80)[:an]
    if starts[0] or any(a>=b for a,b in zip(starts,starts[1:])) or starts[-1]>=samples:
        raise ValueError("invalid chop table")
    for i in range(kn):
        z=struct.unpack_from('<5I2h4B',image,208+i*28)
        off,n,ls,le,zrate,root,pred,idx,lo,hi,loop=z
        if off>kl or not 0<n<=2*(kl-off) or not 0<zrate<=65536 or not ls<=le<n or \
           idx>88 or root!=(60+i)*16 or lo!=60+i or hi!=60+i or loop:
            raise ValueError("invalid drum zone")
    if codec==1:
        if frames or skip or samples>sl*2:
            raise ValueError("invalid ADPCM duration")
    else:
        if rate!=22050 or not 3<=frames<16000 or skip>2304 or samples>frames*576-skip or (frames+1)*4>=sl:
            raise ValueError("invalid MP3 duration")
        index=struct.unpack_from(f'<{frames+1}I',image,so)
        if index[0]!=(frames+1)*4 or index[-1]!=sl:
            raise ValueError("invalid MP3 index")
        for a,b in zip(index,index[1:]):
            if not 0<b-a<=210 or b>sl:
                raise ValueError("invalid MP3 frame range")
            x,y,z,w=image[so+a:so+a+4]; br=z>>4
            if x!=255 or y&254!=0xF2 or z&12 or not 1<=br<=8 or w>>6!=3 or \
               b-a!=72000*(0,8,16,24,32,40,48,56,64)[br]//22050+((z>>1)&1) or image[so+a+4+(0 if y&1 else 2)]:
                raise ValueError("unsupported MP3 frame")
    if version==3:
        if len(image)<2*HEADER: raise ValueError("truncated second song header")
        h=image[HEADER:2*HEADER]
        second=validate(h+image[HEADER:])
        if second[1]!=2 or second[2]!=used or second[3]!=crc or second[11] or second[10] or second[7]<wo+wb*2:
            raise ValueError("invalid second song layout")
    return f


def storage_summary(image):
    f=validate(image)
    def song_at(off):
        h=struct.unpack_from('<20I',image,off)
        name=image[off+656:off+664].split(b'\0',1)[0].decode('ascii','replace')
        return dict(name=name or 'UNTITLED',seconds=round(h[6]/h[5],3),
                    codec='mp3' if h[4]==2 else 'adpcm',bytes=h[8]+h[15]*2)
    songs=[song_at(0)]
    if f[1]==3: songs.append(song_at(HEADER))
    return dict(songs=songs,drums=dict(pads=f[11],bytes=f[10]),
                payload_used=f[2]-HEAD,payload_capacity=SIZE-HEAD,
                free=SIZE-f[2],percent=round((f[2]-HEAD)*100/(SIZE-HEAD),1))


def preview(image):
    s=storage_summary(image)
    print('PIGEON — combined sample storage')
    for i,song in enumerate(s['songs'],1):
        print(f"  Song {i}: {song['name']} / {song['seconds']:.2f}s / {song['codec']} / {song['bytes']/1024:.1f} KiB")
    print(f"  Drum kit: {s['drums']['pads']} pads / {s['drums']['bytes']/1024:.1f} KiB")
    print(f"  Total: {s['payload_used']/1024:.1f} / {s['payload_capacity']/1024:.0f} KiB ({s['percent']}%) — {s['free']/1024:.1f} KiB free")
    print('  Upload replaces both songs and the drum kit together.')


def negotiate(link):
    r=link.req(90,[])
    expected=[0x4B,2]+u21(SIZE)+u21(HEAD)
    if len(r)!=10 or r[0]!=expected[0] or r[1] not in (2,3) or (r[2] | r[3]<<7 | r[4]<<14) not in (0x67000,SIZE) or r[5:8]!=expected[5:8] or r[9]!=3:
        raise ValueError("device does not support this exact KO storage layout; nothing written")
    return r


def request_ok(link,cmd,args,timeout=5):
    r=link.req(cmd,args,timeout)
    if r!=[0]:
        raise ValueError(f"bank command {cmd} failed: {r}")


def upload(link,image):
    fields=validate(image); used=fields[2]
    capability=negotiate(link)
    if fields[1]==3 and capability[1]!=3: raise ValueError("device lacks two-song support; nothing written")
    capacity=capability[2] | capability[3]<<7 | capability[4]<<14
    if used>capacity: raise ValueError("bank exceeds connected firmware capacity; nothing written")
    request_ok(link,91,u21(used))
    for off in range(HEAD,used,256):
        request_ok(link,92,u21(off)+pack7(image[off:min(off+256,used)]))
    header_size=HEADER*(2 if fields[1]==3 else 1)
    for off in range(0,header_size,256):
        request_ok(link,92,u21(off)+pack7(image[off:min(off+256,header_size)]))
    request_ok(link,93,[],30)
    if len(image)>used:
        for off in range(0,4096,256):
            request_ok(link,95,u21(off)+pack7(image[used+off:used+off+256]))


def read(link,off,n):
    r=link.req(94,u21(off)+[n&127,n>>7])
    if not r or r[0]:
        raise ValueError("bank read failed")
    data=unpack7(r[1:])
    if len(data)!=n:
        raise ValueError("truncated bank read")
    return data


def export(link):
    capability=negotiate(link)
    capacity=capability[2] | capability[3]<<7 | capability[4]<<14
    header=b''.join(read(link,off,min(256,HEADER-off)) for off in range(0,HEADER,256))
    used=struct.unpack_from('<I',header,8)[0]
    if struct.unpack_from('<I',header)[0]!=MAGIC or not HEAD<=used<=SIZE:
        raise ValueError("no KO bank to export; use the legacy sample backup for FSMP slots")
    image=b''.join(read(link,off,min(256,used-off)) for off in range(0,used,256))
    image+=b''.join(read(link,capacity+off,256) for off in range(0,4096,256))
    validate(image)
    return image


def main():
    parser=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    sub=parser.add_subparsers(dest='command',required=True)
    b=sub.add_parser('build'); b.add_argument('out'); b.add_argument('song'); b.add_argument('drums',nargs='*')
    b.add_argument('--codec',choices=('mp3','adpcm'),default='mp3'); b.add_argument('--bitrate',type=int,default=32)
    b.add_argument('--rate',type=int,choices=(8000,11025,16000,22050),default=11025); b.add_argument('--name'); b.add_argument('--song2',help='second independent song WAV')
    add=sub.add_parser('add-song');add.add_argument('out');add.add_argument('bank');add.add_argument('song')
    add.add_argument('--codec',choices=('mp3','adpcm'),default='mp3');add.add_argument('--bitrate',type=int,default=32)
    add.add_argument('--rate',type=int,choices=(8000,11025,16000,22050),default=11025)
    sub.add_parser('preview').add_argument('file'); sub.add_parser('upload').add_argument('file'); sub.add_parser('export').add_argument('file')
    sub.add_parser('info'); sub.add_parser('erase')
    d=sub.add_parser('diagnostics'); d.add_argument('--reset',action='store_true')
    a=parser.parse_args()
    try:
        if a.command=='add-song':
            image=combine(Path(a.bank).read_bytes(),build(a.song,(),a.codec,a.bitrate,a.rate))
            Path(a.out).write_bytes(image)
            print('Two-song bank created; first song, kit and existing metadata retained')
            return
        if a.command=='build':
            image=build(a.song,a.drums,a.codec,a.bitrate,a.rate,a.name,a.song2); Path(a.out).write_bytes(image)
            f=validate(image)
            print(json.dumps(dict(seconds=round(f[6]/f[5],3),codec=a.codec,song_bytes=f[8],kit_bytes=f[10],
                                  pads=f[11],used=f[2],capacity=SIZE,free=SIZE-f[2]),indent=2)); return
        if a.command=='preview': preview(Path(a.file).read_bytes()); return
        image=Path(a.file).read_bytes() if a.command=='upload' else None
        if image is not None: preview(image) # validate and show storage before opening MIDI
        link=Link()
        if a.command=='diagnostics':
            r=link.req(97,[1] if a.reset else [])
            if len(r)!=52 or r[:2]!=[0,1]: raise ValueError('firmware does not support diagnostics v1')
            names=('missing_sample_reads','decoder_errors','reused_buffers','decode_calls',
                   'decode_total_us','decode_max_us','poll_gap_max_us','draw_max_us',
                   'ready_attacks','attack_max_us')
            values=[sum(r[2+i*5+j]<<(j*7) for j in range(5)) for i in range(10)]
            print(json.dumps(dict(zip(names,values)),indent=2))
        elif a.command=='upload': upload(link,image); print('KO bank uploaded')
        elif a.command=='export': Path(a.file).write_bytes(export(link)); print('KO bank and edits exported')
        elif a.command=='erase': negotiate(link); request_ok(link,96,[]); print('KO bank headers erased')
        else: print(negotiate(link))
    except (OSError,ValueError,TimeoutError) as e:
        parser.exit(1,f'KO bank: {e}\n')


if __name__=='__main__':
    main()
