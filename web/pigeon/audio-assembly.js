// SPDX-License-Identifier: GPL-3.0-only
// Join mono 22.05 kHz clips without changing gain or their relative sample values.
export function assemble(clips){
 if(!clips.length||clips.length>32)throw Error('Choose 1–32 clips.');
 const pieces=clips.map(c=>{const a=Math.floor(c.start*22050),b=Math.min(c.pcm.length,Math.round(c.end*22050));if(!Number.isFinite(c.start)||!Number.isFinite(c.end)||c.start<0||c.end>c.pcm.length/22050+.001||b-a<64)throw Error('Each clip needs valid trim points and at least 64 samples.');const gain=c.gainDb??0,fade=c.fadeMs??3;
 if(!Number.isFinite(gain)||gain< -24||gain>12||!Number.isFinite(fade)||fade<0||fade>100)throw Error('Clip gain must be −24 to +12 dB and fades 0–100 ms.');
 const out=c.pcm.slice(a,b),level=10**(gain/20),edge=Math.min(Math.round(fade*22.05),Math.floor(out.length/2));
 for(let i=0;i<out.length;i++){const ramp=edge?Math.min(1,i/edge,(out.length-1-i)/edge):1;out[i]=Math.max(-1,Math.min(1,out[i]*level*ramp));}return out;});
 const length=pieces.reduce((n,p)=>n+p.length,0);if(length>22050*600)throw Error('Merged audio must be shorter than 10 minutes.');
 const pcm=new Float32Array(length),segments=[];let position=0;
 pieces.forEach(p=>{pcm.set(p,position);segments.push({a:position,b:position+p.length,pitch:0,flags:0});position+=p.length;});
 return {pcm,segments,duration:length/22050,name:clips.length===1?clips[0].name:'MERGED',filename:clips.map(c=>c.filename).join(' + ')};
}
export function trimmedSegments(segments,start,end){return segments.map(c=>({...c,a:Math.max(start,c.a)-start,b:Math.min(end,c.b)-start})).filter(c=>c.b-c.a>=64);}
