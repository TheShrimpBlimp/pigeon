// SPDX-License-Identifier: GPL-3.0-only
// MIDI input only: no SysEx, controller leases or writes to the instrument.
export async function connectController({navigator,select,status,send}) {
  const access=await navigator.requestMIDIAccess();let input=null;
  const release=()=>{
    if(input)input.onmidimessage=null;input=null;
    for(let ch=0;ch<16;ch++)send({type:'midi',data:[176|ch,123,0]});
  };
  const choose=()=>{
    release();input=access.inputs.get(select.value);
    if(!input){status.textContent='No MIDI controller connected.';return;}
    input.onmidimessage=e=>{const d=e.data;if(d[0]>=128&&d[0]<240)send({type:'midi',data:[d[0],d[1]||0,d[2]||0]});};
    status.textContent=`Connected: ${input.name||'MIDI controller'}. Use its keys to play the selected track.`;
  };
  const refresh=()=>{
    const previous=select.value;select.replaceChildren(...[...access.inputs.values()].filter(p=>p.state==='connected').map(p=>{const o=document.createElement('option');o.value=p.id;o.textContent=p.name||'MIDI input';return o;}));
    if([...select.options].some(o=>o.value===previous))select.value=previous;choose();
  };
  select.onchange=choose;access.onstatechange=refresh;refresh();
  return ()=>{release();access.onstatechange=null;select.onchange=null;};
}

// Optional PIGEON panel mode. Command 110 leases RAM-only control for 1.5 seconds;
// command 111 contains logical button bits, keys, encoder deltas and master.
export async function connectPanel({navigator,inputSelect,outputSelect,status,send}) {
  const access=await navigator.requestMIDIAccess({sysex:true});
  let input=null,output=null,timer=null,timeout=null,seen=false,closed=false;
  const command=on=>output?.send([240,125,70,76,110,on?1:0,247]);
  const clear=()=>{if(timer)clearInterval(timer);if(timeout)clearTimeout(timeout);timer=timeout=null;
    try{command(false);}catch{}if(input)input.onmidimessage=null;input=output=null;
    send({type:'buttons',mask:0});send({type:'keys',mask:0});};
  const choose=()=>{
    clear();seen=false;input=access.inputs.get(inputSelect.value);output=access.outputs.get(outputSelect.value);
    if(!input||!output){status.textContent='Choose the FM1 MIDI input and output.';return;}
    input.onmidimessage=({data:d})=>{
      if(d.length<7||d[0]!==240||d[1]!==125||d[2]!==70||d[3]!==76||d.at(-1)!==247)return;
      if(d[4]===110&&d[5]!==0){clear();status.textContent='This firmware does not support browser panel control.';return;}
      if(d[4]!==111||d.length!==22||d[5]!==1)return;
      seen=true;if(timeout)clearTimeout(timeout);timeout=null;
      send({type:'buttons',mask:d[6]|d[7]<<7});
      send({type:'keys',mask:d[8]|d[9]<<7|d[10]<<14|d[11]<<21});
      for(let role=0;role<7;role++)if(d[12+role]!==64)send({type:'enc',role,n:d[12+role]-64});
      send({type:'master',value:d[19]|d[20]<<7});
      status.textContent='FM1 panel connected: keys, knobs and buttons control the browser. Sound comes from your computer.';
    };
    try{command(true);}catch(e){clear();throw e;}
    timer=setInterval(()=>{try{command(true);}catch{clear();status.textContent='FM1 disconnected.';}},500);
    timeout=setTimeout(()=>{if(!seen){clear();status.textContent='No panel response. This mode needs PIGEON firmware with browser panel support; ordinary MIDI keys can still be used.';}},2500);
    status.textContent='Waiting for the FM1 panel…';
  };
  let firstRefresh=true;
  const refresh=()=>{
    for(const [select,ports] of [[inputSelect,access.inputs],[outputSelect,access.outputs]]){
      const previous=select.value,connected=[...ports.values()].filter(p=>p.state==='connected');
      select.replaceChildren(new Option('Choose FM1 port',''),...connected.map(p=>new Option(p.name||'FM1 MIDI',p.id)));
      select.value=connected.some(p=>p.id===previous)?previous:firstRefresh&&connected.length===1?connected[0].id:'';
    }
    firstRefresh=false;if(!closed)choose();
  };
  inputSelect.onchange=outputSelect.onchange=choose;access.onstatechange=refresh;refresh();
  return ()=>{closed=true;clear();inputSelect.onchange=outputSelect.onchange=null;access.onstatechange=null;};
}
