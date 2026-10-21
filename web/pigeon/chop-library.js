// SPDX-License-Identifier: GPL-3.0-only
// Compact decoded source regions; playback metadata remains independent of audio.
import {songChops,saveChops,savePadSettings,padSettings} from './editing.js';
import {component,repack,replaceKit,inspect} from './bank.js';
import {chopPreview} from './chop-audio.js';
const RATE=22050,MAX=RATE*600;
export function compactChops(pcm,chops){
 if(!(pcm instanceof Float32Array)||!chops.length||chops.length>32)throw Error('Choose 1–32 chops.');
 const ranges=chops.map(c=>{if(!Number.isInteger(c.a)||!Number.isInteger(c.b)||c.a<0||c.b>pcm.length||c.b-c.a<64)throw Error('Invalid chop boundaries.');return {a:c.a,b:c.b};}).sort((a,b)=>a.a-b.a),merged=[];
 for(const r of ranges){const last=merged.at(-1);if(last&&r.a<=last.b)last.b=Math.max(last.b,r.b);else merged.push({...r});}
 let length=0;for(const r of merged){r.offset=length;length+=r.b-r.a;}
 if(length>MAX)throw Error('Keep under 10 minutes of audio per collection.');
 const audio=new Float32Array(length);for(const r of merged)audio.set(pcm.subarray(r.a,r.b),r.offset);
 if(audio.some(n=>!Number.isFinite(n)||Math.abs(n)>32))throw Error('Invalid audio samples.');
 return {pcm:audio,chops:chops.map(c=>{const r=merged.find(r=>c.a>=r.a&&c.b<=r.b);return {...c,a:r.offset+c.a-r.a,b:r.offset+c.b-r.a};})};
}
export function assembleCollected(entries,picks){
 if(!picks.length||picks.length>32)throw Error('Arrange 1–32 chops.');
 const chunks=[],groups=new Map();let length=0;
 for(const pick of picks){const entry=entries.find(e=>e.id===pick.id);if(!entry||!entry.chops[pick.index])throw Error('A collected chop is missing.');if(!groups.has(entry.id)){
  const indices=[...new Set(picks.filter(p=>p.id===entry.id).map(p=>p.index))],packed=compactChops(entry.pcm,indices.map(i=>entry.chops[i]));
  groups.set(entry.id,{offset:length,indices,...packed});chunks.push(packed.pcm);length+=packed.pcm.length;
 }}
 if(length>MAX)throw Error('Keep the assembled song under 10 minutes.');
 const pcm=new Float32Array(length);let offset=0;for(const chunk of chunks){pcm.set(chunk,offset);offset+=chunk.length;}
 return {pcm,chops:picks.map(p=>{const g=groups.get(p.id),c=g.chops[g.indices.indexOf(p.index)];return {...c,a:c.a+g.offset,b:c.b+g.offset};})};
}
// Drum destinations are one-shots. Render their direction, starting point and tuning;
// keep the collected source and all original settings untouched in the library.
export function collectedHit(entry,index){
 const c=entry.chops[index];if(!c)throw Error('Choose a collected chop.');
 const preview=chopPreview(entry.pcm,c,RATE,false),source=preview.pcm.subarray(preview.start),step=preview.playbackRate;
 const length=Math.ceil(source.length/step);if(length<64||length>MAX)throw Error('This chop is too short or long for a drum hit.');
 const out=new Int16Array(length),attack=((c.flags>>2)&31)*RATE*.005,release=((c.flags>>7)&31)*RATE*.01;
 for(let i=0;i<length;i++){const at=i*step,a=Math.floor(at),f=at-a,value=source[a]*(1-f)+(source[Math.min(a+1,source.length-1)]||0)*f,fade=Math.min(1,attack?i/attack:1,release?(length-1-i)/release:1);out[i]=Math.round(Math.max(-1,Math.min(1,value*fade))*32767);}
 return out;
}
export function exportCollection(entry){
 const bytes=new Uint8Array(entry.pcm.length*4),v=new DataView(bytes.buffer);entry.pcm.forEach((n,i)=>v.setFloat32(i*4,n,true));let binary='';for(let i=0;i<bytes.length;i+=16384)binary+=String.fromCharCode(...bytes.subarray(i,i+16384));
 return {format:'pigeon-chops',version:1,name:entry.name,rate:RATE,chops:entry.chops,audio:btoa(binary)};
}
export function importCollection(value){
 if(value?.format!=='pigeon-chops'||value.version!==1||value.rate!==RATE||typeof value.name!=='string'||!value.name.trim()||value.name.length>128||!Array.isArray(value.chops)||typeof value.audio!=='string'||value.audio.length>MAX*4*1.34)throw Error('Invalid chop collection.');
 const raw=atob(value.audio);if(!raw.length||raw.length%4||raw.length>MAX*4)throw Error('Invalid collection audio.');
 const bytes=Uint8Array.from(raw,c=>c.charCodeAt(0)),v=new DataView(bytes.buffer),pcm=Float32Array.from({length:bytes.length/4},(_,i)=>v.getFloat32(i*4,true));
 for(const c of value.chops)if(!Number.isInteger(c.pitch)||c.pitch< -24||c.pitch>24||!Number.isInteger(c.flags)||c.flags<0||c.flags>65535||!Number.isInteger(c.play??0)||(c.play??0)<0||(c.play??0)>65535||![0,1].includes(c.reverse??0)||!Number.isInteger(c.cents??0)||Math.abs(c.cents??0)>100||!Number.isInteger(c.gain??0)||(c.gain??0)<0||(c.gain??0)>4095||!Number.isInteger(c.xfade??0)||(c.xfade??0)<0||(c.xfade??0)>8)throw Error('Invalid collection playback settings.');
 return {id:crypto.randomUUID(),name:value.name,...compactChops(pcm,value.chops)};
}
export function mountChopLibrary(api){
 const $=id=>document.getElementById(id),section=document.createElement('section');section.className='kit';section.id='chop-collection';
 section.innerHTML=`<h2>Chop library</h2><p>Keep useful chops from different songs. Only selected audio regions are stored; overlapping regions share audio. Your original bank stays unchanged. Collections stay in this browser—download copies to keep them elsewhere.</p><div class="edit-fields"><label>Source song<select id="collect-song"><option value="1">Song 1</option><option value="2">Song 2</option></select></label><label>Chops to collect<select id="collect-chops" multiple size="6" aria-describedby="collect-help"></select></label><label>Collection name<input id="collect-name" maxlength="128"></label></div><p id="collect-help">Select one or more applied chops. Use Ctrl/Command-click to select several, or Shift-click for a range.</p><button id="collect-save" data-bank>Collect selected chops</button><button id="collect-trim" data-bank>Keep selected chops and trim song</button><p>Trim removes unselected chops and audio outside the retained regions from the source song. Overlaps share audio. Preview the recovered space before applying; local undo is available.</p><div class="edit-fields"><label>Saved chops<select id="collect-saved" multiple size="7"></select></label></div><div class="song-actions"><button id="collect-preview">Preview saved chop</button><button id="collect-stop">Stop preview</button><button id="collect-add">Add selected to arrangement</button><button id="collect-download">Download collection</button><label class="button">Import collection<input id="collect-import" type="file" accept=".pigeon-chops,application/json"></label><button id="collect-delete">Delete collection</button></div><h3>Build a bank from collected chops</h3><p>Arrange up to 32 chops for a song, or up to 16 one-shots for the drum kit. Song destinations retain playback settings. Drum hits render direction, playback point, pitch, cents, gain and fades into one-shot audio; loop/latch settings do not apply.</p><label>Arrangement<select id="collect-order" size="6" style="display:block;width:100%"></select></label><div class="song-actions"><button id="collect-up">Move up</button><button id="collect-down">Move down</button><button id="collect-remove">Remove from arrangement</button><button id="collect-clear">Clear arrangement</button></div><div class="edit-fields"><label>Destination<select id="collect-target"><option value="1">Song 1</option><option value="2">Song 2</option><option value="drums">Drum kit</option></select></label><label>Song quality<select id="collect-quality"><option value="24">24 kbps</option><option value="32">32 kbps</option><option value="48">48 kbps</option><option value="64" selected>64 kbps</option></select></label><label>Song name<input id="collect-title" maxlength="8" value="COLLECT"></label></div><button id="collect-build">Build into local bank</button><p>Building replaces the chosen destination in this browser, with undo available. Nothing is sent to the FM1 until you send the bank. The usual FM1 storage limit still applies.</p><p id="collect-status" role="status"></p>`;
 document.getElementById('chop-wave').closest('section').after(section);
 section.querySelectorAll('button').forEach(button=>{if(!['collect-save','collect-build'].includes(button.id))button.classList.add('secondary');});
 let entries=[],picks=[],last=null;
 const note=text=>$('collect-status').textContent=text;
 const selected=()=>[...$('collect-saved').selectedOptions].map(o=>{const [id,index]=o.value.split(':');return {id,index:Number(index)};});
 const source=()=>{const bank=api.bank(),slot=Number($('collect-song').value),chops=bank&&component(bank,slot)?songChops(bank,slot):[];$('collect-chops').replaceChildren(...chops.map((c,i)=>new Option(`Chop ${i+1} · ${((c.b-c.a)/component(bank,slot).h[5]).toFixed(2)} s`,i)));};
 const order=()=>{$('collect-order').replaceChildren(...picks.map((p,i)=>new Option(`${i+1}. ${entries.find(e=>e.id===p.id)?.name} / Chop ${p.index+1}`,i)));};
 const refresh=()=>{const chosen=new Set([...$('collect-saved').selectedOptions].map(o=>o.value));$('collect-saved').replaceChildren(...entries.flatMap(e=>e.chops.map((c,i)=>{const o=new Option(`${e.name} / Chop ${i+1} · ${((c.b-c.a)/RATE).toFixed(2)} s`,`${e.id}:${i}`);o.selected=chosen.has(o.value);return o;})));order();};
 const persist=async next=>{await api.store('collected-chops',next);entries=next;refresh();};
 $('collect-song').onchange=source;
 $('collect-save').onclick=()=>api.run(async()=>{
  const name=$('collect-name').value.trim(),indices=[...$('collect-chops').selectedOptions].map(o=>Number(o.value)),slot=Number($('collect-song').value);
  if(!name)throw Error('Name this collection.');if(!indices.length)throw Error('Select chops to collect.');
  const audio=await api.songAudio(slot);let pcm=audio.pcm,chops=songChops(api.bank(),slot).filter((_,i)=>indices.includes(i));
  if(audio.rate!==RATE){pcm=await api.resample(pcm,audio.rate,RATE);chops=chops.map(c=>({...c,a:Math.round(c.a*RATE/audio.rate),b:Math.round(c.b*RATE/audio.rate)}));}
  const entry={id:crypto.randomUUID(),name,...compactChops(pcm,chops)};await persist([...entries,entry]);note(`Collected ${chops.length} chops in “${name}”; kept ${(entry.pcm.length/RATE).toFixed(2)} seconds of audio.`);
 });
 $('collect-trim').onclick=()=>api.run(async()=>{
  const original=api.bank(),slot=Number($('collect-song').value),indices=[...$('collect-chops').selectedOptions].map(o=>Number(o.value));
  if(!indices.length)throw Error('Select chops to keep.');
  const audio=await api.songAudio(slot);let pcm=audio.pcm,chops=songChops(original,slot).filter((_,i)=>indices.includes(i));
  if(audio.rate!==RATE){pcm=await api.resample(pcm,audio.rate,RATE);chops=chops.map(c=>({...c,a:Math.round(c.a*RATE/audio.rate),b:Math.round(c.b*RATE/audio.rate)}));}
  const kept=compactChops(pcm,chops),encoded=await api.encode(kept.pcm,Number($('collect-quality').value),inspect(original).songs[slot-1].name||'TRIMMED',true);
  const next=saveChops(repack(original,encoded,slot),slot,kept.chops);
  const removed=(pcm.length-kept.pcm.length)/RATE,recovered=inspect(original).used-inspect(next).used;
  if(api.bank()!==original)throw Error('The bank changed while preparing the trim. Select the chops again.');
  if(!confirm(`Keep ${chops.length} chops in Song ${slot}? Remove ${removed.toFixed(2)} seconds outside these regions. ${recovered>=0?'Recover':'Use an additional'} ${(Math.abs(recovered)/1024).toFixed(1)} KiB of encoded storage. Other song and kit stay intact. Existing sequence chop numbers may need reassignment. Undo is available.`))return;
  await api.setBank(next);note(`Kept ${chops.length} chops in Song ${slot}; trimmed ${removed.toFixed(2)} seconds. Undo is available.`);
 });
 $('collect-add').onclick=()=>{const next=selected();if(!next.length){note('Select saved chops first.');return;}if(picks.length+next.length>32){note('Arrange at most 32 chops.');return;}picks.push(...next);order();};
 $('collect-preview').onclick=()=>api.run(async()=>{const p=selected()[0];if(!p)throw Error('Select a saved chop.');const e=entries.find(e=>e.id===p.id),preview=chopPreview(e.pcm,e.chops[p.index],RATE,false);await api.play(preview.pcm,RATE,false,null,0,preview);note(`Previewing ${e.name} / Chop ${p.index+1}`);});
 $('collect-stop').onclick=api.stop;
 for(const [id,delta] of [['up',-1],['down',1]])$('collect-'+id).onclick=()=>{const i=$('collect-order').selectedIndex,j=i+delta;if(i<0||j<0||j>=picks.length)return;[picks[i],picks[j]]=[picks[j],picks[i]];order();$('collect-order').selectedIndex=j;};
 $('collect-remove').onclick=()=>{const i=$('collect-order').selectedIndex;if(i>=0)picks.splice(i,1);order();};$('collect-clear').onclick=()=>{picks=[];order();};
 $('collect-download').onclick=()=>api.run(async()=>{const choices=selected();if(new Set(choices.map(p=>p.id)).size>1)throw Error('Select chops from one collection to download it.');const p=choices[0],e=entries.find(e=>e.id===p?.id);if(!e)throw Error('Select a saved collection.');api.download(`${e.name}.pigeon-chops`,JSON.stringify(exportCollection(e)),'application/json');note('Collection downloaded, including audio and playback settings.');});
 $('collect-import').onchange=()=>api.run(async()=>{const file=$('collect-import').files[0];if(!file)return;if(file.size>MAX*4*1.4)throw Error('Collection file is too large.');const entry=importCollection(JSON.parse(await file.text()));await persist([...entries,entry]);$('collect-import').value='';note(`Imported “${entry.name}”.`);});
 $('collect-delete').onclick=()=>api.run(async()=>{const ids=new Set(selected().map(p=>p.id));if(!ids.size)throw Error('Select a collection.');const names=entries.filter(e=>ids.has(e.id)).map(e=>e.name).join(', ');if(!confirm(`Delete collections from this browser: ${names}?`))return;await persist(entries.filter(e=>!ids.has(e.id)));picks=picks.filter(p=>!ids.has(p.id));order();note('Selected collections deleted.');});
 $('collect-build').onclick=()=>api.run(async()=>{
  if(!picks.length)throw Error('Add collected chops to the arrangement.');const target=$('collect-target').value,bank=api.bank();
  if(target==='drums'&&picks.length>16)throw Error('The drum kit has at most 16 pads.');if(!bank&&target!=='1')throw Error('Build Song 1 first.');
  if(bank&&!confirm(`Replace ${target==='drums'?'the drum kit':`Song ${target}`} in the local bank? Other slots and collected chops stay intact. Undo is available.`))return;
  let next;
  if(target==='drums'){
   const hits=picks.map(p=>collectedHit(entries.find(e=>e.id===p.id),p.index));next=replaceKit(bank,hits);
   const settings=padSettings(next);picks.forEach((p,i)=>{const c=entries.find(e=>e.id===p.id).chops[p.index];settings[i]={pitch:0,level:127,decay:0,choke:c.flags>>>14};});next=savePadSettings(next,settings);
  }else{
   const song=assembleCollected(entries,picks),encoded=await api.encode(song.pcm,Number($('collect-quality').value),$('collect-title').value.trim()||'COLLECT',true);
   next=bank?repack(bank,encoded,Number(target)):encoded;next=saveChops(next,Number(target),song.chops);
  }
  await api.setBank(next);note(`Built ${picks.length} chops into ${target==='drums'?'the drum kit':`Song ${target}`} locally. Save the bank or send it when ready.`);
 });
 const ready=api.load('collected-chops').then(value=>{entries=value||[];refresh();source();if(!entries.length)note("No collected chops yet. Choose source chops and name your first collection.");}).catch(error=>note(`Could not load chop library: ${error.message}`));
 return {ready,refresh(){if(last!==api.bank()){last=api.bank();source();}}};
}
