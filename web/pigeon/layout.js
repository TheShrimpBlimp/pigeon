// SPDX-License-Identifier: GPL-3.0-only
// Arrange existing controls without replacing their handlers or state.
export function arrangeWorkspace() {
  const $ = id => document.getElementById(id);
  const group = (title, nodes, extra = '') => {
    const box = document.createElement('div'); box.className = `control-group ${extra}`;
    const label = document.createElement('span'); label.className = 'control-group-label'; label.textContent = title;
    const row = document.createElement('div'); row.className = 'control-row';
    box.append(label, row); nodes.filter(Boolean).forEach(n => row.append(n)); return box;
  };
  const fold = (title, nodes) => {
    const box = document.createElement('details'); box.className = 'workspace-drawer';
    const summary = document.createElement('summary'); summary.textContent = title; box.append(summary);
    nodes.filter(Boolean).forEach(n => box.append(n)); return box;
  };
  const secondary = nodes => nodes.filter(Boolean).forEach(n => n.classList.add('secondary'));
  const toolbar = document.querySelector('.toolbar');
  const local = group('Bank & session', [$('open').closest('label'), $('save'), $('open-emulator')]);
  const history = group('History', [$('undo'), $('redo')]);
  const transfer = group('Transfer', [$('upload')]);
  const device = document.querySelector('.device');
  const deviceActions = group('Device bank', [$('read'), $('backup'), $('restore'), $('recovery')]);
  const transferPanel = document.querySelector('.transfer-panel');
  const deviceDrawer = fold('FM1 connection & backups', [device, deviceActions, transferPanel]);
  deviceDrawer.id = 'device-tools';
  const connectLink = document.createElement('button'); connectLink.type = 'button'; connectLink.className = 'secondary'; connectLink.textContent = 'Connect / back up';
  connectLink.onclick = () => { deviceDrawer.open = true; deviceDrawer.scrollIntoView({behavior:'smooth',block:'start'}); };
  transfer.querySelector('.control-row').append(connectLink);
  toolbar.replaceChildren(local, history, transfer);
  secondary([...toolbar.querySelectorAll('button')].filter(n=>n.id!=='save'));
  secondary([...deviceActions.querySelectorAll('button')]);
  // Sending must reveal the existing review and progress without an extra click.
  $('upload').addEventListener('click', () => { deviceDrawer.open = true; deviceDrawer.scrollIntoView({behavior:'smooth',block:'start'}); });
  document.querySelector('.status-box').before(deviceDrawer);
  const library = $('library-name').closest('section');
  const libraryDrawer = fold('Saved session library', [library]);
  deviceDrawer.before(libraryDrawer);
  secondary([...library.querySelectorAll('button')].filter(n=>n.id!=='library-save'));
  const coffee = document.querySelector('.support-row'); if(coffee) document.querySelector('footer').prepend(coffee);
  const help = toolbar.nextElementSibling;
  if(help?.tagName === 'P') { help.classList.add('shortcut-note'); help.textContent = 'Undo: Ctrl/Cmd+Z · Redo: Ctrl/Cmd+Shift+Z · Edits stay local until you send them to the FM1.'; }
  (help?.tagName === 'P' ? help : toolbar).after(document.querySelector('.status-box'));
  document.querySelector('.preparation-note p').textContent = 'Add audio, edit your selections, then apply each song to the bank. Save it, try it in the emulator, or send it to your FM1 when ready.';
  for(let i=0;i<2;i++) {
    const song = $('wave'+i).closest('article');
    const actions = song.querySelector('.timeline-actions');
    const sourceButtons = [$(`draft-stored${i}`), $(`draft-clear${i}`)];
    $(`draft-undo${i}`).textContent='Undo'; $(`draft-redo${i}`).textContent='Redo'; $(`draft-paste${i}`).textContent='Paste';
    const edit = group('Selection', ['delete','keep','split'].map(n=>$(`draft-${n}${i}`)));
    const clipboard = group('Clipboard', ['cut','copy','paste'].map(n=>$(`draft-${n}${i}`)));
    const undo = group('History', ['undo','redo'].map(n=>$(`draft-${n}${i}`)));
    actions.replaceChildren(edit, clipboard, undo); secondary([...actions.querySelectorAll('button')]);
    const fileLabel = $('file'+i).closest('label'); fileLabel.classList.add('button','secondary','add-audio'); fileLabel.firstChild.textContent='＋ Add audio files';
    const fileHint=document.createElement('p');fileHint.className='file-hint';fileHint.textContent='Choose one or more files for this slot. Add more anytime — they append to the same song.';
    const sourceTools = group('Source', [fileLabel, ...sourceButtons]);
    secondary([...sourceTools.querySelectorAll('button')]);
    song.querySelector('.song-header').after(sourceTools);
    sourceTools.after(fileHint);
    const clipBuilder = song.querySelector('.clip-builder'); clipBuilder.open=false; clipBuilder.querySelector('summary').textContent='Clip settings · trim, gain & fades';
    const appendLabel = $('append'+i).closest('label'); appendLabel.hidden=true; // Main Add audio already appends.
    const settings = fold('Export range & quality', [song.querySelector('.trim'), $('source-name'+i)]);
    clipBuilder.before(settings);
    const playback = $('trim-zoom'+i).closest('.edit-fields'); playback.classList.add('playback-controls');
    const songActions = $('apply'+i).parentElement;
    const play = group('Listen', [$('preview'+i), $('trim-stop'+i), $('stored'+i)]);
    secondary([...play.querySelectorAll('button')]);
    const view = group('View', [ $('trim-zoom'+i).closest('label'), $('trim-cursor'+i), $('trim-loop'+i).closest('label') ]);
    playback.replaceChildren(play,view); song.querySelector('.timeline-strip').after(playback);
    songActions.classList.add('commit-actions');
    song.querySelector('.wave-help').textContent='Drag to select · Backspace deletes · S splits · Scroll to zoom · Drag clips to reorder';
  }
  const chop = $('chop-wave').closest('section'); chop.id='chop-workspace';
  const playbackFields = ['chop-choke','chop-pitch','chop-attack','chop-release','chop-mode','chop-loop'].map(n=>$(n).closest('label'));
  const playbackSettings = fold('Chop playback settings', [group('Playback',playbackFields)]);
  const assign = fold('Copy, move & detect chops', [
    group('Detect',[$('chop-sensitivity').closest('label'),$('chop-auto')]),
    group('Assign keys',[$('chop-copy').closest('label'),...['chop-paste','chop-full','chop-swap','chop-move'].map($)])
  ]);
  const chopActions = $('chop-apply').parentElement;
  chopActions.replaceChildren(group('Edit', ['chop-duplicate','chop-split','chop-merge'].map($)),group('Listen', ['chop-play','chop-stop'].map($)), $('chop-apply'));
  secondary([...chopActions.querySelectorAll('button')].filter(n=>n.id!=='chop-apply'));
  chopActions.after(playbackSettings,assign);
  chop.querySelector('.section-heading').nextElementSibling.textContent='Choose a song and load its waveform. Select a chop to adjust its range, then apply. Scroll to zoom; drag the overview to pan.';
  const nav = document.createElement('nav');nav.className='workspace-nav';nav.setAttribute('aria-label','Sample workspace');
  const drums = $('pads').closest('section'); drums.id='drum-workspace';
  const patterns=$('pattern-grid').closest('section');patterns.id='pattern-workspace';
  drums.after(patterns);
  const firmware = [...document.querySelectorAll('main section')].find(n=>n.querySelector('h2')?.textContent==='Firmware update');
  if(firmware) {const drawer=fold('Firmware update',[]);firmware.before(drawer);drawer.append(firmware);}
  const rename=$('rename-song').closest('.edit-fields');
  const naming=fold('Song label',[rename]);chop.append(naming);
  for(const [id,label] of [['songs','Songs'],['chop-workspace','Chops'],['drum-workspace','Drums'],['pattern-workspace','Patterns'],['device-tools','FM1']]) {
    const a=document.createElement('a');a.href='#'+id;a.textContent=label;nav.append(a);
  }
  document.querySelector('.preparation-note').after(nav);
  secondary([...$('drum-settings-host').querySelectorAll('button')].filter(n=>n.id!=='pad-settings-apply'));
  secondary([...patterns.querySelectorAll('button')].filter(n=>['pattern-apply','session-save'].includes(n.id)===false));
}
