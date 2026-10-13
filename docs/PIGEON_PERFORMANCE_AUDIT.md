# PIGEON performance and compatibility audit

Source audit and host validation, updated 2026-10-13. No device writes or flashes.

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

`firmware/src/voice.c:voices_busy` and the allocator enforce eight shared sounding
voices across all parts. Fading/stolen tails are distinct from that count. More
song tracks do not add polyphony. `firmware/src/ui_input.c:accel_speed` bounds
encoder acceleration and resets it after a gap or direction change; chop edits
retain source-position/decoder checkpoints rather than moving selection.

No decoder was moved into an interrupt and no DSP algorithm was changed in this
pass. Display additions use cached text and existing integer sync calculations.
The pattern queue label now follows the configured bar/pattern-end boundary.

## Budgets and limits

The recovery candidate builds with a 391,104-byte image, including its 3,808-byte
boot-stub slot. The 387,296-byte app body leaves 75,552 bytes below the reserved
commit-marker sector. Target RAM/pool totals are
recorded in the candidate manifest; the build requires at least 4 KiB RAM and
8 KiB pool headroom. Those are static allocation margins, not measured stack or
runtime CPU margins. The encoded bank still has 408 KiB shared by both songs,
drums, waveform data and seek indices. There is no additional sample space from
this audit.

Compiled interrupt cost estimates are 38,842 for audio (reference 37,502) and
35,127 for input/LED scan (reference 33,387), within the existing +10% policy.
`tests/target_budget.py` measures weighted compiled instruction loops, not real
cycles or utilization. Host timings cannot establish a physical CPU percentage.

`tests/ko_storage_test.c` now exercises two songs plus drums with filter, delay, chorus,
reverb, distortion and separate compression enabled, after servicing cold attacks. It checks the shared
voice ceiling and unchanged decoder-error/cache-miss counters over 1,024 blocks.
The host supplies regular decoder polls; this proves scheduling/data integrity
under that model, not a guarantee of hardware deadlines at every pitch.

## Persistence and compatibility

`firmware/src/ko_store.c:ko_store_poll` postpones flash saves while transport or
voices are active. Existing A/B project recovery remains in `storage.c`.
The FUNA layout, parameter IDs, sample flash map and metadata offsets are unchanged.
Existing project, bank corruption, failed programming, torn-write, old-format and
GUI/native codec tests are retained. The dual-song storage tests run with native
and sanitized decoders. Extended conditions/locks still require current firmware.

Evidence is in `build/beta-host-tests.log`, `beta-dual-storage-tests.log`,
`beta-codec-tests.log`, `beta-ui-san.log`, and `beta-target-budget.log`.
The beta script records its own checks separately and compares repeated build
hashes when requested. No new package is considered flashed or hardware verified.

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

## Recovery candidate validation (2026-10-13)

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

Hardware verification is pending. A complete rebuild on 2026-10-13 reproduced
both the application and installable package byte for byte, without changing the
pinned stub. SHA-256:

- Application: `c185f69e2ba6baa890efc458de0a329eb94729d70656a013c99b4098fdd85d81`
- Package: `e66361f80bb03f64ac537b7797936c6c4a4805f88d53a2c5660c29cb0828930d`

See [recovery details and limits](PIGEON_RECOVERY.md).
