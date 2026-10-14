// SPDX-License-Identifier: GPL-3.0-only
// Join mono 22.05 kHz clips without changing gain or their relative sample values.
export function assemble(clips){
 if(!clips.length||clips.length>32)throw Error('Choose 1–32 clips.');
 const pieces=clips.map(c=>{const a=Math.floor(c.start*22050),b=Math.min(c.pcm.length,Math.round(c.end*22050));if(!Number.isFinite(c.start)||!Number.isFinite(c.end)||c.start<0||c.end>c.pcm.length/22050+.001||b-a<64)throw Error('Each clip needs valid trim points and at least 64 samples.');return c.pcm.subarray(a,b);});
 const length=pieces.reduce((n,p)=>n+p.length,0);if(length>22050*600)throw Error('Merged audio must be shorter than 10 minutes.');
 const pcm=new Float32Array(length),segments=[];let position=0;
 pieces.forEach(p=>{pcm.set(p,position);segments.push({a:position,b:position+p.length,pitch:0,flags:0});position+=p.length;});
 return {pcm,segments,duration:length/22050,name:clips.length===1?clips[0].name:'MERGED',filename:clips.map(c=>c.filename).join(' + ')};
}
export function trimmedSegments(segments,start,end){return segments.map(c=>({...c,a:Math.max(start,c.a)-start,b:Math.min(end,c.b)-start})).filter(c=>c.b-c.a>=64);}
