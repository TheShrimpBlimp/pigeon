# PIGEON 0.5 FM1 test run — 2026-10-21

Initial candidate: `build/pigeon-before-settings-fix-20261021.fwsc`, embedded version `v0.5-dev`.
SHA-256: `bb5be04cee08ac779529e93b76ee5c3fc98d4bf7b1b27e04a0c416652bfbed70`.

Prepared bank: `build/pigeon-final-test-drums-20261021.kob`.
Song 1: MERGED (91.30 s, MP3). Song 2: USB TAKE (2.51 s, ADPCM).
Both songs' encoded audio, waveforms and original metadata are retained.
The repacked bank uses about 385.6 / 500 KiB, leaving 114.4 KiB free.

Original full backup: `build/pigeon-before-final-test-20261021.pigeon`.
Final loaded-state backup: `build/pigeon-ready-for-hardware-tests-20261021.pigeon`.
The latter is created only after flash, drum upload and readback succeed.
Keep these files. Full-session restore replaces bank, projects and settings;
bank upload replaces samples/chops while preserving device projects/settings.

## Loaded drum keys

| Pad | Sound | Pad | Sound |
| --- | --- | --- | --- |
| 1 | Kick | 7 | Low tom |
| 2 | Snare | 8 | High tom |
| 3 | Clap | 9 | Cowbell |
| 4 | Closed hat | 10 | Ride |
| 5 | Open hat | 11 | Crash |
| 6 | Rim | 12–16 | Unassigned |

## Automated setup results

- Firmware installation and restart completed successfully.
- New bank capacity reported: 504 KiB total / 500 KiB usable.
- Post-flash readback matched the original bank and all ten project/settings
  objects byte-for-byte.
- Drum-bank readback matched the prepared bank and metadata exactly; all ten
  project/settings objects remained unchanged. Both songs were preserved.
- A complete test-ready `.pigeon` archive and `.kob` readback were saved.
- Ordinary transfers encountered timeouts on read commands 66/94 and one payload
  command 92. The upload was restarted and completed with pacing and bounded
  read/idempotent-payload retries. No header/commit writes were blindly retried.
  Repeat ordinary backup/upload stability before accepting the release.

## 1. Five-minute smoke test

- [ ] Boot shows v0.5-dev. Navigate Song 1 → Drums → Song 2.
- [ ] Trigger several chops from both songs. Listen for missing starts, clicks,
  unexpected silence and stuck notes; check forward and reverse.
- [ ] Play pads 1–11 in order. Check the sound map above, levels and clean tails.
- [ ] Start a pattern. Play drums over both songs; turn knobs and switch pages.
  Listen for dropouts. Stop; confirm no stuck notes or unintended recording.
- [ ] Switch off normally, restart, and repeat song/pad playback.

Record PASS/FAIL and the page/action for any failure before proceeding.

## 2. USB capture and trimming

Backups above permit restoring the test setup after replacement tests.

1. Select **Song 2**, stop playback/release keys, then **EDIT → FX**.
2. On the computer choose the FM1 as its audio **output**, mono 16-bit 22050 Hz.
   Check the input meter. Set K1 threshold 0 and K2 seconds 4.
3. ENV starts preparation/capture. Wait for RECORDING, then send the test tone
   (`build/v05-test-tone.wav`). Expect a clean take without an overrun/dropout.
4. PLAY auditions; PLAY stops. K1/K2 trim start/end finely, K3/K4 coarsely.
   PLAY must follow the retained range. SEL opens confirmation; LFO backs out
   without losing the full take. SEL twice saves.
5. Confirm Song 2 changes, while Song 1 and all 11 pads remain. Restart and check.
6. Repeat with K1 threshold above silence: WAITING FOR AUDIO must wait, then
   start on the source. Separately test K3 one-bar recording at the current BPM.
7. Select a drum pad and **ENV → FX**. Save a short take to that pad. Both songs
   and other pads must remain. Repeat for Song 1 after preserving/restoring the
   test setup as needed; the destination must always match the selected track.
8. Discard a ready take with LFO; previously committed audio must remain. During
   a separate take, stop/disconnect the USB stream: a detected stream error must
   reject the take and preserve committed audio. Reconnect and test a new take.

Do not deliberately cut power during SAVE or a bank upload.

## 3. Chop playback and collection

- [ ] On a song waveform, LFO cycles K1 through start, playback point, direction,
  cents, gain and crossfade. Verify each change by ear; check another chop stays
  unchanged. Undo/reset and normal save/reboot must work.
- [ ] EDIT → LFO → SEL opens Keep selected chops. Mark nonadjacent chops with
  REC. Same-song keep must preserve retained key numbers and make discarded
  keys silent. Confirm the retained audio still plays after reboot.
- [ ] Collect into the other song's empty keys; occupied keys and the kit stay.
- [ ] From the idle USB sampler, SEL twice confirms reclamation. Retained audio,
  chop settings and patterns must remain; reported usable recording time should
  reflect reclaimed space. Cancel a separate confirmation with LFO.

Restore the loaded-state archive after destructive selection/replacement tests.

