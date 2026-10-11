"""Encoded sample relocation, complete archive integrity, and no-write preflight."""
import sys,struct,zlib,base64,json,wave,tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import pigeon_library as lib,ko_bank as bank
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'song.wav'
 with wave.open(str(p),'wb') as w:w.setparams((1,2,22050,0,'NONE','not compressed'));w.writeframes(struct.pack('<22050h',*[(i%100)*100-5000 for i in range(22050)]))
 a=bank.build(p,bitrate=32);b=bank.build(p,bitrate=64);metadata=bytes(4096);a+=metadata
 dual=lib.repack(a,b,2);assert lib.component(dual,1)['song']==lib.component(a,1)['song']
 changed=lib.repack(dual,b,1);assert lib.component(changed,2)['song']==lib.component(dual,2)['song']
 kit=bank.build(p,[p,p],bitrate=32);withkit=lib.repack(dual,kit=kit)
 assert bank.validate(withkit)[11]==2
 assert all(lib.component(withkit,i)['song']==lib.component(dual,i)['song'] for i in (1,2))
 assert withkit[bank.validate(withkit)[2]:]==metadata
 archive=dict(format='pigeon-archive',version=1,objects=[dict(id=i,size=1,crc=zlib.crc32(b'x'),data=base64.b64encode(b'x').decode()) for i in range(10)],bank=base64.b64encode(dual).decode(),bank_crc=zlib.crc32(dual))
 assert lib.validate_archive(archive)==dual
 bad=json.loads(json.dumps(archive));bad['objects'][3]['crc']^=1
 class NoWrites:
  def req(self,*a):raise AssertionError('Invalid archive contacted device')
 try:lib.restore(NoWrites(),bad)
 except ValueError:pass
 else:raise AssertionError('Damaged archive accepted')
 bad=json.loads(json.dumps(archive));bad['objects'].pop()
 try:lib.validate_archive(bad)
 except ValueError:pass
 else:raise AssertionError('Incomplete archive accepted')
 for n in (0,127,128,0xffffffff):assert lib.r32(lib.u32(n))==n
 original_upload=bank.upload
 writes=[]
 bank.upload=lambda link,image:writes.append(('bank',image))
 class Protocol:
  def req(self,cmd,args,timeout=5):
   assert cmd==67;writes.append((args[0],args[1]));return [args[0],args[1],0]
 try:lib.restore(Protocol(),archive)
 finally:bank.upload=original_upload
 assert writes[0][0]=='bank' and writes[-1]==(2,0)
 assert [ident for stage,ident in writes[1:] if stage==0]==list(range(2,10))+[1,0]
 print('Pigeon library tests PASSED')
