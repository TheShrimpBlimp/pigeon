# PIGEON 0.5-dev USB recording acceptance

This is a local development candidate, not a published release. Scratch is
excluded; ordinary chop reverse remains available. CPU clock settings and the
single-core execution model are unchanged.

## Install and prepare

1. Use the sample loader's **Firmware update** section. Choose
   `build/pigeon.fwsc`, then **Back up and install firmware**.
   Keep the downloaded full recovery archive and the released 0.4.1 package.
   Record its SHA-256 and compare it with the current candidate in
   [the acceptance record](PIGEON_V05_ACCEPTANCE.md). Do not use an older
   USB candidate for the final flash-expansion tests.
2. Reconnect after the update. Confirm the device displays **v0.5-dev**.
3. Keep at least **64 KiB free** in the sample bank for this short test. Recording
   appends into unused sectors; a full bank cannot immediately replace a song.
4. Stop transport, disarm recording, and release keys. Open the waveform, press
   **EDIT**, then **FX** for **USB SAMPLER**.

## Capture and save

1. Check the computer offers the FM1 as an **audio output** as well as an input
   and MIDI device. Set its output format to **mono / 16-bit / 22050 Hz**, if the
   host does not select that format automatically.
2. Check the input meter and remaining seconds. Play audio before recording to
   check the meter; clipping must show a persistent warning. Lower the host level,
   then start a fresh take to clear it. Press **ENV**. Wait for **RECORDING** before playing
   `build/v05-test-tone.wav` to that output. It is a four-second 440 Hz tone at
   approximately -18 dBFS, with short fades. Live monitoring is not implemented.
3. Press **ENV** again. Expect **READY / PLAY LISTEN**, without a dropout error.
   PLAY auditions the unsaved take; PLAY again stops. Confirm the sound is clean.
   Adjust K1 start/K2 end (about 1 ms), or K3 start/K4 end (100 ms).
   PLAY must audition only the selected range. Check KEEP ms and remaining time.
   Cancel the SCL confirmation with LFO and confirm the range/full take remain.
   ENV opens a retry prompt; LFO backs out without losing the take.
4. Press **SCL/Select**, then SCL again to confirm. LFO cancels confirmation
   and retains the take. This **replaces the song selected when recording began**.
   Check the destination shown on screen before starting. An empty pool
   can create Song 1. Do not disconnect during saving.
5. Saving opens the recorded waveform. Trigger its first chop.
   Confirm the trimmed duration is correct and audio is clean. Check the other song and drum kit
   still sound as before. Restart the FM1 and confirm the take remains.

Repeat with Song 1 and Song 2 selected in turn. Confirm each save changes only
its selected song, preserving the other song and all kit pads.

Repeat with a short trim and a long take spanning several flash sectors.
Check the loader reports reduced bank usage after saving and accepts a full
backup of the trimmed bank.

## Failure and cancellation checks

- Recording preparation checks unused sectors and erases only those containing
  data. Check a subsequent recording still works after discarding a take.
- Record a second short take, stop, and press **LFO** to discard. Confirm the
  previously saved bank survives a restart.
- Start another take, then stop the computer's USB output stream or disconnect
  the cable while **RECORDING**. A detected stream reset, suspend or overrun must
  reject the take. Discard the error and check the prior saved bank remains.
- Do not deliberately remove power during SAVE. The header-sector rewrite has
  the same power-loss limitation as a bank upload; recovery may be needed.

## Report

Please record the package SHA-256, computer OS, the audio device/output format offered, whether a
clean four-second tone saves and survives restart, whether Song 1/kit remain,
and the exact displayed error if capture fails. USB timing during real flash
programming and controller overrun detection are the outstanding release gates.

## Expanded flash bank candidate

Back up the current bank and projects before installing the candidate.

1. Confirm an existing bank still plays and exports identically after updating.
2. Upload a valid bank using more than 496 KiB of payload, up to 500 KiB.
   Trigger audio stored in the final sector; restart and repeat. Export the
   bank and compare it byte-for-byte with the uploaded bank.
3. Reinstall the candidate and confirm the full bank, saved projects, settings
   and recovery state remain intact.
4. With a bank using the added tail, attempt an older-layout update. Expect
   rejection before staging/erasing (OTA result -17), with the bank unchanged.
   Export and replace it with a smaller bank before attempting a downgrade.

These checks remain pending on the FM1; host tests do not verify physical
flash timing or USB behavior during flash writes.
