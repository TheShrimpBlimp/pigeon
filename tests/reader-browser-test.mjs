// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {PigeonReader,readerFrame} from '../web/emu/reader.js';
const header=fs.readFileSync('build/gen/ann_words.h','utf8'),url=header.match(/https:[^"\n]+/)[0],id=url.split('/').at(-1);
const words=JSON.parse(fs.readFileSync(`web/words/${id}`));
const sent=[],heard=[],voices=[];
class Utterance{constructor(text){this.text=text;}}
const speech={cancelled:0,cancel(){this.cancelled++;},speak(u){voices.push(u);}};
const reader=new PigeonReader({send:x=>sent.push(x),onText:(text,keep)=>heard.push({text,keep}),fetcher:async()=>({ok:true,json:async()=>words}),speech,Utterance});
const frame=(cmd,flags,text)=>[0xf0,0x7d,0x53,0x52,cmd,flags,...Array.from(text,c=>c.charCodeAt(0)),0xf7];
assert.deepEqual(readerFrame(1),[240,125,83,82,1,1,247]);
await reader.receive(frame(16,1,'K1 FEEDBK 10'));assert.equal(heard.length,0);
reader.enable(true);await reader.receive(frame(1,1,`PIGEON v0.4\0${url}`));assert(sent.some(x=>x[4]===3));
await reader.receive(frame(16,1,'K1 FEEDBK 10'));assert.equal(heard.at(-1).text,'knob 1 feedback 10');
reader.mode='voice';reader.say('CANNOT SAVE',3);assert.equal(voices.length,1);const before=speech.cancelled;
reader.say('SAVED',1);assert.equal(speech.cancelled,before,'protected warning must not be interrupted');assert.equal(voices.length,1);
voices[0].onend();assert.equal(voices.length,2);assert.equal(voices[1].text,'saved');
reader.repeat();assert.equal(heard.at(-1).text,'saved');
reader.close();assert.equal(sent.at(-1)[4],2);const count=heard.length;await reader.receive(frame(16,1,'K1 20'));assert.equal(heard.length,count);
console.log('PIGEON reader browser adapter: framing, dictionary, opt-in, WHERE, protected speech and disconnect passed.');
