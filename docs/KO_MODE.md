# PIGEON tutorial — v0.3 candidate

PIGEON has three tracks: Song 1, Drums and Song 2. Each song is one audio
source with up to 32 chops; a source can combine several imported files.
There is no fourth playable track in the sampler profile.

This guide describes the local v0.3 firmware candidate and the published
Sample Loader 2.0. v0.3 has not been published or physically verified. The
published firmware and web tools use 408 KiB shared storage. The candidate
proposes 496 KiB and eight shared song voices plus four dedicated drum voices;
physical performance and storage checks remain pending.

## 1. Prepare your FM1

Back up samples and projects before changing firmware. When coming from
another custom firmware, revert to official M-VAVE V15 using that firmware’s
documented stock-return procedure, then install PIGEON. Use a charged FM1,
a direct USB data cable and desktop Chrome or Edge. Stop playback, release
keys, close other MIDI apps and keep the computer awake during transfers.

The published installer installs its published package, not this local
candidate. Building a candidate does not update the website or your FM1.
See [building](https://github.com/TheShrimpBlimp/pigeon/blob/main/BUILDING.md) and [recovery](PIGEON_RECOVERY.md).

## 2. Make a song in Sample Loader 2.0

Open the [sample loader](https://theshrimpblimp.github.io/pigeon-website/webapp/pigeon/).
It starts empty; audio is decoded and edited on your computer.

1. Under Song 1, choose **Add audio files**. Select one or several files.
   Adding more files later appends them to the same song slot.
2. Preview the audio. Drag the waveform to select a range. Backspace/Delete
   removes that range; **Keep selection** removes everything outside it.
3. Click to place the cursor and press **S**, or **Split at cursor**, to chop
   the timeline. Drag the clip chips to rearrange sections. Copy, cut and paste
   work at the cursor. Ctrl/Cmd+Z undoes a draft edit; Ctrl/Cmd+Shift+Z redoes it.
4. Scroll over the waveform to zoom. Alt-drag or drag the overview to pan.
   **Clip settings · trim, gain & fades** exposes each clip’s precise settings.
   Each included clip becomes a chop when applied, up to 32 per song.
5. Open **Export range & quality** to change exact range or MP3 quality
   (24–64 kbps). Lower quality uses less space. Preview the result, then press
   **Apply to local Song 1**. Draft edits do not affect the bank until Apply.
6. Repeat for Song 2. Both songs and the drum kit share the meter’s capacity;
   the duration limit is combined, not per song. Apply Song 1 first.

The waveform uses a firmware-style 220-column envelope. You can stitch
individual samples into one song and use its chops as separate playable hits.
No chop naming is required. **Edit stored song** returns applied bank audio
to the timeline; **Clear draft** discards the current draft.

## 3. Add drums and refine chops

Choose up to 16 drum files in pad order. Choosing files for the kit applies
those hits locally; selecting a file for one pad replaces that pad. Preview
pads and edit their pitch, level, decay and choke groups under Drum pad settings.

In **Chop editor**, choose a song and **Load waveform**. Select a chop, drag
its S/E markers or enter exact boundaries, then **Apply chop**. Additional
playback settings contain pitch, envelopes, trigger and loop options. **Copy,
move & detect chops** contains transient detection and key assignment tools.
Copy whole chop overwrites a destination; Swap exchanges keys; Move shifts
intervening assignments. Pattern note numbers stay the same, so rearranging
chops changes what those notes play. These operations copy assignments within
the same song, not audio between songs. Applying bank edits does not send USB data.

## 4. Save, try or transfer your bank

- **Save bank** downloads `.kob`: encoded samples, chops and playback settings.
- **Try this bank in emulator** opens the applied bank or loaded full session
  in the hosted emulator. Switch it on to load the staged content.
- In the emulator, **Load bank / session** accepts `.kob` or `.pigeon`. Sessions
  also restore patterns and supported settings. Unlimited browser storage can
  exceed hardware capacity; reduce the bank before moving it to an FM1.
- **Connect / back up** opens FM1 ports, readback and backup actions. Connect
  matching MIDI input/output ports, then **Send bank to FM1** and review the
  replacement. Keep USB connected until readback verification finishes.

Back up the existing device separately before replacement. Sending a bank
replaces samples and chop settings; it does not install firmware or restore
all patterns. **Back up full session** saves the complete supported device
session. **Restore session to FM1** restores samples, projects and settings
from a full session, replacing existing contents and creating a recovery backup
first. Session object commits are separate; interrupted restores can be partial.

## 5. Play and sequence on the FM1

Use ALGORITHM to choose Song 1, Drums or Song 2. K1/K2 set the selected chop’s
start/end, K3 scrolls and K4 zooms. OCT changes pitch. PRESETS reaches the later
chops; all 27 keys are available, with a second bank for chops 28–32.
Drums have 16 one-shot pads. Use the [control table](PIGEON_CONTROLS.md) for
chop copy/swap/move, envelopes, tempo tools and mixer controls.

Press SEQ to open the selected track’s grid. K1 sets BPM, K2 chooses TEMPO or
SAMPLE END timing, K3 selects a step and K4 sets 1–64-step length while stopped.
REC arms entry: keys write steps while stopped or record live while playing.
ENV opens step details; LFO opens timing and recording preferences. PLAY
starts/stops playback. Timing preferences belong to the sample-bank metadata;
sequence notes belong to the current phrase.

## 6. Save a phrase and a complete session

SCL opens Patterns. Select A–D, stop playback and release keys, then press SAVE.
One phrase stores all three sequences together, including timing, automation
and arrangement. Patterns share the loaded bank and chop layouts; changing A–D
does not load different samples. The unsaved indicator warns before discarding
phrase edits. ARP opens the mixer; arrangement tools chain phrases into sections.

In the loader, open a full `.pigeon` session to edit its current project or A–D
patterns. **Save full session** downloads those edits with the bank. In the
emulator, **Save session (.pigeon)** and **Open session in sample loader** carry
patterns and settings as well as audio. A `.kob` bank alone does not contain them.

**Saved session library** stores named banks and full sessions in this browser.
Download copies for durable backups. Working data persists locally, but browser
storage can be cleared or evicted. Undo history is temporary; undo cannot reverse
a device transfer. Wait for **Offline ready** before disconnecting the internet.

## Limits and release status

There is no direct line recording, resampling, USB disk browser or reverse
playback of compressed songs. MIDI controllers (including the FM1) can play
the browser instrument. Full physical panel relay requires compatible firmware
and still needs hardware verification.

Read [storage formats](KO_STORAGE.md), the [v0.3 release checklist](PIGEON_V03_RELEASE.md)
and the [historical technical reference](KO_LEGACY_REFERENCE.md) for more detail.
