# PIGEON 0.5 tutorial and control guide

This guide matches the **v0.5 development candidate** and its browser tools.
Final hardware acceptance is pending. The public site remains on its deployed
release until acceptance. Keep a complete recovery archive before updating;
older firmware cannot interpret all 0.5 playback metadata or its expanded bank.

## Your first song

1. **Back up and install.** Save a full `.pigeon` archive in the sample loader.
   For the candidate use `build/pigeon.fwsc` and follow the
   [hardware checklist](PIGEON_V05_HARDWARE_TEST.md). Keep USB power connected
   until restart. See [recovery](PIGEON_RECOVERY.md) when changing firmware families.
2. **Add audio locally.** In the sample loader, choose **Add audio files** for
   Song 1. Drag a range to select, Keep selection to crop, or S to split at the
   cursor. Add more files to append sections. Drag their chips into order.
3. **Apply the song.** Preview, choose MP3 quality under **Export range & quality**,
   then **Apply to local Song 1**. Repeat for Song 2 and add up to 16 drum hits.
   Both songs and the kit share **500 KiB usable** in this candidate. The meter
   includes encoded audio, waveforms and seek indices; duration is not per track.
4. **Try or send.** Save bank (`.kob`) or open it in the emulator. To use hardware,
   connect matching FM1 MIDI ports, back up, then Send bank. This replaces samples
   and chop settings, preserving device patterns/settings. Wait for readback.
5. **Shape the chops.** Select Song 1 or Song 2 with ALGORITHM. Play a key, adjust
   K1 start/K2 end, K3 scroll/K4 zoom and OCT pitch. LFO cycles K1 through the
   playback controls below. PRESETS reaches later chops, including keys 28–32.
6. **Record a sequence.** SEQ opens the grid; SEQ again opens the piano roll.
   While stopped and unarmed, keys toggle notes on the selected step. Arm REC,
   hold a chord and release all keys to advance once. During playback, armed REC
   records live. K3 selects steps and K4 sets 1–64-step length while stopped.
7. **Save a pattern.** SEL opens Patterns. Choose A–D, stop/release notes and SAVE.
   Each pattern contains all three tracks; all patterns share the loaded samples.
   SEL opens Arrangement. K1 selects a section, K2 its selected-track pattern,
   K3 repeats and K4 inserts/deletes. FX captures track mutes into the section.
8. **Keep a complete session.** Back up the FM1 or save a full emulator session
   as `.pigeon`. A `.kob` alone has no sequence or saved A–D patterns. Keep a
   downloaded copy: browser storage can be cleared and undo does not survive reload.

