# PIGEON v0.3 release gate

The main release is v0.3, without the experimental screen reader. This package was flashed successfully to an FM1, and sample-bank/chop metadata matched before and after the update. Extended physical checks below remain pending. No chop-naming feature is planned for this milestone.

## Software acceptance

- Clip assembly: trim, reorder, gain (−24 to +12 dB), and 0–100 ms fades; 3 ms default; whole clips occupy the first keys and automatic slices fill the remaining slots.
- Patterns: selected A–D destination, unsaved indicator, deliberate confirmation before discarding phrase edits, SAVE for all three tracks.
- Emulator: import/export `.kob` banks and `.pigeon` sessions, validate before replacing the workspace, retain encoded bank bytes and metadata, and preserve sessions across reload.
- Loader: prepare banks/sessions locally, recovery backup before full restore, and verification after device transfer.
- Release: matching source archive, download checksums, hardware/WASM builds, target cost checks, codec/allocator tests, and browser/offline checks.

The legacy recovery test now uses a deterministic synthetic legacy-layout application body, rather than truncating a modern KOPOOL2 boot-stub image and mislabelling it legacy. The fixture is only data for the simulated loader; it is never executable firmware.

## Additional v0.3 features

- Chop swap and move in the loader and firmware. On hardware: EDIT → LFO → ENV opens key assignment; K1 selects the source, K2 the destination, K3 clockwise swaps, K4 clockwise moves and shifts the keys between. Stop playback and release notes first. Existing chop undo restores the previous assignments. Existing sequenced key numbers stay unchanged, so rearranging chops changes what those notes trigger.
- Copy drum audio and settings to any of the 16 pad keys in the sample loader. Empty intermediate keys become silent pads; existing other pads remain assigned. This is a bank edit; send the resulting bank to use it on hardware.
- Loader undo/redo covers committed bank, chop, pad, name and pattern/session edits, with 20 snapshots per open page. Ctrl+Z / Cmd+Z undo, Ctrl+Shift+Z / Cmd+Shift+Z and Ctrl+Y redo. Inputs keep their native text undo. Staged source files and uncommitted trim fields are outside this history. Hardware transfers and library deletion are not undone.
- Named local session library: save, load, rename, delete and download banks or complete sessions. Entries stay in this browser's storage; download backups before clearing browser data.
- FM1 panel connection in the emulator uses existing SysEx commands 110/111. It leases control for 1.5 seconds, renewed every 500 ms, and relays logical keys, buttons, seven encoder roles and master volume. Local playback stops on entry. Explicit disconnect, hidden page and page exit release it; a dropped connection expires on-device. This requires compatible PIGEON firmware and is pending physical verification. Ordinary MIDI input remains available without SysEx permission.

## Recording and external storage investigation

The present USB stack (`firmware/src/usb.c`) implements the FM1 as a USB device. Its optional USB audio endpoint sends the FM1's output to a computer; it does not receive audio into the sampler. The audio HAL provides the current output path and the master-knob ADC, not a sampler line-input capture path. Implementing recording needs a verified usable audio input, buffering/encoding and flash-write timing tests on actual hardware.

External USB disks would require a supported USB host mode, power/role handling, mass-storage transport and a filesystem; these are not implemented by the current device stack. The available source does not establish whether the board can safely provide the required host power or route an audio input. Schematics, controller documentation and physical tests are needed before promising either feature. No hardware impossibility is inferred from their absence in this firmware.

## Local verification

The candidate builds for hardware and WebAssembly. Target cost checks, native chop-copy/reassignment UI tests (including ASan/UBSan), controller protocol tests, loader offline browser tests, standalone editing/browser/launcher tests, emulator browser tests, and static-site link/checksum checks pass. The recovery simulation now installs the expanded layout from the synthetic legacy fixture and retains its damage/refusal and readback checks.

The broader generic Felucca suite still reports existing font-spacing threshold failures (S: 0.266 px; M: 0.253 px, against 0.25 px) and UI lint/alignment findings. Its audio-only harness now supplies the panel ADC mirror required by the existing master-knob code. Do not treat the broad suite or the physical release gate as green.

## PIGEON interface isolation

PIGEON branding now requires the sampler-only profile. That profile excludes the upstream page registry, the legacy SLICE editor and graph, and the generic renderer fallback. A shared-state guard repairs disabled sampler mode, stale page indices and reserved-track selection before input or drawing; page-family navigation resolves to PIGEON screens. Waveform peaks use PIGEON's independent cache instead of the legacy editor's state.

Native isolation tests cover old settings, invalid page/track indices, page-family requests and repeated panel taps, including ASan/UBSan. Hardware and WASM builds, voice allocation, sampler UI/performance, WASM bank/audio/persistence and target-cost checks pass. The standalone WASM audio fixture now waits for the existing three-second splash before testing playback. The main package has been flashed successfully; detailed manual control and endurance checks remain pending.

## Required physical FM1 acceptance — pending

Record the FM1 hardware revision, starting firmware, package SHA-256, test bank, and observed results. Use backed-up test data and a fully charged instrument.

