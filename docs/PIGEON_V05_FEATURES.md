# PIGEON — complete feature list

**0.5.0-beta.1 · THE FINAL BETA** for the M-VAVE FM1. This page covers existing features and additions in 0.5. The [tutorial](PIGEON_V05_GUIDE.md) and [controls](PIGEON_CONTROLS.md) explain how to use them.

## Instrument and sample storage

- Three tracks: **Song 1, a 16-pad drum kit, and Song 2**.
- Up to **32 chops per song**, plus 16 drum hits: up to 80 assignments across the instrument.
- **500 KiB usable shared sample storage**, including encoded audio, waveforms and seek data. Duration depends on format, bitrate and the other sounds loaded.
- Indexed mono MP3 or IMA ADPCM song playback; ADPCM drum hits and USB recordings.
- Eight shared song voices and four additional drum voices. Empty slots stay silent.
- Waveform display, scroll and zoom; two alternate chop layouts sharing source audio.
- Samples and chop settings persist in flash. Patterns share the loaded bank rather than carrying separate copies of its audio.

## Chopping and performance

- Independently editable start/end boundaries and first-pass playback point.
- Forward or reverse per chop; semitone pitch and fine tuning in cents.
- Per-chop nondestructive gain and loop crossfade up to 8 ms; browser normalization.
- Envelope controls, held-loop/latch behavior, one-shot playback and choke groups.
- Split/join, copy whole chop, copy settings, duplicate, swap and move assignments.
- Chop-edit undo/redo and reset to original source assignments.
- Hold SEL and a key to audition the ending and loop while held; audition boundary changes immediately while editing.
- FIFO chop queues with repeated entries, selected-entry removal and queue inspection.
- Tap tempo, reference-loop beat count, loop-to-BPM tools and speed sync. Sync follows tempo by changing playback speed and pitch, within 50–200%.

## USB sampling and output

- Record incoming digital USB audio to the **selected song or drum pad**.
- Input level meter, latched clipping warning and remaining recording-time display.
- Immediate or threshold-triggered start; manual stop, seconds limit or bars limit.
- Audition an unsaved take with PLAY; fine/coarse start/end trimming and selected-range audition.
- Confirmed save to the selected destination, retry confirmation and take discard.
- Other songs/pads remain intact when the selected destination is saved.
- Failed, interrupted or overrun recordings are rejected instead of offered as successful takes.
- Input format: mono 16-bit PCM at 22.05 kHz, encoded to IMA ADPCM.
- Record the FM1's stereo master output on a computer over USB audio.

## Keep sounds and reclaim space

- On-device **Keep selected chops** retains marked regions and trims unused source audio.
- Retain in the same song while preserving key numbers, or append to empty keys in the other song.
- Browser selected-chop trimming with encoded-space preview and local undo; retained chops are renumbered consecutively.
- Reclaim obsolete payloads by compacting the live bank.
- Browser chop collections preserve useful regions from multiple source songs independently of the current bank/session.
- Export/import collections, reorder collected sounds, and assemble song banks or drum kits.

## Sequencer

- Step grid and piano roll edit the same sequences.
- **1–64 steps per track**, with up to **four notes per step**.
- Live recording and chord-based step entry; stopped/unarmed note toggling.
- Velocity, gate, probability, repeats/ratchets, pitch, step nudge and conditions.
- Independent note length and nudge for notes within a chord; piano-roll duration display.
- Conditional fills and a:b conditions, Euclidean rhythms, DOUBLE/HALF tools and velocity/timing humanization.
- Swing, quantization, step duration/division, tempo timing and sample-end timing.
- Metronome and count-in; sequence undo/redo, automation and parameter locks.
- Custom note timing shares a 128-entry budget with automation/locks; the editor reports a full store.

## Patterns and arrangement

- Saved patterns **A–D**, containing all three tracks' sequences, timing and automation.
- Pattern copy/paste, unsaved-work indication and overwrite confirmation.
- Quantized switching at the next bar or pattern end; change or cancel a pending launch.
- A **16-section arrangement** with per-track pattern selections, repeats, section insertion/deletion and captured mutes.
- Complete session backup retains current work, saved patterns, arrangement and sample bank.

## Mixer and effects

- Three-track mixer with levels, mutes and FX sends.
- Filter, delay, chorus, reverb, distortion and compression.
- Drum-pad playback controls and choke grouping.

## MIDI and accessibility

