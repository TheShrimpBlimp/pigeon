// Shared font story interaction; content stays available in the HTML.
const dialog=document.getElementById('font-story');
const button=document.getElementById('font-nerds');
button?.addEventListener('click',()=>dialog.showModal());
dialog?.addEventListener('click',event=>{if(event.target===dialog){const r=dialog.getBoundingClientRect();if(event.clientX<r.left||event.clientX>r.right||event.clientY<r.top||event.clientY>r.bottom)dialog.close();}});
