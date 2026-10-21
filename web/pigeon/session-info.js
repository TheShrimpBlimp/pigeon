// SPDX-License-Identifier: GPL-3.0-only
import {inspect} from './bank.js';
import {validateArchive} from './device.js';
import {project} from './editing.js';
export function sessionInfo(value){
 const image=value.image|| (value.archive?validateArchive(value.archive).bank:null);
 if(!image)return 'Empty workspace';const b=inspect(image),lines=[value.archive?'Complete session: samples, chops, current sequence, A–D patterns and settings.':'Sample bank: samples and chops only; no patterns or device settings.',`Songs: ${b.songs.map(s=>s.name||'Untitled').join(' / ')}. Drum pads: ${b.pads}.`,`${(b.payloadUsed/1024).toFixed(1)} KiB of encoded sample data.`];
 if(value.archive){const validated=validateArchive(value.archive);
  for(const o of validated.objects.filter(o=>o.id===0 || (o.id>=2 && o.id<=5))){
   const label=o.id===0?'Current sequence':`Pattern ${'ABCD'[o.id-2]}`;
   if(!o.size){lines.push(`${label}: empty.`);continue;}
   try{const p=project(o.bytes),name=new TextDecoder().decode(p.b.subarray(3824,3836)).replace(/\0.*$/s,'').trim();lines.push(`${label}${name?' — '+name:''}: saved (${o.size} bytes).`);}catch{lines.push(`${label}: saved, older project format.`);}
  }
 }
 return lines.join('\n');
}
export function replacementNotice(before,after){return `Replace the local workspace?\n\nCURRENT\n${sessionInfo(before)}\n\nINCOMING\n${sessionInfo(after)}\n\nYour connected FM1 is not changed. Download or save the current workspace first if needed.`;}
