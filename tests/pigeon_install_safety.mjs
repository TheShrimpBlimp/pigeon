// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {Updater,classifyFirmware} from '../web/fm1ota.js';
const cases=[['FM-1_015',null,'allow'],['FM-1_000',null,'refuse'],['FM-1_900','PIGEON 0.1-dev','allow'],['FM-1_910','FELUCCA v1.0','allow'],['FM-1_909','FELUCCA 0.9-BETA','refuse'],['FM-1_920','FELUCCA SLOOP 2.2','refuse'],['FM-1_920','ChoralRoot 0.18','allow'],['FM-1_900','MELODEE v0.11.1','allow'],['FM-1_920',null,'refuse'],['FM-1_900','Other v1.0','refuse']];
for(const [identity,info,verdict] of cases) {
 assert.equal(classifyFirmware(identity,info).verdict,verdict);
 const sent=[]; const replies=[];
 const link={drain(){replies.length=0;},send(p){sent.push(p);if(info)replies.push([240,125,70,76,1,...Array.from(info.padEnd(24,'\0'),c=>c.charCodeAt(0)),247]);return true;},async read(){return replies.shift()||null;},close(){}};
 const dev={id:{text:identity,model:'FM-1',version:Number(identity.split('_')[1])},link};
 const up=new Updater({});up.find=async()=>dev;
 const policy=await up.compatibility(dev);assert.equal(policy.verdict,verdict);
 if(verdict==='refuse') {
  await assert.rejects(up.install(new Uint8Array(100),'FM-1_900'),e=>e.code==='unsafe');
  assert(!sent.some(p=>p[1]===34),'No UPGRADE may precede refusal');
 }
}
console.log('Installer safety: source classification, read-only INFO, and pre-UPGRADE refusal passed.');
