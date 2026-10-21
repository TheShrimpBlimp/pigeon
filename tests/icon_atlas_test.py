"""Every icon ID keeps its decoded pixels and ink bounds after atlas deduplication."""
from pathlib import Path
import re, subprocess, sys, tempfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from gen_aa_font import huff_unpack

def arrays(path):
    text=path.read_text()
    return {name:[int(v,0) for v in re.findall(r'0x[0-9a-fA-F]+|\b\d+\b',body)]
            for name,body in re.findall(r'\b(AI\w+)\[\d*\]\s*=\s*\{(.*?)\}',text,re.S)}

with tempfile.TemporaryDirectory() as tmp:
    old,new=Path(tmp)/'reference.h',Path(tmp)/'compact.h'
    for path,extra in [(old,['--no-dedup']),(new,[])]:
        subprocess.run([sys.executable,str(ROOT/'tools/gen_aa_icons.py'),str(path),*extra],check=True,stdout=subprocess.DEVNULL)
    before,after=arrays(old),arrays(new)
    assert before and after,'No generated arrays parsed'
    checked=0
    for size in (12,16,24):
        prefix=f'AI{size}_'
        assert len(before[prefix+'IDX'])==len(after[prefix+'IDX'])
        for a,b in zip(before[prefix+'IDX'],after[prefix+'IDX']):
            assert (a==255)==(b==255),'Icon presence changed'
            if a==255:continue
            def pixels(atlas,index):
                offset=atlas[prefix+'OFF'][index]
                return huff_unpack(atlas['AI_HC'],atlas[prefix+'DATA'][offset:],size*size)
            assert pixels(before,a)==pixels(after,b),'Icon pixels changed'
            assert before[prefix+'INK'][a]==after[prefix+'INK'][b],'Ink bounds changed'
            checked+=1
    assert len(after['AI12_DATA'])<len(before['AI12_DATA']),'No duplicate cells reclaimed'
print(f'Icon atlas: {checked} ID/size combinations retain identical pixels and ink bounds')
