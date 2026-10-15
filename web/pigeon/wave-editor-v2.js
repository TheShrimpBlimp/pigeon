// SPDX-License-Identifier: GPL-3.0-only
export class WaveEditor {
  constructor(canvas,onChange) {
    Object.assign(this,{canvas,onChange,pcm:null,a:0,b:0,cursor:0,zoom:0,scroll:0,drag:null,cache:null});
    canvas.tabIndex=0;canvas.style.touchAction='none';canvas.style.cursor='crosshair';
    canvas.setAttribute('role','group');
    canvas.setAttribute('aria-description','Drag the start and end flags or the selected region. Click to place the cursor. Shift-drag selects a new region. Scroll to zoom; drag the overview to pan. Arrow keys move the cursor.');
    canvas.addEventListener('pointerdown',e=>{
      if(!this.pcm||e.button>1)return;
      e.preventDefault();canvas.focus();canvas.setPointerCapture(e.pointerId);
      const r=canvas.getBoundingClientRect(),p=this.position(e),[,span]=this.window(),t=span*12/r.width;
      this.anchor=p;this.original=[this.a,this.b];this.downX=e.clientX;this.downY=e.clientY;this.oldScroll=this.scroll;
      this.drag=(e.clientY-r.top)/r.height>.86?'overview':e.altKey||e.button===1?'pan':e.shiftKey?'range':Math.abs(p-this.cursor)<t&&(e.clientY-r.top)/r.height>.125&&(e.clientY-r.top)/r.height<.25?'cursor':Math.abs(p-this.a)<t?'start':Math.abs(p-this.b)<t?'end':this.timeline?'range':p>this.a&&p<this.b?'move':'cursor';
      if(this.timeline&&this.drag==='range'){this.a=this.b=p;this.onChange?.(p,p);}
      if(this.drag==='overview')this.move(e);else this.setCursor(p);
    });
    canvas.addEventListener('pointermove',e=>{
      if(this.drag)this.move(e);
      else if(this.pcm){const p=this.position(e),r=canvas.getBoundingClientRect(),[,span]=this.window(),t=span*12/r.width;
        canvas.style.cursor=(e.clientY-r.top)/r.height>.86?'grab':Math.abs(p-this.a)<t||Math.abs(p-this.b)<t?'ew-resize':p>this.a&&p<this.b?'grab':'crosshair';}
    });
    const finish=e=>{if(!this.drag)return;if(Math.hypot(e.clientX-this.downX,e.clientY-this.downY)>3)this.move(e);this.drag=null;};
    canvas.addEventListener('pointerup',finish);canvas.addEventListener('pointercancel',()=>this.drag=null);canvas.addEventListener('lostpointercapture',()=>this.drag=null);
    canvas.addEventListener('wheel',e=>{
      if(!this.pcm)return;e.preventDefault();
      if(e.shiftKey){const [,span]=this.window();this.pan((e.deltaY||e.deltaX)*span/canvas.getBoundingClientRect().width);}
      else this.setZoom(this.zoom-(e.deltaY||e.deltaX)*.005,(e.clientX-canvas.getBoundingClientRect().left)/canvas.getBoundingClientRect().width);
    },{passive:false});
    canvas.addEventListener('dblclick',()=>{if(this.pcm){this.zoom=0;this.scroll=0;this.viewChanged();}});
    if(typeof ResizeObserver!=='undefined')new ResizeObserver(()=>this.draw()).observe(canvas);
    canvas.addEventListener('keydown',e=>{
      if(this.onCommand){const key=e.key.toLowerCase(),modifier=e.ctrlKey||e.metaKey;
        const action=modifier?({z:e.shiftKey?'redo':'undo',y:'redo',c:'copy',x:'cut',v:'paste'})[key]:({backspace:'delete',delete:'delete',s:'split'})[key];
        if(action){e.preventDefault();e.stopPropagation();this.onCommand(action);return;}
        if(modifier&&key==='a'&&this.pcm){e.preventDefault();e.stopPropagation();this.a=0;this.b=this.pcm.length;this.onChange?.(this.a,this.b);this.draw();return;}
      }
      if(!this.pcm||!['ArrowLeft','ArrowRight','Home','End'].includes(e.key))return;e.preventDefault();
      const [,span]=this.window(),step=Math.max(1,Math.round(span/1000))*(e.shiftKey?10:1);
      this.setCursor(e.key==='Home'?0:e.key==='End'?this.pcm.length:this.cursor+(e.key==='ArrowLeft'?-step:step));
    });
  }
  window(){const n=this.pcm?.length||1,span=Math.min(n,Math.max(Math.min(64,n),Math.floor(n/2**this.zoom)));return [Math.round(Math.max(0,Math.min(1,this.scroll))*(n-span)),span];}
  position(e){const [start,span]=this.window(),r=this.canvas.getBoundingClientRect();return Math.max(0,Math.min(this.pcm.length,Math.round(start+(e.clientX-r.left)/r.width*span)));}
  setCursor(value){this.cursor=Math.max(0,Math.min(this.pcm?.length||0,Math.round(value)));this.canvas.dataset.cursor=String(this.cursor);this.onCursorChange?.(this.cursor);this.draw();}
  viewChanged(){this.canvas.dataset.zoom=String(this.zoom);this.canvas.dataset.scroll=String(this.scroll);this.onViewChange?.(this.zoom,this.scroll);this.draw();}
  setZoom(value,anchor=.5){if(!this.pcm)return;anchor=Math.max(0,Math.min(1,anchor));const [start,span]=this.window(),point=start+span*anchor;this.zoom=Math.max(0,Math.min(8,value));const [,next]=this.window();this.scroll=this.pcm.length===next?0:Math.max(0,Math.min(1,(point-next*anchor)/(this.pcm.length-next)));this.viewChanged();}
  pan(samples){const [start,span]=this.window();this.scroll=this.pcm.length===span?0:Math.max(0,Math.min(1,(start+samples)/(this.pcm.length-span)));this.viewChanged();}
  move(e){
    const p=this.position(e),n=this.pcm.length,min=Math.min(64,n),r=this.canvas.getBoundingClientRect(),[,span]=this.window();
    if(this.drag==='overview'){this.scroll=n===span?0:Math.max(0,Math.min(1,((e.clientX-r.left)/r.width*n-span/2)/(n-span)));this.viewChanged();return;}
    if(this.drag==='pan'){this.scroll=this.oldScroll;this.pan((this.downX-e.clientX)/r.width*span);return;}
    if(this.drag==='cursor'){this.setCursor(p);return;}
    if(Math.hypot(e.clientX-this.downX,e.clientY-this.downY)<3)return;
    if(this.drag==='start')this.a=Math.max(0,Math.min(p,this.b-min));
    else if(this.drag==='end')this.b=Math.min(n,Math.max(p,this.a+min));
    else if(this.drag==='move'){const delta=Math.max(-this.original[0],Math.min(n-this.original[1],p-this.anchor));this.a=this.original[0]+delta;this.b=this.original[1]+delta;}
    else {this.a=Math.min(p,this.anchor);this.b=Math.max(p,this.anchor);if(this.b-this.a<min)this.b=Math.min(n,this.a+min);if(this.b-this.a<min)this.a=this.b-min;}
    this.onChange?.(this.a,this.b);this.draw();
  }
  set(pcm,a=0,b=pcm?.length||0){if(pcm!==this.pcm){this.cache=null;this.overview=null;this.zoom=0;this.scroll=0;this.cursor=a;}this.pcm=pcm;this.a=a;this.b=b;this.cursor=Math.max(0,Math.min(pcm?.length||0,this.cursor));this.viewChanged();this.canvas.dataset.cursor=String(this.cursor);if(pcm)this.onCursorChange?.(this.cursor);}
  peaks(start,span,width){return Array.from({length:width},(_,x)=>{let lo=1,hi=-1;const begin=start+Math.floor(x*span/width),end=Math.min(this.pcm.length,start+Math.max(Math.floor((x+1)*span/width),Math.floor(x*span/width)+1));for(let i=begin;i<end;i++){lo=Math.min(lo,this.pcm[i]);hi=Math.max(hi,this.pcm[i]);}return [lo,hi];});}
  draw(){
    const c=this.canvas,w=Math.max(1,Math.round(c.getBoundingClientRect().width)||1000),h=164,dpr=globalThis.devicePixelRatio||1,ctx=c.getContext('2d'),waveHeight=124,columns=220;
    c.width=Math.round(w*dpr);c.height=Math.round(h*dpr);ctx.setTransform(dpr,0,0,dpr,0,0);
    ctx.fillStyle='#11150d';ctx.fillRect(0,0,w,h);if(!this.pcm)return;
    const [start,span]=this.window();
    if(!this.cache||this.cache.start!==start||this.cache.span!==span||this.cache.width!==columns)this.cache={start,span,width:columns,peaks:this.peaks(start,span,columns)};
    if(!this.overview||this.overview.length!==columns)this.overview=this.peaks(0,this.pcm.length,columns);
    ctx.imageSmoothingEnabled=false;c.dataset.waveColumns=String(columns);
    ctx.fillStyle='#a5af99';this.cache.peaks.forEach(([lo,hi],x)=>{
      const upper=34-Math.trunc(Math.min(63,Math.floor(hi*64))/2),lower=34-Math.trunc(Math.max(-64,Math.floor(lo*64))/2);
      ctx.fillRect(Math.floor(x*w/columns),Math.floor(upper*waveHeight/69),Math.max(1,Math.ceil(w/columns)),Math.max(1,Math.ceil((lower-upper+1)*waveHeight/69)));
    });
    for(let n=0;n<(this.segments?.length||0);n++){const segment=this.segments[n],x=(segment.a-start)/span*w;if(x>=0&&x<w){ctx.fillStyle='#748666';ctx.fillRect(Math.floor(x),22,1,waveHeight-22);ctx.font='12px "Little Character"';ctx.textAlign='left';ctx.fillText(String(n+1),x+3,waveHeight-4);}}
    const a=(this.a-start)/span*w,b=(this.b-start)/span*w;
    ctx.fillStyle='#b9ef7726';ctx.fillRect(a,0,b-a,waveHeight);
    for(const [x,label] of [[a,'S'],[b,'E']]){if(x<0||x>w)continue;const edge=Math.max(1,Math.min(w-2,x));ctx.fillStyle='#b9ef77';ctx.fillRect(edge,0,2,waveHeight);ctx.fillRect(Math.max(0,Math.min(w-24,edge-12)),0,24,21);ctx.fillStyle='#11150d';ctx.font= '14px "Little Character"' ;ctx.textAlign='center';ctx.fillText(label,Math.max(12,Math.min(w-12,edge)),16);}
    const cursor=(this.cursor-start)/span*w;ctx.strokeStyle='#f1f4e8';ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(cursor,22);ctx.lineTo(cursor,waveHeight);ctx.stroke();ctx.fillStyle='#f1f4e8';ctx.beginPath();ctx.moveTo(cursor-6,22);ctx.lineTo(cursor+6,22);ctx.lineTo(cursor,29);ctx.fill();
    ctx.fillStyle='#748666';this.overview.forEach(([lo,hi],x)=>ctx.fillRect(Math.floor(x*w/columns),152-hi*9,Math.max(1,Math.ceil(w/columns)),Math.max(1,(hi-lo)*9)));
    ctx.fillStyle='#b9ef7726';ctx.fillRect(start/this.pcm.length*w,141,span/this.pcm.length*w,23);ctx.strokeStyle='#b9ef77';ctx.lineWidth=2;ctx.strokeRect(start/this.pcm.length*w+1,142,Math.max(2,span/this.pcm.length*w-2),21);
    ctx.fillStyle='#a5af99';ctx.font= '12px "Little Character"' ;ctx.textAlign='left';ctx.fillText('DRAG OVERVIEW TO PAN',6,137);
  }
}