## 4. Sequencer and arrangement

- [ ] SEQ toggles grid/piano roll. Unarmed keys toggle selected-step notes.
- [ ] Arm REC, enter a chord and release all keys: advance exactly once. Play
  and record live. Four notes fit; a fifth must not replace an existing note.
- [ ] Edit one note's length/nudge via FX in the piano roll. Test chance, repeats
  and a conditional fill. REC indication must match its armed state.
- [ ] Save a test pattern only in a backed-up/empty slot. Queue another pattern;
  test next-bar and pattern-end timing, then select it again to cancel a queue.
- [ ] Arrangement: choose per-track patterns, repeats, and FX capture mutes.
  Save, restart and confirm the section settings and playback.

## 5. Transfer stability, USB output and accessibility

- [ ] Repeat a full-session backup using the normal sample loader or CLI. Check
  that it completes without stalls/timeouts and that the archive validates.
  Repeat after disconnect/reconnect; report any transfer timeout before release.


- [ ] In a computer recorder, choose FM1 as stereo audio **input**, 44.1 kHz.
  Record playback from both songs and drums. Check clean stereo capture.
- [ ] Connect FM-1 Screen Reader. WHERE reads the current page/controls. Navigate,
  adjust playback settings and inspect the USB sampler; announcements must match
  the display. Confirm opt-out/disconnect stops speech normally.

## 6. Expanded-bank release gate

The loaded bank leaves recording room; it does not exercise the new final sector.
After preserving the loaded-state backup, follow **Expanded flash bank candidate**
in [the hardware test sheet](PIGEON_V05_HARDWARE_TEST.md): a valid bank exceeding
496 KiB payload, tail playback, export comparison, restart/update preservation
and refusal of an older-layout downgrade. Do not mark this passed from the
385.6 KiB drum bank alone. Do not force an incompatible downgrade.

## Report

For each section, report PASS/FAIL, computer OS/recording application, displayed
error, and the exact action that triggered it. Record any missing notes, clicks,
dropouts, incorrect destination, lost pad, or save/reboot mismatch. Software
checks are already recorded in [acceptance](PIGEON_V05_ACCEPTANCE.md); physical
listening and timing checks above remain manual until reported.

## Settings-fix follow-up

The updated package is `build/pigeon.fwsc`, SHA-256
`ee2d3f9cfda5dee874531eeefaa68d8ea1539c334992f5bbacf0ce1d584b2c05`.
Its pre-update session is `build/pigeon-before-settings-fix-20261021.pigeon`.
It changes Settings rendering, not sample capacity or audio behavior.

- Open Settings with HOME from the waveform. Change each tab and move through
  its rows. Check that old labels/highlights do not remain and tabs do not flicker.
- Change values, especially S1/Drum/S2 MIDI channels, note-follow and wake.
  The value should appear immediately instead of an action arrow.
- Leave Settings idle, then visit Info/Credits and return. Check for stale text.
- Exit, play both songs/drums and reboot. Check Settings persist as expected.

Host pixel, sanitizer and budget checks passed; visual hardware confirmation
is still required before marking this follow-up accepted.

The Settings-fix package was flashed successfully on 2026-10-21. Readback
matched the complete bank and nine other objects immediately; current-project
byte 62 (selected track) changed from Song 2 to Song 1 on reboot, with only its
CRC also differing. Restoring the prior track selection made all ten objects
match the pre-update backup exactly. Songs, drums, notes and settings were
preserved. Backups: `build/pigeon-before-settings-fix-20261021.pigeon` and
`build/pigeon-after-settings-fix-20261021.pigeon`. Visual hardware confirmation
of Settings remains pending.

## Recorder-text follow-up

Latest package: `build/pigeon.fwsc`, SHA-256
`be6b5f3ac9a8c37a07efec0aef6535d33b2cbca8bcb7ddaf0fd127ceb71e5344`. The preceding Settings-fix package is
`build/pigeon-before-recorder-render-fix-20261021.fwsc`.

Open EDIT → FX and leave the USB sampler idle for several seconds. Threshold,
seconds/bars and the bottom status must stay steady. Turn K1/K2/K3 and confirm
only the relevant value changes. Check that the destination does not cut into
the USB SAMPLER heading. After a short take, check Ready, Retry and Save
confirmation text for the same issue. Keep selected chops should also have a
steady bottom status line.

The recorder-render package was flashed and restarted successfully. Readback
confirmed the full bank, metadata and all ten project/settings objects match
`build/pigeon-before-recorder-render-fix-20261021.pigeon`, after restoring the
pre-update selected track. The verified post-update archive is
`build/pigeon-after-recorder-render-fix-20261021.pigeon`. Hardware visual
confirmation of the steady recorder footer remains pending.

## Maintainer hardware result

On 2026-10-21 the maintainer reported all remaining hardware checks tested and passed, including USB recording and save/reboot, expanded flash tail and downgrade refusal, ordinary transfers, USB audio output, screen reader, display transitions and interrupted-update recovery. The final publication rebuild changes SCL button text to the physical SEL label; recording and storage behavior are unchanged.
