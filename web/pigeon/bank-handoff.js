// SPDX-License-Identifier: GPL-3.0-only
// Same-origin, local-only handoff. Never connects to a device or uploads audio.
import {inspect} from './bank.js';
import {validateArchive} from './device.js';
const validate=value=>value?.format==='pigeon-archive'?validateArchive(value):inspect(value);
async function files(mode, action) {
 const db=await new Promise((resolve,reject)=>{const r=indexedDB.open('pigeon-library',1);r.onupgradeneeded=()=>r.result.createObjectStore('files');r.onsuccess=()=>resolve(r.result);r.onerror=()=>reject(r.error);});
 try{return await new Promise((resolve,reject)=>{const tx=db.transaction('files',mode);let value;const r=action(tx.objectStore('files'));if(r)r.onsuccess=()=>value=r.result;tx.oncomplete=()=>resolve(value);tx.onerror=()=>reject(tx.error);tx.onabort=()=>reject(tx.error||Error('Local bank handoff cancelled.'));});}finally{db.close();}
}
export async function stageBank(image){validate(image);const id=crypto.randomUUID();await files('readwrite',s=>s.put(image,'emulator:'+id));return id;}
export async function readBank(id){if(!/^[0-9a-f-]{36}$/i.test(id))throw Error('Invalid emulator bank link.');const image=await files('readonly',s=>s.get('emulator:'+id));if(!image)throw Error('Emulator bank is no longer available. Return to the emulator and open the sample loader again.');validate(image);return image;}
export async function finishBank(id){await files('readwrite',s=>s.delete('emulator:'+id));}
