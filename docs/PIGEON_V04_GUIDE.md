# PIGEON v0.4 tutorial

These controls describe the released PIGEON v0.4 firmware, verified on physical FM1 hardware.
For build status and verification notes, see [development notes](PIGEON_V04_DEVELOPMENT.md).

## Install and make your first phrase

1. Export a complete recovery archive in the sample tool. Keep the downloaded
   file outside the browser. v0.4 projects cannot be opened by public v0.3.1.
2. Coming from another custom firmware? Revert to official M-VAVE **V15** using
   that firmware's stock-return procedure first. Use a charged FM1, a direct USB
   data cable and desktop Chrome or Edge. Close other MIDI applications. Install
   the approved PIGEON package; keep power and USB connected until reboot.
3. Open the sample tool. Add one or more audio files to Song 1; adding another
   file appends it. Select and delete unwanted regions with Backspace, split and
   rearrange clips, and use Undo if needed. Whole sections occupy the first chop
   keys; automatic slices fill the remaining keys, up to 32 per song.
4. Apply the edited song, optionally add Song 2 and drum one-shots, then send the
   bank to the FM1. Both songs, drums, seek indices and waveform data share
   **496 KiB**. The limited mode shows actual capacity and compression choices;
   unlimited storage is for the browser emulator only.
5. On the waveform, K1/K2 adjust the selected chop start/end, K3 scrolls and K4
   zooms. Keys trigger chops. Select Song 1, Drums or Song 2 with ALGORITHM.
6. Press SEQ for the grid, again for the piano roll. While stopped and unarmed,
   tap keys to toggle notes on the selected step. With REC armed, hold a chord
   and release all keys to advance once. PLAY starts live recording while REC
   stays armed. Both views support four notes per step and 1–64 steps per track.
7. Tap SEL from the waveform to open Patterns. Choose A–D, then SAVE to save
   all three sequences and their timing/automation together. Samples and chop
   layouts remain shared across patterns. Export a full session for samples,
   current project and saved patterns; a bank alone does not contain patterns.

Stop playback and release notes before saving or replacing a bank. HOME returns
to the waveform; HOME on the waveform opens Settings. The splash lasts three
seconds. Empty song slots are silent.

## Scratch a chop

Select Song 1 or Song 2, stop the sequencer and disarm REC. From the waveform,
press **EDIT**, then **FX**. Tap a chop key to start its looping deck. Another
chop key replaces it. Releasing a key does not stop the deck.

| Control | Performance controls | Motor controls (tap EDIT) |
| --- | --- | --- |
| K1 | Jog backward/forward | Seek within the chop, approximately 1/256 per detent |
| K2 | Level: silence to full, 16 increments | Signed motor speed, −400% to +400%, in 1% steps |
| K3 | Waveform or record view | Brake time, 0–2000 ms, in 10 ms steps |
| K4 | Jog sensitivity, eight levels | Release glide, 0–500 ms, in 5 ms steps |

Both control sets keep the waveform or record visible. Low sensitivities allow
fine jogs; higher sensitivities allow faster movement, capped at ±4×.

| Button/encoder | Action in either scratch control set |
| --- | --- |
| PRESETS | Tune the motor by 1 cent, between −100 and +100 cents |
| SEL | Reset cent tuning to exactly zero |
| PLAY | Start the motor or brake to a stop |
| ARP | Reverse the motor direction |
| ENV | Set the cue at the current position |
| LFO | Return to the cue |
| EDIT | Switch between the two control sets |
| FX | Leave the deck and return to chop playback settings |
| HOME | Stop the deck and return to the waveform |

The cue appears as a white dashed line on the waveform, or a white outer-groove
mark on the record. The coloured line indicates playback position. Each chop remembers its cue while the device stays on, including after leaving
and reopening the deck. Replacing the audio or changing that chop’s bounds
resets its cue to the beginning. Cue return does not start a stopped motor.

Motor speed changes pitch with speed. Cent tuning also works in reverse;
one hundred cents is one semitone. Jogging
keeps its physical sensitivity; when released, it returns to tuned motor speed.
Motor changes use the glide time; stopping uses the brake time. Cuts mute the
sound while the platter continues moving.

### Remembered settings

Each song remembers motor speed/direction, cent tuning, sensitivity, brake,
glide and waveform/record preference. They reopen unchanged and save with that
song's sample metadata after the deck and other voices stop, transport is idle,
and the automatic save completes. Let saving finish before
powering off. Song 1 and Song 2 keep separate preferences.

The motor never starts automatically on boot. Cue positions are retained per chop in memory but are not saved across power-off.
The running voice, fader
level and control-set selection are temporary; reopening starts at full level
with performance controls. Replacing a song resets its preferences.
Older banks with no preference record use defaults: 100% speed, zero cents,
sensitivity 5 of 8, 250 ms brake and 60 ms glide. Preferences live with sample
metadata, rather than being switched by each pattern. Complete bank exports
that include metadata and full recovery archives retain them; sequence-only
projects do not carry them. Older firmware may discard these preferences when saving sample edits.

## Queue chops in order

From the waveform, press **EDIT**, then **ARP**. Turn K1 right to enable queue
mode. Play a chop: it plays once to the end. Press other chops while it plays to
append them, including repeated presses of the same chop. Up to 64 chops can
wait behind the currently playing chop. The page shows the current chop and waiting count. K3 selects a waiting entry. The wire follows the selection in groups of four,
with chop numbers beneath the birds and an underline marking the selection.
PICK identifies its position and chop number. Repeated numbers are separate plays.