- [ ] Revert to official V15 by the current firmware's documented procedure, then install the candidate. Verify version, three-second splash, controls, and sound.
- [ ] Send a two-song bank plus a 16-pad kit. Read it back and compare; power-cycle and confirm it persists.
- [ ] Hold eight overlapping song voices shared across both songs; add four drum voices. Test maximum song pitch and effects for sustained playback. Record decoder underruns, errors, CPU/load and voice shedding. A software allocation limit is not a guarantee of sustained hardware performance.
- [ ] Verify extra drum hits steal drums without cutting song voices, and extra song hits leave drums intact.
- [ ] Record three sequences, SAVE to A, edit and save B, then switch, power-cycle, and reload. Verify notes, timing, automation, arrangement, selected slot, and unsaved warnings.
- [ ] Export an emulator session, restore it to hardware, then capture/read back. Compare audio, chop settings, patterns, and supported session objects.
- [ ] Connect the physical FM1 panel to the emulator. Verify all keys, each encoder direction, all buttons, master volume, quick taps, held combinations, disconnect, tab hiding and lease timeout. Confirm local playback resumes without stuck notes or unintended edits.
- [ ] Make a recovery archive, change the device contents, and restore the archive. Verify the previous session returns.
- [ ] Verify the documented recovery procedure on suitable backed-up test hardware. Record the first-stub-install protection limit separately; do not describe that first installation as interruption-safe.

If sustained 8+4 playback fails, optimize or reduce the advertised hardware limit before v0.3. Do not mark physical checks complete based on host or emulator tests.

## First song walkthrough

1. Open the sample loader. Choose one or more audio files for Song 1. Drag to select and delete sections, split at the cursor and drag clip chips to reorder. Open Clip settings for gain/fades, then preview.
2. Apply to local Song 1. Check the storage meter. Add Song 2 and drum hits if desired, and save a bank backup.
3. With PIGEON installed, connect the FM1, stop playback, and send the bank. Wait for verification before disconnecting.
4. Select Song 1 and play its chops. SEQ opens its sequence; REC arms recording, and PLAY starts it. Stop and release pads when finished.
5. SCL opens Patterns. Choose an empty A–D destination and press SAVE. The phrase contains all three track sequences. Samples and chop settings stay in the shared bank.
6. Save a complete session backup to preserve both patterns and samples. In the emulator, Save session downloads `.pigeon`; Open session in sample loader prepares it for Restore session to FM1.

## Limits to state with the release

The published v0.3 firmware, emulator and loader share 496 KiB between both songs and the kit. Earlier firmware retains 408 KiB; the loader checks device capacity before writing. Extended expanded-pool hardware endurance checks remain pending. Drum audio is ADPCM; song MP3 quality trades duration for fidelity. USB drives, direct recording and resampling are not supported. Reverse playback of compressed songs is not supported. Physical 12-voice performance and update recovery remain unverified until the above checks are completed.

## v0.3 release verification — October 15, 2026

- Hardware package builds successfully and two consecutive candidate builds are byte-identical.
- Target memory and instruction-budget checks pass: app 336,268 bytes, RAM 55,464 / 65,536 bytes, audio pool 355,664 / 376,832 bytes.
- Profile audit rejects legacy screens, unused synth renderers, raw splash pixels, preset RAM banks, factory patches and bitmap keycaps. The lossless splash is 18,295 bytes.
- Full PIGEON product suite passes, including sanitizer runs and all 92 unchanged reference audio renders.
- Expanded-bank codec/storage tests pass in normal and sanitizer builds: full 496 KiB payload, old-layout physical addresses, protected neighboring firmware/boot sectors, old banks and reboot CRC validation.
- Recovery simulation, boot-stub checks and installer safety checks pass. Official V15 restore simulation is skipped because its package is unavailable; this does not establish physical recovery safety.
- v0.3 WebAssembly builds and passes empty-slot silence, MP3 playback, expanded-bank playback, unlimited mode, CRC rejection, legacy backup preservation, transport and persistence checks.
- Published files and checksums are under `build/releases/pigeon-0.3/` and [GitHub Releases](https://github.com/TheShrimpBlimp/pigeon/releases/tag/v0.3). Basic installation/reboot and sample/chop preservation were checked on an FM1.

The remaining extended physical checks above are pending; completed software checks do not establish hardware endurance or interruption safety.

## Boot-selection and empty-slot correction

A physical flash of the earlier candidate exposed a boot issue: restoring a
project selected Song 2 and initialized that song context while the shared
sampler state remained disabled. This allowed an inherited missing-sample tone
to sound. Browser builds had a separate silence guard and did not expose it.

The v0.3 build selects Song 1 after autosave restoration, enables the
shared sampler state, repairs legacy sources on the three active sampler tracks
while preserving sequence notes, and removes the missing-audio tone generator
and all its call sites from SAMPLE, SLICE and GRAIN. The source no longer contains
that fallback. Reserved compatibility-track data remains round-trip safe.

Native hardware-path regression tests cover restored Song 2, empty Song 2, source
repair, preserved sequence data, and project reload. Generic source tests require
zero output and ended voices for missing audio. The corrected main build was flashed and restarted successfully. Manual control, power-cycle and endurance checks remain separate from these regression tests.

## Latest changes

- Page buttons open immediately on press. HOME returns to the waveform; HOME on the waveform opens Settings.
- Hold EDIT and trigger a chop to audition its final two seconds without recording or changing boundaries.
- Merged audio keeps whole imported sections on the first chop keys, then fills remaining keys with automatic slices (up to 32 total). Reapply existing merged songs in the updated loader to generate these slots.
- Firmware, emulator and loader now share the 496 KiB pool. Older 408 KiB devices are checked before upload and cannot receive oversized banks.
