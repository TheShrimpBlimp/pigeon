# PIGEON v0.5 development guide

These features are local development work, not the published v0.4.x firmware. Do not
install a v0.5 bank on older firmware. Keep a complete recovery archive before
updating; old firmware ignores the newer playback metadata.

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
point to use the normal start/end for the chosen direction. SEL ending audition
and Scratch retain their dedicated trigger behavior. Each chop's direction,
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
may need reassignment. This is a loader operation, not an on-device trim command.

## Record audio from USB (hardware candidate)

Scratch mode is excluded from the 0.5 product build to reserve memory and code
space for USB sampling. Ordinary per-chop reverse remains available.

1. Stop playback and release keys. Open **EDIT → Chop playback → FX USB sample**.
2. On the computer, select the FM1 as an audio output. The recording interface
   accepts mono, 16-bit PCM at **22,050 Hz**; the host must convert other formats.
   This is digital USB audio, not the MIDI jack or an analog microphone input.
3. Check **INPUT / 100** and remaining seconds. A latched **CLIP** warning means
   lower the computer's output level. Each new take clears the warning.
4. Press **ENV** to start. Nonblank free sectors are erased before capture begins.
   Wait for RECORDING, then play the source. Press **ENV** again to stop; capture
   also stops when available space is full. There is no live input monitoring.
5. On a ready take, **PLAY** auditions it through the normal master output;
   press PLAY again to stop, or let it finish. Audition does not commit the bank.
   Before saving, **K1** adjusts trim start and **K2** trim end in approximately
   1 ms steps; **K3/K4** adjust the same boundaries in 100 ms steps. PLAY auditions
   only that range. KEEP ms shows its duration. At least 64 samples are retained.
6. **SCL/Select** asks to replace the song selected when recording started.
   Press SCL again to confirm, or LFO to return to the ready take. Saving preserves
   the other song and drum kit, then opens the recorded song's waveform for chopping.
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
USB host enumeration and capture during real flash writes still require hardware
acceptance; host tests alone do not establish reliable recording on the device.
