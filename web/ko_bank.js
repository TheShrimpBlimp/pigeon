/* SPDX-License-Identifier: GPL-3.0-only
 * Pure local bank preview. No MIDI access or upload occurs here. */
(function(root) {
  const SIZE=0x67000, HEAD=4096, HEADER=668;
  function crc(b) { let c=0xffffffff; for(const x of b) { c^=x; for(let i=0;i<8;i++) c=(c>>>1)^((c&1)?0xedb88320:0); } return (c^0xffffffff)>>>0; }
  function inspect(bytes) {
    const b=new Uint8Array(bytes), v=new DataView(b.buffer,b.byteOffset,b.byteLength);
    if(b.length<HEAD) throw Error('Truncated bank');
    const u=(o)=>v.getUint32(o,true), used=u(8), version=u(4);
    if(u(0)!==0x32424f4b || ![2,3].includes(version) || used<HEAD || used>SIZE || ![used,used+4096].includes(b.length) || crc(b.subarray(HEAD,used))!==u(12)) throw Error('Invalid bank length or CRC');
    function song(off) {
      const f=Array.from({length:20},(_,i)=>u(off+4*i));
      if(f[0]!==0x32424f4b || crc(b.subarray(off,off+HEADER-4))!==u(off+HEADER-4) || f[2]!==used || f[3]!==u(12) || ![1,2].includes(f[4]) || f[5]<4000 || f[5]>44100 || !f[6] || f[6]>8000000 || f[11]>16 || f[16]<1 || f[16]>32 || f[15]<1 || f[15]>4096 || !(HEAD<=f[9] && f[9]+f[10]<=f[7] && f[7]<f[7]+f[8] && f[7]+f[8]<=f[14] && f[14]+f[15]*2<=used)) throw Error('Invalid song descriptor');
      if(f[18] || f[19] || f[17] !== (off===0 && version===3 ? HEADER : 0) || f[1] !== (off===0 ? version : 2)) throw Error('Unsupported song descriptor');
      const starts=Array.from({length:f[16]},(_,i)=>u(off+80+i*4));
      if(starts[0] || starts.some((n,i)=>n>=f[6] || (i && n<=starts[i-1]))) throw Error('Invalid chops');
      if(f[4]===1) { if(f[12] || f[13] || f[6]>f[8]*2) throw Error('Invalid ADPCM duration'); }
      else {
        if(f[5]!==22050 || f[12]<3 || f[12]>=16000 || f[13]>2304 || f[6]>f[12]*576-f[13] || (f[12]+1)*4>=f[8]) throw Error('Invalid MP3 duration');
        const ix=Array.from({length:f[12]+1},(_,i)=>u(f[7]+i*4));
        if(ix[0]!==ix.length*4 || ix[ix.length-1]!==f[8]) throw Error('Invalid MP3 index');
        for(let i=0;i<ix.length-1;i++) {
          const n=ix[i+1]-ix[i], p=f[7]+ix[i], br=b[p+2]>>>4;
          if(n<=0 || n>210 || ix[i+1]>f[8] || b[p]!==255 || (b[p+1]&254)!==242 || (b[p+2]&12) || br<1 || br>8 || b[p+3]>>>6!==3 || n!==Math.floor(72000*[0,8,16,24,32,40,48,56,64][br]/22050)+((b[p+2]>>>1)&1) || b[p+4+((b[p+1]&1)?0:2)]) throw Error('Unsupported MP3 frame');
        }
      }
      for(let i=0;i<f[11];i++) {
        const p=off+208+i*28, n=u(p+4), end=u(p+12), rate=u(p+16);
        if(u(p)>f[10] || !n || n>2*(f[10]-u(p)) || u(p+8)>end || end>=n || !rate || rate>65536 || v.getInt16(p+20,true)!==(60+i)*16 || b[p+24]>88 || b[p+25]!==60+i || b[p+26]!==60+i || b[p+27]) throw Error('Invalid drum zone');
      }
      const name=Array.from(b.subarray(off+656,off+664)).map(c=>c>=32 && c<=126 ? String.fromCharCode(c):c===0?'':'?').join('') || 'UNTITLED';
      return {name,seconds:f[6]/f[5],codec:f[4]===2?'MP3':'ADPCM',bytes:f[8]+f[15]*2,fields:f};
    }
    const songs=[song(0)];
    if(version===3) {
      const second=song(HEADER), f=second.fields;
      if(f[11] || f[10] || f[7]<songs[0].fields[14]+songs[0].fields[15]*2) throw Error('Overlapping second song');
      songs.push(second);
    }
    return {version,used,songs,pads:u(44),kitBytes:u(40),payloadUsed:used-HEAD,capacity:SIZE-HEAD,free:SIZE-used,headerBytes:HEADER*songs.length};
  }
  root.KOBank={inspect,SIZE,HEAD,HEADER};
})(globalThis);
