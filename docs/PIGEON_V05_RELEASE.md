# PIGEON V0.5 release gate

The published firmware remains V0.1. V0.5 is not ready for a release label until the physical FM1 checks below are recorded. No chop-naming feature is planned for this milestone.

## Software acceptance

- Clip assembly: trim, reorder, gain (−24 to +12 dB), and 0–100 ms fades; 3 ms default; each clip becomes a chop.
- Patterns: selected A–D destination, unsaved indicator, deliberate confirmation before discarding phrase edits, SAVE for all three tracks.
- Emulator: import/export `.kob` banks and `.pigeon` sessions, validate before replacing the workspace, retain encoded bank bytes and metadata, and preserve sessions across reload.
- Loader: prepare banks/sessions locally, recovery backup before full restore, and verification after device transfer.
- Release: matching source archive, download checksums, hardware/WASM builds, target cost checks, codec/allocator tests, and browser/offline checks.

The broad legacy suite also needs its old-package fixture reconciled with the current recovery-stub layout before the V0.5 gate is signed off.

## Required physical FM1 acceptance — pending

Record the FM1 hardware revision, starting firmware, package SHA-256, test bank, and observed results. Use backed-up test data and a fully charged instrument.

- [ ] Revert to official V15 by the current firmware's documented procedure, then install the candidate. Verify version, three-second splash, controls, and sound.
- [ ] Send a two-song bank plus a 16-pad kit. Read it back and compare; power-cycle and confirm it persists.
- [ ] Hold eight overlapping song voices shared across both songs; add four drum voices. Test maximum song pitch and effects for sustained playback. Record decoder underruns, errors, CPU/load and voice shedding. A software allocation limit is not a guarantee of sustained hardware performance.
- [ ] Verify extra drum hits steal drums without cutting song voices, and extra song hits leave drums intact.
- [ ] Record three sequences, SAVE to A, edit and save B, then switch, power-cycle, and reload. Verify notes, timing, automation, arrangement, selected slot, and unsaved warnings.
- [ ] Export an emulator session, restore it to hardware, then capture/read back. Compare audio, chop settings, patterns, and supported session objects.
- [ ] Make a recovery archive, change the device contents, and restore the archive. Verify the previous session returns.
- [ ] Verify the documented recovery procedure on suitable backed-up test hardware. Record the first-stub-install protection limit separately; do not describe that first installation as interruption-safe.

If sustained 8+4 playback fails, optimize or reduce the advertised hardware limit before V0.5. Do not mark physical checks complete based on host or emulator tests.

## First song walkthrough

1. Open the sample loader. Choose one or more audio files for Song 1. In Cut and merge clips, trim, adjust gain/fades, and reorder clips. Rebuild the song and preview it.
2. Apply to local Song 1. Check the storage meter. Add Song 2 and drum hits if desired, and save a bank backup.
3. With PIGEON installed, connect the FM1, stop playback, and send the bank. Wait for verification before disconnecting.
4. Select Song 1 and play its chops. SEQ opens its sequence; REC arms recording, and PLAY starts it. Stop and release pads when finished.
5. SCL opens Patterns. Choose an empty A–D destination and press SAVE. The phrase contains all three track sequences. Samples and chop settings stay in the shared bank.
6. Save a complete session backup to preserve both patterns and samples. In the emulator, Save session downloads `.pigeon`; Open session in sample loader prepares it for Restore session to FM1.

## Limits to state with the release

408 KiB is shared by both songs and the kit. Drum audio is ADPCM; song MP3 quality trades duration for fidelity. USB drives, direct recording and resampling are not supported. Reverse playback of compressed songs is not supported. Physical 12-voice performance and update recovery remain unverified until the above checks are completed.
