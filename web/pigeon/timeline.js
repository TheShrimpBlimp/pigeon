// SPDX-License-Identifier: GPL-3.0-only
// Non-destructive edits reference original PCM; history never duplicates long recordings.
export const RATE=22050;
export const cloneClips=clips=>clips.map(c=>({...c}));
export const clipLength=c=>Math.round(c.end*RATE)-Math.round(c.start*RATE);
export const timelineLength=clips=>clips.reduce((n,c)=>n+clipLength(c),0);
export function sliceTimeline(clips,a,b){
 const total=timelineLength(clips);a=Math.max(0,Math.min(total,Math.round(a)));b=Math.max(a,Math.min(total,Math.round(b)));
 const out=[];let at=0;
 for(const c of clips){const n=clipLength(c),lo=Math.max(a,at),hi=Math.min(b,at+n);if(hi>lo){const base=Math.round(c.start*RATE);out.push({...c,start:(base+lo-at)/RATE,end:(base+hi-at)/RATE});}at+=n;}
 return out;
}
export function deleteRange(clips,a,b){if(b<=a)return cloneClips(clips);return [...sliceTimeline(clips,0,a),...sliceTimeline(clips,b,timelineLength(clips))];}
export function splitTimeline(clips,at){return [...sliceTimeline(clips,0,at),...sliceTimeline(clips,at,timelineLength(clips))];}
export function pasteTimeline(clips,at,insert){return [...sliceTimeline(clips,0,at),...cloneClips(insert),...sliceTimeline(clips,at,timelineLength(clips))];}
export function moveClip(clips,from,to){const next=cloneClips(clips);if(from<0||from>=next.length||to<0||to>=next.length)return next;const [clip]=next.splice(from,1);next.splice(to,0,clip);return next;}
