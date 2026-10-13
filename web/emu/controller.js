// SPDX-License-Identifier: GPL-3.0-only
export function panelFrame(data) {
  if (data.length !== 22 || data[0] !== 240 || data[1] !== 125 || data[2] !== 70 || data[3] !== 76 || data[4] !== 111 || data[5] !== 1 || data[21] !== 247) return null;
  if ([...data.slice(1,21)].some(v=>v>127)) return null;
  return {buttons:data[6]|data[7]<<7, keys:data[8]|data[9]<<7|data[10]<<14|data[11]<<21,
    enc:[...data.slice(12,19)].map(v=>v-64), master:Math.min(1023,data[19]|data[20]<<7)};
}
export async function connectController({navigator, select, status, send, onPanel}) {
  const access = await navigator.requestMIDIAccess({sysex:true});
  let input=null, output=null, timer=null, full=false;
  const command=on=>output?.send([240,125,70,76,110,on?1:0,247]);
  const release=()=>{
    if(timer) clearInterval(timer);timer=null;
    try{command(false);}catch{}
    if(input) input.onmidimessage=null;
    input=output=null;full=false;
    onPanel({buttons:0,keys:0,enc:Array(7).fill(0),master:null});
    for(let channel=0;channel<16;channel++) send({type:'midi',data:[176|channel,123,0]});
  };
  const choose=()=>{
    release();input=access.inputs.get(select.value);
    if(!input){status.textContent='No MIDI input connected. Connect your FM1 by USB.';return;}
    output=[...access.outputs.values()].find(p=>p.state==='connected' && p.name===input.name);
    input.onmidimessage=e=>{
      const d=e.data, frame=panelFrame(d);
      if(frame){full=true;status.textContent='FM1 connected · keys, knobs, buttons and master';onPanel(frame);return;}
      if(d.length===8 && d[0]===240 && d[4]===110 && d[5]===0 && d[6]===1){full=true;status.textContent='FM1 connected · full panel controller';return;}
      if(!full && d[0]>=128 && d[0]<240)send({type:'midi',data:[d[0],d[1]||0,d[2]||0]});
    };
    status.textContent='MIDI keys connected. Full panel requires controller-enabled Pigeon firmware.';
    if(output){command(true);timer=setInterval(()=>{try{command(true);}catch{release();status.textContent='FM1 disconnected.';}},500);}
  };
  const refresh=()=>{
    const previous=select.value;
    select.replaceChildren(...[...access.inputs.values()].filter(p=>p.state==='connected').map(p=>{
      const option=document.createElement('option');option.value=p.id;option.textContent=p.name||'MIDI input';return option;
    }));
    if([...select.options].some(o=>o.value===previous))select.value=previous;
    else {const fm=[...select.options].find(o=>/fm.?1|pigeon/i.test(o.textContent));if(fm)select.value=fm.value;}
    choose();
  };
  select.onchange=choose;access.onstatechange=refresh;refresh();
  return ()=>{release();access.onstatechange=null;select.onchange=null;};
}
