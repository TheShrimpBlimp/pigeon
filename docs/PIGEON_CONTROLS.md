# PIGEON controls

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

Both songs and the kit share 408 KiB. The songs share eight sounding voices; the drum kit has four additional voices. White song keys
cover chops 1–16; black keys add 17–27; PRESETS selects later chops and changes the key bank to reach 28–32.
Drums have 16 one-shot pads. Held loops default on, latch defaults off.

Save banks and full recovery archives in the sample tool before replacement.
The full workflow and format details are in [KO_MODE.md](KO_MODE.md).

## Saving a phrase

SCL opens Patterns. Choose A–D, stop playback and release the pads, then press SAVE. This saves all three tracks’ sequences, timing, automation and arrangement together. The screen confirms the destination. REC on Patterns/Arrangement remains a save shortcut; on the sequencer it arms recording. Samples and chop layouts belong to the shared sample bank, so switching A–D does not replace them. In the browser, Save session (.pigeon) includes the current phrase, saved A–D patterns and the sample bank; Save bank (.kob) contains samples and chop settings only.

## Installing from another firmware

Before installing PIGEON from another custom firmware, revert to official M-VAVE V15 using your current firmware’s documented stock-return procedure, then install PIGEON. Back up first; physical hardware verification of this beta remains pending.

## Unsaved patterns

Patterns shows UNSAVED when the current phrase differs from its saved state or the destination is empty. Selecting an occupied slot with unsaved work warns first; select the same slot again to discard those edits and load it. SAVE stores the phrase, timing, automation and arrangement to the selected A–D destination. Sample/chop settings remain in the shared bank.

See [the first-song walkthrough and V0.5 release gate](PIGEON_V05_RELEASE.md) for the complete workflow and pending physical checks.

## Copy a whole chop to another key

Within the same song, EDIT opens chop settings and LFO opens chop operations. Stop playback and release pads. K1 selects the destination chop/key; K2 selects the source. Press REC to copy the source's complete boundaries, pitch, envelope, trigger/loop mode and choke group onto the destination. Existing keys keep their numbers and no extra audio storage is used. The existing chop undo can reverse the overwrite. K4 clockwise still copies settings only.

In the sample loader Chop editor, select the destination with Chop, choose the source in Copy from, then press Copy whole chop here. Send the bank to apply the edited layout to hardware. This does not copy between different songs.