| Control | Action |
| --- | --- |
| Keys / MIDI notes | Append a chop; start immediately if idle |
| K3 | Select any waiting entry |
| ENV | Remove the selected waiting entry, keeping the current chop |
| K2 left | Remove the last waiting entry |
| K2 right | Clear all waiting entries, keeping the current chop |
| K4 right, EDIT or PLAY | Stop and clear the selected song's queue |
| K1 left | Disable queue mode |
| ARP | Return to chop playback settings |
| HOME | Return to the waveform with queue mode still enabled |

An empty waiting queue has nothing to remove: removing the last entry does not
stop the current chop. The preview scrolls automatically as entries play.
Queues are temporary. Transport, REC arming, project restore or replacing the
bank clears performance queues. Scratch mode and queue mode cannot run together.

## Copy and save patterns

Open Patterns with SEL from the waveform. FX copies the current live phrase,
including unsaved edits, all three tracks and automation. Choose a destination
A–D with K1 or the pattern keys, then press LFO to paste and save. K2 can copy a
saved source instead of the live phrase. Existing OCT−/OCT+ and K4 copy/paste
shortcuts remain available.

Loading an occupied pattern with unsaved work first displays the destination
and asks you to select it again. SAVE keeps the current edits in their current
slot instead. Pasting over an occupied destination or unsaved phrase also asks
for a second paste press. Changing the destination, clipboard or phrase requires
a new confirmation; leaving Patterns cancels pending confirmations. Paste
preserves loaded samples, chop edits and mixer levels. SAVE names its target
and saves all three tracks together.

## Longer notes and individual groove

Press **SEQ**, then **SEQ** again to show the piano roll. Stop playback to edit.
Tap **FX** for the selected step's note editor. K1 chooses one of its four notes;
K2 changes its length, K3 changes that note's nudge, and K4 selects a step.

Tap **FX** to return to the roll. The selected chord note stays selected and is
highlighted. Its pitch, length and individual nudge appear below the roll. GATE
means the existing track gate; explicit lengths are shown in steps. Nudges are
sixteenths of a step, independent of the track-wide step nudge. PRESETS scrolls
the visible pitch range. Full controls, supported lengths, timing modes and
storage limits: [Per-note timing](PIGEON_NOTE_TIMING.md).

## Recording and end audition

REC arms the selected track; PLAY preserves that arming. With transport running,
play notes to record them. While stopped, armed chord entry advances after all
held chord keys are released. Disarm REC to edit steps with the keys.

On the song waveform, hold **SEL** and trigger a chop to audition its last two
seconds, then loop from its beginning while held. Releasing the note stops the
audition. A plain SEL tap opens Patterns. Inside the scratcher SEL instead resets
tuning, so audition and scratch controls do not open another page unexpectedly.

## Screen reader

[Screen-reader setup](SCREEN_READER.md) covers the FM-1 Screen Reader connection,
local pronunciation file and emulator announcements. Page changes announce the
page; control changes announce the changed value. WHERE reads current controls,
including scratch tuning or the queue's next eight entries. Queues draining,
platter rotation, decorative pigeon movement and playhead ticks do not generate automatic speech. Important
errors and explicit cue/reset/removal confirmations are announced.


## Sequence tools

From **SEL → Patterns**, press **ENV** for arrangement tools, **ENV** again for
variations, then **FX** for sequence tools. Stop playback and release the keys
before applying a tool. **K1** chooses the operation; **ENV** applies it,
**FX** returns, and holding **SAVE** undoes or redoes the last operation.

- **DOUBLE** repeats the selected track, up to 64 steps. Notes, chords,
  conditions, nudges, automation and individual note timing move together.
- **HALF** keeps the first half and clears the discarded half, including its
  automation. Other tracks stay unchanged.
- **EUCLID** generates an evenly distributed rhythm. **K2** sets hits, **K3**
  sets the repeating cycle length, and **K4** rotates it. Existing notes on
  retained hits stay; new hits use the first chop/pad (note 60). Empty steps
  lose their locks and individual note timing. Ordinary automation stays.
- **HUMANIZE** sets random velocity variation with **K2** and timing variation
  with **K3**, each from 0–63. Zero disables that variation. Timing delays
  tempo-driven steps by up to 30 ms, bounded by the step duration; ratcheted
  steps and sample-end timing stay on their existing timing. These values are
  saved with the track, using modulation routes 3 and 4.

## Conditional steps and fills

From the sequencer, press **ENV** for step dynamics, then **ENV** for pitch and
timing. **K3** selects the condition. Existing EVERY 2ND, EVERY 4TH, FIRST PASS
and AFTER FIRST settings retain their meanings. Additional `a:b` settings play
on pass `a` of each `b` passes: for example, `3:4` plays on the third pass of
four. **FILL** plays while **ARP** is held on the grid, roll or step-detail
pages; **NOT FILL** plays when it is released. ARP stays on the current page.

## MIDI routing

In **Settings → MIDI**, set **MIDI IN** to **TRACK**, then choose **S1 CHANNEL**,
**DRUM CHANNEL** and **S2 CHANNEL** independently. Each accepts channels 1–16,
**OMNI** (channels not assigned to another track) or **OFF**. If two tracks use
the same channel, the first track receives it. Channel changes release notes
held under the old routing. Local keys transmit on the selected track's channel;
OMNI/OFF use that track's default channel for output.

**AUTO SELECT** is off by default. Enabling it follows incoming notes, with a
250 ms holdoff to prevent flickering between parts and protection while editing.
**SCREEN WAKE** defaults to **+MIDI**; choose **PANEL** to keep MIDI notes from
waking the screen. Clock, CC and editor traffic do not wake it.