- USB MIDI note/control integration and MIDI clock synchronization.
- Per-track MIDI channels, OMNI/OFF choices, incoming-note selection and MIDI screen wake.
- Standard firmware screen-reader support through the FM-1 Screen Reader protocol; no separate firmware fork required.
- Page/control/value announcements, WHERE/read-current-screen, transport/recording states, errors and confirmations.
- Emulator browser speech or screen-reader live-region output, repeat-last announcement and remembered preferences.
- Keyboard-accessible virtual keys, buttons and knobs.

## Sample loader and session tools

- Browser-local audio import, editing, previews and encoding; Song 1/Song 2 assembly and up to 16 drum hits.
- Append multiple source files to a song, select sections, trim, rearrange and automatically slice remaining keys.
- Chop boundaries, playback point/direction, pitch/cents, gain, normalization, crossfade and matching audition previews.
- Waveform selection, zoom, numeric trim, validated drafts, Apply, reset and workspace undo/redo.
- Bank library and full-session library with summaries and overwrite previews.
- Export/import **.kob** sample banks, **.pigeon** complete sessions and **.pigeon-chops** collections.
- Connected-device backup/restore, bank upload/readback and firmware installation with recovery backup.
- Capacity/capability checks for older firmware and expanded 0.5 banks.
- Offline-capable sample tool; user audio stays local to the browser/device workflow.

## Emulator and website

- Browser emulator built from the firmware source, with the instrument screen, sound, mouse/touch knobs and playable keys.
- Desktop/mobile layouts, sample loading and browser persistence.
- Output recording with downloadable **WAV or MP3**.
- Screen-reader support and common navigation across the website, emulator and sample loader.
- Tutorials, control maps, hardware checklist, validation record, source downloads, licensing and credits.

## Updates and provenance

- Firmware installation uses M-UPGRADE over USB MIDI SysEx.
- Compatibility checks, complete-session backup and recovery boot-stub support.
- Expanded-bank downgrade protection refuses an older layout when it cannot preserve the live bank.
- Tagged releases are built and attested by Firmware Hub's shared GitHub workflow. Verify the exact release file with the instructions in [release signing](RELEASE_SIGNING.md); a local rebuild is not the attested artifact.
- Free and open source under GPL-3.0-only, based on Felucca 1.5.1, with upstream and creative credits.

## Limits

Scratch mode and synth engines are excluded from the 0.5 product profile. There is no internal resampling or live USB-input monitoring. USB sampling needs free space alongside the existing bank. Sample-rate/format conversion is the host's responsibility. Speed sync is not pitch-preserving time stretching. Save/reclaim operations require steady power; keep downloaded recovery archives. The emulator does not emulate the physical USB recording peripheral.

The maintainer reports the hardware checklist completed on 2026-10-21. Version/label-only publication changes are covered by software checks; see [validation](PIGEON_V05_ACCEPTANCE.md).

## New and expanded in 0.5

- **USB sampling into the FM1:** record to the selected song or drum pad, with input metering, clipping indication, sound-triggered start, and seconds or bars limits.
- **Review before saving:** audition an unsaved take, trim its start/end, retry or discard, and confirm before replacing the destination.
- **Keep selected chops:** retain useful regions, trim away the rest, and build up either song. On-device retention preserves key numbers; browser trimming renumbers them.
- **Reclaim sample storage:** compact live audio and remove obsolete payloads. The shared sample bank now has **500 KiB usable**.
- **More control per chop:** independent playback start, reverse, fine tuning, nondestructive gain, and loop crossfade. The loader adds normalization and matching previews.
- **Safer chop editing:** reset to source assignments, undo/redo, and immediate boundary audition while holding SEL and a chop key.
- **Reusable chop collections:** collect sounds from multiple sources in the browser, export/import collections, and assemble song banks or drum kits.
- **Pattern launch controls:** choose next-bar or pattern-end switching and cancel a pending launch.
- **Clearer session management:** bank/session summaries and overwrite previews help distinguish sample-only transfers from full-session restores.
- **Emulator output recording:** download performances as WAV or MP3.
- **Accessibility and interface polish:** screen-reader support, shared site navigation, corrected SEL button hints, and fixes for Settings and recorder text flicker.
- **Update safeguards:** expanded-bank compatibility checks prevent an older updater from overwriting audio in the new flash region; recovery documentation and complete-session backups accompany the beta.

Scratch mode is excluded to make room for USB sampling. Ordinary per-chop reverse remains available.

The maintainer reports the hardware checklist complete on 2026-10-21. The final SEL label-only rebuild has automated software checks; it does not change recording or storage behavior. See [validation](PIGEON_V05_ACCEPTANCE.md) and the [tutorial](PIGEON_V05_GUIDE.md) for evidence, controls and limits.
