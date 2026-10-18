# PIGEON v0.4 release checklist

This is a local release candidate. Public v0.3.1 stays online until deployment is
explicitly approved. Do not flash or publish as part of candidate preparation.
The final automated validation report and candidate manifest identify the exact
files to test; they do not claim a physical hardware pass.

## What changes

- FIFO chop queues, including repeats and selection/removal of waiting entries.
- Two scratch decks: waveform/record views, jog sensitivity, level cuts, motor
  reverse, brake/glide, per-chop temporary cues and cent tuning. Per-song deck
  preferences save with sample metadata. Cues reset at power-off.
- Independent length and nudge for the four notes on a step, in the piano roll.
- Conditional fills and `a:b` conditions, Euclidean rhythms, DOUBLE/HALF and
  velocity/timing humanization.
- Per-track MIDI channels with OMNI/OFF, optional incoming-note selection and
  MIDI screen wake.
- Standard screen-reader support and opt-in emulator announcements.
- SEL plus a chop key auditions the ending, then loops from the beginning while
  held. A normal SEL tap still opens Patterns.
- Pattern copy/paste and destination-specific confirmation of overwrites.
- The sample tool reads current v0.4 projects and retains older project layouts.

The [tutorial](PIGEON_V04_GUIDE.md), [controls](PIGEON_CONTROLS.md),
[per-note timing reference](PIGEON_NOTE_TIMING.md) and
[reader setup](SCREEN_READER.md) describe the actual controls.

## Update and compatibility

Export a **complete recovery archive** before installing. Keep the file outside
the browser. From another custom firmware, first restore official M-VAVE **V15**
using that firmware's documented stock-return procedure. Then install PIGEON
with a direct USB data cable and a fully charged instrument.

The package retains the existing USB/update identity and recovery boot stub.
Both songs, drums, waveform data and seek indices share 496 KiB. Empty slots
remain silent. Songs share eight sounding voices; drums have four dedicated
voices. Sequence length remains 1–64 steps with four notes per step.

v0.4 projects have 115 parameters and are **not readable by public v0.3.1**.
Keep the pre-update archive if a downgrade might be necessary. Existing
104–111-parameter projects remain readable; imported routing defaults are
preserved. New conditions require v0.4. A bank contains sample/chop settings,
not patterns; export a full session to keep both.

## Quick hardware smoke test

Use the exact candidate package and hash from `build/releases/pigeon-0.4/`.
Save a recovery archive before requesting a flash. Do not substitute a different
binary after the test.

1. **Boot and samples:** confirm `PIGEON v0.4`, the three-second splash and Song 1
   selection. Play both loaded songs and drums. Empty slots must be silent.
2. **Audition and queues:** on the waveform, hold SEL and a long chop key. Hear
   its ending and the loop back to its start; release to stop. Check a plain SEL
   tap opens Patterns. EDIT → ARP: enable queue, enter repeated chops, remove a
   selected waiting entry and confirm the remaining FIFO order.
3. **Record and timing:** arm REC, start PLAY and record a short chord. Confirm
   the REC light stays on and notes play back in both SEQ views. From the roll,
   FX: lengthen one note beyond a step and nudge another; listen to the result.
4. **Scratch and persistence:** EDIT → FX: jog in both directions, cut level,
   reverse, set/return to a cue and tune by cents. HOME must stop the deck. Save
   a phrase and changed deck preferences, let saving finish, power off/on and
   confirm the phrase, samples and preferences return with the motor stopped.
5. **Busy playback and MIDI:** run a saved phrase across both songs and drums,
   with chords and effects, for a minute. Listen for clicks, gaps or stuck notes.
   If using MIDI/panel control or a screen reader, check that connection and one
   knob/button/WHERE action with the matching local v0.4 emulator/dictionary.

Report any failure before release. Host tests cannot establish analog sound
quality, USB reliability on every computer, power-loss behavior in every physical
state or audio deadlines under every musical workload. Those limits are not
converted into a hardware-pass claim by a successful build.

## Release handoff

- Candidate: versioned `.fwsc`, `.bin`, SHA-256 hashes and memory metrics.
- Reproducible target build and full product/recovery/reader checks.
- Current v0.4 emulator and complete sample-tool/browser checks.
- Updated matching source archive, installer, website, tutorial and dictionaries.
- A local site release with the exact same package hash as the candidate.
- Final report: automated checks and physical smoke-test status kept separate.

After the hardware smoke test passes, deployment still needs explicit approval.
Do not silently replace the candidate or publish unfinished local changes.
