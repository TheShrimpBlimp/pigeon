// SPDX-License-Identifier: GPL-3.0-only
export function normalizedGain(pcm,a=0,b=pcm.length,target=.95){
 let peak=0;for(let i=a;i<b;i++){const v=pcm[i];if(!Number.isFinite(v))throw Error('Invalid audio sample');peak=Math.max(peak,Math.abs(v));}
 return peak?Math.max(1,Math.min(4095,Math.floor(target/peak*256))):256;
}
export function chopPreview(pcm,c,rate,loop=false){
 const out=Float32Array.from(pcm.subarray(c.a,c.b)),len=out.length;if(!len)throw Error('Empty chop');
 if(c.reverse)out.reverse();const gain=(c.gain||256)/256;for(let i=0;i<len;i++)out[i]*=gain;
 let n=loop?Math.min(192,Math.floor(len/2),Math.floor((c.xfade||0)*rate/1000)):0;
 for(let i=0;i<n;i++){const p=len-n+i;out[p]+=(out[i]-out[p])*(i+1)/n;}
 const point=Math.floor(len*(c.play||0)/65536),start=c.play?(c.reverse?len-1-point:point):0;
 return {pcm:out,start,loopStart:n,playbackRate:2**(((c.pitch||0)+(c.cents||0)/100)/12),mapPosition:p=>c.reverse?c.b-1-p:c.a+p};
}
