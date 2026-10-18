# PIGEON controls — v0.4

Start with the [v0.4 tutorial](PIGEON_V04_GUIDE.md); [per-note timing](PIGEON_NOTE_TIMING.md) documents the longer-note and individual-nudge controls.

PIGEON is sampler firmware for the M-VAVE FM1: Song 1, drum kit, and Song 2.
Choose the track first; edits belong to that track. Song 2 can hold drum loops.

| Page | Controls |
| --- | --- |
| Song waveform | K1 start, K2 end, K3 scroll, K4 horizontal zoom; OCT changes pitch |
| Chop settings (EDIT) | Held loop, latch, split/join, undo/redo; ENV opens envelopes, LFO chop operations |
| Envelopes | K1 attack, K2 release, K3 one-shot/hold/latch, K4 loop |
| Sequencer (SEQ) | K1 BPM, K2 tempo/sample-end timing, K3 step, K4 length (1–64); OCT step duration; REC arm |
| Step details (ENV from sequencer) | Velocity, gate, chance and repeats; ENV advanced pitch/nudge/conditions |
| Timing (LFO from sequencer) | Swing and timing; LFO recording preferences |
| Recording preferences | Independent metronome/count-in, live quantization OFF/1/16/1/8, song choke |
| Tempo tools (ENV from recording preferences) | Tap tempo (EDIT), loop beat count and loop-to-BPM; FX opens sync |
| Speed sync | K1 reference beats, K2 enable, K3 whole-song/chop reference, K4 chop |
| Patterns | Keys 1–4 choose A–D; K1 select, K2 copy source, K3 chain add/remove, K4 paste/copy |
| Arrangement | Section, pattern, repeat count, insert/delete; ENV opens launch/section tools |

While stopped and armed, keys write steps. While playing and armed, keys record
live. Playback LEDs identify the chop sounding; recording LEDs identify the
current step. The sequencer screen explicitly shows stopped, armed, recording,
playing or count-in. Shortening a sequence keeps hidden later steps for reuse.

Pattern changes during playback are queued for the next bar or pattern end,
according to the launch preference. The pattern screen names that boundary.
Patterns change notes and timing; they share the loaded samples and chop edits.

Sync changes speed **and pitch**. Set the actual number of beats in the captured
reference; it follows project BPM within 50–200% speed. Changing chop boundaries
does not recapture the reference until a sync control is adjusted. SPEED % shows
the sync factor, before OCT and step pitch. It is not pitch-preserving stretching.

v0.4 firmware and web tools share 496 KiB between both songs and the kit. The songs share eight sounding voices; the drum kit has four additional voices. White song keys
cover chops 1–16; black keys add 17–27; PRESETS selects later chops and changes the key bank to reach 28–32.
Drums have 16 one-shot pads. Held loops default on, latch defaults off.

Save banks and full recovery archives in the sample tool before replacement.
The full workflow and format details are in [KO_MODE.md](KO_MODE.md).

## Saving a phrase

SCL opens Patterns. Choose A–D, stop playback and release the pads, then press SAVE. This saves all three tracks’ sequences, timing, automation and arrangement together. The screen confirms the destination. REC on Patterns/Arrangement remains a save shortcut; on the sequencer it arms recording. Samples and chop layouts belong to the shared sample bank, so switching A–D does not replace them. In the browser, Save session (.pigeon) includes the current phrase, saved A–D patterns and the sample bank; Save bank (.kob) contains samples and chop settings only.

## Installing from another firmware

Before installing PIGEON from another custom firmware, revert to official M-VAVE V15 using your current firmware’s documented stock-return procedure, then install PIGEON. Back up first; basic installation and sample/chop preservation were checked; extended hardware verification remains pending.

## Unsaved patterns

Patterns shows UNSAVED when the current phrase differs from its saved state or the destination is empty. Selecting an occupied slot with unsaved work warns first; select the same slot again to discard those edits and load it. SAVE stores the phrase, timing, automation and arrangement to the selected A–D destination. Sample/chop settings remain in the shared bank.

See [the first-song walkthrough and v0.4 release gate](PIGEON_V04_RELEASE.md) for the complete workflow and pending physical checks.

