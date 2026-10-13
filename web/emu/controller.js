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