For the full shared feature map, see [quick controls](PIGEON_CONTROLS.md),
[sequence tools retained from 0.4](PIGEON_V04_GUIDE.md#sequence-tools) and
[per-note timing](PIGEON_NOTE_TIMING.md). Scratch is excluded from 0.5.

## Chop controls on the FM1

On a song waveform, tap **LFO** to cycle the function of **K1**:

| Mode | K1 |
| --- | --- |
| Loop start | Moves the original chop boundary |
| Playback point | Chooses the first-pass starting point inside the chop |
| Direction | Right: reverse; left: forward |
| Cents | Fine tuning from −100 to +100 cents |
| Gain | Adjusts the chop's level; 100% is unchanged |
| Crossfade | Loop-edge overlap from 0 to 8 ms |

K2 still moves the end, K3 scrolls and K4 zooms. Higher zoom gives finer playback
point adjustment. PRESETS or a chop key selects the chop. OCT changes semitone
pitch. Six LFO taps return to boundary mode. Settings save automatically after
transport and voices stop; let the save finish before switching off.

Reset restores the original assignments stored with the audio, including their
boundaries. It resets the current song/layout only; the other song and alternate
layout stay intact. **EDIT → K4 right** resets all chops; **K4 left** toggles undo/redo for the last chop edit,
including reset, split/join, playback points and direction. Audio is unchanged.

Hold **SEL and a chop key** while moving K1/K2 to audition your edit immediately:
K1 retriggers with the current playback settings, K2 auditions the ending. This
local audition bypasses recording, MIDI output and sequencing. Release the key
to stop; ordinary editing stays silent.

A separate playback point affects the first pass only. A forward held loop
returns to its original start; reverse returns to its original end. Reset the
point to use the normal start/end for the chosen direction. SEL ending audition retains its dedicated trigger behavior. Each chop's direction,
tuning, level and crossfade follow copies, moves, duplicates and undo/redo.

Crossfade blends the last part of a loop into its first part, then skips the
already-overlapped audio. Each repeated loop is shorter by that overlap. Short
chops limit the overlap to half their length. One-shots do not crossfade. The
maximum is 8 ms (192 source samples at most). Both ADPCM and MP3 are supported;
edge preparation and MP3 decoding run in the main loop, not the audio interrupt.

## Sample loader

1. Load or import a song, then open **Chops** and **Load waveform**.
2. Select the destination chop. Open **Chop playback settings**.
3. Set the playback point, direction, cents, gain and loop crossfade. Drag the
   orange **P** marker or use **Start playback at cursor** to position the start.
4. **Preview chop** hears the current fields before applying. Enable **Audition
   edits** to restart preview automatically after boundary or playback changes.
   **Stop preview** stops the current take. **Apply chop** saves the fields locally.
   Preview includes direction, starting point, pitch/cents, gain and loop overlap.
5. **Normalize chop** measures the decoded selected region and stores a gain
   setting targeting 95% peak. It does not rewrite or re-encode the source audio.
   Gain is capped below 16×; extremely quiet material may not reach that target.
   Silence retains unity gain. **Reset playback point** restores the normal
   direction-dependent start.
6. Save a bank/session or open it in the emulator. A v0.5 capability check blocks
   sending modern chop metadata to older hardware before any erase or write.

**Reset all chops** restores the original bank assignments and default playback
settings. The chop editor’s **Undo chop edit / Redo chop edit** controls use the
same local workspace history as the main toolbar. Previewing never adds history
or changes the bank. Applied edits and resets can be undone/redone.

The source audio remains unchanged by normalization, gain, cents, direction and
crossfade settings. All bank/session transfers retain the new metadata. Editing
chops remains compatible with importing older banks; new edits require v0.5.

## Pattern launches

This extends the existing Patterns/Arrangement workflow, not the chop queue.

- Tap SEL from the waveform to open **Patterns**. Play a saved pattern.
- While playing, **K2 right** chooses **pattern end** and **K2 left** chooses
  **next bar**. While stopped, K2 retains its saved-pattern copy function.
- Choose A–D using the first four chop keys or K1. The pending tile blinks and
  the page states when the change will occur. All three tracks switch together.
- Choose the same pending pattern again to cancel it. Choosing another pattern
  replaces the pending choice; it does not append a FIFO list.
- Pattern-end timing follows the pattern clock track. For standalone sample-end
  playback, it waits for the last step's sounding chop instead of a tempo estimate.
- Arrangement sections and repeat counts remain available for planned songs.

## Session manager

A **bank (.kob)** contains samples and chop settings. A **complete session
(.pigeon)** also contains the current sequence, saved A–D patterns and device
settings. To obtain a complete session, open one from the emulator/computer or
back up the connected FM1. A bank alone cannot supply missing patterns.

The local library shows what each entry contains, song names, drum-pad count,
encoded size, and saved pattern names when available. Choose a useful library
name, and select **Sample bank only** if you deliberately want to exclude patterns.

- Saving under an existing name previews what will be overwritten.
- Loading an entry or opening another file previews the replacement. Loading
  changes only the browser workspace; it does not write to the FM1.
- Library loads participate in Ctrl/Cmd+Z undo.
- **Download current session** exports the complete session currently open.
  **Download selected** exports the selected library entry in its own format.
- **Send bank** preserves device patterns/settings. **Restore session** replaces
  them together with the bank, with a complete recovery archive saved first.
- Delete/rename actions retain explicit names and confirmations; local library
  data is browser storage, so keep downloaded copies outside the browser too.

## Collect chops from different songs

The sample loader's **Chop library** is separate from saved banks and sessions.
It stores compact decoded audio plus each chop's playback settings in this
browser. Changing the open bank or replacing a song does not replace the library.

1. Open a bank and apply any chop edits. Choose **Source song**, select one or
   more **Chops to collect**, name the collection and press **Collect selected
   chops**. Only regions used by those chops are kept; overlaps share audio.
2. Open another bank/song and collect more. Each collection stays independent.
3. Select saved chops and **Add selected to arrangement**. Move them up/down or
   remove assignments. Adding the same chop again creates another assignment
   without duplicating its source audio in a song bank.
4. Choose Song 1, Song 2 or Drum kit, then **Build into local bank**. Song slots
   retain independent boundaries, playback points, direction, gain, tuning,
   envelopes and loop settings. The other slots remain intact. Song 1 can also
   start a new bank; Song 2 and kit destinations require an existing bank.
5. **Download collection** saves the selected collection as `.pigeon-chops`,
   including its audio and settings. **Import collection** restores it in this
   or another browser. Browser storage alone is not a backup.

Collections are mono 22.05 kHz. Collecting a lower-rate bank converts its decoded
sample rate without changing its pitch. Song assembly encodes a fresh MP3 at the
chosen quality and enforces the 500 KiB development bank limit. Uploads also
check the connected firmware’s capacity; older layouts retain their smaller limits. The library itself
uses browser storage, with a 10-minute limit per collection/assembled song.

Drum destinations support up to 16 one-shots. Their direction, playback point,
pitch/cents, gain and envelope fades are rendered into the extracted hit; choke
settings transfer. Loop/latch and loop crossfade do not apply to one-shots. The
library's original audio and settings remain available for later song assembly.

Collecting and previewing do not change the bank. Building replaces only the
chosen local destination and participates in the existing workspace undo/redo.
Collections and the arrangement are separate from that history; deleting a
collection requires confirmation. The arrangement is temporary, while collected
chops persist across reloads. Hardware is written only through the existing
Send bank workflow. This feature does not record live output or resample a
performance on the FM1.

## Keep selected chops and trim the rest

In the loader's Chop library, select the source song and the chops to retain,
then choose **Keep selected chops and trim song**. The preview gives the discarded
length and the actual encoded space change before confirmation. Only the union
of retained regions is encoded; overlaps share audio and chop boundaries move
with their regions. Playback settings remain attached to the retained chops.
The other song and kit stay intact, and local Undo restores the original bank.
Chop numbers become consecutive, so sequences referring to old chop numbers
may need reassignment. The FM1 operation below instead preserves key numbers.

## Keep selected chops on the FM1

1. Select a song, stop playback and release notes. Open **EDIT → LFO → SEL**.
2. K1 selects a chop; keys audition. REC toggles its mark, or K3 right/left marks
   or unmarks it. K4 right marks all; left clears marks.
3. K2 selects Song 1 or Song 2 as destination. Keeping in the same song preserves
   key numbers and leaves discarded keys silent. Choosing the other song appends
   to its empty keys while retaining its existing assignments.
4. SEL asks to keep the selection; SEL confirms and LFO backs out. The retained
   regions are merged, decoded and encoded as 22.05 kHz ADPCM, preserving playback
   settings. This rewrites audio, unlike ordinary boundary/gain edits. Back up first.

## Reclaim unused sample space

Open the USB sampler while idle, then press **SEL twice** to confirm reclamation.
This compacts live payloads and removes obsolete stored audio; it does not choose
which chops you want. Use Keep selected chops first when discarding unused source
regions. LFO cancels before confirmation. Let the operation finish before power-off.
Keep a full recovery archive; use the recovery guide if an interrupted operation
leaves storage locked. The loader can also rebuild a compact bank locally.

## Record audio from USB (hardware candidate)

Scratch mode is excluded from the 0.5 product build to reserve memory and code
space for USB sampling. Ordinary per-chop reverse remains available.

1. Select the destination song or drum pad, stop playback and release keys.
   On a song, open **EDIT → FX**. On drums, open **ENV → FX**. Check the displayed
   destination; a recording replaces that selected song or pad, not always Song 2.
2. On the computer, select the FM1 as an audio output. The recording interface
   accepts mono, 16-bit PCM at **22,050 Hz**; the host must convert other formats.
   This is digital USB audio, not the MIDI jack or an analog microphone input.
3. Check **INPUT / 100** and remaining seconds. A latched **CLIP** warning means
   lower the computer's output level. Each new take clears the warning.
4. Before starting, K1 sets the input threshold (0 starts immediately), K2 sets
   a seconds limit (0 means manual), or K3 sets a bars limit at the current BPM.
   Seconds and bars are mutually exclusive. Press **ENV** to start. Nonblank free sectors are erased before capture begins.
   With a threshold, WAITING FOR AUDIO changes to RECORDING when the input crosses it.
   Otherwise wait for RECORDING, then play the source. Press **ENV** again to stop; capture
   also stops at the selected duration or available-space limit. There is no live input monitoring.
5. On a ready take, **PLAY** auditions it through the normal master output;
   press PLAY again to stop, or let it finish. Audition does not commit the bank.
   Before saving, **K1** adjusts trim start and **K2** trim end in approximately
   1 ms steps; **K3/K4** adjust the same boundaries in 100 ms steps. PLAY auditions
   only that range. KEEP ms shows its duration. At least 64 samples are retained.
6. **SEL** asks to replace the song or pad selected when recording started.
   Press SEL again to confirm, or LFO to return to the ready take. Saving a song preserves
   the other song and kit, then opens its waveform. Saving a pad preserves both
   songs and the other pads, then returns to pad settings.
   An empty pool can create Song 1. Takes are IMA ADPCM at 22.05 kHz. A confirmed
   trim rewrites just the kept audio and its waveform, freeing the excluded space.
   Trimming uses one additional ADPCM encode pass; the sample rate stays unchanged.
   Cancelling confirmation preserves the full take and your selected range.
7. **ENV** on a ready take asks to retry. ENV confirms a new recording; LFO
   returns to the existing take. **LFO** on the ready take discards it. **FX/Home**
   exits when idle. Ordinary notes, transport and preset saves are blocked here.

Recordings use currently unused pool space, so replacing a song requires room
for the new take alongside the old bank. Cancelled takes leave the committed
header unchanged; their space is erased and reused on the next recording.
Use the loader to repack/trim a bank to reclaim old song payloads after saving.
An overrun, malformed packet, USB reset/disconnect or write failure rejects the
take; partial or gapped audio is not silently offered as a successful recording.

Keep a complete recovery archive before saving. The final header-sector rewrite
has the same power-loss limitation as a bank upload: power loss during that save
can require restoring the bank. Do not disconnect or switch off while saving.
The maintainer reports USB enumeration and capture during real flash writes tested
on hardware. Keep a backup and report any host-specific recording problems.

## Record the emulator output

Switch on the emulator and load samples or a session. Under **Record output**,
start recording, perform, then stop. Preview or download **WAV** or **MP3**.
The recording captures stereo emulator output on this computer; it does not
write to the FM1. Each new take replaces the previous download links, so save
a take before starting another. Hardware USB sampling captures in the opposite
direction: audio sent from your computer into the FM1.

## Record the FM1 output on your computer

1. Connect the FM1 with a USB data cable. In your DAW or audio recorder, choose
   the FM1 as the **audio input** and create a stereo recording track.
2. Use 16-bit stereo at 44.1 kHz, or 48 kHz if offered by the host. Play PIGEON
   and check the recorder's input meter before starting a take.
3. Record in the computer application, then stop and export there. This captures
   the FM1 master output; the result is not saved into the FM1 sample bank.
4. Under Settings → Audio, **USB LEVEL FIXED** keeps the USB recording level
   independent of the speaker/master-volume adjustment. Check levels in the
   recorder after changing it.

For sampling in the other direction, choose the FM1 as the computer's **audio
output** and use the USB sampler described above. Firmware updates and bank
transfers use USB MIDI SysEx, separately from these USB audio streams.
