# PIGEON 0.5 final beta validation

THE FINAL BETA, prepared for publication 2026-10-21. The maintainer reports all
remaining hardware checks passed: recording, persistence, expanded flash tail,
transfers, audio output, screen reader, display and interrupted-update recovery.
These are maintainer-reported hardware results; automated evidence is listed below.
The subsequent SEL label-only rebuild is covered by software checks. Source remains
on the development branch; this is a beta, not the final 0.5 release.

## Signed GitHub build

Publication tag: `v0.5.0-beta.1`. The tagged source changes version and splash
labels only after the maintainer hardware report. The canonical download is the exact GitHub-built release asset.
The full product/recovery checks and signed-release guards passed in GitHub run
[39653247306](https://github.com/TheShrimpBlimp/pigeon/actions/runs/39653247306).
Independent `gh attestation verify` passed for the trusted shared workflow at v1,
the release tag and source commit `48f028f2ee7f6a0140dfef9d9426dc3237f2e466`.
SHA-256: `a6cbb4fbd80baa38eb99400bb4bef5ddd5d4167024a8342189a07bc6b8861bed`
(611106 bytes), matching release SHA256SUMS. Local builds are not substitutes
for that artifact. The website serves those exact attested bytes.

## Previous locally flashed candidate

Use `build/pigeon.fwsc` (611106 bytes), not the historical USB candidates
below. SHA-256: `3e25f9644fddaffcabcd2b6ae32a4d9e35248d95395756a4582254c4b007179d`.

- Embedded version: `v0.5-dev`.
- Application: 365,076 bytes; body growth room: 7372 bytes (4096 required).
- Main RAM: 53,544 / 65,536; pool RAM: 353,652 / 376,832.
- Sample bank: 504 KiB total, **500 KiB usable**.
- Host storage/codec, expanded-bank, update/recovery and loader-capacity checks
  pass. Hardware checklist completion was reported by the maintainer before the SEL label-only rebuild.
- Matching emulator WASM rebuilt successfully: 557,247 bytes, PIGEON-0.5-dev.
- Full product and recovery suites pass, including sanitizers and target budgets.
- Chrome browser checks pass for emulator playback/persistence, loader editing
  and offline reload, chop collections, screen-reader live announcements,
  desktop/mobile layout, and WAV/MP3 recording with stereo/download checks.
- Release guards reject a 0.5.0 tag while the embedded version is v0.5-dev;
  no release package was signed or published.

A rebuild may change the package hash. Record the hash of the actual tested
package with the hardware results. Earlier entries below are historical
evidence, not acceptance of this candidate.

## Hardware preparation — 2026-10-21

The current `build/pigeon.fwsc` was installed and restarted successfully. Full
post-flash readback matched both songs, the complete bank and all ten stored
project/settings objects byte-for-byte. A repacked bank adding 11 drum pads was
uploaded and matched exact bank/metadata readback, preserving both songs and all
ten objects. It leaves about 114.4 KiB free. The loaded-state recovery archive is
`build/pigeon-ready-for-hardware-tests-20261021.pigeon`.

Transfer timeouts occurred during initial standard backup/upload attempts;
paced transfers with bounded read and idempotent payload retries completed.
Ordinary transfer stability remains a release check. Listening, recording,
reboot and expanded-tail acceptance are still manual. Use the
[loaded-candidate test run](PIGEON_V05_TEST_RUN_20261021.md).

## Settings-fix hardware follow-up

The Settings-fix package was flashed successfully on 2026-10-21. Readback
matched the complete bank and nine other objects immediately; current-project
byte 62 (selected track) changed from Song 2 to Song 1 on reboot, with only its
CRC also differing. Restoring the prior track selection made all ten objects
match the pre-update backup exactly. Songs, drums, notes and settings were
preserved. Backups: `build/pigeon-before-settings-fix-20261021.pigeon` and
`build/pigeon-after-settings-fix-20261021.pigeon`. Visual hardware confirmation
of Settings remains pending.

## Recorder-render hardware follow-up

The recorder-render package was flashed and restarted successfully. Readback
confirmed the full bank, metadata and all ten project/settings objects match
`build/pigeon-before-recorder-render-fix-20261021.pigeon`, after restoring the
pre-update selected track. The verified post-update archive is
`build/pigeon-after-recorder-render-fix-20261021.pigeon`. Hardware visual
confirmation of the steady recorder footer remains pending.

## Historical USB candidate

- Package: `build/pigeon-0.5-dev-usb.fwsc` (610995 bytes).
- SHA-256: `6d66fadda93d5f7aef419616366b8ac08f038892316b0730e0690a749d8efe76`.
- Application: 375812 bytes, 1020 bytes below the reserved image limit.
- Main RAM: 61384 / 65536 bytes; 4152 free (56 above the required 4096).
- Pool: 363652 / 376832 bytes; 13180 free (4988 above the required 8192).
- Scratch disabled; per-chop reverse retained. USB sampling enabled.
- No CPU-clock or second-core changes.

## Software evidence

Native product, persistence, malformed-request, compatibility, golden-audio and
sanitizer suites passed. New playback-point, crossfade/gain/tuning, reset/history,
USB receive and USB NOR-persistence checks are included in the product runner.
The native phase passed; remaining gates were rerun successfully after updating
the target-profile audit to require Scratch's absence for this product variant.
All target audio-cost checks passed without increasing their budgets.

USB tests cover valid descriptors and collection order, mono PCM delivery,
ring wrap, malformed/oversized packets, overflow, reset/disconnect, creating a
new bank, a replacement Song 2, original Song 1 preservation, cancelled takes
across a rescan, actual ADPCM decoding, odd sample counts, capacity stops, failed
programming, UI navigation and all USB screen states without text clipping.
Ordinary MP3 reverse passed with Scratch disabled. Product isolation and asset
checks passed on the target build.

Loader unit/browser checks passed for modern/legacy metadata, normalized gain,
playback previews, reset/undo, named bank/session save/overwrite/load, exported
session contents, collected chops, ordered assembly, import/export, selected-chop
trimming, preserved other slots and undo. The standalone loader development copy
was synchronized and its editing checks passed. Browser reader-adapter protocol
checks passed.

## Release gates

Follow [the hardware test sheet](PIGEON_V05_HARDWARE_TEST.md). Host tests cannot
establish USB enumeration, endpoint-DMA behavior, real page-program timing or
reliable reception while flash access masks interrupts. Rejecting an overrun is
intentional; any recording dropout still blocks release.

The header save is not power-loss atomic; keep a full recovery archive. A new
recording uses unused pool sectors alongside the existing bank, so saving a
replacement does not reclaim its old payload automatically. The FM1 has an
explicit confirmed reclaim action; the loader can also repack the bank.
Selected-chop retention is available on the FM1 and in the loader, with different
key-number behavior described in the [tutorial](PIGEON_V05_GUIDE.md).

The public site/emulator stay on the existing release. The matching 0.5
WebAssembly build and browser acceptance now pass locally. The final release
version/tag, signed GitHub build, attestation verification and deployment follow
hardware acceptance. Local build checks do not constitute GitHub attestation.

## First hardware result and receive-path correction

The original candidate reported an immediate USB dropout and rejected the take.
A fresh recovery archive confirmed the committed bank CRC was unchanged. The
follow-up `build/pigeon-0.5-dev-usb-rx-fix.fwsc` uses the existing OUT endpoint
FIFO flush/rearm sequence rather than only clearing packet-ready, and separates
stream-reset, packet-size, ring-full, receive-overrun, disconnect and suspend
errors. Receiver/persistence tests and target build passed; recording success
on hardware remains unconfirmed. Its SHA-256 is
`7d188e822f10d132050dd501c3b99b3d45bbe2880f7f570e27ebcef9fdd27bdb`.

### USB receive overrun follow-up (2026-10-20)

The RX rearm candidate reported `USB RECEIVE OVERRUN` on hardware. Flash page
programming masks interrupts while XIP is unavailable, preventing the regular
USB service from draining the host's 1 ms packets. The new candidate polls USB
OUT from RAM during page-program busy waits; it retains FIFO/ring overrun
rejection and bounds SIE waits. No USB servicing is added to sector erases:
recording is armed only after pre-erasure finishes.

Candidate: `build/pigeon-0.5-dev-usb-ram-fix.fwsc`, SHA-256
`74b338247ba60258d83f3a0f6b75c6eb64d79627b88963faa1853a5d01be5fb1`.
Target build: 376448-byte image, unchanged RAM/pool consumption; RAM audit
1066 instructions with no calls. USB descriptor/ring, RAM receive mock,
recording persistence and target budget checks passed. Backup before flash:
`build/pigeon-before-usb-ram-fix-20261020.pigeon`; saved bank CRC matches the
previous backup. Real-device recording acceptance remains pending.

### Recorder refinement (2026-10-20)

The user reports USB recording works on the RAM-service candidate. This confirms
basic capture on the FM1; it does not establish every acceptance check above.

The next candidate protects finished and failed takes against accidental ENV
restart: Save or Discard is required first. Preparation checks each free sector
and erases only sectors containing data, avoiding needless erase cycles on blank
flash. Tests cover an entirely blank bank, dirty first/last bytes in sectors,
near-full capture with waveform reserve, and preserving unsaved takes.

The FM1 was disconnected during refinement, so this candidate is not flashed.

Candidate: `build/pigeon-0.5-dev-usb-polish.fwsc`, SHA-256
`9bc902f25829819bb996cd753d2fa13d2508bc30e8ab461279fdb97a6dcea5ad`.
Native recording persistence and ASan/UBSan checks passed, including the new
protection and erase tests. Target build, RAM call audit, target budgets and
whitespace checks passed. Image is 376480 bytes; RAM and pool usage unchanged.

### Selected-song USB destination (2026-10-20)

USB capture now remembers Song 1 or Song 2 at recording start. Save replaces
that song and preserves the other song, kit zones and kit payload. The screen
shows the destination; reader help describes selected-song saving. Empty-bank
Song 2 and drum-track capture are refused rather than silently redirected.

Replacing Song 1 appends its payload after existing Song 2 audio. Firmware,
Python and browser validators accept either nonoverlapping song order while
still rejecting overlap and kit collisions. This layout requires the updated
0.5 firmware/loader; retain a complete backup before downgrading.

Candidate: `build/pigeon-0.5-dev-usb-selected.fwsc`, SHA-256
`3b1271b435004ac76263c8226780b8c17bca3c361fae8d2381dcb8667a9ffc25`.
Target image: 376544 bytes, unchanged RAM/pool consumption, RAM audit has no
calls. Selected-song capture/persistence and ASan/UBSan checks passed, including
preservation of a real 16-pad kit and the other song. Browser bank tests accept
both song orders and reject overlap. Target budgets and whitespace checks pass.
This candidate has not been flashed; selected-song hardware acceptance remains.

The full storage/codec native and sanitizer regression suite also passed,
including existing dual-song banks, expanded storage, MP3 scratch compatibility
and per-chop reverse.

### Recorder workflow candidate (2026-10-20)

Implemented live input peak and latched full-scale clipping feedback, elapsed
and available seconds, unsaved PLAY audition, protected ENV retry, two-press
SEL replacement confirmation with LFO back, and returning to the recorded song's
waveform after Save. Discard clears the elapsed counter. Reader help exposes
input, clipping and remaining time and describes the new controls.

Preview matches the saved IMA decoder sample-for-sample in host tests. Tests
also cover natural audition completion, stopping preview, cancelled replacement
and retry, destination preservation, empty/full banks, all recorder screen states,
failed takes and preservation of the other song and a real 16-pad kit. Native
and ASan/UBSan recording tests passed. KO regression, USB receive and RAM-poll
tests passed, as did target budgets and whitespace checks.

Lossless splash compression saves 1273 asset bytes without extra canvas RAM.
The compiled decoder matches all 57600 expected RGB565 pixels under sanitizers;
the existing actual-screen splash regression passes. The image appearance is
unchanged.

Candidate: `build/pigeon-0.5-dev-usb-workflow.fwsc`, SHA-256
`284bf431ed943a50ffea51efa08d9b0ab640ce08902a1c6e32b9ecc1fa3ed17e`.
Target image is 376484 bytes; main RAM 61384/65536 bytes, pool 363668/376832
bytes. RAM audit: 1066 instructions with no calls. This candidate has not been
flashed. Check incoming meter refresh, preview level/stop, confirmation gestures,
recording during LCD refresh and immediate waveform editing on the FM1.

### Trim-before-save candidate (2026-10-20)

Ready takes expose K1/K2 start/end at 22-sample steps (approximately 1 ms),
with K3/K4 at 2205 samples (100 ms). Bounds retain at least 64 samples. PLAY
reconstructs the original IMA state and auditions only the selected interval.
Trim is applied only on confirmed Save; cancellation retains the complete take
and the current bounds. Saving re-encodes the retained interval, rebuilds the
waveform and reduces bank usage. Display shows retained milliseconds; reader
knob announcements follow both fine and coarse controls.

Native and ASan/UBSan tests passed for odd starts/counts, multi-sector output,
exact-sector output with following waveform erase, 64-sample takes, tail-only
trims and large leading cuts. Tests compare audition with an independent IMA
decoder, check retained-audio error after re-encoding, exact saved counts and
reduced bank usage, reboot validation, full-take retention on cancellation and
committed-bank survival after a trim write failure. Other-song and 16-pad kit
preservation remain covered. KO/splash regression, reader browser adapter,
compiled splash pixel checks and target budgets pass. No whitespace errors.

RGB565-identical splash palette entries are merged before compression; all
57600 displayed pixels remain identical. Asset size is now 15169 bytes.

Candidate: `build/pigeon-0.5-dev-usb-trim.fwsc`, SHA-256
`ad95412948644de71fe6284572a71157b74d638880d0d3f9cd90698297eeefa6`.
Target image: 375800 bytes; main RAM 61384/65536, pool 363676/376832. RAM audit
has 1066 instructions with no calls. This candidate has not been flashed.
Hardware acceptance must check knob response, trimmed audition, Save timing,
reclaimed space and persistence with the other song/kit preserved.

## THE FINAL BETA publication

Maintainer reported all remaining hardware checks passed on 2026-10-21.
Final SEL label-only package: SHA-256 `3e25f9644fddaffcabcd2b6ae32a4d9e35248d95395756a4582254c4b007179d`; 611106 bytes.
Embedded version remains `v0.5-dev`; the website labels this candidate **THE FINAL BETA**.
No audio, storage, USB protocol or clock changes were made after that report.
