# PIGEON performance and compatibility audit

Source audit and host validation for PIGEON v0.4. The exact release passed the owner’s physical hardware smoke test, with samples and all ten project/settings objects preserved.

## Audio and display

`firmware/src/ko_pcm.c:ko_pcm_poll` services one cooperative decode job at a time,
prioritizing running voices before cold attacks and optional loop bridges.
`ko_pcm_next` consumes published PCM; a missing frame counts an underrun instead
of blocking the audio clock. `firmware/src/ko_mp3.c:ko_mp3_step` performs decoder
work in the main loop. Its decoder buffers are preallocated; `ko_mad_malloc` and
`ko_mad_calloc` fail closed rather than allocating during performance.

`firmware/src/main.c` calls `ko_pcm_poll` around main-loop/UI work.
`firmware/src/lcd.c:lcd_audio_service`, `lcd_sync` and `lcd_blit` keep decoder
service running during LCD transfers; blits use at most 240 pixels per chunk.
`firmware/src/ko_ui.c:ko_wave_draw`, `ko_playhead` and `ko_seq_draw` cache unchanged
regions and publish final canvas colours. Preserve these paths when adding UI.

The sampler allocator reserves eight shared song voices and four dedicated drum voices. Fading/stolen tails are distinct from the sounding-voice count; more song tracks do not add polyphony. Chop edits retain source-position/decoder checkpoints. The SEL+key ending audition uses MP3 seeking or an existing ADPCM checkpoint, without a long decoder scan in the interrupt.

Decoding remains outside the audio interrupt. Display additions use cached text
and existing integer sync calculations.
The pattern queue label now follows the configured bar/pattern-end boundary.

## Budgets and limits

The v0.4 candidate application is 375,128 bytes; RAM is 57,304 / 65,536 bytes and the audio pool is 366,292 / 376,832 bytes. Two target builds produced identical binaries and packages. The bank contains a 4 KiB header plus 496 KiB shared by songs, drums, waveform data and seek indices. Earlier firmware has 408 KiB and is checked before transfer.

`tests/target_budget.py` passes for the local v0.4 candidate. It measures weighted compiled instruction loops, not real cycles or utilization; host timings cannot establish physical CPU percentage or runtime deadlines. Static memory headroom does not measure stack use.

`tests/ko_storage_test.c` now exercises two songs plus drums with filter, delay, chorus,
reverb, distortion and separate compression enabled, after servicing cold attacks. It checks the shared
voice ceiling and unchanged decoder-error/cache-miss counters over 1,024 blocks.
The host supplies regular decoder polls; this proves scheduling/data integrity
under that model, not a guarantee of hardware deadlines at every pitch.

## Persistence and compatibility

`firmware/src/ko_store.c:ko_store_poll` postpones flash saves while transport or
voices are active. Existing A/B project recovery remains in `storage.c`.
The FUNA layout, parameter IDs, existing sample extents and metadata offsets remain compatible. v0.3 appends the KOPOOL3 sample extent.
Existing project, bank corruption, failed programming, torn-write, old-format and
GUI/native codec tests are retained. The dual-song storage tests run with native
and sanitized decoders. Extended conditions/locks still require current firmware.

Release evidence is in `build/v03-product-release.log`, `build/v03-recovery-release.log`, `build/v03-loader-browser.log`, `build/v03-browser-release.log` and `build/v03-wasm-protocol.log`. Release downloads include `manifest.json`, `validation.json` and SHA-256 checksums. The preparation script’s own host-suite flag is separate from the independently run full product suite; it is not rewritten to imply that invocation ran tests.

## Remaining hardware checks

Verify cold simultaneous attacks and repeated retriggers while scrolling/zooming,
both song tracks with drums and all FX, sync at its speed limits, recording LEDs,
queued pattern boundaries, save/reboot, and USB bank send/readback. Read decoder
underrun, error, attack, draw and poll-gap diagnostics before/after each session.
Do not claim the sample-loader issue fixed solely from simulated transfer tests.

## Inherited screen-lint limitation

The broad `tests/run_tests.sh` also draws retired Felucca synth pages and checks
their original font alignment. With PIGEON's Little Character font it reports
legacy layout/alignment failures; that whole suite is not claimed green. Those
checks remain intact. `tests/run_pigeon_tests.sh` is the product-specific gate,
retaining audio/storage compatibility checks and adding `pigeon_screen_test.c`
for actual PIGEON pages: 815 text draws across all three tracks, zero fixed-label
clipping failures. This clipping check does not prove every layout is visually
ideal or replace a physical screen review.

## Historical recovery validation (2026-10-13)

This section records the earlier stub v4 validation. Published v0.3 uses stub v5; see [the recovery guide](PIGEON_RECOVERY.md) for current behavior.

The ChoralRoot-derived PIGEON stub v4 is pinned by SHA-256 in
`firmware/stub/stub.lock`; the built image matches that pin. Target compilation
and memory checks passed. The complete PIGEON product suite, JS transport tests,
Python installer simulations, browser install-refusal test, and desktop/mobile
website checks passed. All 92 golden renders remain unchanged.

The recovery suite passes 16,143 stub checks. Flash-operation auditing covers
1,684 erase/program steps for a synthetic first install and 35 steps for a
same-stub update with another body. Interrupted snapshots resume to the expected
flash image. The first-install stub-write window is 16 flash operations; the
same-stub update has none. AddressSanitizer and UndefinedBehaviorSanitizer pass
on the stub and recovery tests. Existing tests confirm all reclaimed sample
extents are preserved, and a legacy downgrade with an active bank is refused.

Physical interrupted-update verification remains pending. A historical rebuild on 2026-10-13 reproduced
both the application and installable package byte for byte, without changing the
pinned stub. SHA-256:

- Application: `c185f69e2ba6baa890efc458de0a329eb94729d70656a013c99b4098fdd85d81`
- Package: `e66361f80bb03f64ac537b7797936c6c4a4805f88d53a2c5660c29cb0828930d`

See [recovery details and limits](PIGEON_RECOVERY.md).

## V0.1 feedback update: voice budgets

PIGEON now isolates an eight-voice song pool from a four-voice drum pool. Existing per-track voice arrays and MP3 buffers are unchanged. Dedicated host tests verify 12 active voices, drum-only stealing, sharing between both songs, and SAVE to the selected phrase. Two-song MP3/ADPCM codec stress and the configured sanitizer checks pass. The target build retains the same 56,040-byte static RAM and 366,760-byte pool allocation. Compiled render-cost checks pass; these are not a physical FM1 real-time polyphony guarantee. The overload protection remains active.

## v0.4 rendering paths

Scratch rendering is kept out of the normal `slice_render` hot path, and scratch
control values that are stable during an audio block are computed once per block
(or when touch expires), rather than once per sample. Reverse access remains a
separate compiled function. Existing normal-playback budget values are retained.
The new scratch render/cache paths have explicit independent static budgets;
no old budget was raised to conceal the additional path. These are instruction
loop regression estimates, not a wall-clock or physical CPU measurement. See the
local candidate manifest and final validation report for current memory figures.

The v0.4 115-parameter project layout consumes the four previously spare track
parameter bytes. Per-note timing and new routing are documented in
[PIGEON_NOTE_TIMING.md](PIGEON_NOTE_TIMING.md) and the
[v0.4 tutorial](PIGEON_V04_GUIDE.md). Public v0.3.1 cannot load new projects.
