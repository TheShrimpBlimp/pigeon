// SPDX-License-Identifier: GPL-3.0-only
import {normalize,imaEncode,onsets,join,crc} from '../pigeon/bank.js';
export const FM1_POOL=0x67000, HEAD=4096, HEADER=668;
export function buildBank(songs,pads=[],unlimited=false) {
  if(!songs[0])throw Error('Load Song 1 first.');
  if(pads.length>16)throw Error('Choose up to 16 drum samples.');
  const encodedPads=pads.map(p=>imaEncode(normalize(p.pcm))), kit=join(encodedPads);
  const chunks=[kit,new Uint8Array(-kit.length&255)];let length=chunks.reduce((n,p)=>n+p.length,0);
  const headers=[];
  for(let i=0;i<2;i++){
    const s=songs[i];if(!s)continue;
    const pcm=normalize(s.pcm), audio=imaEncode(pcm),bins=Math.min(4096,pcm.length),wave=new Uint8Array(bins*2);
    if(!pcm.length)throw Error('The sample is empty.');
    for(let b=0;b<bins;b++){
      let lo=32767,hi=-32768;
      for(let p=Math.floor(b*pcm.length/bins);p<Math.floor((b+1)*pcm.length/bins);p++){lo=Math.min(lo,pcm[p]);hi=Math.max(hi,pcm[p]);}
      wave[b*2]=(lo>>9)&255;wave[b*2+1]=(hi>>9)&255;
    }
    const starts=onsets(pcm),h=[0x32424f4b,!i&&songs[1]?3:2,0,0,1,22050,pcm.length,HEAD+length,audio.length,HEAD+(i?length:0),i?0:kit.length,i?0:pads.length,0,0,HEAD+length+audio.length,bins,starts.length,!i&&songs[1]?HEADER:0,0,0];
    chunks.push(audio,wave);length+=audio.length+wave.length;headers.push({h,starts,name:s.name});
    if(!i&&songs[1]){const pad=new Uint8Array(-(HEAD+length)&255);chunks.push(pad);length+=pad.length;}
  }
  const used=HEAD+length;
  if(!unlimited && used>FM1_POOL)throw Error(`These samples use ${(length/1024).toFixed(1)} KiB. The FM1 has 408 KiB shared by both songs and all 16 pads. Shorten the audio or select unlimited mode.`);
  const image=new Uint8Array(used);image.fill(255,0,HEAD);const payload=join(chunks);image.set(payload,HEAD);const checksum=crc(payload),v=new DataView(image.buffer);
  headers.forEach(({h,starts,name},i)=>{
    const off=i*HEADER;image.fill(0,off,off+HEADER);h[2]=used;h[3]=checksum;h.forEach((n,j)=>v.setUint32(off+j*4,n,true));starts.forEach((n,j)=>v.setUint32(off+80+j*4,n,true));
    image.set(new TextEncoder().encode(name.replace(/[^\x20-\x7e]/g,'?')).subarray(0,8),off+656);
    if(!i){let position=0;pads.forEach((p,j)=>{const z=208+j*28,n=p.pcm.length;[position,n,0,n-1,32768].forEach((x,k)=>v.setUint32(z+k*4,x,true));v.setInt16(z+20,(60+j)*16,true);image[z+25]=image[z+26]=60+j;position+=encodedPads[j].length;});}
    v.setUint32(off+HEADER-4,crc(image.subarray(off,off+HEADER-4)),true);
  });
  return image;
}
export async function decodeSample(file,context){
  const audio=await context.decodeAudioData(await file.arrayBuffer());
  if(!audio.length)throw Error('The sample is empty.');
  const offline=new OfflineAudioContext(1,Math.ceil(audio.duration*22050),22050),source=offline.createBufferSource();source.buffer=audio;source.connect(offline.destination);source.start();
  const rendered=await offline.startRendering();return {name:file.name,pcm:rendered.getChannelData(0).slice()};
}
export async function bankStorage(value){
  const db=await new Promise((resolve,reject)=>{const r=indexedDB.open('pigeon-emulator-samples',1);r.onupgradeneeded=()=>r.result.createObjectStore('samples');r.onsuccess=()=>resolve(r.result);r.onerror=()=>reject(r.error);});
  try{return await new Promise((resolve,reject)=>{const tx=db.transaction('samples',value===undefined?'readonly':'readwrite'),s=tx.objectStore('samples'),r=value===undefined?s.get('current'):s.put(value,'current');let result;r.onsuccess=()=>result=r.result;tx.oncomplete=()=>resolve(result);tx.onerror=()=>reject(tx.error);});}finally{db.close();}
}