## Copy a whole chop to another key

Within the same song, EDIT opens chop settings and LFO opens chop operations. Stop playback and release pads. K1 selects the destination chop/key; K2 selects the source. Press REC to copy the source's complete boundaries, pitch, envelope, trigger/loop mode and choke group onto the destination. Existing keys keep their numbers and no extra audio storage is used. The existing chop undo can reverse the overwrite. K4 clockwise still copies settings only.

In the sample loader Chop editor, select the destination with Chop, choose the source in Copy from, then press Copy whole chop here. Send the bank to apply the edited layout to hardware. This does not copy between different songs.


## Swap and move chops

From chop operations (EDIT → LFO), tap ENV to open key assignment. K1 selects the source and K2 the destination. Turn K3 clockwise once to swap both assignments, or K4 clockwise once to move the source to the destination while shifting intervening keys. Stop playback and release notes first. ENV returns to chop operations. Chop undo restores the previous assignments; existing pattern key numbers stay unchanged and play their newly assigned chops.

The sample loader offers the same Swap and Move operations. It also copies drum audio and settings: choose a source Pad, a Copy to pad destination, and Copy pad to key. Send the resulting bank to use it on hardware.

Loader Undo/Redo supports Ctrl+Z / Cmd+Z, Ctrl+Shift+Z / Cmd+Shift+Z and Ctrl+Y for committed local edits. Text inputs retain text undo. The Session library keeps multiple named banks or full sessions locally, with download backups. Neither workspace undo nor library loading writes to the FM1.

## Held-button shortcuts

Page buttons open their pages immediately on press; releasing them does not navigate again. HOME returns to the waveform from another page. Press HOME on the waveform to open Settings, then HOME again to return. Hold GLO for mixer shortcuts and SEQ for sequence tools. FX opens the PIGEON effects pages, SCL opens Patterns, EDIT opens chops, and REC arms recording. Their inherited macro, scale/chord, engine-picker and recording overlays have been removed. Recording preferences remain available through LFO from Timing.

## Reading the screen

Parameter pages use a consistent four-card grid: K1/K2 on the top row and K3/K4 on the bottom row. Page actions and navigation appear along the bottom. The top strip shows track, phrase, transport/record state, BPM and battery. FX labels EDIT as RESET; HOME returns to the waveform. See [building and rendering](../BUILDING.md) for the render workflow.

### Audition a chop ending

On either song's waveform, hold **SEL** and trigger a chop to hear its final two
seconds, then loop from the beginning while the key stays held. Short chops start
at the beginning. Releasing the note stops the audition. This bypasses recording,
quantization and MIDI output without changing chop boundaries. A plain SEL tap
opens Patterns; it resets cent tuning inside Scratch instead.

## Sequencer grid and piano roll

Press SEQ to open the grid; press SEQ again to switch to the piano roll. SEQ alternates views and HOME exits. Both edit the same **1–64-step sequence**. K3 selects the step, K4 changes length while stopped, and PRESETS scrolls piano-roll rows.

Both views support **four notes per step**. While unarmed and stopped, tap keys to add/remove individual notes on the selected step. With REC armed and transport stopped, hold a chord and release all keys to advance once. During playback, REC records live. A fifth note does not replace a stored note. The grid displays `2N`, `3N` or `4N` for chords; the roll draws each note. REC stays lit when armed in every LED mode.

See [v0.4 release preparation and validation](PIGEON_V04_RELEASE.md).

For sequence tools, conditional fills and per-track MIDI routing, see
the [v0.4 guide](PIGEON_V04_GUIDE.md#sequence-tools).

## Queue, scratch and MIDI

EDIT → ARP opens the queue; K1 enables it. EDIT → FX opens Scratch; EDIT switches
its performance/motor controls. HOME returns to the waveform. From the piano
roll, FX opens individual note length/nudge. The [v0.4 guide](PIGEON_V04_GUIDE.md)
has full controls for these pages, fills, sequence tools and per-track MIDI.

v0.4 projects use 115 parameters and are not readable by public v0.3.1. Export
a complete recovery archive before updating or downgrading.
