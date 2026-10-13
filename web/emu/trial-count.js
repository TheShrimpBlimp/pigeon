// SPDX-License-Identifier: GPL-3.0-only
const endpoint=new URL('../../api/trials',import.meta.url);
const KEY='pigeon-emulator-trial-v1';
export async function trialTotal(){const r=await fetch(endpoint,{cache:'no-store'});if(!r.ok)throw Error('Counter unavailable');const data=await r.json();if(!Number.isSafeInteger(data.total)||data.total<0)throw Error('Invalid counter');return data.total;}
export async function recordTrial(){
 let id;try{id=localStorage.getItem(KEY);if(!id){id=crypto.randomUUID();localStorage.setItem(KEY,id);}}catch{return trialTotal();}
 const r=await fetch(endpoint,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id})});
 if(!r.ok)throw Error('Counter unavailable');const data=await r.json();if(!Number.isSafeInteger(data.total)||data.total<0)throw Error('Invalid counter');return data.total;
}
