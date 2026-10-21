// SPDX-License-Identifier: GPL-3.0-only
// PIGEON's emulator adapter for Charles Vestal's fm1-reader protocol.
// reader-words.js is an unmodified copy of fm1-reader/web/words.js.
import {spoken,wordsOf} from './reader-words.js';
export const readerFrame=(cmd)=>[0xf0,0x7d,0x53,0x52,cmd,...(cmd===1?[1]:[]),0xf7];
export class PigeonReader {
  constructor({send,onText=()=>{},fetcher=globalThis.fetch,speech=globalThis.speechSynthesis,Utterance=globalThis.SpeechSynthesisUtterance}={}) {
    this.send=send;this.onText=onText;this.fetcher=fetcher;this.speech=speech;this.Utterance=Utterance;
    this.last=null;this.enabled=false;this.words=[];this.pending=[];this.queue=[];this.mode='screen-reader';this.helloId='';
  }
  enable(on){
    this.enabled=on;
    if(this.timer)clearInterval(this.timer);this.timer=null;
    if(on){this.helloId="";this.send(readerFrame(1));this.timer=setInterval(()=>this.send(readerFrame(1)),2000);}
    else{this.send(readerFrame(2));this.pending=[];this.queue=[];this.speech?.cancel();this.busy=false;this.protected=false;this.loading=false;}
  }
  where(){if(this.enabled)this.send(readerFrame(3));}
  async receive(input){
    const d=Array.from(input);if(!this.enabled||d.length<7||d[0]!==0xf0||d.at(-1)!==0xf7||d.slice(1,4).join()!=='125,83,82')return;
    const text=String.fromCharCode(...d.slice(6,-1));
    if(d[4]===1){
      const [name,url='']=text.split('\0');if(url===this.helloId)return;this.helloId=url;this.loading=true;
      // The local copy allows review before the fork's Pages site is published.
      const id=/\/([a-f0-9]{10}\.json)$/.exec(url)?.[1];
      try{
        let response=id?await this.fetcher(new URL(`./words/${id}`,import.meta.url)):null;
        if(!response?.ok && /^https:\/\//.test(url))response=await this.fetcher(url);
        if(response?.ok)this.words=wordsOf(await response.json());
      }catch{this.onText('Pronunciation file unavailable. Using common reading rules.',true);}
      if(!this.enabled)return;this.loading=false;
      for(const item of this.pending.splice(0))this.say(...item);
      this.where();return name;
    }
    if(d[4]===16){if(this.loading)this.pending.push([text,d[5]]);else this.say(text,d[5]);}
  }
  repeat(){if(this.enabled && this.last)this.say(this.last.text,this.last.flags|1);}
  say(text,flags){
    this.last={text,flags};
    const words=spoken(text,this.words);if(!words)return;
    this.onText(words,!!(flags&2),flags);
    if(this.mode!=='voice'||!this.speech||!this.Utterance)return;
    if(flags&1){this.queue=[];if(!this.protected){this.speech.cancel();this.busy=false;this.protected=false;this.loading=false;}}
    this.queue.push({words,keep:!!(flags&2)});this.flush();
  }
  flush(){
    if(this.busy||!this.queue.length||!this.enabled)return;
    const {words,keep}=this.queue.shift(),utterance=new this.Utterance(words);
    this.busy=true;this.protected=keep;utterance.onend=utterance.onerror=()=>{this.busy=false;this.protected=false;this.flush();};
    this.speech.speak(utterance);
  }
  close(){this.enable(false);this.helloId='';}
}
export function attachReader({send,root=document}){
  const toggle=root.getElementById('reader-enable'),mode=root.getElementById('reader-output'),read=root.getElementById('reader-where');
  const transcript=root.getElementById('reader-transcript'),again=root.getElementById('reader-again');
  try {
    const preferences=JSON.parse(localStorage.getItem('pigeon-emulator-reader')||'null');
    if(preferences){toggle.checked=preferences.enabled===true;if(['voice','screen-reader'].includes(preferences.mode))mode.value=preferences.mode;}
  } catch { /* Storage may be unavailable. */ }
  const save=()=>{try{localStorage.setItem('pigeon-emulator-reader',JSON.stringify({enabled:toggle.checked,mode:mode.value}));}catch{}};
  let pending=[],timer;
  const clear=()=>{clearTimeout(timer);timer=null;pending=[];};
  const reader=new PigeonReader({send,onText(text,keep,flags=0){
    if(again)again.disabled=false;
    // WHERE arrives as several protocol frames: expose one coherent live-region
    // update, rather than rapidly replacing and losing individual controls.
    if(flags&1 || keep)clear();
    if(keep){
      transcript.textContent=text;
      const region=root.getElementById('reader-alert');region.textContent='';
      if(reader.mode==='screen-reader')requestAnimationFrame(()=>{if(reader.enabled && reader.mode==='screen-reader')region.textContent=text;});
      return;
    }
    pending.push(text);if(pending.length>8)pending.shift();
    transcript.textContent=pending.join('. ');
    clearTimeout(timer);timer=setTimeout(()=>{
      const combined=pending.join('. ');pending=[];timer=null;
      if(reader.enabled && reader.mode==='screen-reader'){
        const region=root.getElementById('reader-live');region.textContent='';
        requestAnimationFrame(()=>{if(reader.enabled && reader.mode==='screen-reader')region.textContent=combined;});
      }
    },100);
  }});
  toggle.onchange=()=>{clear();reader.enable(toggle.checked);read.disabled=!toggle.checked;if(again)again.disabled=!toggle.checked||!reader.last;root.getElementById('reader-live').textContent='';root.getElementById('reader-alert').textContent='';save();};
  mode.onchange=()=>{clear();reader.mode=mode.value;reader.queue=[];reader.speech?.cancel();reader.busy=false;reader.protected=false;save();if(reader.enabled)reader.where();};read.onclick=()=>reader.where();
  if(again)again.onclick=()=>reader.repeat();
  reader.mode=mode.value;
  if(toggle.checked)toggle.onchange();
  window.addEventListener('pagehide',()=>{clear();reader.close();});return reader;
}
