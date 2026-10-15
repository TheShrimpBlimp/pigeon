"""Local storage preview and browser bank validation (no device access).
Run with .venv/bin/python; optional QuickJS from build/qol-js tests browser code.
"""
import json
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import ko_bank
banks=[ROOT/'build/ko-fixtures/minute-mp3.kob',ROOT/'build/ko-fixtures/minute-ima.kob',ROOT/'build/ko-fixtures/dual.kob']
for path in banks:
    data=path.read_bytes();summary=ko_bank.storage_summary(data)
    assert summary['payload_used']+summary['free']==summary['payload_capacity']==496*1024
    assert len(summary['songs'])==(2 if ko_bank.validate(data)[1]==3 else 1)
sys.path.insert(0,str(ROOT/'build/qol-js'))
try: import quickjs
except ImportError:
    print('Python preview passed; browser checks need QuickJS or Node')
    sys.exit(0)
c=quickjs.Context();c.eval((ROOT/'web/ko_bank.js').read_text())
html=(ROOT/'web/editor.html').read_text();inline=html.split('<script>')[1].split('</script>')[0]
c.eval('new Function('+json.dumps(inline)+')')
for path in banks:
    data=path.read_bytes();c.eval('var bytes=new Uint8Array('+json.dumps(list(data))+');')
    actual=json.loads(c.eval('JSON.stringify(KOBank.inspect(bytes))'))
    expected=ko_bank.storage_summary(data)
    assert actual['payloadUsed']==expected['payload_used'] and actual['free']==expected['free']
    assert actual['pads']==expected['drums']['pads'] and actual['kitBytes']==expected['drums']['bytes']
    for a,b in zip(actual['songs'],expected['songs']):
        assert a['name']==b['name'] and abs(a['seconds']-b['seconds'])<0.001
    for index in (0,4,12,664,len(data)-1):
        # Backups include opaque metadata after used: corrupt payload, not metadata.
        used=ko_bank.validate(data)[2]; index=min(index,used-1)
        c.eval(f'bytes[{index}]^=1')
        assert c.eval('(()=>{try{KOBank.inspect(bytes);return false}catch(e){return true}})()')
        c.eval(f'bytes[{index}]^=1')
print('Python/browser storage previews, CRC rejection and editor syntax: PASSED')

# Exercise the real UI upload handler with a fake MIDI session, including refusal
# before erase on an incompatible firmware. No physical device is opened.
c.eval("""
var elements={},events={},dev={koBankVersion:3},smpBusy=false,messages=[],writes=[];
function $(id) {return elements[id]||(elements[id]={disabled:false,files:[],addEventListener:(event,f)=>events[id+event]=f});}
function recActive(){return false;} function beginBusy(){return ()=>{};}
function sayK(k,msg){messages.push(msg);} function pack7(b){return Array.from(b);}
async function sessionRequest(d,r,opt){writes.push(r);if(r[0]===90)return [75,d.koBankVersion,0,96,25,0,32,0,1,3];return [0];}
""")
start=inline.index('let koImportBytes=');end=inline.index('function renderSamples()',start)
c.eval(inline[start:end])
data=banks[-1].read_bytes()
c.eval('koImportBytes=new Uint8Array('+json.dumps(list(data))+');koImportInfo=KOBank.inspect(koImportBytes);koImportRefresh();events["ko-uploadclick"]();')
while c.execute_pending_job(): pass
commands=json.loads(c.eval('JSON.stringify(writes.map(r=>r[0]))'))
assert commands[0:2]==[90,91] and 93 in commands
assert commands.index(93)>max(i for i,cmd in enumerate(commands) if cmd==92)
assert c.eval('smpBusy===false && messages[messages.length-1]==="Sample bank uploaded."')
c.eval('writes=[];dev.koBankVersion=2;elements["ko-upload"].disabled=false;events["ko-uploadclick"]();')
while c.execute_pending_job(): pass
assert c.eval('writes.length===1 && writes[0][0]===90')
print('Browser upload order, commit-last and capability rejection: PASSED')
