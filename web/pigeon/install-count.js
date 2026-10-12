// SPDX-License-Identifier: GPL-3.0-only
// Only completed, identity-verified installs are reported; never notes or samples.
const key='pigeon-pending-install-events';
let flushing=false;
export async function flushInstallCounts(){
 if(flushing || !navigator.onLine || location.protocol!=='https:')return;
 flushing=true;
 try{
  const pending=JSON.parse(localStorage.getItem(key)||'[]');
  for(const event of pending){
   const response=await fetch(new URL('../../api/installs',import.meta.url),{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(event),signal:AbortSignal.timeout(5000)});
   if(!response.ok)break;
   const latest=JSON.parse(localStorage.getItem(key)||'[]').filter(e=>e.id!==event.id);localStorage.setItem(key,JSON.stringify(latest));
  }
 }catch{/* Statistics must never interrupt an update or offline editing. */}finally{flushing=false;}
}
export async function recordInstall(release,source){
 try{const pending=JSON.parse(localStorage.getItem(key)||'[]');pending.push({id:crypto.randomUUID(),release,source});localStorage.setItem(key,JSON.stringify(pending));}catch{return;}
 void flushInstallCounts();
}
window.addEventListener('online',()=>void flushInstallCounts());void flushInstallCounts();
