// SPDX-License-Identifier: GPL-3.0-only
// Reuse the sample loader's waveform editor for browser-emulator imports.
import {WaveEditor} from '../pigeon/wave-editor-v2.js';
export function trimAudio(source,start,end){
  if(!source)throw Error('Choose an audio file first.');
  const duration=source.pcm.length/22050;
  if(!Number.isFinite(start)||!Number.isFinite(end)||start<0||end<=start||end>duration+0.001)throw Error('Choose valid start and end points inside the audio.');
  const a=Math.floor(start*22050),b=Math.min(source.pcm.length,Math.round(end*22050));
  if(b<=a)throw Error('Choose a nonempty section.');
  return {name:source.name,pcm:source.pcm.slice(a,b)};
}
export class FileEditor{
  constructor(container,{label,context,onApply,onStatus,settings,drum=false}){
    this.context=context;this.onApply=onApply;this.onStatus=onStatus;this.settings=settings;this.drum=drum;this.source=null;this.playing=null;this.busy=false;
    container.innerHTML=`<p class="edit-source song-info">Choose a file to edit.</p><canvas class="file-wave wave" aria-label="${label} waveform and trim selection"></canvas><div class="playback-controls"><div class="control-group"><span class="control-group-label">Listen</span><div class="control-row"><button type="button" class="preview-section secondary">Preview section</button><button type="button" class="preview-stop secondary">Stop preview</button></div></div><div class="control-group"><span class="control-group-label">View</span><div class="control-row"><label>Zoom<input class="trim-zoom" type="range" min="0" max="8" step="0.01" value="0"></label><output class="trim-cursor">Cursor 0.000 s</output><label class="loop-preview">Loop preview<select><option value="0">Off</option><option value="1">On</option></select></label></div></div></div><p class="wave-help">Drag to select · Drag S/E flags to trim · Scroll to zoom · Drag the overview to pan</p><details class="workspace-drawer export-settings"><summary>Export range${drum?'':' & quality'}</summary><div class="trim-fields"><label>Start (seconds)<input class="trim-start" type="number" min="0" step="0.01" value="0"></label><label>End (seconds)<input class="trim-end" type="number" min="0" step="0.01" value="0"></label></div><button type="button" class="trim-reset secondary">Use full file</button></details><p class="trim-estimate">Choose a file to trim and estimate its size.</p>${onApply?`<div class="song-actions commit-actions"><button type="button" class="apply-section">Apply to ${label}</button></div>`:''}`;
    this.container=container;this.start=container.querySelector('.trim-start');this.end=container.querySelector('.trim-end');
    this.wave=new WaveEditor(container.querySelector('canvas'),(a,b)=>{this.start.value=(a/22050).toFixed(5);this.end.value=(b/22050).toFixed(5);this.estimate();});
    this.wave.timeline=true;
    this.wave.onCursorChange=cursor=>{container.querySelector('.trim-cursor').textContent=`Cursor ${(cursor/22050).toFixed(3)} s`;};
    this.wave.onViewChange=zoom=>{container.querySelector('.trim-zoom').value=zoom;};
    for(const input of [this.start,this.end])input.oninput=()=>{this.wave.set(this.source?.pcm,Number(this.start.value)*22050,Number(this.end.value)*22050);this.estimate();};
    container.querySelector('.trim-zoom').oninput=e=>{this.wave.setZoom(Number(e.target.value));};
    container.querySelector('.preview-section').onclick=()=>this.preview();
    container.querySelector('.preview-stop').onclick=()=>this.stop();
    container.querySelector('.trim-reset').onclick=()=>{if(this.source)this.set(this.source);};
    if(onApply)container.querySelector('.apply-section').onclick=()=>onApply();
    this.setBusy(false);
  }
  set(source,range){
    this.stop();this.source=source;this.wave.zoom=0;this.wave.scroll=0;
    this.container.querySelector('.trim-zoom').value=0;
    const duration=source?source.pcm.length/22050:0;
    this.start.value=range?.start??0;this.end.value=range?.end??duration;
    for(const input of [this.start,this.end])input.max=duration;
    this.container.querySelector('.edit-source').textContent=source?`Selected: ${source.name} · ${duration.toFixed(2)} s`:'Choose a file to edit.';
    this.wave.set(source?.pcm,Number(this.start.value)*22050,Number(this.end.value)*22050);this.estimate();this.setBusy(this.busy);
  }
  range(){return {start:Number(this.start.value),end:Number(this.end.value)};}
  selection(){const {start,end}=this.range();return trimAudio(this.source,start,end);}
  estimate(){
    const {start,end}=this.range(),seconds=Math.max(0,end-start),{unlimited,bitrate}=this.settings();
    const audioBytes=seconds*(this.drum||unlimited?22050/2:bitrate*125+22050/576*4);
    this.container.querySelector('.trim-estimate').textContent=this.source?`Section: ${seconds.toFixed(2)} s · about ${((audioBytes+(this.drum?0:8704))/1024).toFixed(1)} KiB. Final size is checked when you apply.`:'Choose a file to trim and estimate its size.';
  }
  setBusy(busy){
    this.busy=busy;this.wave.canvas.style.pointerEvents=busy?'none':'';
    this.container.querySelectorAll('input,select,button').forEach(e=>e.disabled=busy||!this.source);
    this.container.querySelector('.preview-stop').disabled=!this.playing;
    if(busy)this.stop();
  }
  stop(){if(this.playing){const node=this.playing;this.playing=null;node.stop();node.disconnect();}this.container.querySelector('.preview-stop').disabled=true;}
  async preview(){
    try{
      const pcm=this.selection().pcm;this.stop();const context=this.context();await context.resume();
      const buffer=context.createBuffer(1,pcm.length,22050);buffer.copyToChannel(pcm,0);
      const node=context.createBufferSource();node.buffer=buffer;node.loop=this.container.querySelector('.loop-preview select').value==='1';node.connect(context.destination);this.playing=node;
      node.onended=()=>{if(this.playing===node){this.playing=null;node.disconnect();this.container.querySelector('.preview-stop').disabled=true;}};
      node.start();this.container.querySelector('.preview-stop').disabled=false;this.onStatus('Playing the selected section.');
    }catch(e){this.onStatus(e.message);}
  }
}

