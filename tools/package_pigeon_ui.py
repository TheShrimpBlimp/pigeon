#!/usr/bin/env python3
"""Package native firmware screenshots into labeled review sheets and a local gallery."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import html,json,textwrap,zipfile
root=Path('build/pigeon-ui-atlas');native=root/'native';(root/'large').mkdir(exist_ok=True)
rows=[dict(zip(('id','category','label'),line.split('\t'))) for line in (root/'pages.tsv').read_text().splitlines()]
font_path=Path('assets/fonts/LittleCharacterRegular.ttf');label_font=ImageFont.truetype(str(font_path),14);heading_font=ImageFont.truetype(str(font_path),22)
for row in rows:
 im=Image.open(native/(row['id']+'.ppm')).convert('RGB');assert im.size==(240,240);assert len(im.getcolors(57600))>2,row['id'];im.save(native/(row['id']+'.png'));im.resize((720,720),Image.Resampling.NEAREST).save(root/'large'/(row['id']+'.png'))
cols,per_sheet=4,24;sheet_paths=[]
for start in range(0,len(rows),per_sheet):
 group=rows[start:start+per_sheet];n=(len(group)+cols-1)//cols
 sheet=Image.new('RGB',(1120,76+n*302),'#11150d');d=ImageDraw.Draw(sheet);number=start//per_sheet+1
 d.text((24,14),f'PIGEON v0.3 / UI atlas / sheet {number}',font=heading_font,fill='#b9ef77')
 d.text((24,44),'Actual firmware pixels / synthetic fixture data / current local build',font=label_font,fill='#a5af99')
 for index,row in enumerate(group):
  x=24+(index%cols)*276;y=76+(index//cols)*302
  d.text((x,y),f"{start+index+1:03d} / {row['category']}",font=label_font,fill='#b9ef77')
  for line_index,line in enumerate(textwrap.wrap(row['label'],width=30)[:2]):d.text((x,y+18+line_index*16),line,font=label_font,fill='#dce5d1')
  sheet.paste(Image.open(native/(row['id']+'.png')),(x,y+54))
 p=root/f'sheet-{number:02d}.png';sheet.save(p);sheet_paths.append(p)
# A compact review image covers the main layout families without shrinking pixels.
review_keys=('track-1-view-00','track-1-view-01','track-1-view-05','track-1-view-19',
             'track-1-view-07','track-1-view-14','track-1-view-08','track-1-view-09',
             'track-1-view-21','track-1-view-10','track-1-layer-2','menu-2-row-09')
review=[]
for key in review_keys:
 matches=[r for r in rows if r['id'].endswith(key)]
 if matches:review.append(matches[0])
# Menu IDs are compatibility IDs; select the audio tab by its rendered label.
if len(review)<12:
 review.append(next(r for r in rows if r['category']=='Settings' and 'SPEAKER EQ' in r['label']))
overview=Image.new('RGB',(1120,76+3*302),'#11150d');d=ImageDraw.Draw(overview)
d.text((24,14),'PIGEON v0.3 / unified UI / local preview',font=heading_font,fill='#b9ef77')
d.text((24,44),'Actual firmware renders / Little Character / synthetic fixtures',font=label_font,fill='#a5af99')
for i,r in enumerate(review[:12]):
 x=24+(i%4)*276;y=76+(i//4)*302
 for j,line in enumerate(textwrap.wrap(r['label'],30)[:2]):d.text((x,y+j*16),line,font=label_font,fill='#dce5d1')
 overview.paste(Image.open(native/(r['id']+'.png')),(x,y+44))
overview.save(root/'overview.png')
Image.open(sheet_paths[0]).convert('RGB').save(root/'PIGEON-v0.3-UI-atlas.pdf',save_all=True,append_images=[Image.open(p).convert('RGB') for p in sheet_paths[1:]],resolution=120)
manifest={'version':'v0.3 local build','screens':len(rows),'data':'Synthetic audio and patterns; no user samples or device reads.','excluded':['Unused view 17','Disabled engine picker, synth graph pages and user-preset pages','ROM bootloader and target-only crash screens, which are outside the application UI renderer'],'pages':rows}
(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
figures=''.join(f'<figure data-category="{html.escape(r["category"])}" data-label="{html.escape(r["label"])}"><a href="large/{r["id"]}.png"><img loading="lazy" width="240" height="240" src="native/{r["id"]}.png" alt="{html.escape(r["label"])}"></a><figcaption>{html.escape(r["id"])}<br>{html.escape(r["label"])}</figcaption></figure>' for r in rows)
options=''.join(f'<option>{html.escape(c)}</option>' for c in dict.fromkeys(r['category'] for r in rows))
(root/'index.html').write_text('''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>PIGEON UI atlas</title><style>body{background:#11150d;color:#dce5d1;font:15px monospace;margin:24px}h1,a{color:#b9ef77}header{max-width:850px}nav{display:flex;gap:16px;flex-wrap:wrap;position:sticky;top:0;background:#11150d;padding:15px 0}select,input{font:inherit;background:#1a2116;color:inherit;border:1px solid #526546;padding:8px;max-width:100%;box-sizing:border-box}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:20px}figure{margin:0;min-width:0}figure img{image-rendering:pixelated;max-width:100%;height:auto}figcaption{font-size:12px;margin:8px 0;overflow-wrap:anywhere}[hidden]{display:none!important}</style><header><h1>PIGEON v0.3 UI atlas</h1><p>CURRENT_COUNT renders from the current local firmware. Synthetic samples and patterns; no device access. Click any screen for a 3× render.</p><p>Includes application pages, track variants, settings rows, credits, dialogs, overlays, calibration prompts, and representative transport/empty states. Disabled legacy pages, ROM bootloader and target-only crash screens are excluded.</p><p><a href="PIGEON-v0.3-UI-atlas.pdf">Contact-sheet PDF</a> · <a href="manifest.json">Page manifest</a></p></header><nav><input id="search" placeholder="Search pages" aria-label="Search pages"><select id="category" aria-label="Category"><option value="">All categories</option>'''+options+'''</select><output id="count">CURRENT_COUNT screens</output></nav><main>'''+figures+'''</main><script>const search=document.querySelector('#search'),category=document.querySelector('#category');function filter(){let n=0;document.querySelectorAll('figure').forEach(f=>{f.hidden=!!((category.value&&f.dataset.category!==category.value)||!f.textContent.toLowerCase().includes(search.value.toLowerCase()));if(!f.hidden)n++});document.querySelector('#count').textContent=n+' screens'}search.oninput=category.onchange=filter;</script>''')
gallery=root/'index.html'
gallery.write_text(gallery.read_text().replace('CURRENT_COUNT',str(len(rows))))
with zipfile.ZipFile(root/'PIGEON-v0.3-UI-renders.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in [root/'index.html',root/'manifest.json',root/'pages.tsv',root/'PIGEON-v0.3-UI-atlas.pdf',root/'overview.png',*sheet_paths,*native.glob('*.png'),*(root/'large').glob('*.png')]:z.write(p,p.relative_to(root))
print(f'{len(rows)} native screens, {len(sheet_paths)} sheets, searchable gallery, PDF and ZIP created')
