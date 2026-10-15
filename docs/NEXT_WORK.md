# PIGEON workflow expansion — historical work log

For the current v0.3 candidate and pending hardware checks, use
[PIGEON_V03_RELEASE.md](PIGEON_V03_RELEASE.md). The entries below record earlier
implementation work and are not a current release checklist.

Authorized scope: both requested feature lists, excluding firmware performance
macros. Build and test; do not flash, publish, push, or touch user audio files.

GUI: visual chop editing and transient sensitivity; pad pitch/level/decay/chokes;
waveform drag trimming/zoom/loop preview; full source names/renaming/pad reorder
and removal; project-pattern grid with velocity/chance/repeats/length; firmware
version and local-package installer with recovery backup.

Firmware: per-chop attack/release; explicit one-shot/hold/latch; duplicate/split/
merge/copy chop settings; optional live 1/16 or 1/8 quantization; song track choke;
toggleable metronome and independent count-in; pattern variations with selective
note/lock clearing; verify last-known-good project recovery.

Compatibility: retain FUNA project layout, existing parameter IDs and 544-byte
FKO1 song records. Add envelopes/mode in previously unused chop flag bits; track
preferences in a separate CRC-bound metadata record. Host GUI writes exact
existing serialized formats. A/B project recovery already exists in storage.c;
verify it with torn-write tests rather than allocating more sample flash.

Checks: native/sanitizer playback and editing; project/metadata round trips;
8-voice/CPU and RAM/flash budgets; browser offline operation and redraw/layout;
GUI/firmware codec and serialization agreement. Physical-device verification
remains separate from host verification.

Implementation status: both lists are implemented except the deliberately
excluded performance macros. Metronome and count-in each retain an OFF option.
Project recovery reuses the existing A/B commit mechanism; torn-write checks pass.
GUI source labels remain host metadata and do not expand the device sample bank.

Verification logs are recorded in build/pigeon-ready.json. Native/sanitizer
playback, 92 unchanged golden renders, static target CPU budgets, storage/codec
round trips, baseline UI, GUI/firmware serialization, simulated USB update/backup
protocols, and browser editing/reload with the server shut down are checked.
Physical USB and performance testing of this build remain pending. No flash,
push, publication, or sample changes were performed during this work.

Second feature selection (2,3,4,5,6,8,9,11): tap and loop tempo calculation;
per-step transpose locks; existing nudge controls; held drum rolls; per-chop
choke groups; non-destructive song track gain; two A/B layouts per song;
conditional steps. All are implemented. Conditions replace step probability;
nudges retain the inherited single-repeat restriction. Alternate layouts share
audio and fit in the pre-existing metadata sector. No resampling or fill-button
feature was added. Final build/test evidence is in the performance-* logs and
build/pigeon-ready.json. Hardware verification remains pending.