// Match the regular loader's Source / Listen / View / Export arrangement.
export function arrangeSampleWorkspace(){
 const $=id=>document.getElementById(id),root=document.querySelector('.sample-loader');
 const group=(title,nodes)=>{const box=document.createElement('div');box.className='control-group';const label=document.createElement('span');label.className='control-group-label';label.textContent=title;const row=document.createElement('div');row.className='control-row';row.append(...nodes);box.append(label,row);return box;};
 const toolbar=document.createElement('div');toolbar.className='sample-toolbar';toolbar.append(group('Bank & session',[$('import-session').closest('label'),$('save-bank'),$('save-session')]),group('Continue editing',[$('open-loader')]),group('Workspace',[$('clear-samples')]));
 root.querySelector('h2').after(toolbar);
 const status=document.createElement('div');status.className='sample-status-box';status.append($('sample-status'));toolbar.after(status);
 const storage=document.createElement('div');storage.className='sample-storage';const modeGroup=group('Storage',[$('storage-mode').previousElementSibling,$('storage-mode')]);storage.append(modeGroup,$('sample-meter'),$('sample-usage'),$('storage-help'));status.after(storage);
 for(let i=1;i<=2;i++){
  const input=$(`song${i}-file`),slot=input.closest('.sample-slot'),label=slot.querySelector(`label[for="song${i}-file"]`),name=$(`song${i}-name`);
  const heading=document.createElement('div');heading.className='song-header';const title=document.createElement('p');title.className='eyebrow';title.textContent=`SONG ${i}`;name.classList.add('song-name');heading.append(title,name);slot.prepend(heading);
  label.textContent='＋ Choose audio file';label.className='button secondary add-audio';label.append(input);heading.after(group('Source',[label]));
  const quality=slot.querySelector('.compression-controls');slot.querySelector('.export-settings').querySelector('.trim-fields').append(quality);
 }
 const kit=$('kit-files').closest('.sample-slot');kit.id='emulator-drums';
 const drumsLabel=kit.querySelector('label');drumsLabel.className='button secondary';drumsLabel.textContent='Choose drum files';drumsLabel.append($('kit-files'));
 const heading=document.createElement('div');heading.className='song-header';const title=document.createElement('h3');title.textContent='16 one-shot pads';heading.append(title,drumsLabel);kit.prepend(heading);
 const commit=document.createElement('div');commit.className='song-actions commit-actions';commit.append($('apply-kit'));kit.append(commit);
}
